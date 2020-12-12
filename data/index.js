/*
***************************************************************************  
**  Program  : index.js, part of ESP_ticker
**  Version  : v0.3.1
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  const APIGW='http://'+window.location.host+'/api/';

"use strict";

  let needReload  = true;
  
  window.onload=bootsTrapMain;
  window.onfocus = function() {
    if (needReload) {
      window.location.reload(true);
    }
  };
    
  //============================================================================  
  function bootsTrapMain() {
    console.log("bootsTrapMain()");
    document.getElementById('saveMsg').addEventListener('click',function() 
                                                {saveMessages();});
    document.getElementById('M_FSexplorer').addEventListener('click',function() 
                                                { console.log("newTab: goFSexplorer");
                                                  location.href = "/FSexplorer";
                                                });
    document.getElementById('S_FSexplorer').addEventListener('click',function() 
                                                { console.log("newTab: goFSexplorer");
                                                  location.href = "/FSexplorer";
                                                });
    document.getElementById('back').addEventListener('click',function() 
                                                { console.log("newTab: goBack");
                                                  location.href = "/";
                                                });
    document.getElementById('Settings').addEventListener('click',function() 
                                                {settingsPage();});
    document.getElementById('saveSettings').addEventListener('click',function() 
                                                {saveSettings();});
    needReload = false;
    refreshDevTime();
    refreshDevInfo();
    refreshMessages();

    document.getElementById("displayMainPage").style.display       = "block";
    document.getElementById("displaySettingsPage").style.display   = "none";
  
  } // bootsTrapMain()

  function settingsPage()
  {
    document.getElementById("displayMainPage").style.display       = "none";

    var settingsPage = document.getElementById("settingsPage");
    refreshSettings();
    document.getElementById("displaySettingsPage").style.display   = "block";
    
  } // settingsPage()

  
  //============================================================================  
  function refreshDevTime()
  {
    //console.log("Refresh api/v0/devtime ..");
    fetch(APIGW+"v0/devtime")
      .then(response => response.json())
      .then(json => {
        console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        for( let i in json.devtime ){
            if (json.devtime[i].name == "dateTime")
            {
              //console.log("Got new time ["+json.devtime[i].value+"]");
              document.getElementById('theTime').innerHTML = json.devtime[i].value;
            }
          }
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     
      
  } // refreshDevTime()
    
  
  //============================================================================  
  function refreshDevInfo()
  {
    document.getElementById('devName').innerHTML = "";
    fetch(APIGW+"v0/devinfo")
      .then(response => response.json())
      .then(json => {
        //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        data = json.devinfo;
        for( let i in data )
        {
            if (data[i].name == "fwversion")
            {
              document.getElementById('devVersion').innerHTML = json.devinfo[i].value;

            } else if (data[i].name == 'hostname')
            {
              document.getElementById('devName').innerHTML += data[i].value+" ";
              
            } else if (data[i].name == 'ipaddress')
            {
              document.getElementById('devName').innerHTML += " ("+data[i].value+") ";
            }
        }
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     
  } // refreshDevInfo()
    
  //============================================================================  
  function refreshMessages()
  {
    console.log("refreshMessages() ..");
    data = {};
    fetch(APIGW+"v0/messages")
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        msg = json.messages;
        for( let i in msg )
        {
          console.log("["+msg[i].name+"]=>["+msg[i].value+"]");
          var messages = document.getElementById('mainPage');
          if( ( document.getElementById("msgR_"+msg[i].name)) == null )
          {
            var rowDiv = document.createElement("div");
            rowDiv.setAttribute("class", "msgDiv");
            rowDiv.setAttribute("id", "msgR_"+msg[i].name);
            rowDiv.setAttribute("style", "text-align: right;");
            rowDiv.style.marginLeft = "10px";
            rowDiv.style.marginRight = "10px";
            rowDiv.style.width = "850px";
            rowDiv.style.border = "thick solid lightblue";
            rowDiv.style.background = "lightblue";
            //--- field Name ---
              var fldDiv = document.createElement("div");
                  fldDiv.setAttribute("style", "margin-right: 10px;");
                  fldDiv.style.width = "30px";
                  fldDiv.style.float = 'left';
                  fldDiv.textContent = msg[i].name;
                  rowDiv.appendChild(fldDiv);
            //--- input ---
              var inputDiv = document.createElement("div");
                  inputDiv.setAttribute("style", "text-align: left;");

                    var sInput = document.createElement("INPUT");
                    sInput.setAttribute("id", "M_"+msg[i].name);
                    sInput.setAttribute("style", "background: white");

                    if (msg[i].type == "s")
                    {
                      sInput.setAttribute("type", "text");
                      sInput.setAttribute("maxlength", msg[i].maxlen);
                      sInput.setAttribute("size", 100);
                    
                      sInput.setAttribute("value", msg[i].value);
                      //console.log("addEventListener(M_"+msg[i].name+")");
                      sInput.addEventListener('change',
                                function() { setBackGround("M_"+msg[i].name, "lightgray"); },
                                            false
                                );
                    }
                    inputDiv.appendChild(sInput);
                  
            rowDiv.appendChild(inputDiv);
            messages.appendChild(rowDiv);
          }
          else
          {
            document.getElementById("M_"+msg[i].name).style.background = "white";
            document.getElementById("M_"+msg[i].name).value = data[i].value;
          }
        }
        document.getElementById("waiting").innerHTML = "";
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     

  } // refreshMessages()
  
    
  //============================================================================  
  function refreshSettings()
  {
    console.log("refreshSettings() ..");
    data = {};
    fetch(APIGW+"v0/settings")
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        data = json.settings;
        for( let i in data )
        {
          console.log("["+data[i].name+"]=>["+data[i].value+"]");
          var settings = document.getElementById('settingsPage');
          if( ( document.getElementById("D_"+data[i].name)) == null )
          {
            var rowDiv = document.createElement("div");
            rowDiv.setAttribute("class", "settingDiv");
      //----rowDiv.setAttribute("id", "settingR_"+data[i].name);
            rowDiv.setAttribute("id", "D_"+data[i].name);
            rowDiv.setAttribute("style", "text-align: right;");
            rowDiv.style.marginLeft = "10px";
            rowDiv.style.marginRight = "10px";
            rowDiv.style.width = "800px";
            rowDiv.style.border = "thick solid lightblue";
            rowDiv.style.background = "lightblue";
            //--- field Name ---
              var fldDiv = document.createElement("div");
                  fldDiv.setAttribute("style", "margin-right: 10px;");
                  fldDiv.style.width = "270px";
                  fldDiv.style.float = 'left';
                  fldDiv.textContent = translateToHuman(data[i].name);
                  rowDiv.appendChild(fldDiv);
            //--- input ---
              var inputDiv = document.createElement("div");
                  inputDiv.setAttribute("style", "text-align: left;");

                    var sInput = document.createElement("INPUT");
              //----sInput.setAttribute("id", "setFld_"+data[i].name);
                    sInput.setAttribute("id", data[i].name);

                    if (data[i].type == "s")
                    {
                      sInput.setAttribute("type", "text");
                      sInput.setAttribute("maxlength", data[i].maxlen);
                      sInput.setAttribute("size", data[i].maxlen);
                    }
                    else if (data[i].type == "f")
                    {
                      sInput.setAttribute("type", "number");
                      sInput.max = data[i].max;
                      sInput.min = data[i].min;
                      sInput.step = (data[i].min + data[i].max) / 1000;
                    }
                    else if (data[i].type == "i")
                    {
                      sInput.setAttribute("type", "number");
                      sInput.setAttribute("size", 10);
                      sInput.max = data[i].max;
                      sInput.min = data[i].min;
                      //sInput.step = (data[i].min + data[i].max) / 1000;
                      sInput.step = 1;
                    }
                    sInput.setAttribute("value", data[i].value);
                    sInput.addEventListener('change',
                                function() { setBackGround(data[i].name, "lightgray"); },
                                            false
                                );
                  inputDiv.appendChild(sInput);
                  
            rowDiv.appendChild(inputDiv);
            settings.appendChild(rowDiv);
          }
          else
          {
      //----document.getElementById("setFld_"+data[i].name).style.background = "white";
            document.getElementById(data[i].name).style.background = "white";
      //----document.getElementById("setFld_"+data[i].name).value = data[i].value;
            document.getElementById(data[i].name).value = data[i].value;
          }
        }
        //console.log("-->done..");
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     

  } // refreshSettings()
    
  
  //============================================================================  
  function saveMessages() 
  {
    console.log("Saving messages ..");
    let changes = false;
    
    //--- has anything changed?
    var page = document.getElementById("mainPage"); 
    var mRow = page.getElementsByTagName("input");
    //var mRow = document.getElementById("mainPage").getElementsByTagName('div');
    for(var i = 0; i < mRow.length; i++)
    {
      //do something to each div like
      var msgId = mRow[i].getAttribute("id");
      var field = msgId.substr(2);
      //console.log("msgId["+msgId+", msgNr["+field+"]");
      value = document.getElementById(msgId).value;
      //console.log("==> name["+field+"], value["+value+"]");

      changes = false;

      if   (getBackGround("M_"+field) == "lightgray")
      {
        setBackGround("M_"+field, "white");
        changes = true;
      }
      if (changes) {
        console.log("Changes where made in ["+field+"]["+value+"]");
        //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
        sendPostMessages(field, value);
      }
    } 

  } // saveMessages()

  
  //============================================================================  
  function saveSettings() 
  {
    console.log("saveSettings() ...");
    let changes = false;
    
    //--- has anything changed?
    var page = document.getElementById("settingsPage"); 
    var mRow = page.getElementsByTagName("input");
    //var mRow = document.getElementById("mainPage").getElementsByTagName('div');
    for(var i = 0; i < mRow.length; i++)
    {
      //do something to each div like
      var msgId = mRow[i].getAttribute("id");
      var field = msgId;
      //console.log("msgId["+msgId+", msgNr["+field+"]");
      value = document.getElementById(msgId).value;
      //console.log("==> name["+field+"], value["+value+"]");

      changes = false;

      if   (getBackGround(field) == "lightgray")
      {
        setBackGround(field, "white");
        changes = true;
      }
      if (changes) {
        console.log("Changes where made in ["+field+"]["+value+"]");
        //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
        sendPostSetting(field, value);
      }
    } 
    
  } // saveSettings()

  
/****  
  //============================================================================  
  function saveSettings() 
  {
    console.log("saveSettings() ...");
    var settings = document.getElementById("settingsPage").getElementsByTagName('div');;
    for(var i = 0; i < settings.length; i++){
      //do something to each div like
      //console.log(settings[i].innerHTML);
      Dname = settings[i].getAttribute("id");
      if (Dname != null)
      {
        //console.log("Dname["+Dname+"]");
        field = Dname.substr(2);
        value = document.getElementById(field).value;
        console.log("==> name["+field+"], value["+value+"]");
        sendPostSetting(field, value) 
        //console.log("value["+value+"]");
      }
    }    
    
  } // saveSettings()
****/
    
  //============================================================================  
  function sendPostMessages(field, value) 
  {
    const jsonString = {"name" : field, "value" : value};
    console.log("sending: "+JSON.stringify(jsonString));
    const other_params = {
        headers : { "content-type" : "application/json; charset=UTF-8"},
        body : JSON.stringify(jsonString),
        method : "POST",
        mode : "cors"
    };

    fetch(APIGW+"v0/messages", other_params)
      .then(function(response) {
      }, function(error) {
        console.log("Error["+error.message+"]"); //=> String
      });
      
  } // sendPostMessages()

    
  //============================================================================  
  function sendPostSetting(field, value) 
  {
    const jsonString = {"name" : field, "value" : value};
    console.log("sending: "+JSON.stringify(jsonString));
    const other_params = {
        headers : { "content-type" : "application/json; charset=UTF-8"},
        body : JSON.stringify(jsonString),
        method : "POST",
        mode : "cors"
    };

    fetch(APIGW+"v0/settings", other_params)
      .then(function(response) {
            //console.log(response.status );    //=> number 100–599
            //console.log(response.statusText); //=> String
            //console.log(response.headers);    //=> Headers
            //console.log(response.url);        //=> String
            //console.log(response.text());
            //return response.text()
      }, function(error) {
        console.log("Error["+error.message+"]"); //=> String
      });
      
  } // sendPostSetting()

  
  //============================================================================  
  function translateToHuman(longName) {
    //for(var index = 0; index < (translateFields.length -1); index++) 
    for(var index = 0; index < translateFields.length; index++) 
    {
        if (translateFields[index][0] == longName)
        {
          return translateFields[index][1];
        }
    };
    return longName;
    
  } // translateToHuman()

  
   
  //============================================================================  
  function setBackGround(field, newColor) {
    //console.log("setBackground("+field+", "+newColor+")");
    document.getElementById(field).style.background = newColor;
    
  } // setBackGround()

   
  //============================================================================  
  function getBackGround(field) {
    //console.log("getBackground("+field+")");
    return document.getElementById(field).style.background;
    
  } // getBackGround()

  
  //============================================================================  
  function round(value, precision) {
    var multiplier = Math.pow(10, precision || 0);
    return Math.round(value * multiplier) / multiplier;
  }

  
  //============================================================================  
  function printAllVals(obj) {
    for (let k in obj) {
      if (typeof obj[k] === "object") {
        printAllVals(obj[k])
      } else {
        // base case, stop recurring
        console.log(obj[k]);
      }
    }
  } // printAllVals()
  
  

  
/*
***************************************************************************
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
***************************************************************************
*/
