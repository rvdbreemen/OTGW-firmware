/*
*************************************************************************  
**  Program  : Header file: OTGW-upgrade.ino
**  Version  : v0.7.5
**
**  Copyright (c) 2021 Copyright (c) 2021 - Schelte Bron
**  Based on version 0.4: https://github.com/hvxl/otgwmcu
**  Modified version of : upgrade.cpp
**
**  TERMS OF USE: MIT License. See bottom of file.                            
**************************************************************************
*/  

#define I2CSCL D1
#define I2CSDA D2
#define BUTTON D3
#define PICRST D5

#define LED1 D4
#define LED2 D0

#define BANNER "OpenTherm Gateway"

char fwversion[16];


#include <Ticker.h>
// #include "upgrade.h"
// #include "otgwmcu.h"
// #include "proxy.h"
// #include "debug.h"

#define STX 0x0F
#define ETX 0x04
#define DLE 0x05

#define XFER_MAX_ID 16

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

enum {
  ERROR_NONE,
  ERROR_READFILE,
  ERROR_MAGIC,
  ERROR_RESET,
  ERROR_RETRIES,
  ERROR_MISMATCHES
};

static byte fwstate = FWSTATE_IDLE;

struct fwupdatedata {
  unsigned char buffer[80];
  unsigned char datamem[256];
  unsigned char eedata[256];
  unsigned short codemem[4096];
  unsigned short failsafe[4];
  unsigned short prot[2];
  unsigned short errcnt, retries;
  char *version;
} *fwupd;

struct xferdata {
  unsigned short addr;
  byte size, mask;
};

Ticker timeout;

void toggle() {
  int state = digitalRead(LED1);
  digitalWrite(LED1, !state);
}

void blink(short delayms = 200) {
  static Ticker blinker;
  if (delayms) {
    blinker.attach_ms(delayms, toggle);
  } else {
    blinker.detach();
  }
}

void picreset() {
  pinMode(PICRST,OUTPUT);
  digitalWrite(PICRST,LOW);
  Serial.print("GW=R\r");
  delay(100);
  digitalWrite(PICRST,HIGH);
  pinMode(PICRST,INPUT);
}

int vcompare(const char *version1, const char* version2) {
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

int eeprom(const char *version, struct xferdata *xfer) {
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
      if (vcompare(version, buffer + p1) < 0) continue;
      // DebugTf("xfer[%d] = {%02x, %d, %0x2}\n", id, addr, size, mask);
      xfer[id].addr = addr;
      xfer[id].size = size;
      xfer[id].mask = mask;
      if (id > last) last = id;
    }
  }
  f.close();
  return last;
}

int transfer(const char *ver1, const char *ver2) {
  struct xferdata xfer1[XFER_MAX_ID] = {}, xfer2[XFER_MAX_ID] = {};
  int last, i, j, mask;
  byte value;

  last = min(eeprom(ver1, xfer1), eeprom(ver2, xfer2));
  for (i = 0; i <= last; i++) {
    if (xfer1[i].size) {
      for (j = 0; j < xfer1[i].size; j++) {
        if (xfer1[i].addr < 0x100) {
          value = fwupd->eedata[xfer1[i].addr + j];
        } else {
          value = xfer1[i].addr & 0xff;
        }
        if (j < xfer2[i].size && xfer2[i].addr < 0x100) {
          // Combine the masks
          mask = xfer1[i].mask | xfer2[i].mask;
          // Insert the 
          DebugTf("Transfer id %d from %02x to %02x, mask = %02x: %02x -> %02x\n",
            i, xfer1[i].addr + j, xfer2[i].addr + j, ~mask & 0xff, fwupd->datamem[xfer2[i].addr + j], value);
          fwupd->datamem[xfer2[i].addr + j] = fwupd->datamem[xfer2[i].addr + j] & mask | value & ~mask;
        }
      }
    }
  }
}

unsigned char hexcheck(char *hex, int len) {
  unsigned char sum = 0;
  int val;

  while (len-- > 0) {
    sscanf(hex, "%02x", &val);
    sum -= val;
    hex += 2;
  }
  return sum;
}

bool readhex(const char *hexfile, unsigned short *codemem, unsigned char *datamem, unsigned short *config = nullptr) {
  char hexbuf[48];
  int len, addr, tag, data, offs, linecnt = 0;
  bool rc = false;
  File f;

  f = LittleFS.open(hexfile, "r");
  if (!f) return false;
  memset(codemem, -1, 4096 * sizeof(short));
  memset(datamem, -1, 256 * sizeof(char));
  f.setTimeout(0);
  while (f.readBytesUntil('\n', hexbuf, sizeof(hexbuf)) != 0) {
    linecnt++;
    if (sscanf(hexbuf, ":%2x%4x%2x", &len, &addr, &tag) != 3) {
      // Parse error
      Serial.println(hexbuf);
      break;
    }
    if (len & 1) {
      // Invalid data size
      Serial.println("Invalid data size");
      break;
    }
    if (hexcheck(hexbuf + 1, len + 5) != 0) {
      // Checksum error
      Serial.println("Checksum error");
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
          datamem[addr++] = byteswap(data);
          offs += 4;
          len--;
        }
      } else if (addr >= 0x4000) {
        // Configuration bits
        if (config == nullptr) continue;
        addr = (addr - 0x4000) >> 1;
        while (len > 0) {
          if (sscanf(hexbuf + offs, "%04x", &data) != 1) break;
          config[addr++] = byteswap(data);
          offs += 4;
          len--;
        }
      } else {
        // Program memory
        addr >>= 1;
        while (len > 0) {
          if (sscanf(hexbuf + offs, "%04x", &data) != 1) {
            Serial.printf("Didn't find hex data at offset %d\n", offs);
            break;
          }
          codemem[addr++] = byteswap(data);
          offs += 4;
          len--;
        }
      }
      if (len) break;
    } else if (tag == 1) {
      rc = true;
      break;
    }
  }
  f.close();
  return rc;
}

void fwupgradefail();

void fwupgradecmd(const unsigned char *cmd, int len) {
  byte i, ch, sum = 0;

  Serial.write(STX);
  for (i = 0; i <= len; i++) {
    ch = i < len ? cmd[i] : sum;
    if (ch == STX || ch == ETX || ch == DLE) {
      Serial.write(DLE);
    }
    Serial.write(ch);
    sum -= ch;
  }
  Serial.write(ETX);
}

bool erasecode(short addr, bool rc = false) {
  byte fwcommand[] = {CMD_ERASEPROG, 1, 0, 0};
  short i;
  for (i = 0; i < 32; i++) {
    if (fwupd->codemem[addr + i] != 0xffff) {
      rc = true;
      break;
    }
  }
  if (rc) {
    fwcommand[2] = addr & 0xff;
    fwcommand[3] = addr >> 8;
    fwupgradecmd(fwcommand, sizeof(fwcommand));
  }
  return rc;
}

void loadcode(short addr, const unsigned short *code, short len = 32) {
  byte i, fwcommand[4 + 2 * len];
  unsigned short *data = (unsigned short *)fwcommand + 2;
  fwcommand[0] = CMD_WRITEPROG;
  fwcommand[1] = len >> 2;
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  for (i = 0; i < len; i++) {
    data[i] = code[i] & 0x3fff;
  }
  fwupgradecmd(fwcommand, sizeof(fwcommand));  
}

void readcode(short addr, short len = 32) {
  byte fwcommand[] = {CMD_READPROG, 32, 0, 0};
  fwcommand[1] = len;
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  fwupgradecmd(fwcommand, sizeof(fwcommand));
}

bool verifycode(const unsigned short *code, const unsigned short *data, short len = 32) {
  short i;
  bool rc = true;

  for (i = 0; i < len; i++) {
    if (data[i] != (code[i] & 0x3fff)) {
      fwupd->errcnt++;
      rc = false;
    }
  }
  return rc;
}

void loaddata(short addr) {
  byte i;
  byte fwcommand[68] = {CMD_WRITEDATA, 64};
  fwcommand[2] = addr & 0xff;
  fwcommand[3] = addr >> 8;
  for (i = 0; i < 64; i++) {
    fwcommand[i + 4] = fwupd->datamem[addr + i];
  }
  fwupgradecmd(fwcommand, sizeof(fwcommand));  
}

void readdata(short addr) {
  byte fwcommand[] = {CMD_READDATA, 64, 0, 0};
  fwcommand[2] = addr & 0xff;
  fwupgradecmd(fwcommand, sizeof(fwcommand));
}

bool verifydata(short pc, const byte *data, short len = 64) {
  short i;
  bool rc = true;

  for (i = 0; i < len; i++) {
    if (data[i] != fwupd->datamem[pc + i]) {
      fwupd->errcnt++;
      rc = false;
    }
  }
  return rc;
}

void fwupgradestop(int result) {
  DebugTf("Upgrade finished: Errorcode = %d\n%d retries, %d errors\n",
    result, fwupd->retries, fwupd->errcnt);
  free((void *)fwupd);
  fwupd = nullptr;
  fwstate = FWSTATE_IDLE;
  timeout.detach();
  if (result == ERROR_NONE) {
    digitalWrite(LED1, HIGH);
  } else if (result == ERROR_READFILE) {
    blink(500);
  } else if (result == ERROR_MAGIC) {
    blink(500);
  } else {
    blink(100);
  }
}

void fwupgradestep(const byte *packet = nullptr, int len = 0) {
  const unsigned short *data = (const unsigned short *)packet;
  static short pc;
  static byte lastcmd = 0;
  byte cmd = 0;

  if (packet == nullptr || len == 0) {
    cmd = lastcmd;
  } else {
    cmd = packet[0];
    lastcmd = cmd;
  }

  // DebugTf("fwupgradestep: state = %d, cmd = %d, packet = %d\n", fwstate, cmd, packet != nullptr);

  switch (fwstate) {
    case FWSTATE_IDLE:
      fwupd->errcnt = 0;
      fwupd->retries = 0;
      picreset();
      fwstate = FWSTATE_RSET;
      break;
    case FWSTATE_RSET:
      if (packet != nullptr) {
        byte fwcommand[] = {CMD_VERSION, 3};
        fwupgradecmd(fwcommand, sizeof(fwcommand));
        fwstate = FWSTATE_VERSION;
      } else if (++fwupd->retries > 5) {
        fwupgradestop(ERROR_RETRIES);
      } else {
        picreset();
      }
      break;
    case FWSTATE_VERSION:
      if (data != nullptr) {
        fwupd->prot[0] = data[2];
        fwupd->prot[1] = data[3];
        fwupd->failsafe[0] = 0x158a;                          // BSF PCLATH,3
        if ((fwupd->prot[0] & 0x800) == 0) {
          fwupd->failsafe[0] = 0x118a;                        // BCF PCLATH,3
        }
        fwupd->failsafe[1] = 0x2000 | fwupd->prot[0] & 0x7ff; // CALL SelfProg
        fwupd->failsafe[2] = 0x118a;                          // BCF PCLATH,3
        fwupd->failsafe[3] = 0x2820;                          // GOTO 0x20
        if (*fwversion && fwupd->version) {
          // Both old and new versions are known
          // Dump the current eeprom data to be able to transfer the settings
          pc = 0;
          readdata(pc);
          fwstate = FWSTATE_DUMP;
        } else {
          pc = 0x20;
          erasecode(pc, true);
          fwstate = FWSTATE_PREP;
        }
      } else if (++fwupd->retries > 10) {
        fwupgradestop(ERROR_RETRIES);
      } else {
        byte fwcommand[] = {CMD_VERSION, 3};
        fwupgradecmd(fwcommand, sizeof(fwcommand));
        fwstate = FWSTATE_VERSION;
      }
      break;
    case FWSTATE_DUMP:
      if (packet != nullptr) {
        memcpy(fwupd->eedata + pc, packet + 4, 64);
        pc += 64;
      } else if (++fwupd->retries > 5) {
        fwupgradestop(ERROR_RETRIES);
        break;
      }
      if (pc < 0x100) {
        readdata(pc);
      } else {
        // Transfer the EEPROM settings
        transfer(fwversion, fwupd->version);
        pc = 0x20;
        erasecode(pc);
        fwstate = FWSTATE_PREP;
      }
      break;
    case FWSTATE_PREP:
      // Programming has started; invalidate the firmware version
      *fwversion = '\0';
      if (cmd == CMD_ERASEPROG) {
        loadcode(pc, fwupd->failsafe, 4);
      } else if (cmd == CMD_WRITEPROG) {
        readcode(pc, 4);
      } else {
        if (packet != nullptr && packet[1] == 4 && data[1] == pc && verifycode(fwupd->failsafe, data + 2, 4)) {
          pc = 0;
          erasecode(pc);
          fwstate = FWSTATE_CODE;
        } else {
          // Failed. Try again.
          if (++fwupd->retries > 5) {
            fwupgradestop(ERROR_RETRIES);
          } else {
            erasecode(pc, true);
          }
        }
      }
      break;
    case FWSTATE_CODE:
      if (cmd == CMD_ERASEPROG) {
        // digitalWrite(LED2, LOW);
        loadcode(pc, fwupd->codemem + pc);
      } else if (cmd == CMD_WRITEPROG) {
        // digitalWrite(LED2, HIGH);
        readcode(pc);
      } else if (cmd == CMD_READPROG) {
        if (packet != nullptr && packet[1] == 32 && data[1] == pc && verifycode(fwupd->codemem + pc, data + 2)) {
          do {
            do {
              pc += 32;
            } while (pc + 31 >= fwupd->prot[0] && pc <= fwupd->prot[1]);
            if (pc >= 0x1000) {
              pc = 0;
              loaddata(pc);
              fwstate = FWSTATE_DATA;
              break;
            } else if (erasecode(pc)) {
              break;
            }
          } while (pc < 0x1000);
        } else {
          if (++fwupd->retries > 100) {
            fwupgradestop(ERROR_RETRIES);
          } else {
            erasecode(pc);
          }
        }
      }
      break;
    case FWSTATE_DATA:
      if (cmd == CMD_WRITEDATA) {
        // digitalWrite(LED2, HIGH);
        readdata(pc);
      } else if (cmd == CMD_READDATA) {
        if (packet != nullptr && verifydata(pc, packet + 4)) {
          pc += 64;
          if (pc < 0x100) {
            // digitalWrite(LED2, LOW);
            loaddata(pc);
          } else {
            byte fwcommand[] = {CMD_RESET, 0};
            fwupgradecmd(fwcommand, sizeof(fwcommand));
            fwupgradestop(ERROR_NONE);
          }
        } else if (++fwupd->retries > 100) {
          fwupgradestop(ERROR_RETRIES);
        } else {
          // digitalWrite(LED2, LOW);
          loaddata(pc);
        }
      }
      break;
  }
  if (fwstate == FWSTATE_IDLE) {
    timeout.detach();
  } else {
    timeout.once_ms_scheduled(1000, fwupgradefail);
  }
}

void fwupgradestart(const char *hexfile) {
  unsigned short ptr;
  char *s;

  if (fwupd != nullptr) {
    DebugTf("Previous upgrade has not finished\n");
    return;
  }

  blink(0);
  digitalWrite(LED1, LOW);

  fwupd = (struct fwupdatedata *)malloc(sizeof(struct fwupdatedata));
  if (!readhex(hexfile, fwupd->codemem, fwupd->datamem)) {
    fwupgradestop(ERROR_READFILE);
    return;
  }

  if (fwupd->codemem[0] != 0x158a || (fwupd->codemem[1] & 0x3e00) != 0x2600) {
    // Bad magic
    fwupgradestop(ERROR_MAGIC);
    return;
  }

  // Look for the new firmware version
  fwupd->version = nullptr;
  ptr = 0;
  while (ptr < 256) {
    s = strstr((char *)fwupd->datamem + ptr, BANNER);
    if (s == nullptr) {
      ptr += strnlen((char *)fwupd->datamem + ptr, 256 - ptr) + 1;
    } else {
      s += sizeof(BANNER);   // Includes the terminating '\0'
      fwupd->version = s;
      DebugTf("New firmware version: %s\n", s);
      break;
    }
  }

  fwupgradestep();
}

void fwupgradefail() {
  // Send a non-DLE byte in case the PIC is waiting for a byte following DLE
  Serial.write(STX);
  fwupgradestep();
}

void upgradeevent() {
  static unsigned int pressed = 0;
  static bool dle = false;
  static byte len, sum;
  int ch;

  if (fwstate == FWSTATE_IDLE) {
    if (digitalRead(BUTTON) == 0) {
      if (pressed == 0) {
        // In the very unlikely case that millis() happens to be 0, the 
        // user will have to press the button for one millisecond longer.
        pressed = millis();
      } else if (millis() - pressed > 2000) {
        fwupgradestart();
        pressed = 0;
      }
    } else {
      pressed = 0;
      //proxyevent();
    }
  } else if (Serial.available() > 0) {
    ch = Serial.read();
    if (!dle && ch == STX) {
      digitalWrite(LED2, LOW);
      len = 0;
      sum = 0;
    } else if (!dle && ch == DLE) {
      dle = true;
    } else if (!dle && ch == ETX) {
      digitalWrite(LED2, HIGH);
      if (sum == 0) {
        fwupgradestep(fwupd->buffer, len);
      } else {
        fwupgradestep();
      }
      len = 0;
    } else if (fwstate != FWSTATE_RSET) {
      fwupd->buffer[len++] = ch;
      sum -= ch;
      dle = false;
    }
  }
}



void upgradenow() {
  static bool dle = false;
  static uint8_t len, sum;
  int ch;
  // Only start, when not already programming the flash.
  if (fwstate != FWSTATE_IDLE) {
    DebugTln("Error: PIC already in programming in progress.");
    return;
  } 
  // Start the upgrade now...
  blink(0);
  digitalWrite(LED1, LOW);
  DebugTln("Start PIC upgrade now.");
  fwupgradestart();
  
  // Ready to program, the PIC reset just happend, so we should get a STX next, if all goes well that is.
  uint32_t tmrTimeout = millis();
  while (fwstate != FWSTATE_IDLE) {
    // keep feeding the dog from time to time... 
    feedWatchDog();
    // Let's check what the PIC sends
    if (Serial.available() > 0) {
      ch = Serial.read();
      if (!dle && ch == STX) {
        digitalWrite(LED2, LOW);
        len = 0;
        sum = 0;
      } else if (!dle && ch == DLE) {
        dle = true;
      } else if (!dle && ch == ETX) {
        digitalWrite(LED2, HIGH);
        if (sum == 0) {
          fwupgradestep(fwupd->buffer, len);
        } else {
          fwupgradestep();
        }
        len = 0;
      } else if (fwstate != FWSTATE_RSET) {
        fwupd->buffer[len++] = ch;
        sum -= ch;
        dle = false;
      }
    }
  }
  // When you are done, then reset the PIC one more time, to capture the actual fwversion of the OTGW
  resetOTGW();
}


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
