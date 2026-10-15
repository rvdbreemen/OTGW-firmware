// OTGW safeboot updater (ADR-191, TASK-1223.4).
//
// Runs from the app/factory partition of the 4MB layout. The bootloader starts
// it when otadata is blank (the main firmware erased it to hand over) or when
// ota_0 does not validate or was rolled back. It joins WiFi with the
// credentials the main firmware stored in NVS, serves one upload page, writes
// the uploaded image to ota_0 and restarts into it.
//
// Pattern after MycilaSafeBoot (MIT, Mathieu Carbou) and Tasmota safeboot:
// point the boot partition back at ota_0 on start, so a plain reboot returns
// to a valid main firmware. Differences: WiFi credentials, hostname and the
// admin password come from NVS (written by the main firmware), and a
// rolled-back ota_0 is never booted again until a new image is uploaded.
//
// UART0 is the PIC link on Classic boards: nothing here writes to it. Serial
// is the USB CDC port (ARDUINO_USB_CDC_ON_BOOT=1), and IDF logging is off.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_log.h>

#define SAFEBOOT_VERSION "1.0.0"

// NVS namespace shared with the main firmware (safebootSyncNvs()).
static const char NVS_NS[] = "otgw-sb";

static WebServer server(80);
static String updError;
static bool bootPointedAtApp = false;
static bool appRolledBack = false;
static char hostName[33] = "OTGW";
static char adminPw[41] = "";

static const char PAGE_HEAD[] PROGMEM =
  "<!doctype html><html><head><meta charset=utf-8>"
  "<meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>OTGW safeboot</title><style>"
  "body{font-family:-apple-system,'Segoe UI',Roboto,Arial,sans-serif;max-width:560px;margin:24px auto;padding:0 16px;"
  "background:#f1f7f8;color:#2a2d2f}"
  "@media(prefers-color-scheme:dark){body{background:#1a1a1a;color:#e0e0e0}.c{background:#2c2c2e!important}}"
  ".c{background:#fff;border-radius:8px;padding:16px;margin:12px 0;box-shadow:0 2px 4px rgba(0,0,0,.1)}"
  ".w{border-left:4px solid #dc3545}button,input[type=submit]{padding:8px 16px;border:0;border-radius:6px;"
  "background:#0288a0;color:#fff;cursor:pointer}progress{width:100%}"
  "</style></head><body><h2>OTGW safeboot</h2>";

static const char PAGE_FORM[] PROGMEM =
  "<div class=c><h3>Upload firmware</h3>"
  "<p>Select the <b>.ino.bin</b> firmware file (not the littlefs or merged image).</p>"
  "<input type=file id=f accept='.bin'> <button onclick=up()>Upload</button>"
  "<p><progress id=p max=100 value=0 hidden></progress></p><p id=m></p></div>"
  "<div class=c><form method=POST action=/reboot><input type=submit value='Restart'></form></div>"
  "<script>"
  "function up(){var f=document.getElementById('f').files[0],m=document.getElementById('m'),"
  "p=document.getElementById('p');if(!f){m.textContent='Choose a file first.';return;}"
  "var d=new FormData();d.append('firmware',f,f.name);var x=new XMLHttpRequest();"
  "x.upload.onprogress=function(e){if(e.lengthComputable){p.hidden=false;p.value=e.loaded*100/e.total;}};"
  "x.onload=function(){m.textContent=x.responseText;if(x.status==200)"
  "setTimeout(function(){location.href='/';},20000);};"
  "x.onerror=function(){m.textContent='Upload failed (connection lost).';};"
  "x.open('POST','/update');x.send(d);m.textContent='Uploading, do not power off...';}"
  "</script></body></html>";

static const esp_partition_t* appPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

static void loadNvs() {
  Preferences prefs;
  if (!prefs.begin(NVS_NS, true)) return;   // read-only; absent on a first boot
  prefs.getString("host", hostName, sizeof(hostName));
  prefs.getString("pw", adminPw, sizeof(adminPw));
  prefs.end();
  if (hostName[0] == '\0') strlcpy(hostName, "OTGW", sizeof(hostName));
}

// Same rule as isSameOriginRequest() in the main firmware (restAPI.ino): the host
// part of Origin (else Referer) must equal the Host header exactly. No Origin and
// no Referer means a non-browser client (curl), which is allowed.
static bool sameOrigin() {
  String origin = server.header("Origin");
  if (origin.length() == 0) origin = server.header("Referer");
  if (origin.length() == 0) return true;
  String host = server.hostHeader();
  if (host.length() == 0) return true;
  int sep = origin.indexOf("://");
  if (sep < 0) return false;
  int start = sep + 3;
  int slash = origin.indexOf('/', start);
  String originHost = origin.substring(start, slash < 0 ? origin.length() : slash);
  return originHost.equalsIgnoreCase(host);
}

// ADR-056 parity: Basic Auth (user "admin") and the same-origin check, both only
// when the main firmware has an admin password set.
static bool authorized() {
  if (adminPw[0] == '\0') return true;
  if (!server.authenticate("admin", adminPw)) {
    server.requestAuthentication();
    return false;
  }
  if (server.method() == HTTP_POST && !sameOrigin()) {
    server.send(403, "text/plain", "CSRF protection: invalid origin");
    return false;
  }
  return true;
}

static void handleRoot() {
  if (!authorized()) return;
  String page = FPSTR(PAGE_HEAD);
  if (appRolledBack) {
    page += F("<div class='c w'><b>The firmware failed to start and was rolled back.</b>"
              " Upload a working firmware image.</div>");
  } else if (!bootPointedAtApp) {
    page += F("<div class='c w'><b>No valid firmware is installed.</b> Upload a firmware image.</div>");
  }
  page += FPSTR(PAGE_FORM);
  server.send(200, "text/html", page);
}

static void handleInfo() {
  if (!authorized()) return;
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"safeboot\":\"%s\",\"hostname\":\"%s\",\"ip\":\"%s\",\"ap_mode\":%s,"
           "\"app_valid\":%s,\"app_rolled_back\":%s,\"auth\":%s,\"heap\":%lu}",
           SAFEBOOT_VERSION, hostName,
           (WiFi.getMode() & WIFI_AP) ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str(),
           (WiFi.getMode() & WIFI_AP) ? "true" : "false",
           bootPointedAtApp ? "true" : "false", appRolledBack ? "true" : "false",
           adminPw[0] ? "true" : "false", (unsigned long)ESP.getFreeHeap());
  server.send(200, "application/json", buf);
}

static void handleUpdateDone() {
  if (!authorized()) return;
  if (Update.hasError() || updError.length()) {
    server.send(500, "text/plain", "Update failed: " + updError + ". The unit stays in safeboot.");
    Serial.printf("[safeboot] update FAILED: %s\n", updError.c_str());
    return;
  }
  server.send(200, "text/plain", "Update OK. Restarting into the new firmware...");
  Serial.println("[safeboot] update OK, restarting");
  delay(500);
  ESP.restart();
}

static void handleUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    updError = "";
    // Auth and origin are checked here, before Update.begin(), so an unauthorised
    // or cross-origin upload never touches ota_0. The headers arrive before the body.
    if (adminPw[0] && !server.authenticate("admin", adminPw)) { updError = "unauthorized"; return; }
    if (adminPw[0] && !sameOrigin()) { updError = "invalid origin"; return; }
    Serial.printf("[safeboot] upload start %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) updError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE && !updError.length()) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) updError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_END && !updError.length()) {
    // end(true) verifies the image and sets ota_0 as the boot partition.
    if (!Update.end(true)) updError = Update.errorString();
    Serial.printf("[safeboot] upload end %u bytes\n", (unsigned)up.totalSize);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    updError = "aborted";
  }
}

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);   // IDF console is UART0 = PIC link
  Serial.begin(115200);                   // USB CDC
  delay(200);
  Serial.println("\n[safeboot] start " SAFEBOOT_VERSION);

  loadNvs();

  // A rolled-back ota_0 (bootloader app rollback, ADR-191) must not run again:
  // stay here until a new image is uploaded. Otherwise point the boot partition
  // back at ota_0; esp_ota_set_boot_partition() validates the image first, so
  // an invalid ota_0 also keeps us in safeboot.
  const esp_partition_t* app = appPartition();
  esp_ota_img_states_t state;
  if (app && esp_ota_get_state_partition(app, &state) == ESP_OK &&
      (state == ESP_OTA_IMG_ABORTED || state == ESP_OTA_IMG_INVALID)) {
    appRolledBack = true;
  } else if (app && esp_ota_set_boot_partition(app) == ESP_OK) {
    bootPointedAtApp = true;
  }
  Serial.printf("[safeboot] app valid %d, rolled back %d\n", bootPointedAtApp, appRolledBack);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostName);
  WiFi.begin();                           // stored credentials from NVS
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[safeboot] WiFi %s ip %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    char ap[40];
    snprintf(ap, sizeof(ap), "%s-safeboot", hostName);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap);
    Serial.printf("[safeboot] no WiFi, AP %s ip %s\n", ap, WiFi.softAPIP().toString().c_str());
  }

  static const char* hdrs[] = {"Origin", "Referer"};
  server.collectHeaders(hdrs, 2);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/reboot", HTTP_POST, []() {
    if (!authorized()) return;
    server.send(200, "text/plain", "Restarting...");
    delay(500);
    ESP.restart();
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpload);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();
  Serial.println("[safeboot] http ready");
}

void loop() {
  server.handleClient();
  delay(2);
}
