/*
***************************************************************************  
**  Program  : settingsStuff
**  Version  : v0.1.0
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//=======================================================================
void writeSettings(bool show) 
{
  DebugTf("Writing to [%s] ..\r\n", SETTINGS_FILE);
  File file = SPIFFS.open(SETTINGS_FILE, "w"); // open for reading and writing
  if (!file) 
  {
    DebugTf("open(%s, 'w') FAILED!!! --> Bailout\r\n", SETTINGS_FILE);
    return;
  }
  yield();

  DebugT(F("Start writing setting data "));

  file.print("Hostname = ");  file.println(settingHostname);  Debug(F("."));

  file.close();  
  
  Debugln(F(" done"));

  if (show) 
  {
    DebugTln(F("Wrote this:"));
    DebugT(F("        Hostname = ")); Debugln(settingHostname);

  } // show
  
} // writeSettings()


//=======================================================================
void readSettings(bool show) 
{
  String sTmp;
  String words[10];
  char cTmp[CMSG_SIZE], cVal[101], cKey[101];
  
  File file;
  
  DebugTf(" %s ..\r\n", SETTINGS_FILE);

  snprintf(settingHostname,    sizeof(settingHostname), "%s", _HOSTNAME);

  if (!SPIFFS.exists(SETTINGS_FILE)) 
  {
    DebugTln(F(" .. file not found! --> created file!"));
    writeSettings(show);
  }

  for (int T = 0; T < 2; T++) 
  {
    file = SPIFFS.open(SETTINGS_FILE, "r");
    if (!file) 
    {
      if (T == 0) DebugTf(" .. something went wrong opening [%s]\r\n", SETTINGS_FILE);
      else        DebugT(T);
      delay(100);
    }
  } // try T times ..

  DebugTln(F("Reading settings:\r"));
  while(file.available()) 
  {
    sTmp = file.readStringUntil('\n');
    snprintf(cTmp, sizeof(cTmp), "%s", sTmp.c_str());
    strTrimCntr(cTmp, sizeof(cTmp));
    int sEq = strIndex(cTmp, "=");
    strCopy(cKey, 100, cTmp, 0, sEq -1);
    strCopy(cVal, 100, cTmp, sEq +1, strlen(cTmp));
    strTrim(cKey, sizeof(cKey), ' ');
    strTrim(cVal, sizeof(cVal), ' ');
    DebugTf("cKey[%s], cVal[%s]\r\n", cKey, cVal);

    //strToLower(cKey);
    if (stricmp(cKey, "hostname") == 0)         strCopy(settingHostname,         sizeof(settingHostname), cVal);

  } // while available()
  
  file.close();  

  //--- this will take some time to settle in
  //--- probably need a reboot before that to happen :-(
  MDNS.setHostname(settingHostname);    // start advertising with new(?) settingHostname

  DebugTln(F(" .. done\r"));

  if (!show) return;
  
  Debugln(F("\r\n==== read Settings ===================================================\r"));
  Debugf("                 Hostname : %s\r\n",  settingHostname);
  
  Debugln(F("-\r"));

} // readSettings()


//=======================================================================
void updateSetting(const char *field, const char *newValue)
{
  DebugTf("-> field[%s], newValue[%s]\r\n", field, newValue);

  if (!stricmp(field, "Hostname")) {
    strCopy(settingHostname, sizeof(settingHostname), newValue); 
    if (strlen(settingHostname) < 1) strCopy(settingHostname, sizeof(settingHostname), _HOSTNAME); 
    char *dotPntr = strchr(settingHostname, '.') ;
    if (dotPntr != NULL)
    {
      byte dotPos = (dotPntr-settingHostname);
      if (dotPos > 0)  settingHostname[dotPos] = '\0';
    }
    Debugln();
    DebugTf("Need reboot before new %s.local will be available!\r\n\n", settingHostname);
  }

  writeSettings(false);
  
} // updateSetting()


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
