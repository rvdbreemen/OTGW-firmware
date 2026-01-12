// attempt to get version info from .hex file
// modified hex file parser from OTGWSerial.cpp 
//
// -tjfs

#define byteswap(val) ((val << 8) | (val >> 8))

static const char banner[] = "OpenTherm Gateway ";

String GetVersion(const String hexfile){
  char hexbuf[48]={0};
  int len, addr, tag, data, offs, linecnt = 0;
  char datamem[256]={0}; // prevent buffer overrun
  unsigned short ptr;
  File f;
  DebugTf(PSTR("GetVersion opening %s\r\n"),hexfile.c_str());
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
    while (ptr < 256)
    {
      //DebugTf(PSTR("checking for %s at char pos %d\r\n"),banner,ptr);
      char *s = strstr((char *)datamem + ptr, banner);
      if (!s)
      {
        //DebugTf(PSTR("did not find the banner\r\n"));
        ptr += strnlen((char *)datamem + ptr, 256 - ptr) + 1;
      } else {
        //DebugTf(PSTR("hit the banner! returning version string %s\r\n"),s);
        s += sizeof(banner) - 1;
        return String(s);
      }
    }
  }
  return ("");
}
