// attempt to get version info from .hex file
// modified hex file parser from OTGWSerial.cpp 
//
// -tjfs

#define byteswap(val) ((val << 8) | (val >> 8))

static const char banner[] PROGMEM = "OpenTherm Gateway ";

void GetVersion(const char* hexfile, char* version, size_t destSize){
  if (!version || destSize == 0) return;
  version[0] = '\0';
  
  char hexbuf[48]={0};
  int len, addr, tag, data, offs, linecnt = 0;
  char datamem[256]={0}; // prevent buffer overrun
  unsigned short ptr;
  File f;
  DebugTf(PSTR("GetVersion opening %s\r\n"),hexfile);
  f = LittleFS.open(hexfile, "r");
  if (f)  // only proceed if file exists
  {
    while (f.readBytesUntil('\n', hexbuf, sizeof(hexbuf)) != 0) {
      linecnt++;
      // DebugTf(PSTR("reading line %d %s\n"),linecnt,hexbuf);
      if (sscanf(hexbuf, ":%2x%4x%2x", &len, &addr, &tag) != 3)
      {
        DebugTf(PSTR("Parse error on line %d\n"),linecnt);// Parse error
        break;
      }
      // DebugTf(PSTR("Read in %2x %4x %2x\n"),len,addr,tag);
      if (len & 1)
      {
        DebugTf(PSTR("Invalid data size on line %d\n"),linecnt);// Invalid data size
        break;
      }
      offs = 9;
      len >>= 1;
      if (tag == 0)
      {
       // DebugTf(PSTR("Checking address %4x on line %d\r\n"),addr,linecnt);// Invalid data size
        if (addr >= 0x4200 && addr <= 0x4300)
        {
          // Data memory 16f88
          addr = (addr - 0x4200) >> 1;
          while (len > 0)
          {
            if (sscanf(hexbuf + offs, "%04x", &data) != 1)
              break;
            // if (!bitRead(datamap, addr / 64)) weight += WEIGHT_DATAPROG;
            // bitSet(datamap, addr / 64);
            //DebugTf(PSTR("storing %x in %x\r\n"),data,addr);
            datamem[addr++] = byteswap(data);
            offs += 4;
            len--;
          }
        } else if (addr >= 0xe000 && addr <= 0xe100)
        {
          // Data memory 16f1847
          addr = (addr - 0xe000) >> 1;
          while (len > 0)
          {
            if (sscanf(hexbuf + offs, "%04x", &data) != 1)
              break;
            // if (!bitRead(datamap, addr / 64)) weight += WEIGHT_DATAPROG;
            // bitSet(datamap, addr / 64);
            //DebugTf(PSTR("storing %x in %x\n"),data,addr);
            datamem[addr++] = byteswap(data);
            offs += 4;
            len--;
          }
        }
        //if (len) {
        //DebugTf(PSTR("len>0\n"));
        //break;
        //}
      } else if (tag == 1) {
        //DebugTf(PSTR("tag==1\n"));
        break;
      }
    }
    f.close();
    //DebugTf(PSTR("closing file and hunting for banner\n"));
    ptr = 0; 
    size_t bannerLen = sizeof(banner) - 1;
    
    // Safer sliding window search:
    // 1. Iterate byte-by-byte (ptr++) instead of skipping over strings, so we can't miss a banner inside a block.
    // 2. Ensure reading stays strictly within bounds (256 - bannerLen).
    for (ptr = 0; ptr <= (256 - bannerLen); ptr++) {
        // Safe comparison with PROGMEM string using memcmp_P for binary data
        if (memcmp_P((char *)datamem + ptr, banner, bannerLen) == 0) {
             // Match found!
             char * content = (char *)datamem + ptr + bannerLen;
             size_t maxContentLen = 256 - (ptr + bannerLen);
             
             // Extract version string safely
             // Stop at:
             // 1. End of datamem buffer (maxContentLen)
             // 2. Destination buffer full (destSize - 1)
             // 3. Null terminator
             // 4. Non-printable character (garbage) 
             size_t vLen = 0;
             while(vLen < maxContentLen && vLen < (destSize - 1)) {
                 char c = content[vLen];
                 if (c == '\0' || !isprint(c)) break; // Stop at end of valid string
                 vLen++;
             }
             
             memcpy(version, content, vLen);
             version[vLen] = '\0';
             return;
        }
    }
    DebugTf(PSTR("GetVersion: banner not found in %s\r\n"), hexfile);
  }
}
