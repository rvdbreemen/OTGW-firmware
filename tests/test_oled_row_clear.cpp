// TASK-1151 — OLED status rows must not keep the tail of longer previous text.
//
// Reported on Discord 2026-06-12 by tjfs: plugging Ethernet into an OTGW32 showed
// "Ethernet6617823" (the digits were the tail of the previous SSID) and unplugging
// showed "0.0.0.0.1.150" (the tail of the previous IP).
//
// OLED.ino clears the display only on a page change; SSD1306Ascii overwrites text
// in place, so a shorter redraw leaves the remainder of the old text on screen.
//
// This test models the display as a character grid and exercises both the old and
// the new positioning helper against the exact strings from the report. The old
// behaviour MUST reproduce the reported corruption (otherwise the test proves
// nothing) and the new one MUST NOT.
//
// It also scans OLED.ino itself and asserts every row write goes through oledRow(),
// because a fix that relies on 38 call sites each remembering to clear is a fix that
// will rot. That static check is what actually covers the whole class.
//
// Build: g++ -std=c++17 tests/test_oled_row_clear.cpp -o tests/test_oled_row_clear.out
// Run:   ./tests/test_oled_row_clear.out   (exit 0 = pass)

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

static int g_failures = 0;

static void check(bool pass, const std::string& what) {
  std::cout << (pass ? "PASS: " : "FAIL: ") << what << "\n";
  if (!pass) g_failures++;
}

// --------------------------------------------------------------------------
// Minimal SSD1306Ascii stand-in: 8 rows x 21 columns of characters.
// Mirrors the subset OLED.ino uses: setCursor(col,row), setRow, setCol,
// clearToEOL, print, clear.
// --------------------------------------------------------------------------
static constexpr int OLED_COLS = 21;
static constexpr int OLED_ROWS = 8;

class FakeDisplay {
 public:
  FakeDisplay() { clear(); }

  void clear() {
    for (int r = 0; r < OLED_ROWS; r++)
      for (int c = 0; c < OLED_COLS; c++) cell_[r][c] = ' ';
    row_ = col_ = 0;
  }
  // SSD1306Ascii::setRow sets the text row and leaves the column alone.
  void setRow(int row) { row_ = row; }
  void setCol(int col) { col_ = col; }
  void setCursor(int col, int row) { col_ = col; row_ = row; }
  void clearToEOL() {
    for (int c = col_; c < OLED_COLS; c++) cell_[row_][c] = ' ';
    col_ = OLED_COLS;
  }
  void print(const char* s) {
    for (const char* p = s; *p && col_ < OLED_COLS; ++p) cell_[row_][col_++] = *p;
  }

  std::string rowText(int row) const {
    std::string out(cell_[row], cell_[row] + OLED_COLS);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
  }

 private:
  char cell_[OLED_ROWS][OLED_COLS];
  int row_ = 0, col_ = 0;
};

// The two positioning strategies under test.
static void oldRowStart(FakeDisplay& d, int row) {  // pre-TASK-1151
  d.setRow(row);
  d.setCol(0);
}
static void newRowStart(FakeDisplay& d, int row) {  // oledRow() in OLED.ino
  d.setCursor(0, row);
  d.clearToEOL();
  d.setCursor(0, row);
}

int main() {
  // ---------------------------------------------------------------- Ethernet
  // tjfs: "Ethernet6617823" — "Ethernet" written over "WiFi: <ssid>".
  {
    FakeDisplay d;
    oldRowStart(d, 2); d.print("WiFi: "); d.print("Net6617823");
    const std::string before = d.rowText(2);
    oldRowStart(d, 2); d.print("Ethernet");
    const std::string oldResult = d.rowText(2);

    check(before == "WiFi: Net6617823", "setup: row 2 initially shows the WiFi SSID (got \"" + before + "\")");
    check(oldResult != "Ethernet",
          "negative control: old behaviour leaves a tail (got \"" + oldResult + "\")");
    check(oldResult.rfind("Ethernet", 0) == 0 && oldResult.size() > 8,
          "negative control: the tail is the remainder of the SSID, as reported");
  }
  {
    FakeDisplay d;
    newRowStart(d, 2); d.print("WiFi: "); d.print("Net6617823");
    newRowStart(d, 2); d.print("Ethernet");
    const std::string got = d.rowText(2);
    check(got == "Ethernet", "fixed: row 2 reads exactly \"Ethernet\" (got \"" + got + "\")");
  }

  // ---------------------------------------------------------------------- IP
  // tjfs: "0.0.0.0.1.150" — a shorter IP written over a longer one.
  {
    FakeDisplay d;
    oldRowStart(d, 3); d.print("IP: "); d.print("192.168.1.150");
    oldRowStart(d, 3); d.print("IP: "); d.print("0.0.0.0");
    const std::string oldResult = d.rowText(3);
    check(oldResult == "IP: 0.0.0.0.1.150",
          "negative control: old behaviour reproduces the reported \"0.0.0.0.1.150\" (got \"" + oldResult + "\")");
  }
  {
    FakeDisplay d;
    newRowStart(d, 3); d.print("IP: "); d.print("192.168.1.150");
    newRowStart(d, 3); d.print("IP: "); d.print("0.0.0.0");
    const std::string got = d.rowText(3);
    check(got == "IP: 0.0.0.0", "fixed: row 3 reads exactly \"IP: 0.0.0.0\" (got \"" + got + "\")");
  }

  // ------------------------------------------------------- conditional rows
  // A row the page decides not to write keeps its old content; oledClearRow()
  // blanks it. Models the RSSI row when the transport switches to Ethernet.
  {
    FakeDisplay d;
    newRowStart(d, 4); d.print("RSSI: -62 dBm");
    check(d.rowText(4) == "RSSI: -62 dBm", "setup: RSSI row populated while on WiFi");
    newRowStart(d, 4);  // oledClearRow() is oledRow() with nothing printed
    check(d.rowText(4).empty(), "fixed: skipped RSSI row is blank on Ethernet (got \"" + d.rowText(4) + "\")");
  }

  // ------------------------------------------- static guarantee over OLED.ino
  // Every row write must go through oledRow(); a raw setRow() would be a site
  // that silently keeps the old hazard.
  {
    const char* candidates[] = {
      "src/OTGW-firmware/OLED.ino",
      "../src/OTGW-firmware/OLED.ino",
      "../../src/OTGW-firmware/OLED.ino",
    };
    std::string src;
    for (const char* path : candidates) {
      std::ifstream f(path);
      if (f) { std::stringstream ss; ss << f.rdbuf(); src = ss.str(); break; }
    }
    if (src.empty()) {
      std::cout << "SKIP: OLED.ino not found from this working directory\n";
    } else {
      size_t raw = 0, pos = 0;
      while ((pos = src.find("oledDisplay.setRow(", pos)) != std::string::npos) { raw++; pos++; }
      size_t viaHelper = 0; pos = 0;
      while ((pos = src.find("oledRow(", pos)) != std::string::npos) { viaHelper++; pos++; }
      check(raw == 0, "OLED.ino has no raw oledDisplay.setRow() left (found " + std::to_string(raw) + ")");
      check(viaHelper > 30, "OLED.ino routes its rows through oledRow() (" + std::to_string(viaHelper) + " references)");
      check(src.find("clearToEOL") != std::string::npos, "OLED.ino actually calls clearToEOL()");
    }
  }

  std::cout << "\n" << g_failures << " failures\n";
  return g_failures == 0 ? 0 : 1;
}
