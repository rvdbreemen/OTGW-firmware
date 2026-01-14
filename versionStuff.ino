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
    while (ptr < 256)
    {
      // Fixed safe search for PROGMEM string in buffer
      bool match = false;
      if (ptr + bannerLen <= 256) {
           if (strncmp_P((char *)datamem + ptr, banner, bannerLen) == 0) {
               match = true;
           }
      }
      
      if (match)
      {
         // Found banner
         char *s = (char *)datamem + ptr + bannerLen;
         
         // Safely copy version string, ensuring we don't read past end of datamem
         // Calculate max possible length of version string in datamem
         size_t maxLen = 256 - (ptr + bannerLen);
         
         // Determine maximum number of bytes we are allowed to copy:
         // limited by remaining datamem space (maxLen) and destination buffer size minus one.
         size_t maxCopy = maxLen;
         if (destSize > 0 && (destSize - 1) < maxCopy) {
           maxCopy = destSize - 1;
         }
         
         // We can't rely on strlcpy finding a null terminator quickly in non-string binary data
         // But GetVersion expects a string. The hex file data for version IS usually null-terminated in the EEPROM image.
         // Use strnlen to find length within the copy bounds.
         size_t verLen = strnlen(s, maxCopy);
         
         memcpy(version, s, verLen);
         version[verLen] = '\0';
         
         return;
      } else {
        // Move to next string (skip until null or end) or just increment
        // OTGWSerial implementation uses strnlen to skip current string.
        // But datamem might not be null terminated correctly everywhere.
        // Safer to just increment or scan for 0.
        
        // Original logic: ptr += strnlen((char *)datamem + ptr, 256 - ptr) + 1;
        // This logic mimics strstr skipping behavior if we assume list of strings.
        // Use bounded strnlen and ensure we don't advance past end of buffer
        size_t len = strnlen((char *)datamem + ptr, 256 - ptr);
        ptr += len;
        if (ptr < 256) {
          ptr++;
        }
      }
    }
  }
}
