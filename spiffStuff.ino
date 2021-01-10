/*
***************************************************************************  
**  Program  : spiffStuff.ino
**
**  Copyright (c) 2021 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//------------------------------------------------------------------------
bool readFileById(const char* fName, uint8_t mId)
{
  String rTmp;

  DebugTf("read [%s] ", fName);
  
  if (!SPIFFS.exists(fName)) 
  {
    Debugln("Does not exist!");
    return false;
  }

  File f = SPIFFS.open(fName, "r");

  while(f.available()) 
  {
    yield();
//  rTmp = f.readStringUntil('\n');
//  rTmp.replace("\r", "");
  }
  f.close();

  Debugln("Done!");
  return true;
  
} // readFileById()

//------------------------------------------------------------------------
bool writeFileById(const char* fName, uint8_t mId, const char *data)
{
  String rTmp;

  DebugTf("write [%s] ", fName);

  File file = SPIFFS.open(fName, "w");
  if (!file) 
  {
    Debugf("open(%s, 'w') FAILED!!! --> Bailout\r\n", fName);
    return false;
  }
  yield();

  Debugln(F("Start writing data .. \r"));
  file.println(data);
  file.close();

  return true;
  
} // writeFileById()
 

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
