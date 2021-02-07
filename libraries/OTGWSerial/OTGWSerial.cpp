/*
  OTGWSerial.cpp - Library for OTGW PIC communication
  Copyright (c) 2021 - Schelte Bron

  MIT License

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "OTGWSerial.h"

#define STX ((uint8_t)0x0F)
#define ETX ((uint8_t)0x04)
#define DLE ((uint8_t)0x05)

#define XFER_MAX_ID 16

// Relative duration of each programming operation (each unit is actually about 21ms)
#define WEIGHT_RESET    8
#define WEIGHT_VERSION  1
#define WEIGHT_DATAREAD 4
#define WEIGHT_CODEPROG 10
#define WEIGHT_DATAPROG 20

#define byteswap(val) ((val << 8) | (val >> 8))

enum {
  FWSTATE_IDLE,
  FWSTATE_RSET,
  FWSTATE_VERSION,
  FWSTATE_DUMP,
  FWSTATE_PREP,
  FWSTATE_CODE,
  FWSTATE_DATA
};

enum {
  CMD_VERSION,
  CMD_READPROG,
  CMD_WRITEPROG,
  CMD_ERASEPROG,
  CMD_READDATA,
  CMD_WRITEDATA,
  CMD_READCFG,
  CMD_WRITECFG,
  CMD_RESET
};

static const char banner[] = "OpenTherm Gateway ";

OTGWSerial::OTGWSerial(int resetPin, int progressLed)
: HardwareSerial(UART0), _reset(resetPin), _led(progressLed) {
  HardwareSerial::begin(9600, SERIAL_8N1);
  // The PIC may have been confused by garbage on the
  // serial interface when the NodeMCU resets.
  resetPic();
  _upgrade_stage = FWSTATE_IDLE;
  _version[0] = '\0';
  _version_pos = 0;
}

int OTGWSerial::available() {
  if (upgradeEvent()) return 0;
  return HardwareSerial::available();
}

int OTGWSerial::read() {
  if (upgradeEvent()) return -1;
  return HardwareSerial::read();
}

// // Reimplement the read function. Other read functions call this to implement their functionality.
// int OTGWSerial::read() {
//   int retval;
//   char ch;
//   if (upgradeEvent()) return -1;
//   retval = HardwareSerial::read();
//   if (retval >= 0) {
//     ch = retval;
//     matchBanner(&ch);
//   }
//   return retval;
// }

int OTGWSerial::availableForWrite() {
  if (upgradeEvent()) return 0;
  return HardwareSerial::availableForWrite();
}

size_t OTGWSerial::write(uint8_t c) {
  if (upgradeEvent()) return 0;
  return HardwareSerial::write(c);
}


size_t OTGWSerial::write(const uint8_t *buffer, size_t len) {
  if (upgradeEvent()) return 0;
  return HardwareSerial::write(buffer, len);
}

bool OTGWSerial::busy() {
  return upgradeEvent();
}

void OTGWSerial::resetPic() {
  if (_reset >= 0) {
    pinMode(_reset, OUTPUT);
    digitalWrite(_reset, LOW);
  }
  HardwareSerial::print("GW=R\r");
  if (_reset >= 0) {
    delay(100);
    digitalWrite(_reset, HIGH);
    pinMode(_reset, INPUT);
  }
  _banner_matched = 0;
}

const char *OTGWSerial::firmwareVersion() {
  return _version;
}

void OTGWSerial::registerFinishedCallback(OTGWUpgradeFinished *func) {
  _finishedFunc = func;
}

void OTGWSerial::registerProgressCallback(OTGWUpgradeProgress *func) {
  _progressFunc = func;
}

void OTGWSerial::SetLED(int state) {
  if (_led >= 0) {
    digitalWrite(_led, state ? LOW : HIGH);
  }
}

void OTGWSerial::progress(int weight) {
  if (_progressFunc) {
    _upgrade_data->progress += weight;
    _progressFunc(_upgrade_data->progress * 100 / _upgrade_data->total);
  }
}

// Look for the banner in the incoming data and extract the version number
void OTGWSerial::matchBanner(const char *str, int len) {
  for (int i = 0; i < len; i++) {
    if (banner[_banner_matched] == '\0') {
      if (isspace(str[i])) {
        _version[_version_pos] = '\0';
        _banner_matched = 0;
        _version_pos = 0;
      } else {
        _version[_version_pos++] = str[i];
      }
    } else if (str[i] != banner[_banner_matched++]) {
      _banner_matched = 0;
    }
  }
}

unsigned char OTGWSerial::hexChecksum(char *hex, int len) {
  unsigned char sum = 0;
  int val;

  while (len-- > 0) {
    sscanf(hex, "%02x", &val);
    sum -= val;
    hex += 2;
  }
  return sum;
}

OTGWError OTGWSerial::readHexFile(const char *hexfile, int *total) {
  char hexbuf[48];
  int len, addr, tag, data, offs, linecnt = 0, weight;
  uint32_t codemap[4] = {};
  byte datamap = 0;
  OTGWError rc = OTGW_ERROR_HEX_FORMAT;
  File f;

  f = LittleFS.open(hexfile, "r");
  if (!f) return OTGW_ERROR_HEX_ACCESS;
  memset(_upgrade_data->codemem, -1, 4096 * sizeof(short));
  memset(_upgrade_data->datamem, -1, 256 * sizeof(char));
  weight = WEIGHT_RESET + WEIGHT_VERSION;
  f.setTimeout(0);
  while (f.readBytesUntil('\n', hexbuf, sizeof(hexbuf)) != 0) {
    linecnt++;
    if (sscanf(hexbuf, ":%2x%4x%2x", &len, &addr, &tag) != 3) {
      // Parse error
      rc = OTGW_ERROR_HEX_FORMAT;
      break;
    }
    if (len & 1) {
      // Invalid data size
      rc = OTGW_ERROR_HEX_DATASIZE;
      break;
    }
    if (hexChecksum(hexbuf + 1, len + 5) != 0) {
      // Checksum error
      rc = OTGW_ERROR_HEX_CHECKSUM;
      break;
    }
    offs = 9;
    len >>= 1;
    if (tag == 0) {
      if (addr >= 0x4400) {
        // Bogus addresses
        continue;
      } else if (addr >= 0x4200) {
        // Data memory
        addr = (addr - 0x4200) >> 1;
        while (len > 0) {
          if (sscanf(hexbuf + offs, "%04x", &data) != 1) break;
          if (!bitRead(datamap, addr / 64)) weight += WEIGHT_DATAPROG;
          bitSet(datamap, addr / 64);
          _upgrade_data->datamem[addr++] = byteswap(data);
          offs += 4;
          len--;
        }
      } else if (addr >= 0x4000) {
        // Configuration bits
        continue;
      } else {
        // Program memory
        addr >>= 1;
        while (len > 0) {
          if (sscanf(hexbuf + offs, "%04x", &data) != 1) {
            // Didn't find hex data
            break;
          }
          if (!bitRead(codemap[addr / (32 * 32)], (addr / 32) & 31)) weight += WEIGHT_CODEPROG;
          bitSet(codemap[addr / (32 * 32)], (addr / 32) & 31);
          _upgrade_data->codemem[addr++] = byteswap(data);
          offs += 4;
          len--;
        }
      }
      if (len) break;
    } else if (tag == 1) {
      rc = OTGW_ERROR_NONE;
      break;
    }
  }
  f.close();
  if (total) *total = weight;
  return rc;
}

int OTGWSerial::versionCompare(const char *version1, const char* version2) {
  const char *s1 = version1, *s2 = version2;
  int v1, v2, n;

  while (*s1 && *s2) {
    if (!sscanf(s1, "%d%n", &v1, &n)) return 0;
    s1 += n;
    if (!sscanf(s2, "%d%n", &v2, &n)) return 0;
    s2 += n;
    if (v1 < v2) return -1;
    if (v1 > v2) return 1;
    if (*s1 != *s2) {
      // Alpha versions
      if (*s1 == 'a') return -1;
      if (*s2 == 'a') return 1;
      // Beta versions
      if (*s1 == 'b') return -1;
      if (*s2 == 'b') return 1;
      // Subversions
      if (*s1 == 0) return -1;
      if (*s2 == 0) return 1;
    }
    s1++;
    s2++;
  }
}

int OTGWSerial::eepromSettings(const char *version, OTGWTransferData *xfer) {
  char buffer[64];
  int last = 0, len, id, p1, p2, addr, size, mask, n;
  File f;

  f = LittleFS.open("/transfer.dat", "r");
  if (!f) return last;
  while (f.available()) {
    len = f.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    if ((n = sscanf(buffer, "%d %n%*s%n %x %d %x", &id, &p1, &p2, &addr, &size, &mask)) == 4) {
      if (id >= XFER_MAX_ID);
      buffer[p2] = '\0';
      if (versionCompare(version, buffer + p1) < 0) continue;
      xfer[id].addr = addr;
      xfer[id].size = size;
      xfer[id].mask = mask;
      if (id > last) last = id;
    }
  }
  f.close();
  return last;
}

void OTGWSerial::transferSettings(const char *ver1, const char *ver2) {
  OTGWTransferData xfer1[XFER_MAX_ID] = {}, xfer2[XFER_MAX_ID] = {};
  int last, i, j, mask;
  byte value;

  last = min(eepromSettings(ver1, xfer1), eepromSettings(ver2, xfer2));
  for (i = 0; i <= last; i++) {
    if (xfer1[i].size) {
      for (j = 0; j < xfer1[i].size; j++) {
        if (xfer1[i].addr < 0x100) {
          value = _upgrade_data->eedata[xfer1[i].addr + j];
        } else {
          value = xfer1[i].addr & 0xff;
        }
        if (j < xfer2[i].size && xfer2[i].addr < 0x100) {
          // Combine the masks
          mask = xfer1[i].mask | xfer2[i].mask;
          // Insert the old data into the data array
          _upgrade_data->datamem[xfer2[i].addr + j] = _upgrade_data->datamem[xfer2[i].addr + j] & mask | value & ~mask;
        }
      }
    }
  }
}

void OTGWSerial::fwCommand(const unsigned char *cmd, int len) {
  uint8_t i, ch, sum = 0;

  HardwareSerial::write(STX);
  for (i = 0; i <= len; i++) {
    ch = i < len ? cmd[i] : sum;
    if (ch == STX || ch == ETX || ch == DLE) {
      HardwareSerial::write(DLE);
    }
    HardwareSerial::write(ch);
    sum -= ch;
  }
  HardwareSerial::write(ETX);
}

bool OTGWSerial::eraseCode(short addr, bool rc) {
  byte fwcommand[] = {CMD_ERASEPROG, 1, 0, 0};
  short i;
  for (i = 0; i < 32; i++) {
    if (_upgrade_data->codemem[addr + i] != 0xffff) {
      rc = true;
      break;
    }
  }
  if (rc) {
    fwcommand[2] = addr & 0xff;
    fwcommand[3] = addr >> 8;
    fwCommand(fwcommand, sizeof(fwcommand));
  }
  return rc;
}

void OTGWSerial::loadCode(short addr, const unsigned short *code, short len) {
  byte i, fwcommand[4 + 2 * len];
  unsigned short *data = (unsigned short *)fwcommand + 2;
  fwcommand[0] = CMD_WRITEPROG;
  fwcommand[1] = len >> 2;
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  for (i = 0; i < len; i++) {
    data[i] = code[i] & 0x3fff;
  }
  fwCommand(fwcommand, sizeof(fwcommand));  
}

void OTGWSerial::readCode(short addr, short len) {
  byte fwcommand[] = {CMD_READPROG, 32, 0, 0};
  fwcommand[1] = len;
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  fwCommand(fwcommand, sizeof(fwcommand));
}

bool OTGWSerial::verifyCode(const unsigned short *code, const unsigned short *data, short len) {
  short i;
  bool rc = true;

  for (i = 0; i < len; i++) {
    if (data[i] != (code[i] & 0x3fff)) {
      _upgrade_data->errcnt++;
      rc = false;
    }
  }
  return rc;
}

void OTGWSerial::loadData(short addr) {
  byte i;
  byte fwcommand[68] = {CMD_WRITEDATA, 64};
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  for (i = 0; i < 64; i++) {
    fwcommand[i + 4] = _upgrade_data->datamem[addr + i];
  }
  fwCommand(fwcommand, sizeof(fwcommand));  
}

void OTGWSerial::readData(short addr) {
  byte fwcommand[] = {CMD_READDATA, 64, 0, 0};
  fwcommand[2] = addr & 0xff;
  fwCommand(fwcommand, sizeof(fwcommand));
}

bool OTGWSerial::verifyData(short pc, const byte *data, short len) {
  short i;
  bool rc = true;

  for (i = 0; i < len; i++) {
    if (data[i] != _upgrade_data->datamem[pc + i]) {
      _upgrade_data->errcnt++;
      rc = false;
    }
  }
  return rc;
}

OTGWError OTGWSerial::startUpgrade(const char *hexfile) {
  int weight;
  unsigned short ptr;
  char *s;
  OTGWError result;

  if (_upgrade_data != nullptr) {
    return OTGW_ERROR_INPROG;
  }

  _upgrade_data = (OTGWUpgradeData *)malloc(sizeof(OTGWUpgradeData));
  if (_upgrade_data == nullptr) {
    return OTGW_ERROR_MEMORY;
  }

  result = readHexFile(hexfile, &weight);
  if (result != OTGW_ERROR_NONE) {
    return finishUpgrade(result);
  }

  if (_upgrade_data->codemem[0] != 0x158a || (_upgrade_data->codemem[1] & 0x3e00) != 0x2600) {
    // Bad magic
    return finishUpgrade(OTGW_ERROR_MAGIC);
  }
  // The self-programming code will be skipped (assume 256 program words)
  weight -= 8 * WEIGHT_CODEPROG;

  // Look for the new firmware version
  _upgrade_data->version = nullptr;
  ptr = 0;
  while (ptr < 256) {
    s = strstr((char *)_upgrade_data->datamem + ptr, banner);
    if (s == nullptr) {
      ptr += strnlen((char *)_upgrade_data->datamem + ptr, 256 - ptr) + 1;
    } else {
      s += sizeof(banner) - 1;   // Drop the terminating '\0'
      _upgrade_data->version = s;
      if (*_version) {
        // Reading out the EEPROM settings takes 4 reads of 64 bytes
        weight += 4 * WEIGHT_DATAREAD;
      }
      break;
    }
  }

  _upgrade_data->total = weight;
  stateMachine();
  return OTGW_ERROR_NONE;
}

void OTGWSerial::stateMachine(const unsigned char *packet, int len) {
  const unsigned short *data = (const unsigned short *)packet;
  byte cmd = 0;

  if (packet == nullptr || len == 0) {
    cmd = _upgrade_data->lastcmd;
  } else {
    cmd = packet[0];
    _upgrade_data->lastcmd = cmd;
  }

  if (_upgrade_stage != FWSTATE_IDLE && packet == nullptr) {
    int maxtries = (_upgrade_stage == FWSTATE_CODE || _upgrade_stage == FWSTATE_DATA ? 100 : 10);
    if (++_upgrade_data->retries >= maxtries) {
      finishUpgrade(OTGW_ERROR_RETRIES);
      return;
    }
  }

  switch (_upgrade_stage) {
    case FWSTATE_IDLE:
      _upgrade_data->errcnt = 0;
      _upgrade_data->retries = 0;
      _upgrade_data->progress = 0;
      resetPic();
      _upgrade_stage = FWSTATE_RSET;
      break;
    case FWSTATE_RSET:
      if (packet != nullptr) {
        byte fwcommand[] = {CMD_VERSION, 3};
        progress(WEIGHT_RESET);
        fwCommand(fwcommand, sizeof(fwcommand));
        _upgrade_stage = FWSTATE_VERSION;
      } else {
        resetPic();
      }
      break;
    case FWSTATE_VERSION:
      if (data != nullptr) {
        _upgrade_data->protectstart = data[2];
        _upgrade_data->protectend = data[3];
        _upgrade_data->failsafe[0] = 0x158a;                          // BSF PCLATH,3
        if ((_upgrade_data->protectstart & 0x800) == 0) {
          _upgrade_data->failsafe[0] = 0x118a;                        // BCF PCLATH,3
        }
        _upgrade_data->failsafe[1] = 0x2000 | _upgrade_data->protectstart & 0x7ff; // CALL SelfProg
        _upgrade_data->failsafe[2] = 0x118a;                          // BCF PCLATH,3
        _upgrade_data->failsafe[3] = 0x2820;                          // GOTO 0x20
        progress(WEIGHT_VERSION);
        if (*_version && _upgrade_data->version) {
          // Both old and new versions are known
          // Dump the current eeprom data to be able to transfer the settings
          _upgrade_data->pc = 0;
          readData(_upgrade_data->pc);
          _upgrade_stage = FWSTATE_DUMP;
        } else {
          _upgrade_data->pc = 0x20;
          eraseCode(_upgrade_data->pc, true);
          _upgrade_stage = FWSTATE_PREP;
        }
      } else {
        byte fwcommand[] = {CMD_VERSION, 3};
        fwCommand(fwcommand, sizeof(fwcommand));
        _upgrade_stage = FWSTATE_VERSION;
      }
      break;
    case FWSTATE_DUMP:
      if (packet != nullptr) {
        progress(WEIGHT_DATAREAD);
        memcpy(_upgrade_data->eedata + _upgrade_data->pc, packet + 4, 64);
        _upgrade_data->pc += 64;
      }
      if (_upgrade_data->pc < 0x100) {
        readData(_upgrade_data->pc);
      } else {
        // Transfer the EEPROM settings
        transferSettings(_version, _upgrade_data->version);
        _upgrade_data->pc = 0x20;
        eraseCode(_upgrade_data->pc);
        _upgrade_stage = FWSTATE_PREP;
      }
      break;
    case FWSTATE_PREP:
      // Programming has started; invalidate the firmware version
      *_version = '\0';
      if (cmd == CMD_ERASEPROG) {
        loadCode(_upgrade_data->pc, _upgrade_data->failsafe, 4);
      } else if (cmd == CMD_WRITEPROG) {
        readCode(_upgrade_data->pc, 4);
      } else {
        if (packet != nullptr && packet[1] == 4 && data[1] == _upgrade_data->pc && verifyCode(_upgrade_data->failsafe, data + 2, 4)) {
          progress(WEIGHT_CODEPROG);
          _upgrade_data->pc = 0;
          eraseCode(_upgrade_data->pc);
          _upgrade_stage = FWSTATE_CODE;
        } else {
          // Failed. Try again.
          eraseCode(_upgrade_data->pc, true);
        }
      }
      break;
    case FWSTATE_CODE:
      if (cmd == CMD_ERASEPROG) {
        // digitalWrite(LED2, LOW);
        loadCode(_upgrade_data->pc, _upgrade_data->codemem + _upgrade_data->pc);
      } else if (cmd == CMD_WRITEPROG) {
        // digitalWrite(LED2, HIGH);
        readCode(_upgrade_data->pc);
      } else if (cmd == CMD_READPROG) {
        if (packet != nullptr && packet[1] == 32 && data[1] == _upgrade_data->pc && verifyCode(_upgrade_data->codemem + _upgrade_data->pc, data + 2)) {
          do {
            do {
              _upgrade_data->pc += 32;
            } while (_upgrade_data->pc + 31 >= _upgrade_data->protectstart && _upgrade_data->pc <= _upgrade_data->protectend);
            if (_upgrade_data->pc >= 0x1000) {
              _upgrade_data->pc = 0;
              loadData(_upgrade_data->pc);
              _upgrade_stage = FWSTATE_DATA;
              break;
            } else if (eraseCode(_upgrade_data->pc)) {
              progress(WEIGHT_CODEPROG);
              break;
            }
          } while (_upgrade_data->pc < 0x1000);
        } else {
          eraseCode(_upgrade_data->pc);
        }
      }
      break;
    case FWSTATE_DATA:
      if (cmd == CMD_WRITEDATA) {
        // digitalWrite(LED2, HIGH);
        readData(_upgrade_data->pc);
      } else if (cmd == CMD_READDATA) {
        if (packet != nullptr && verifyData(_upgrade_data->pc, packet + 4)) {
          progress(WEIGHT_DATAPROG);
          _upgrade_data->pc += 64;
          if (_upgrade_data->pc < 0x100) {
            // digitalWrite(LED2, LOW);
            loadData(_upgrade_data->pc);
          } else {
            byte fwcommand[] = {CMD_RESET, 0};
            fwCommand(fwcommand, sizeof(fwcommand));
            finishUpgrade(OTGW_ERROR_NONE);
          }
        } else {
          // digitalWrite(LED2, LOW);
          loadData(_upgrade_data->pc);
        }
      }
      break;
  }

  if (_upgrade_stage != FWSTATE_IDLE) {
    _upgrade_data->lastaction = millis();
  }
}

OTGWError OTGWSerial::finishUpgrade(OTGWError result) {
  short errors = _upgrade_data->errcnt, retries = _upgrade_data->retries;
  free((void *)_upgrade_data);
  _upgrade_data = nullptr;
  _upgrade_stage = FWSTATE_IDLE;
  if (_finishedFunc) {
    _finishedFunc(result, errors, retries);
  }
  return result;
}

// Proceed with the upgrade, if applicable
bool OTGWSerial::upgradeEvent() {
  int ch;
  bool dle;
  if (_upgrade_stage == FWSTATE_IDLE) return false;
  while (HardwareSerial::available()) {
    ch = HardwareSerial::read();
    dle = _upgrade_data->bufpos < sizeof(_upgrade_data->buffer) && _upgrade_data->buffer[_upgrade_data->bufpos] == DLE;
    if (!dle && ch == STX) {
      SetLED(1);
      _upgrade_data->bufpos = 0;
      _upgrade_data->checksum = 0;
      _upgrade_data->buffer[0] = '\0';
    } else if ((!dle || _upgrade_stage == FWSTATE_RSET) && ch == ETX) {
      SetLED(0);
      if (_upgrade_data->checksum == 0 || _upgrade_stage == FWSTATE_RSET) {
        stateMachine(_upgrade_data->buffer, _upgrade_data->bufpos);
      } else {
        // Checksum mismatch
        stateMachine();
      }
    } else if (_upgrade_data->bufpos >= sizeof(_upgrade_data->buffer)) {
      // Prevent overwriting the program data
    } else if (!dle && ch == DLE) {
      _upgrade_data->buffer[_upgrade_data->bufpos] = ch;
    } else {
      _upgrade_data->buffer[_upgrade_data->bufpos++] = ch;
      _upgrade_data->checksum -= ch;
      _upgrade_data->buffer[_upgrade_data->bufpos] = '\0';
    }
  }
  if (_upgrade_stage != FWSTATE_IDLE && millis() - _upgrade_data->lastaction > 1000) {
    // Too much time has passed since the last action
    // Send a non-DLE byte in case the PIC is waiting for a byte following a DLE
    HardwareSerial::write(STX);
    stateMachine();    
  }
  return true;
}
