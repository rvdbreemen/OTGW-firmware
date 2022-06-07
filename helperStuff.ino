/* 
***************************************************************************  
**  Program  : helperStuff
**  Version  : v0.9.5
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <Arduino.h>  // for type definitions

template <typename T> void PROGMEM_readAnything (const T * sce, T& dest)
{
  memcpy_P (&dest, sce, sizeof (T));
}

template <typename T> T PROGMEM_getAnything (const T * sce)
{
  static T temp;
  memcpy_P (&temp, sce, sizeof (T));
  return temp;
}

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


//===========================================================================================
uint8_t splitString(String inStrng, char delimiter, String wOut[], uint8_t maxWords) 
{
    uint8_t wordCount = 0;
    size_t inxE = 0, inxS = 0; 
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
float formatFloat(float v, int dec)
{
  return (String(v, dec).toFloat());

} //  formatFloat()
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

bool updateRebootLog(String text)
{
  #define REBOOTLOG_FILE "/reboot_log.txt"
  #define TEMPLOG_FILE "/reboot_log.t.txt"
  #define LOG_LINES 20
  #define LOG_LINE_LENGTH 140

  char log_line[LOG_LINE_LENGTH] = {0};
  char log_line_regs[LOG_LINE_LENGTH] = {0};
  char log_line_excpt[LOG_LINE_LENGTH] = {0};
  uint32_t errorCode = -1;

  //waitforNTPsync();
  loopNTP(); // make sure time is up to date


  struct	rst_info	*rtc_info	=	system_get_rst_info();
  
  if (rtc_info == NULL) {
    DebugTf("no reset info available:	%x\r\n",	errorCode);
  } else {

    DebugTf("reset reason:	%x\r\n",	rtc_info->reason);
    errorCode = rtc_info->reason;
    // Rst cause No.    Cause                     GPIO state
    //--------------    -------------------       -------------
    // 0                Power reboot              Changed
    // 1                Hardware WDT reset        Changed
    // 2                Fatal exception           Unchanged
    // 3                Software watchdog reset   Unchanged
    // 4                Software reset            Unchanged
    // 5                Deep-sleep                Changed
    // 6                Hardware reset            Changed

    if	(rtc_info->reason	==	REASON_WDT_RST	|| rtc_info->reason	==	REASON_EXCEPTION_RST	|| rtc_info->reason	==	REASON_SOFT_WDT_RST)	{

      //The	address	of	the	last	crash	is	printed,	which	is	used	to	debug	garbled	output
      snprintf(log_line_regs, LOG_LINE_LENGTH,"ESP register contents: epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\r\n", rtc_info->epc1, rtc_info->epc2, rtc_info->epc3, rtc_info->excvaddr, rtc_info->depc);
      Debugf(log_line_regs);
    }

    if (rtc_info->reason == REASON_EXT_SYS_RST) {
      //external reset, so try to fetch the reset reason from the tiny watchdog and print that
      snprintf(log_line_regs, LOG_LINE_LENGTH,"External Reason: External Watchdog reason: %s\r\n", CSTR(initWatchDog()));
      Debugf(log_line_regs);      
    }

    if	(rtc_info->reason	==	REASON_EXCEPTION_RST)	{

      // Fatal exception No.    Description             Possible Causes
      // -------------------    --------------          -------------------
      //  0                     Invalid command         1. Damaged BIN binaries
      //                                                2. Wild pointers
      //
      //  6                     Division by zero        Division by zero
      //
      //  9                     Unaligned read/write    1. Unaligned read/write Cache addresses
      //                        operation addresses     2. Wild pointers
      //
      //  28/29                 Access to invalid       1. Access to Cache after it is turned off
      //                        address                 2. Wild pointers
      //
      // more reasons can be found in the "corebits.h" file of the Extensa SDK

      switch(rtc_info->exccause) {
        case 0:   snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Invalid command (0)"); break;
        case 6:   snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Division by zero (6)"); break;
        case 9:   snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Unaligned read/write operation addresses (9)"); break;
        case 28:  snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Access to invalid address (28)"); break;
        case 29:  snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Access to invalid address (29)"); break;
        default:  snprintf(log_line_excpt, LOG_LINE_LENGTH, "- Other (not specified) (%d)", rtc_info->exccause); break;
      }

      Debugf("Fatal exception (%d): %s\r\n",	rtc_info->exccause, log_line_excpt);
    }
  }

  snprintf(log_line, LOG_LINE_LENGTH, "%d-%02d-%02d %02d:%02d:%02d - reboot cause: %s (%x) %s\r\n", year(),  month(), day(), hour(), minute(), second(), CSTR(text), errorCode, log_line_excpt);

  if (LittleFS.begin()) {
    //start with opening the file
    File outfh = LittleFS.open(TEMPLOG_FILE, "w");

    if (outfh) {
      //write to _reboot to file
      outfh.print(log_line);

      if (strlen(log_line_regs)>2) {
        outfh.print(log_line_regs);
      }

      File infh = LittleFS.open(REBOOTLOG_FILE, "r");
      
      int i = 1;
      if (infh) {
        //read from file
        while (infh.available() && (i < LOG_LINES)){
          //read the first line 
          String line = infh.readStringUntil('\n');
          if (line.length() > 3) { //TODO: check is no longer needed?
            outfh.print(line);
          }
          i++;
        }
        infh.close();
      }
      outfh.close();
      
      if (LittleFS.exists(REBOOTLOG_FILE)) {
        LittleFS.remove(REBOOTLOG_FILE);
      }
      
      LittleFS.rename(TEMPLOG_FILE, REBOOTLOG_FILE);

      return true; // succesfully logged
    }
  }
  
  return false; // logging unsuccesfull
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
    return (strcasecmp(CSTR(_githash), _VERSION_GITHASH)==0);
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

String strHTTPmethod(HTTPMethod method)
{
  switch (method)
  {
    case HTTPMethod::HTTP_GET:
      return "GET";
    case HTTPMethod::HTTP_POST:
      return "POST";
    case HTTPMethod::HTTP_PUT:
      return "PUT";
    case HTTPMethod::HTTP_PATCH:
      return "PATCH";
    case HTTPMethod::HTTP_DELETE:
      return "DELETE";
    case HTTPMethod::HTTP_OPTIONS:
      return "OPTIONS";
    case HTTPMethod::HTTP_HEAD:
      return "HEAD";
    default:
      return "";
  }
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
