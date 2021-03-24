/* 
***************************************************************************  
**  Program  : helperStuff
**  Version  : v0.8.1
**
**  Copyright (c) 2021 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/


//===========================================================================================
bool compare(String x, String y) 
{ 
    for (int i = 0; i < min(x.length(), y.length()); i++) { 
      if (x[i] != y[i]) 
      {
        return (bool)(x[i] < y[i]); 
      }
    } 
    return x.length() < y.length(); 
    
} // compare()


//===========================================================================================
bool isNumericp(const char *timeStamp, int8_t len)
{
  for (int i=0; (i<len && i<12);i++)
  {
    if (timeStamp[i] < '0' || timeStamp[i] > '9')
    {
      return false;
    }
  }
  return true;
  
} // isNumericp()

//===========================================================================================
// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}


bool splitCString(char *sIn, const char *del, char *cKey, char *cValue)
{
  //printf("sIn=[%s]\r\n", sIn);
  static char _key[128];
  static char _value[256];

  char * token = strtok(sIn, del);
  // loop through the string to extract all other tokens
  while( token != NULL ) {
      //now trim spaces from token and copy it to wOut
      //strlcpy(wOut[wc++], trimwhitespace(token), sizeof(wOut[0]));
      token = strtok(NULL, del);
  }
    
  return true;
}

int8_t splitCString(char *sIn, const char *del, char wOut[][10], const int maxWords)
{
    //printf("sIn=[%s]\r\n", sIn);
    int wc=0;    
    for(int i = 0; i<maxWords; i++){
        memset(wOut[i], '\0', sizeof(wOut[0]));  //make sure the c-strings are blank
    }
    char * token = strtok(sIn, del);
    // loop through the string to extract all other tokens
    while( token != NULL  && wc < maxWords) {
        //now trim spaces from token and copy it to wOut
        strlcpy(wOut[wc++], trimwhitespace(token), sizeof(wOut[0]));
        token = strtok(NULL, del);
    }
        
    // for (int i = 0; i<maxWords; i++){
    //     printf("wOut[%d]=[%s]\r\n", i, wOut[i]);
    // }

    return wc;
}

//===========================================================================================
int8_t splitString(String inStrng, char delimiter, String wOut[], uint8_t maxWords) 
{
  int16_t inxS = 0, inxE = 0, wordCount = 0;
  
    inStrng.trim();
    while(inxE < inStrng.length() && wordCount < maxWords) 
    {
      inxE  = inStrng.indexOf(delimiter, inxS);         //finds location of first ,
      wOut[wordCount] = inStrng.substring(inxS, inxE);  //captures first data String
      wOut[wordCount].trim();
      //DebugTf("[%d] => [%c] @[%d] found[%s]\r\n", wordCount, delimiter, inxE, wOut[wordCount].c_str());
      inxS = inxE;
      inxS++;
      wordCount++;
    }
    // zero rest of the words
    for(int i=wordCount; i< maxWords; i++)
    {
      wOut[wordCount][0] = 0;
    }
    // if not whole string processed place rest in last word
    if (inxS < inStrng.length()) 
    {
      wOut[maxWords-1] = inStrng.substring(inxS, inStrng.length());  // store rest of String      
    }

    return wordCount;
    
} // splitString()



//===========================================================================================
void strConcat(char *dest, int maxLen, const char *src)
{
  if (strlen(dest) + strlen(src) < maxLen) 
  {
    strcat(dest, src);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()


//===========================================================================================
void strConcat(char *dest, int maxLen, float v, int dec)
{
  static char buff[25];
  if (dec == 0)       sprintf(buff,"%.0f", v);
  else if (dec == 1)  sprintf(buff,"%.1f", v);
  else if (dec == 2)  sprintf(buff,"%.2f", v);
  else if (dec == 3)  sprintf(buff,"%.3f", v);
  else if (dec == 4)  sprintf(buff,"%.4f", v);
  else if (dec == 5)  sprintf(buff,"%.5f", v);
  else                sprintf(buff,"%f",   v);

  if (strlen(dest) + strlen(buff) < maxLen) 
  {
    strcat(dest, buff);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()


//===========================================================================================
void strConcat(char *dest, int maxLen, int v)
{
  static char buff[25];
  sprintf(buff,"%d", v);

  if (strlen(dest) + strlen(buff) < maxLen) 
  {
    strcat(dest, buff);
  } 
  else
  {
    DebugTf("Combined string > %d chars\r\n", maxLen);
  }
  
} // strConcat()


//===========================================================================================
void strToLower(char *src)
{
  for (int i = 0; i < strlen(src); i++)
  {
    if (src[i] == '\0') return;
    if (src[i] >= 'A' && src[i] <= 'Z')
        src[i] += 32;
  }
} // strToLower()

//===========================================================================================
// a 'save' string copy
void strCopy(char *dest, int maxLen, const char *src, int frm, int to)
{
  int d=0;
//DebugTf("dest[%s], src[%s] max[%d], frm[%d], to[%d] =>\r\n", dest, src, maxLen, frm, to);
  dest[0] = '\0';
  for (int i=0; i<=frm; i++)
  {
    if (src[i] == 0) return;
  }
  for (int i=frm; (src[i] != 0  && i<=to && d<maxLen); i++)
  {
    dest[d++] = src[i];
  }
  dest[d] = '\0';
    
} // strCopy()

//===========================================================================================
// a 'save' version of strncpy() that does not put a '\0' at
// the end of dest if src >= maxLen!
void strCopy(char *dest, int maxLen, const char *src)
{
  dest[0] = '\0';
  strcat(dest, src);
    
} // strCopy()

//===========================================================================================
// 'tttABCtDEtFGHttt' => 'ABCtDEtFGHttt'
void strLTrim(char *dest, int maxLen, const char tChar )
{
  char tmp[maxLen];
  int  tPos = 0;
  bool done = false;
  
  tmp[0] = '\0';

  for (int dPos=0; (dPos<maxLen && dest[dPos] != '\0'); dPos++)
  {
    if (dest[dPos] != tChar || done)
    {
      tmp[tPos++] = dest[dPos];
      done = true;
    }
  }
  tmp[tPos] = '\0';
  strCopy(dest, maxLen, tmp);
    
} // strLTrim()

//===========================================================================================
// 'tttABCtDEtFGHttt' => 'tttABCtDEtFGH'
void strRTrim(char *dest, int maxLen, const char tChar )
{
  char tmp[maxLen];
  int dPos, tPos, dMax;
  bool done = false;
  
  for(dMax=0; dMax<maxLen; dMax++) { tmp[dMax] = '\0'; }
  
  dMax = strlen(dest)-1;
  for (dPos=dMax; (dPos>=0 && !done); dPos--)
  {
    if (dest[dPos] == tChar)
    {
      tPos = dPos;
      tmp[tPos] = '\0';
    }
    else done = true;
  }
  dPos++;
  for(dMax = 0; dMax <= dPos; dMax++)
  {
    tmp[dMax] = dest[dMax];
  }
  tmp[dMax+1] = '\0';
  strCopy(dest, maxLen, tmp);
    
} // strRTrim()

//===========================================================================================
// 'tttABCtDEtFGHttt' => 'ABCtDEtFGH'
void strTrim(char *dest, int maxLen, const char tChar )
{
  char sTmp[maxLen];
  
  strCopy(sTmp, maxLen, dest); 
  strLTrim(sTmp, maxLen, tChar);
  strRTrim(sTmp, maxLen, tChar);
  strCopy(dest, maxLen, sTmp); 
    
} // strTrim()

//===========================================================================================
void strRemoveAll(char *dest, int maxLen, const char tChar)
{
  char tmp[maxLen];
  int  tPos = 0;
  tmp[0] = '\0';
  for (int dPos=0; (dPos<maxLen && dest[dPos] != '\0'); dPos++)
  {
    if (dest[dPos] != tChar)
    {
      tmp[tPos++] = dest[dPos];
    }
  }
  tmp[tPos] = '\0';
  strCopy(dest, maxLen, tmp);
    
} // strRemoveAll()


//===========================================================================================
void strTrimCntr(char *dest, int maxLen)
{
  char tmp[maxLen];
  int tPos = 0;
  tmp[0] = '\0';
  for (int dPos=0; (dPos<maxLen && dest[dPos] != '\0'); dPos++)
  {
    if (dest[dPos] >= ' ' && dest[dPos] <= '~')  // space = 32, '~' = 127
    {
      tmp[tPos++] = dest[dPos];
    }
  }
  tmp[tPos] = '\0';
  strCopy(dest, maxLen, tmp);
    
} // strTrimCntr()

//===========================================================================================
int strIndex(const char *haystack, const char *needle, int start)
{
  char *p = strstr (haystack+start, needle);
  if (p) {
    //DebugTf("found [%s] at position [%d]\r\n", needle, (p - haystack));
    return (p - haystack);
  }
  return -1;
  
} // strIndex()

//===========================================================================================
int strIndex(const char *haystack, const char *needle)
{
  return strIndex(haystack, needle, 0);
  
} // strIndex()

//===========================================================================================
int stricmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
    
} // stricmp()

//===========================================================================================
char *intToStr(int32_t v)
{
  static char buff[25];
  sprintf(buff,"%d", v);
  return buff;
  
} // intToStr()

//===========================================================================================
char *floatToStr(float v, int dec)
{
  static char buff[25];
  if (dec == 0)       sprintf(buff,"%.0f", v);
  else if (dec == 1)  sprintf(buff,"%.1f", v);
  else if (dec == 2)  sprintf(buff,"%.2f", v);
  else if (dec == 3)  sprintf(buff,"%.3f", v);
  else if (dec == 4)  sprintf(buff,"%.4f", v);
  else if (dec == 5)  sprintf(buff,"%.5f", v);
  else                sprintf(buff,"%f",   v);
  return buff;
  
} // floattToStr()

//===========================================================================================
float formatFloat(float v, int dec)
{
  return (String(v, dec).toFloat());

} //  formatFloat()

//===========================================================================================
float strToFloat(const char *s, int dec)
{
  float r =  0.0;
  int   p =  0;
  int   d = -1;
  
  r = strtof(s, NULL);
  p = (int)(r*pow(10, dec));
  r = p / pow(10, dec);
  //DebugTf("[%s][%d] => p[%d] -> r[%f]\r\n", s, dec, p, r);
  return r; 

} //  strToFloat()

//===========================================================================================
void parseJsonKey(const char *sIn, const char *key, char *val, int valLen)
{
  // json key-value pair looks like:
  //      "samenv": "Zwaar bewolkt",
  // or   "samenv": "Zwaar bewolkt"}
  int keyStart   = strIndex(sIn, key);
  int sepStart   = strIndex(sIn, ",", keyStart);
  if (sepStart == -1) 
  {
    sepStart   = strIndex(sIn, "}", keyStart);
  }
  strCopy(val, valLen, sIn, keyStart+strlen(key), sepStart);
  strRemoveAll(val, valLen, ':');
  strRemoveAll(val, valLen, ',');
  strRemoveAll(val, valLen, '}');
  strRemoveAll(val, valLen, '"');
  strTrim(val, valLen, ' ');
  
} // parseJsonKey()


//===========================================================================================
boolean isValidIP(IPAddress ip)
{
 /* Works as follows:
   *  example: 
   *  127.0.0.1 
   *   1 => 127||0||0||1 = 128>0 = true 
   *   2 => !(false || false) = true
   *   3 => !(false || false || false || false ) = true
   *   4 => !(true && true && true && true) = false
   *   5 => !(false) = true
   *   true && true & true && false && true = false ==> correct, this is an invalid addres
   *   
   *   0.0.0.0
   *   1 => 0||0||0||0 = 0>0 = false 
   *   2 => !(true || true) = false
   *   3 => !(false || false || false || false) = true
   *   4 => !(true && true && true && tfalse) = true
   *   5 => !(false) = true
   *   false && false && true && true && true = false ==> correct, this is an invalid addres
   *   
   *   192.168.0.1 
   *   1 => 192||168||0||1 =233>0 = true 
   *   2 => !(false || false) = true
   *   3 +> !(false || false || false || false) = true
   *   4 => !(false && false && true && true) = true
   *   5 => !(false) = true
   *   true & true & true && true && true = true ==> correct, this is a valid address
   *   
   *   255.255.255.255
   *   1 => 255||255||255||255 =255>0 = true 
   *   2 => !(false || false ) = true
   *   3 +> !(true || true || true || true) = false
   *   4 => !(false && false && false && false) = true
   *   5 => !(true) = false
   *   true && true && false && true && false = false ==> correct, this is an invalid address
   *   
   *   0.123.12.1       => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.0.0.0         => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.255.0.1       => true && true && false && true && true = false  ==> correct, this is an invalid address 
   *   150.150.255.150  => true && true && false && true && true = false  ==> correct, this is an invalid address 
   *   
   *   123.21.1.99      => true && true && true && true && true = true    ==> correct, this is annvalid address 
   *   1.1.1.1          => true && true && true && true && true = true    ==> correct, this is annvalid address 
   *   
   *   Some references on valid ip addresses: 
   *   - https://www.quora.com/How-do-you-identify-an-invalid-IP-address
   *   
   */
  boolean _isValidIP = false;
  _isValidIP = ((ip[0] || ip[1] || ip[2] || ip[3])>0);                             // if any bits are set, then it is not 0.0.0.0
  _isValidIP &= !((ip[0]==0) || (ip[3]==0));                                       // if either the first or last is a 0, then it is invalid
  _isValidIP &= !((ip[0]==255) || (ip[1]==255) || (ip[2]==255) || (ip[3]==255)) ;  // if any of the octets is 255, then it is invalid  
  _isValidIP &= !(ip[0]==127 && ip[1]==0 && ip[2]==0 && ip[3]==1);                 // if not 127.0.0.0 then it might be valid
  _isValidIP &= !(ip[0]>=224);                                                     // if ip[0] >=224 then reserved space  
  
  // DebugTf( "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  // if (_isValidIP) 
  //   Debugln(F(" = Valid IP")); 
  // else 
  //   Debugln(F(" = Invalid IP!"));
    
  return _isValidIP;
  
} //  isValidIP()


uint32_t updateRebootCount()
{
  //simple function to keep track of the number of reboots 
  //return: number of reboots (if it goes as planned)
  uint32_t _reboot = 0;
  #define REBOOTCNT_FILE "/reboot_count.txt"
  if (LittleFS.begin()) {
    //start with opening the file
    File fh = LittleFS.open(REBOOTCNT_FILE, "r");
    if (fh) {
      //read from file
      if (fh.available()){
        //read the first line 
        _reboot  = fh.readStringUntil('\n').toInt();
      }
    }
    fh.close();
    //increment reboot counter
    _reboot++;
    //write back the reboot counter
    fh = LittleFS.open(REBOOTCNT_FILE, "w");
    if (fh) {
      //write to _reboot to file
      fh.println(_reboot);
    }
    fh.close();
  }
  DebugTf("Reboot count = [%d]\r\n", rebootCount);
  return _reboot;
}

void doRestart(const char* str) {
  DebugTln(str);
  delay(2000);  // Enough time for messages to be sent.
  ESP.restart();
  delay(5000);  // Enough time to ensure we don't return.
}

String upTime() 
{
  char    calcUptime[20];

  snprintf(calcUptime, sizeof(calcUptime), "%d(d)-%02d:%02d(H:m)"
                                          , int((upTimeSeconds / (60 * 60 * 24)) % 365)
                                          , int((upTimeSeconds / (60 * 60)) % 24)
                                          , int((upTimeSeconds / (60)) % 60));

  return calcUptime;

} // upTime()

bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

bool dayChanged(){
  static int8_t lastday = 0;
  if (lastday==0) lastday = day();
  return (lastday != day());
}

bool hourChanged(){
  static int8_t lasthour = 0;
  if (lasthour==0) lasthour = hour();
  return (lasthour != hour());
}

/*
  check if the version githash is in the littlefs as version.hash

*/
bool checklittlefshash(){
  #define GITHASH_FILE "/version.hash"
  String _githash="";
  if (LittleFS.begin()) {
    //start with opening the file
    File fh = LittleFS.open(GITHASH_FILE, "r");
    if (fh) {
      //read from file
      if (fh.available()){
        //read the first line 
         _githash = fh.readStringUntil('\n');
      }
    }
    DebugTf("Check githash = [%s]\r\n", CSTR(_githash));
    return (stricmp(CSTR(_githash), _VERSION_GITHASH)==0);
  }
  return false;
}


/*
** Does not generate hex character constants.
** Always generates triple-digit octal constants.
** Always generates escapes in preference to octal.
** Escape question mark to ensure no trigraphs are generated by repetitive use.
** Handling of 0x80..0xFF is locale-dependent (might be octal, might be literal).
*/

void chr_cstrlit(unsigned char u, char *buffer, size_t buflen)
{
    if (buflen < 2)
        *buffer = '\0';
    else if (isprint(u) && u != '\'' && u != '\"' && u != '\\' && u != '\?')
        sprintf(buffer, "%c", u);
    else if (buflen < 3)
        *buffer = '\0';
    else
    {
        switch (u)
        {
        case '\a':  strcpy(buffer, "\\a"); break;
        case '\b':  strcpy(buffer, "\\b"); break;
        case '\f':  strcpy(buffer, "\\f"); break;
        case '\n':  strcpy(buffer, "\\n"); break;
        case '\r':  strcpy(buffer, "\\r"); break;
        case '\t':  strcpy(buffer, "\\t"); break;
        case '\v':  strcpy(buffer, "\\v"); break;
        case '\\':  strcpy(buffer, "\\\\"); break;
        case '\'':  strcpy(buffer, "\\'"); break;
        case '\"':  strcpy(buffer, "\\\""); break;
        case '\?':  strcpy(buffer, "\\\?"); break;
        default:
            if (buflen < 5)
                *buffer = '\0';
            else
                sprintf(buffer, "\\%03o", u);
            break;
        }
    }
}

void str_cstrlit(const char *str, char *buffer, size_t buflen)
{
    unsigned char u;
    size_t len;

    while ((u = (unsigned char)*str++) != '\0')
    {
        chr_cstrlit(u, buffer, buflen);
        if ((len = strlen(buffer)) == 0)
            return;
        buffer += len;
        buflen -= len;
    }
    *buffer = '\0';
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
