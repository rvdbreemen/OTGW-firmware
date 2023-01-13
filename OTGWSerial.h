/*
  OTGWSerial.h - Library for OTGW PIC communication
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

#ifndef OTGWSerial_h
#define OTGWSerial_h

#include <HardwareSerial.h>

typedef enum {
    PIC16F88,
    PIC16F1847,
    PICCOUNT,
    PICUNKNOWN,
    PICPROBE
} OTGWProcessor;

typedef enum {
    FIRMWARE_OTGW,
    FIRMWARE_DIAG,
    FIRMWARE_INTF,
    FIRMWARE_COUNT,
    FIRMWARE_UNKNOWN
} OTGWFirmware;

typedef enum {
    OTGW_ERROR_NONE,        // No error
    OTGW_ERROR_MEMORY,      // Not enough space
    OTGW_ERROR_HEX_ACCESS,  // Could not open hex file
    OTGW_ERROR_HEX_FORMAT,  // Invalid format of hex file
    OTGW_ERROR_HEX_DATASIZE,// Wrong data size in hex file
    OTGW_ERROR_HEX_CHECKSUM,// Bad checksum in hex file
    OTGW_ERROR_INPROG,      // Firmware upgrade in progress
    OTGW_ERROR_MAGIC,       // Hex file does not contain expected data
    OTGW_ERROR_RESET,       // PIC reset failed
    OTGW_ERROR_RETRIES,     // Too many retries
    OTGW_ERROR_MISMATCHES,  // Too many mismatches
    OTGW_ERROR_DEVICE       // Wrong PIC (16F88 <=> 16F1847)
} OTGWError;

struct PicInfo {
    unsigned short datasize;
    unsigned short codesize;
    unsigned short confsize;
    unsigned short cfgbase;
    unsigned short eebase;
    unsigned short erasesize;
    unsigned short groupsize;
    byte blockwrite;
    unsigned short magic[4];
    unsigned short (*recover)(unsigned short, unsigned short *);
};

typedef struct {
    unsigned short addr;
    byte size, mask;
} OTGWTransferData;

typedef void OTGWUpgradeFinished(OTGWError result, short errors, short retries);
typedef void OTGWUpgradeProgress(int pct);
typedef void OTGWFirmwareReport(OTGWFirmware fw, const char *version);
typedef void OTGWDebugFunction(const char *fmt, ...);

class OTGWSerial;

class OTGWUpgrade {
public:
   OTGWUpgrade(OTGWSerial *serial);
   ~OTGWUpgrade();
   OTGWError start(const char *hexfile);
   void upgradeEvent(int ch);
   bool upgradeTick();
protected:
   void progress(int weight);
   unsigned char hexChecksum(char *hex, int len);
   OTGWError readHexRecord();
   OTGWError readHexFile(const char *hexfile);
   int versionCompare(const char *version1, const char* version2);
   int eepromSettings(const char *version, OTGWTransferData *xfer);
   void transferSettings(const char *ver1, const char *ver2);
   int prepareCode(unsigned short *buffer);
   void fwCommand(const unsigned char *cmd, int len);
   void eraseCode(short addr);
   short loadCode(short addr, const unsigned short *code, short len = 32);
   void readCode(short addr, short len = 32);
   bool verifyCode(const unsigned short *code, const unsigned short *data, short len = 32);
   short loadData(short addr);
   void readData(short addr, short len = 64);
   bool verifyData(short addr, const byte *data, short len = 64);
   void stateMachine(const unsigned char *packet = nullptr, int len = 0);
   OTGWError finishUpgrade(OTGWError result);

   OTGWSerial *serial;
   unsigned char buffer[80];
   unsigned char datamem[256];
   unsigned char eedata[256];
   // Enough for one memory row
   unsigned short codemem[32];
   unsigned short failsafe[4];
   unsigned short protectstart, protectend;
   unsigned short pc, errcnt, retries, done, total;
   byte bufpos, checksum, cmdcode, model, stage;
   unsigned long lastaction;
   struct PicInfo info;
   // Variables for reading a hex file line by line
   File hexfd;
   int hexaddr;
   short hexseg;
   byte hexlen, hexpos;
   unsigned short hexdata[8];
   union {
       char *version;
       const char *filename;
   };
};

class OTGWSerial: public HardwareSerial {
   friend class OTGWUpgrade;
public:
   OTGWSerial(int resetPin = -1, int progressLed = -1);
   int available();
   int read();
   int availableForWrite();
   size_t write(uint8_t c);
   size_t write(const uint8_t *buffer, size_t len);
   size_t write(const char *buffer, size_t len) {
       return write((const uint8_t *)buffer, len);
   }
   size_t write(const char *buffer) {
       if (buffer == nullptr) return 0;
       return write((const uint8_t *)buffer, strlen(buffer));
   }
   // These handle ambiguity for write(0) case, because (0) can be a pointer or an integer
   inline size_t write(short t) {return write((uint8_t)t);}
   inline size_t write(unsigned short t) {return write((uint8_t)t);}
   inline size_t write(int t) {return write((uint8_t)t);}
   inline size_t write(unsigned int t) {return write((uint8_t)t);}
   inline size_t write(long t) {return write((uint8_t)t);}
   inline size_t write(unsigned long t) {return write((uint8_t)t);}
   // Enable write(char) to fall through to write(uint8_t)
   inline size_t write(char c) {return write((uint8_t) c);}
   inline size_t write(int8_t c) {return write((uint8_t) c);}

   const char *firmwareVersion();
   OTGWFirmware firmwareType();
   String firmwareToString(OTGWFirmware fw);
   String firmwareToString();
   OTGWProcessor processor();
   String processorToString(OTGWProcessor pic);
   String processorToString();
   bool busy();
   void resetPic();
   OTGWError startUpgrade(const char *hexfile);
   void registerFinishedCallback(OTGWUpgradeFinished *func);
   void registerProgressCallback(OTGWUpgradeProgress *func);
   void registerFirmwareCallback(OTGWFirmwareReport *func);
#ifdef DEBUG
   void registerDebugFunc(OTGWDebugFunction *func);
#endif

protected:
   OTGWUpgrade *_upgrade = nullptr;
   OTGWUpgradeFinished *_finishedFunc = nullptr;
   OTGWUpgradeProgress *_progressFunc = nullptr;
   OTGWFirmwareReport *_firmwareFunc = nullptr;
   OTGWProcessor model = PIC16F88;
   int _reset, _led;
   byte _banner_matched[FIRMWARE_COUNT], _version_pos;

   OTGWError finishUpgrade(OTGWError result, short errors, short retries);
   void SetLED(int state);
   void progress(int pct);
   void putbyte(uint8_t c);
   void matchBanner(char ch);
   bool upgradeEvent();
};

#endif
