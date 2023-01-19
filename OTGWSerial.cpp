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
#define WEIGHT_MAXIMUM  2000

#define byteswap(val) (((val << 8) | (val >> 8)) & 0xffff)

#ifdef DEBUG
#define Dprintf(...) if (debugfunc) debugfunc(__VA_ARGS__)
OTGWDebugFunction *debugfunc = nullptr;
#else
#define Dprintf(...) {}
#endif

static OTGWFirmware firmware = FIRMWARE_UNKNOWN;
static char fwversion[16];

const char hexbytefmt[] = "%02x";
const char hexwordfmt[] = "%04x";

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

unsigned short p16f88recover(unsigned short, unsigned short *);
unsigned short p16f1847recover(unsigned short, unsigned short *);

const struct PicInfo PicInfo[] PROGMEM = {
    {
        256, 4096, 9, 0x2000, 0x2100, 32, 4, true,
        {0x3fff, 0x158a, 0x3e00, 0x2600},
        p16f88recover
    }, {
        256, 8192, 9, 0x8000, 0xf000, 32, 32, false,
        {0x3fff, 0x319f, 0x3e00, 0x2600},
        p16f1847recover
    }
};

const char banner1[] PROGMEM = "OpenTherm Gateway ";
const char banner2[] PROGMEM = "Opentherm gateway diagnostics - Version ";
const char banner3[] PROGMEM = "OpenTherm Interface ";
const char* const banners[] PROGMEM = {banner1, banner2, banner3};
const byte newpic[] = {6, 2, 2};

unsigned short p16f88recover(unsigned short addr, unsigned short *code) {
    unsigned short cnt = 0;
    code[cnt++] = addr & 0x800 ? 0x158a : 0x118a;   // pagesel SelfProg
    code[cnt++] = 0x2000 | addr & 0x7ff;            // call    SelfProg
    code[cnt++] = 0x118a;                           // pagesel 0x0000
    code[cnt++] = 0x2820;                           // goto    0x0020
    return cnt;
}

unsigned short p16f1847recover(unsigned short addr, unsigned short *code) {
    unsigned short cnt = 0;
    code[cnt++] = 0x3180 | addr >> 8;               // pagesel SelfProg
    code[cnt++] = 0x2000 | addr & 0x7ff;            // call    SelfProg
    code[cnt++] = 0x3180;                           // pagesel 0x0000
    code[cnt++] = 0x2820;                           // goto    0x0020
    return cnt;
}

OTGWUpgrade::OTGWUpgrade(OTGWSerial *serial)
  : serial(serial), stage(FWSTATE_IDLE) {
}

OTGWUpgrade::~OTGWUpgrade() {
    if (hexfd) hexfd.close();
}

OTGWError OTGWUpgrade::start(const char *hexfile) {
    if (hexfile[0] == '/') {
        // Absolute file name specifies the exact file to load
        OTGWError rc = readHexFile(hexfile);
        if (rc != OTGW_ERROR_NONE) return rc;
    } else {
        // A relative file name takes the file from the directory for the
        // current PIC, which is determined based on the bootloader version
        model = PICPROBE;
        // Copy the file name, in case it points to some temporary storage
        strncpy(filename, hexfile, sizeof(filename));
        total = WEIGHT_MAXIMUM;
    }
    stateMachine();
    return OTGW_ERROR_NONE;
}

// Inform the parent object about the upgrade progress
void OTGWUpgrade::progress(int weight) {
    done += weight;
    // Prevent figures above 100%
    if (done > total) done = total;
    serial->progress(done * 100 / total);
}

unsigned char OTGWUpgrade::hexChecksum(char *hex, int len) {
    unsigned char sum = 0;
    int val;

    while (len-- > 0) {
        sscanf(hex, hexbytefmt, &val);
        sum -= val;
        hex += 2;
    }
    return sum;
}

OTGWError OTGWUpgrade::readHexRecord() {
    char hexbuf[48];
    int len, addr, tag, data, offs, i;
    while (hexfd.readBytesUntil('\n', hexbuf, sizeof(hexbuf)) != 0) {
        if (sscanf(hexbuf, ":%2x%4x%2x", &len, &addr, &tag) != 3) break;
        if (len & 1) {
            // Invalid data size
            return OTGW_ERROR_HEX_DATASIZE;
        }
        if (hexChecksum(hexbuf + 1, len + 5) != 0) {
            // Checksum error
            return OTGW_ERROR_HEX_CHECKSUM;
        }
        offs = 9;
        if (tag == 0) {
            // Data record
            hexaddr = (addr >> 1) + (hexseg << 3);
            len >>= 1;
            hexlen = len;
            for (i = 0; i < len; i++) {
                if (sscanf(hexbuf + offs, hexwordfmt, &data) != 1) {
                    // Didn't find hex data
                    break;
                }
                hexdata[i] = byteswap(data);
                offs += 4;
            }
            if (i != len) break;
            return OTGW_ERROR_NONE;
        } else if (tag == 1) {
            // End-of-file record
            hexlen = 0;
            return OTGW_ERROR_NONE;
        } else if (tag == 2) {
            // Extended segment address record
            if (sscanf(hexbuf + offs, hexwordfmt, &data) != 1) break;
            hexseg = data;
        } else if (tag == 4) {
            // Extended linear address record
            if (sscanf(hexbuf + offs, hexwordfmt, &data) != 1) break;
            hexseg = data << 12;
        }
    }
    return OTGW_ERROR_HEX_FORMAT;
}

OTGWError OTGWUpgrade::readHexFile(const char *hexfile) {
    int linecnt = 0, addr = 0, weight, rowsize;
    byte datamap = 0;
    OTGWError rc = OTGW_ERROR_NONE;

    hexfd = LittleFS.open(hexfile, "r");
    if (!hexfd) return finishUpgrade(OTGW_ERROR_HEX_ACCESS);
    hexfd.setTimeout(0);

    model = PICUNKNOWN;
    memset(datamem, -1, 256 * sizeof(char));
    memset(eedata, -1, 256 * sizeof(char));
    weight = WEIGHT_RESET + WEIGHT_VERSION;

    hexseg = 0;
    hexaddr = 0;
    hexpos = 0;
    while (rc == OTGW_ERROR_NONE) {
        rc = readHexRecord();
        if (hexlen == 0) break;
        linecnt++;
        if (rc != OTGW_ERROR_NONE) break;
        if (hexaddr < addr) {
            rc = OTGW_ERROR_HEX_FORMAT;
            break;
        }
        if (hexaddr == 0) {
            // Determine the target PIC
            int i, data;
            for (i = 0; i < PICCOUNT; i++) {
                data = hexdata[0] & pgm_read_word(&PicInfo[i].magic[0]);
                if (data != pgm_read_word(&PicInfo[i].magic[1])) continue;
                data = hexdata[1] & pgm_read_word(&PicInfo[i].magic[2]);
                if (data != pgm_read_word(&PicInfo[i].magic[3])) continue;
                break;
            }
            if (i == PICCOUNT) {
                rc = OTGW_ERROR_MAGIC;
                break;
            }
            model = i;
            memcpy_P(&info, PicInfo + i, sizeof(struct PicInfo));
            rowsize = info.erasesize;
        }
        if (hexaddr < info.codesize) {
            // Program memory
            // The PIC model must be known at this point
            if (model == PICUNKNOWN) {
                rc = OTGW_ERROR_HEX_FORMAT;
                break;
            }
            // In theory the next two conditions could both happen
            // They should therefor not be combined into a single if
            if ((addr - 1) / rowsize != hexaddr / rowsize) {
                // The first code word is in a new row
                weight += WEIGHT_CODEPROG;
            }
            if (hexaddr / rowsize != (hexaddr + hexlen - 1) / rowsize) {
                // The last code word is in a new row
                weight += WEIGHT_CODEPROG;
            }
            addr = hexaddr + hexlen;
        } else if (hexaddr < info.eebase) {
            // Configuration bits or bogus data
        } else if (hexaddr < info.eebase + info.datasize) {
            // Data memory
            int eeaddr = hexaddr - info.eebase;
            for (int i = 0; i < hexlen; i++, eeaddr++) {
                int data = hexdata[i];
                if (!bitRead(datamap, eeaddr / 64)) weight += WEIGHT_DATAPROG;
                bitSet(datamap, eeaddr / 64);
                datamem[eeaddr] = data;
                // Indicate the address probably needs to be written
                eedata[eeaddr] = ~data;
            }
        }
        addr = hexaddr + hexlen;
    }
    if (rc != OTGW_ERROR_NONE) return finishUpgrade(rc);

    Dprintf("model: %d\n", model);

    // The self-programming code will be skipped (assume 256 program words)
    weight -= 8 * WEIGHT_CODEPROG;

    // Look for the new firmware version
    version = nullptr;
    unsigned short ptr = 0;
    while (ptr < info.datasize) {
        char *s = strstr_P((char *)datamem + ptr, banner1);
        if (s == nullptr) {
            ptr += strnlen((char *)datamem + ptr,
              info.datasize - ptr) + 1;
        } else {
            s += sizeof(banner1) - 1;   // Drop the terminating '\0'
            version = s;
            Dprintf("Version: %s\n", version);
            if (firmware == FIRMWARE_OTGW && *fwversion) {
                // Reading out the EEPROM settings takes 4 reads of 64 bytes
                weight += 4 * WEIGHT_DATAREAD;
            }
            break;
        }
    }

    total = weight;
    return OTGW_ERROR_NONE;
}

int OTGWUpgrade::versionCompare(const char *version1, const char* version2) {
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

int OTGWUpgrade::eepromSettings(const char *version, OTGWTransferData *xfer) {
    char buffer[64];
    int last = 0, len, id, p1, p2, addr, size, mask, n;
    File f;

    f = LittleFS.open("/transfer.dat", "r");
    if (!f) return last;
    while (f.available()) {
        len = f.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';
        if ((n = sscanf(buffer, "%d %n%*s%n %x %d %x", &id, &p1, &p2, &addr, &size, &mask)) == 4) {
            if (id >= XFER_MAX_ID) break;
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

void OTGWUpgrade::transferSettings(const char *ver1, const char *ver2) {
    OTGWTransferData xfer1[XFER_MAX_ID] = {}, xfer2[XFER_MAX_ID] = {};
    int last, i, j, mask;
    byte value;

    last = min(eepromSettings(ver1, xfer1), eepromSettings(ver2, xfer2));
    for (i = 0; i <= last; i++) {
        if (xfer1[i].size) {
            for (j = 0; j < xfer1[i].size; j++) {
                if (xfer1[i].addr < info.datasize) {
                    value = eedata[xfer1[i].addr + j];
                } else {
                    value = xfer1[i].addr & 0xff;
                }
                if (j < xfer2[i].size && xfer2[i].addr < info.datasize) {
                    // Combine the masks
                    mask = xfer1[i].mask | xfer2[i].mask;
                    // Insert the old data into the data array
                    datamem[xfer2[i].addr + j] = datamem[xfer2[i].addr + j] & mask | value & ~mask;
                }
            }
        }
    }
}

int OTGWUpgrade::prepareCode(unsigned short *buffer) {
    unsigned int addr, start, n;
    const unsigned int rowsize = info.erasesize;
    const unsigned int mask = rowsize - 1;

    memset(buffer, -1, rowsize * sizeof(short));

    addr = hexaddr + hexpos;
    start = addr & ~mask;
    n = addr - start;

    while (true) {
        if (hexpos >= hexlen) {
            readHexRecord();
            if (hexlen == 0) break;
            addr = hexaddr;
            hexpos = 0;
            if (n == 0) {
                start = addr & ~mask;
            }
            n = addr - start;
        }
        if (n >= rowsize) break;
        buffer[n++] = hexdata[hexpos++];
    }
    return start;
}

void OTGWUpgrade::fwCommand(const unsigned char *cmd, int len) {
    uint8_t i, ch, sum = 0;

    cmdcode = *cmd;
    serial->putbyte(STX);
    for (i = 0; i <= len; i++) {
        ch = i < len ? cmd[i] : sum;
        if (ch == STX || ch == ETX || ch == DLE) {
            serial->putbyte(DLE);
        }
        serial->putbyte(ch);
        // Dprintf("%02x ", ch);
        sum -= ch;
    }
    serial->putbyte(ETX);
    // Dprintf("\n");
}

void OTGWUpgrade::eraseCode(short addr) {
    byte fwcommand[] = {CMD_ERASEPROG, 1, 0, 0};
    Dprintf("Erase Program Memory %d blocks @0x%04x\n", 1, addr);
    fwcommand[2] = addr & 0xff;
    fwcommand[3] = addr >> 8;
    fwCommand(fwcommand, sizeof(fwcommand));
}

short OTGWUpgrade::loadCode(short addr, const unsigned short *code, short len) {
    byte i, fwcommand[4 + 2 * len];
    unsigned short *data = (unsigned short *)fwcommand + 2;
    short size = 0;

    Dprintf("Write Program Memory %d words @0x%04x\n", len, addr);
    for (i = 0; i < len; i++) {
        data[i] = code[i] & 0x3fff;
        if (data[i] != 0x3fff) size = i + 1;
    }
    fwcommand[0] = CMD_WRITEPROG;
    if (info.blockwrite) {
        int block = info.groupsize;
        size = (size + block - 1) / block;
        fwcommand[1] = size;
        size *= block;
    } else {
        fwcommand[1] = size;
    }
    fwcommand[2] = addr & 0xff;
    fwcommand[3] = addr >> 8;
    fwCommand(fwcommand, 4 + 2 * size);
    return size;
}

void OTGWUpgrade::readCode(short addr, short len) {
    byte fwcommand[] = {CMD_READPROG, 32, 0, 0};
    Dprintf("Read Program Memory %d words @0x%04x\n", len, addr);
    fwcommand[1] = len;
    fwcommand[2] = addr & 0xff;
    fwcommand[3] = addr >> 8;
    fwCommand(fwcommand, sizeof(fwcommand));
}

bool OTGWUpgrade::verifyCode(const unsigned short *code, const unsigned short *data, short len) {
    short i;
    bool rc = true;

    for (i = 0; i < len; i++) {
        if (data[i] != (code[i] & 0x3fff)) {
            Dprintf("Verify Program 0x%04x: 0x%04x <> 0x%04x\n",
              pc + i, data[i], code[i] & 0x3fff);
            errcnt++;
            rc = false;
        }
    }
    return rc;
}

short OTGWUpgrade::loadData(short addr) {
    short first = -1, last;
    byte fwcommand[68] = {CMD_WRITEDATA};
    for (short i = 0, pc = addr, ptr = 4; i < 64; i++, pc++) {
        if (datamem[pc] != eedata[pc]) {
            if (first < 0) first = pc;
            last = pc;
        } else if (first < 0) {
            continue;
        }
        fwcommand[ptr++] = datamem[pc];
    }
    if (first < 0) return 0;
    Dprintf("Write EEDATA Memory %d bytes @0x%04x\n", last - first + 1, first);
    fwcommand[1] = last - first + 1;
    fwcommand[2] = first & 0xff;
    fwcommand[3] = first >> 8;
    fwCommand(fwcommand, last - first + 5);
    return last - first + 1;
}

void OTGWUpgrade::readData(short addr, short len) {
    byte fwcommand[] = {CMD_READDATA, (byte)len, 0, 0};
    Dprintf("Read EEDATA Memory %d bytes @0x%04x\n", len, addr);
    fwcommand[2] = addr & 0xff;
    fwCommand(fwcommand, sizeof(fwcommand));
}

bool OTGWUpgrade::verifyData(short addr, const byte *data, short len) {
    bool rc = true;

    for (short i = 0, pc = addr; i < len; i++, pc++) {
        if (datamem[pc] != eedata[pc]) {
            if (data[i] != datamem[pc]) {
                Dprintf("Verify EEDATA 0x%04x: 0x%02x <> 0x%02x\n",
                  pc, data[i], datamem[pc]);
                errcnt++;
                rc = false;
            }
            eedata[pc] = data[i];
        }
    }
    return rc;
}

void OTGWUpgrade::stateMachine(const unsigned char *packet, int len) {
    const unsigned short *data = (const unsigned short *)packet;
    byte cmd = cmdcode;

    if (stage != FWSTATE_IDLE && packet == nullptr) {
        int maxtries = (stage == FWSTATE_CODE || stage == FWSTATE_DATA ? 100 : 10);
        if (++retries >= maxtries) {
            serial->resetPic();
            finishUpgrade(OTGW_ERROR_RETRIES);
            return;
        }
        Dprintf("Retry (%d): stage = %d, pc = 0x%04x, cmd = %d\n",
          retries, stage, pc, cmdcode);
    } else {
        // Determine the (most likely) next command
        switch (cmdcode) {
         case CMD_READPROG:
            cmd = CMD_ERASEPROG;
            break;
         case CMD_WRITEPROG:
            cmd = CMD_READPROG;
            break;
         case CMD_ERASEPROG:
            cmd = CMD_WRITEPROG;
            break;
         case CMD_READDATA:
            cmd = CMD_WRITEDATA;
            break;
         case CMD_WRITEDATA:
            cmd = CMD_READDATA;
            break;
        }
    }

    switch (stage) {
     case FWSTATE_IDLE:
        errcnt = 0;
        retries = 0;
        done = 0;
        serial->resetPic();
        stage = FWSTATE_RSET;
        break;
     case FWSTATE_RSET:
        if (packet != nullptr) {
            byte fwcommand[] = {CMD_VERSION, 3};
            progress(WEIGHT_RESET);
            fwCommand(fwcommand, sizeof(fwcommand));
            stage = FWSTATE_VERSION;
        } else {
            serial->resetPic();
        }
        break;
     case FWSTATE_VERSION:
        if (data != nullptr) {
            Dprintf("Bootloader version %d.%d\n", packet[3], packet[4]);
            OTGWProcessor pic;
            switch (packet[3]) {
             case 1: pic = PIC16F88; break;
             case 2: pic = PIC16F1847; break;
             default:
                finishUpgrade(OTGW_ERROR_DEVICE);
                return;
            }
            if (model == PICPROBE) {
                // Select the file depending on the detected PIC model
                char hexfile[40];
                snprintf_P(hexfile, sizeof(hexfile), "/%s/%s",
                  serial->processorToString(pic).c_str(), filename);
                OTGWError rc = readHexFile(hexfile);
                if (rc == OTGW_ERROR_NONE) {
                    // Correct the progress now the total has been determined
                    progress(0);
                } else {
                    finishUpgrade(rc);
                    return;
                }
            }
            // Check bootloader version against the target PIC
            if (pic == model) {
                // PIC matches the firmware
            } else {
                // Device doesn't match the firmware
                finishUpgrade(OTGW_ERROR_DEVICE);
                return;
            }
            protectstart = data[2];
            protectend = data[3];
            info.recover(protectstart, failsafe);
            progress(WEIGHT_VERSION);
            if (firmware == FIRMWARE_OTGW && *fwversion && version) {
                // Both old and new gateway firmware versions are known
                // Dump the current eeprom data to be able to transfer the settings
                pc = 0;
                readData(pc);
                stage = FWSTATE_DUMP;
            } else {
                eraseCode(info.erasesize);
                stage = FWSTATE_PREP;
            }
        } else {
            byte fwcommand[] = {CMD_VERSION, 3};
            fwCommand(fwcommand, sizeof(fwcommand));
            stage = FWSTATE_VERSION;
        }
        break;
     case FWSTATE_DUMP:
        if (packet != nullptr) {
            progress(WEIGHT_DATAREAD);
            const unsigned char *bytes = packet + 4;
            Dprintf("Dump EEPROM: 0x%04x\n", pc);
            for (int i = 0; i < 64; i++, pc++) {
                if (datamem[pc] == eedata[pc]) {
                    // The new firmware doesn't use this EEPROM address
                    // Keep the bytes the same to keep track of this
                    datamem[pc] = bytes[i];
                }
                eedata[pc] = bytes[i];
            }
        }
        if (pc < info.datasize) {
            readData(pc);
        } else {
            // Transfer the EEPROM settings
            transferSettings(fwversion, version);
            eraseCode(info.erasesize);
            stage = FWSTATE_PREP;
        }
        break;
     case FWSTATE_PREP:
        // Write a jump to the self-programming code in the second code block
        // Should programming be aborted between erasing and reprogramming the
        // first code block, then this jump will still allow the PIC to be
        // recovered.

        // Programming has started; invalidate the firmware version
        *fwversion = '\0';
        if (cmd == CMD_WRITEPROG) {
            loadCode(info.erasesize, failsafe, 4);
        } else if (cmd == CMD_READPROG) {
            readCode(info.erasesize, 4);
        } else {
            if (packet != nullptr && packet[1] == 4 && data[1] == info.erasesize && verifyCode(failsafe, data + 2, 4)) {
                Dprintf("Fail safe code installed\n");
                // The fail safe is in place, programming can start
                progress(WEIGHT_CODEPROG);
                // Return to the start of the file
                hexfd.seek(0, SeekSet);
                hexseg = 0;
                hexaddr = 0;
                hexpos = 0;
                pc = prepareCode(codemem);
                eraseCode(pc);
                stage = FWSTATE_CODE;
            } else {
                // Failed. Try again.
                eraseCode(info.erasesize);
            }
        }
        break;
     case FWSTATE_CODE:
        if (cmd == CMD_WRITEPROG) {
            // digitalWrite(LED2, LOW);
            loadCode(pc, codemem);
        } else if (cmd == CMD_READPROG) {
            // digitalWrite(LED2, HIGH);
            readCode(pc);
        } else if (cmd == CMD_ERASEPROG) {
            if (packet != nullptr && packet[1] == 32 && data[1] == pc && verifyCode(codemem, data + 2)) {
              do {
                  pc = prepareCode(codemem);
              } while (pc + 31 >= protectstart && pc <= protectend);
                if (pc >= info.codesize) {
                    pc = 0;
                    stage = FWSTATE_DATA;
                    while (loadData(pc) == 0) {
                        pc += 64;
                        if (pc >= info.datasize) {
                            finishUpgrade(OTGW_ERROR_NONE);
                            break;
                        }
                    }
                    break;
                } else {
                    eraseCode(pc);
                    progress(WEIGHT_CODEPROG);
                    break;
                }
            } else {
                eraseCode(pc);
            }
        }
        break;
     case FWSTATE_DATA:
        if (cmd == CMD_READDATA) {
            // digitalWrite(LED2, HIGH);
            readData(pc);
        } else if (cmd == CMD_WRITEDATA) {
            if (packet != nullptr && verifyData(pc, packet + 4)) {
                progress(WEIGHT_DATAPROG);
                do {
                    pc += 64;
                    if (pc >= info.datasize) {
                        finishUpgrade(OTGW_ERROR_NONE);
                        break;
                    }
                }
                while (loadData(pc) == 0);
            } else {
                Dprintf("Data block failed: 0x%04x\n", pc);
                // Data is incorrect, try again
                // digitalWrite(LED2, LOW);
                loadData(pc);
            }
        }
        break;
    }

    if (stage != FWSTATE_IDLE) {
        lastaction = millis();
    }
}

OTGWError OTGWUpgrade::finishUpgrade(OTGWError result) {
    if (stage != FWSTATE_IDLE) {
        byte fwcommand[] = {CMD_RESET, 0};
        fwCommand(fwcommand, sizeof(fwcommand));
        stage = FWSTATE_IDLE;
    }
    if (hexfd) hexfd.close();

    serial->finishUpgrade(result, errcnt, retries);
    return result;
}

void OTGWUpgrade::upgradeEvent(int ch) {
    bool dle;

    // Dprintf("upgradeEvent(%d)\n", ch);
    dle = bufpos < sizeof(buffer) && buffer[bufpos] == DLE;
    if (!dle && ch == STX) {
        serial->SetLED(1);
        bufpos = 0;
        checksum = 0;
        buffer[0] = '\0';
    } else if ((!dle || stage == FWSTATE_RSET) && ch == ETX) {
        serial->SetLED(0);
        if (checksum == 0 || stage == FWSTATE_RSET) {
            stateMachine(buffer, bufpos);
        } else {
            // Checksum mismatch
            Dprintf("Invalid checksum: 0x%02x\n", checksum);
            stateMachine();
        }
    } else if (bufpos >= sizeof(buffer)) {
        // Prevent overwriting the program data
    } else if (!dle && ch == DLE) {
        buffer[bufpos] = ch;
    } else {
        buffer[bufpos++] = ch;
        checksum -= ch;
        buffer[bufpos] = '\0';
    }
}

bool OTGWUpgrade::upgradeTick() {
    if (stage == FWSTATE_IDLE) {
        return false;
    } else if (millis() - lastaction > 1000) {
        // Too much time has passed since the last action
        Dprintf("Timeout:");
        if (bufpos) {
            for (int i = 0; i < bufpos; i++) {
                Dprintf(" %02x", buffer[i]);
            }
            Dprintf("\n");
            bufpos = 0;
        }
        // Send a non-DLE byte in case the PIC is waiting for a byte following
        // a DLE. Choosing newline so a next GW=R command will be recognized.
        serial->putbyte('\n');
        stateMachine();
        return true;
    }
}

OTGWSerial::OTGWSerial(int resetPin, int progressLed)
: HardwareSerial(UART0), _reset(resetPin), _led(progressLed) {
    HardwareSerial::begin(9600, SERIAL_8N1);
    // The PIC may have been confused by garbage on the
    // serial interface when the NodeMCU resets.
    resetPic();
    fwversion[0] = '\0';
    _version_pos = 0;
}

int OTGWSerial::available() {
    if (upgradeEvent()) return 0;
    return HardwareSerial::available();
}

// Reimplement the read function. Other read functions call this to implement their functionality.
int OTGWSerial::read() {
    int retval;
    if (upgradeEvent()) return -1;
    retval = HardwareSerial::read();
    if (retval >= 0) {
        matchBanner(retval);
    }
    return retval;
}

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
    Dprintf("resetPic()\n");
    if (_reset >= 0) {
        pinMode(_reset, OUTPUT);
        digitalWrite(_reset, LOW);
    }
    // Can't use HardwareSerial::print because that calls Serial::write()
    const unsigned char cmd[] = "GW=R\r";
    for (const uint8_t *s = cmd; *s; s++) {
        HardwareSerial::write(*s);
    }
    if (_reset >= 0) {
        delay(100);
        digitalWrite(_reset, HIGH);
        pinMode(_reset, INPUT);
    }

    for (int i = 0; i < FIRMWARE_COUNT; i++) _banner_matched[i] = 0;
}

const char *OTGWSerial::firmwareVersion() {
    return fwversion;
}

OTGWFirmware OTGWSerial::firmwareType() {
    return firmware;
}

String OTGWSerial::firmwareToString(OTGWFirmware fw) {
    switch (fw) {
     case FIRMWARE_OTGW:
        return F("gateway");
     case FIRMWARE_DIAG:
        return F("diagnose");
     case FIRMWARE_INTF:
        return F("interface");
     default:
        return F("unknown");
    }
}

String OTGWSerial::firmwareToString() {
    return firmwareToString(firmwareType());
}

OTGWProcessor OTGWSerial::processor() {
    int major;
    if (sscanf(fwversion, "%d", &major) < 1) {
        return PICUNKNOWN;
    } else if (major < newpic[firmware]) {
        return PIC16F88;
    } else {
        return PIC16F1847;
    }
}

String OTGWSerial::processorToString(OTGWProcessor pic) {
    switch (pic) {
     case PIC16F88:
        return F("pic16f88");
     case PIC16F1847:
        return F("pic16f1847");
     default:
        return F("unknown");
    }
}

String OTGWSerial::processorToString() {
    return processorToString(processor());
}

#ifdef DEBUG
void OTGWSerial::registerDebugFunc(OTGWDebugFunction *func) {
    debugfunc = func;
}
#endif

void OTGWSerial::registerFinishedCallback(OTGWUpgradeFinished *func) {
    _finishedFunc = func;
}

void OTGWSerial::registerProgressCallback(OTGWUpgradeProgress *func) {
    _progressFunc = func;
}

void OTGWSerial::registerFirmwareCallback(OTGWFirmwareReport *func) {
    _firmwareFunc = func;
}

void OTGWSerial::SetLED(int state) {
    if (_led >= 0) {
        digitalWrite(_led, state ? LOW : HIGH);
    }
}

void OTGWSerial::putbyte(uint8_t c) {
    HardwareSerial::write(c);
}

void OTGWSerial::progress(int pct) {
    if (_progressFunc) _progressFunc(pct);
}

// Look for banners in the incoming data and extract the version number
void OTGWSerial::matchBanner(char ch) {
    for (int i = 0; i < FIRMWARE_COUNT; i++) {
        const char *banner = banners[i];
        char c = pgm_read_byte(banner + _banner_matched[i]);
        if (c == '\0') {
            if (isspace(ch)) {
                fwversion[_version_pos] = '\0';
                firmware = (OTGWFirmware)i;
                _banner_matched[i] = 0;
                _version_pos = 0;
                if (_firmwareFunc) {
                    _firmwareFunc(firmware, fwversion);
                }
            } else {
                fwversion[_version_pos++] = ch;
            }
        } else if (ch == c) {
            _banner_matched[i]++;
        } else {
            _banner_matched[i] = 0;
            c = pgm_read_byte(banner + _banner_matched[i]);
            if (ch == c) _banner_matched[i]++;
        }
    }
}

OTGWError OTGWSerial::startUpgrade(const char *hexfile) {
    if (_upgrade != nullptr) {
        return OTGW_ERROR_INPROG;
    }

    _upgrade = new OTGWUpgrade(this);

    if (_upgrade == nullptr) {
        return OTGW_ERROR_MEMORY;
    }

    return _upgrade->start(hexfile);
}

OTGWError OTGWSerial::finishUpgrade(OTGWError result, short errors, short retries) {
    if (_finishedFunc) {
        _finishedFunc(result, errors, retries);
    }
    // Destroy the upgrade object to free the used memory and be ready
    // for a next upgrade
    delete _upgrade;
    _upgrade = nullptr;

    return result;
}

// Proceed with the upgrade, if applicable
bool OTGWSerial::upgradeEvent() {
    int ch;
    if (!_upgrade) return false;
    while (HardwareSerial::available()) {
        ch = HardwareSerial::read();
        _upgrade->upgradeEvent(ch);
    }
    // The upgrade object may have been destroyed
    if (!_upgrade) return false;
    return _upgrade->upgradeTick();
}
