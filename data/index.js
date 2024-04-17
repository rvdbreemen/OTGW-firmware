/*
***************************************************************************  
**  Program  : index.js, part of OTGW-firmware project
**  Version  : v0.10.3
**
**  Copyright (c) 2021-2023 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  const localURL=window.location.protocol+'//'+window.location.host; 
  const APIGW=window.location.protocol+'//'+window.location.host+'/api/';
  
  "use strict";

  let needReload  = true;
  refreshDevTime();

  console.log("Hash="+window.location.hash);
  window.onload=initMainPage;
 

  window.onfocus = function() {
    if (needReload) {
      needReload=false;
      window.location.reload(true);
    }
  };

  
  var tid = 0;
  var timeupdate = setInterval(function(){refreshDevTime(); }, 1000); //delay is in milliseconds
    
  //============================================================================  
  function initMainPage() {
    console.log("initMainPage()");
  
    Array.from(document.getElementsByClassName('FSexplorer')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click',function() {
          console.log("newTab: goFSexplorer");
          location.href = "/FSexplorer";
        })        
      }
    );
    Array.from(document.getElementsByClassName('btnSaveSettings')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click',function(){
          saveSettings();
          toggleHidden('adv_dropdown', true);
          toggleHidden('btnSaveSettings', true);
        });
      }
    );
    Array.from(document.getElementsByClassName('tabDeviceInfo')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click',function(){
          deviceinfoPage();
          toggleHidden('adv_dropdown', true);
          toggleHidden('btnSaveSettings', true);
        });
      }
    );
    Array.from(document.getElementsByClassName('tabPICflash')).forEach(
      function (el, idx, arr) {
        el.addEventListener('click',function() {
          firmwarePage();
          toggleHidden('adv_dropdown', true);
          toggleHidden('btnSaveSettings', true);
        });
      }
    );
    Array.from(document.getElementsByClassName('basicSettings')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click',function() {
          settingsPage();
          toggleHidden('adv_dropdown', true);
          toggleHidden('btnSaveSettings', false);
        });
      }
    );
    Array.from(document.getElementsByClassName('adminSettings')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click', function() {toggleHidden('adv_dropdown', false);});
      }
    );
    Array.from(document.getElementsByClassName('home')).forEach(
      function(el, idx, arr) {
        el.addEventListener('click', function() {
          console.log("newTab: goBack");
          location.href = "/";
        });
      }
    );
 
    needReload = false;
    refreshDevInfo();
    refreshOTmonitor();
    tid = setInterval(function(){refreshOTmonitor(); }, 1000); //delay is in milliseconds 

    document.getElementById("displayMainPage").style.display       = "block";
    document.getElementById("displaySettingsPage").style.display   = "none";
    document.getElementById("displayDeviceInfo").style.display     = "none";
    document.getElementById("displayPICflash").style.display     = "none";
    
    if (window.location.hash == "#tabPICflash"){
      setTimeout(function () {
        firmwarePage();
      }, 150);
    };
  } // initMainPage()

  function firmwarePage()
  {
    clearInterval(tid);
    refreshDevTime();
    document.getElementById("displayMainPage").style.display       = "none";
    document.getElementById("displaySettingsPage").style.display   = "none";
    document.getElementById("displayDeviceInfo").style.display     = "none";
    var firmwarePage = document.getElementById("displayPICflash");
    refreshFirmware();
    document.getElementById("displayPICflash").style.display     = "block";    
  } // deviceinfoPage()

  function deviceinfoPage()
  {
    clearInterval(tid);
    refreshDevTime();
    document.getElementById("displayMainPage").style.display       = "none";
    document.getElementById("displaySettingsPage").style.display   = "none";
    document.getElementById("displayPICflash").style.display     = "none";
    var deviceinfoPage = document.getElementById("deviceinfoPage");
    refreshDeviceInfo();
    document.getElementById("displayDeviceInfo").style.display     = "block";
    
  } // deviceinfoPage()

  function settingsPage()
  {
    clearInterval(tid);
    refreshDevTime();
    document.getElementById("displayMainPage").style.display       = "none";
    document.getElementById("displayDeviceInfo").style.display     = "none";    
    document.getElementById("displayPICflash").style.display     = "none";
    var settingsPage = document.getElementById("settingsPage");
    refreshSettings();
    document.getElementById("displaySettingsPage").style.display   = "block";
    
  } // settingsPage()
  
  function toggleHidden(className, hideOnly) {
    Array.from(document.getElementsByClassName(className)).forEach(
      function(el, idx, arr) {
        if ( ! el.classList.contains("hidden")) {
          el.classList.add("hidden");
        } else if (! hideOnly ) {
          el.classList.remove("hidden");
        }
      }
    );
  }

  //============================================================================  
  function refreshDevTime()
  {
    //console.log("Refresh api/v0/devtime ..");
    fetch(APIGW+"v0/devtime")
      .then(response => response.json())
      .then(json => {
        //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        for( let i in json.devtime ){
            if (json.devtime[i].name == "dateTime")
            {
              //console.log("Got new time ["+json.devtime[i].value+"]");
              document.getElementById('theTime').innerHTML = json.devtime[i].value;
            }
            if (json.devtime[i].name == "message") document.getElementById('message').innerHTML = json.devtime[i].value;
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
  function refreshFirmware(){
    console.log("refreshFirmware() .. "+APIGW+"firmwarefilelist");
    fetch(APIGW+"firmwarefilelist")
      .then(response => response.json())
      .then(files => {
        console.log("parsed ... data is ["+ JSON.stringify(files)+"]");
        
        let displayPICpage = document.getElementById('displayPICpage');
        while (displayPICpage.lastChild) {
          displayPICpage.lastChild.remove();
        }
        let tableDiv = document.createElement("div");
        tableDiv.setAttribute("class", "pictable");
        
        var rowDiv = document.createElement("div");
        rowDiv.setAttribute("class", "picrow");
        rowDiv.setAttribute("id", "firmwarename");
        rowDiv.style.background = "lightblue";
        rowDiv.style.fontWeight = "bold";
        //--- field Name ---
        var fldDiv = document.createElement("div");
        fldDiv.setAttribute("class", "piccolumn1");
        fldDiv.textContent = "Firmware name"
        rowDiv.appendChild(fldDiv);
        //--- version on screen ---
        var valDiv = document.createElement("div");
        valDiv.setAttribute("class", "piccolumn2");                  
        valDiv.textContent = "Version" 
        rowDiv.appendChild(valDiv);
        //--- size on screen ---
        var sizDiv = document.createElement("div");
        sizDiv.setAttribute("class", "piccolumn3");                  
        sizDiv.textContent = "Size" 
        rowDiv.appendChild(sizDiv);
        //--- refresh icon ---
        var btn = document.createElement("div");
        btn.setAttribute("class", "piccolumn4");
        rowDiv.appendChild(btn); 
        //--- flash to pic icon---
        var btn = document.createElement("div");
        rowDiv.appendChild(btn); 
        tableDiv.appendChild(rowDiv);

        for( let i in files )
        {
          console.log("["+files[i].name+"]=>["+files[i].version+"]=>["+files[i].size+"]");

          // var displayPICflash = document.getElementById('displayPICflash');          
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "picrow");
          rowDiv.setAttribute("id", "firmware_"+files[i].name);
          rowDiv.style.background = "lightblue";
          //--- field Name ---
          var fldDiv = document.createElement("div");
          fldDiv.setAttribute("class", "piccolumn1");
          fldDiv.textContent = files[i].name;
          rowDiv.appendChild(fldDiv);
          //--- version on screen ---
          var valDiv = document.createElement("div");
          valDiv.setAttribute("class", "piccolumn2");                  
          valDiv.textContent = files[i].version; 
          rowDiv.appendChild(valDiv);
          //--- size on screen ---
          var sizDiv = document.createElement("div");
          sizDiv.setAttribute("class", "piccolumn3");                  
          sizDiv.textContent = files[i].size; 
          rowDiv.appendChild(sizDiv);
          //--- refresh icon ---
          var btn = document.createElement("div");
          btn.setAttribute("class", "piccolumn4");
          var a = document.createElement('a');
          // a.title = "Update";
          a.href = localURL+'/pic?action=refresh&name='+files[i].name+'&version='+files[i].version;
          var img = document.createElement('img'); 
          img.src = localURL+'/update.png';
          img.title = "Update firmware from web";
          img.style.width = '16px';
          img.style.height = 'auto';
          img.setAttribute=("alt", "Update");
          a.appendChild(img);
          btn.appendChild(a); 
          rowDiv.appendChild(btn); 
          //--- flash to pic icon---
          var btn = document.createElement("div");
          btn.setAttribute("class", "piccolumn5");
          var a = document.createElement('a');
          a.href = localURL+'/pic?action=upgrade&name='+files[i].name+'&version='+files[i].version;
          var img = document.createElement('img'); 
          img.src = localURL+'/system_update.png'
          img.title = "Install firmware onto pic";
          img.style.width = '16px';
          img.style.height = 'auto';
          img.setAttribute=("alt", "Install");
          a.appendChild(img);
          btn.appendChild(a); 
          rowDiv.appendChild(btn); 
          tableDiv.appendChild(rowDiv);
        }
        displayPICpage.appendChild(tableDiv);
 
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });   

   
  }

  
  //============================================================================  
  function refreshDevInfo()
  {
    document.getElementById('devName').innerHTML = "";
    fetch(APIGW+"v0/devinfo")
      .then(response => response.json())
      .then(json => {
        console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
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
  function refreshOTmonitor()
  {
    console.log("refreshOTmonitor() ..");
 
    data = {};
    fetch(APIGW+"v1/otgw/otmonitor")  //api/v1/otgw/otmonitor
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        needReload = false;
        data = json.otmonitor;

        let otMonPage = document.getElementById('mainPage');
        while (otMonPage.lastChild) {
          otMonPage.lastChild.remove();
        }
        let otMonTable = document.createElement("div");
        otMonTable.setAttribute("class", "otmontable");
        
        for( let i in data )
        {
          //console.log("["+data[i].name+"]=>["+data[i].value+"]");
          
          if( ( document.getElementById("otmon_"+data[i].name)) == null )
          { // if element does not exists yet, then build page
            var rowDiv = document.createElement("div");
            rowDiv.setAttribute("class", "otmonrow");
            //rowDiv.setAttribute("id", "otmon_"+data[i].name);
            rowDiv.style.background = "lightblue";
            rowDiv.style.visibility = ((data[i].epoch==0)?"collapse":"visible");
            // rowDiv.style.display = ((data[i].epoch==0)?"none":"table-row");
            var epoch = document.createElement("INPUT");
            epoch.setAttribute("type", "hidden");
            epoch.setAttribute("id", "otmon_epoch_"+data[i].name);
            epoch.name = data[i].name;
            epoch.value = data[i].epoch; 
            rowDiv.appendChild(epoch); 
            //--- field Name ---
            var fldDiv = document.createElement("div");
            fldDiv.setAttribute("class", "otmoncolumn1");
            fldDiv.textContent = translateToHuman(data[i].name);
            rowDiv.appendChild(fldDiv);
            //--- Value ---
            var valDiv = document.createElement("div");
            valDiv.setAttribute("class", "otmoncolumn2");
            valDiv.setAttribute("id", "otmon_"+data[i].name);
            valDiv.textContent = data[i].value; 
            rowDiv.appendChild(valDiv);      
            //--- Unit  ---
            var unitDiv = document.createElement("div");
            unitDiv.setAttribute("class", "otmoncolumn3");
            unitDiv.textContent = data[i].unit; 
            rowDiv.appendChild(unitDiv);
            otMonTable.appendChild(rowDiv);
          }
          else
          { //if the element exists, then update the value
            var update = document.getElementById("otmon_"+data[i].name);
            update.style.visibility = ((data[i].epoch==0)?"collapse":"visible");
            var epoch = document.getElementById("otmon_epoch_"+data[i].name);
            // if ((Number(epoch.value)==0) && (Number(data[i].epoch)>0)) {
            //   //console.log ("unhide based on epoch");
            //   //setTimeout(function () { update.style.visibility = 'visible';}, 0);
            //   needReload = true;
            // } 
            epoch.value = data[i].epoch;
            update.textContent = data[i].value;
            //if (update.style.visibility == 'visible') update.textContent = data[i].value;

          }
        }
        otMonPage.appendChild(otMonTable);
        if (needReload) window.location.reload(true);
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     

  } // refreshOTmonitor()
  
  
  function refreshDeviceInfo()
  {
    console.log("refreshDeviceInfo() ..");
 
    data = {};
    fetch(APIGW+"v0/devinfo")
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        data = json.devinfo;
        for( let i in data )
        {
          console.log("["+data[i].name+"]=>["+data[i].value+"]");
          var deviceinfoPage = document.getElementById('deviceinfoPage');
          if( ( document.getElementById("devinfo_"+data[i].name)) == null )
          { // if element does not exists yet, then build page
            var rowDiv = document.createElement("div");
            rowDiv.setAttribute("class", "devinforow");
            rowDiv.setAttribute("id", "devinfo_"+data[i].name);
            rowDiv.style.background = "lightblue";
            //--- field Name ---
            var fldDiv = document.createElement("div");
            fldDiv.setAttribute("class", "devinfocolumn1");
            fldDiv.textContent = translateToHuman(data[i].name);
            rowDiv.appendChild(fldDiv);
            //--- value on screen ---
            var valDiv = document.createElement("div");
            valDiv.setAttribute("class", "devinfocolumn2");                  
            valDiv.textContent = data[i].value; 
            rowDiv.appendChild(valDiv);
            deviceinfoPage.appendChild(rowDiv);
          }
        }
      })
      .catch(function(error) {
        var p = document.createElement('p');
        p.appendChild(
          document.createTextNode('Error: ' + error.message)
        );
      });     

  } // refreshDeviceInfo()

  //============================================================================  
  function refreshSettings()
  {
    console.log("refreshSettings() ..");
    data = {};
    fetch(APIGW+"v0/settings")
      .then(response => response.json())
      .then(json => {
        console.log("then(json => ..)");
        console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
        data = json.settings;
        document.getElementById("settingMessage").innerHTML = "";
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
            // rowDiv.style.marginLeft = "10px";
            // rowDiv.style.marginRight = "10px";
            rowDiv.style.minWidth = "850px";
            rowDiv.style.border = "thick solid lightblue";
            rowDiv.style.background = "lightblue";
            //--- field Name ---
              var fldDiv = document.createElement("div");
                  fldDiv.setAttribute("style", "margin-right: 10px;");
                  fldDiv.style.width = "320px";
                  fldDiv.style.float = 'left';
                  fldDiv.textContent = translateToHuman(data[i].name);
                  rowDiv.appendChild(fldDiv);
            //--- input ---
              var inputDiv = document.createElement("div");
                  inputDiv.setAttribute("style", "text-align: left;");

                    var sInput = document.createElement("input");
              //----sInput.setAttribute("id", "setFld_"+data[i].name);
                    sInput.setAttribute("id", data[i].name);
                    if (data[i].type == "b")
                    {
                      sInput.setAttribute("type", "checkbox");
                      sInput.checked = strToBool(data[i].value);
                    }
                    else if (data[i].type == "s")
                    {
                      sInput.setAttribute("type", "text");
                      sInput.setAttribute("maxlength", data[i].maxlen);
                      sInput.setAttribute("size", (data[i].maxlen > 20 ? 20 : data[i].maxlen));
                    }                    
                    else if (data[i].type == "p")
                    {
                      sInput.setAttribute("type", "password");
                      sInput.setAttribute("maxlength", data[i].maxlen);
                      sInput.setAttribute("size", (data[i].maxlen > 20 ? 20 : data[i].maxlen));
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
                    sInput.addEventListener('keydown',
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
            // document.getElementById(data[i].name).value = data[i].value;
            // FIX If checkbox change checked iso value
            if (data[i].type == "b")
              document.getElementById(data[i].name).checked = strToBool(data[i].value);
            else document.getElementById(data[i].name).value = data[i].value;

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
  function saveSettings()
  {
    console.log("saveSettings() ...");
    let changes = false;
    
    //--- has anything changed?
    var page = document.getElementById("settingsPage"); 
    var inputs = page.getElementsByTagName("input");
    //var mRow = document.getElementById("mainPage").getElementsByTagName('div');
    for(var i = 0; i < inputs.length; i++)
    {
      //do something to each div like
      var field = inputs[i].getAttribute("id");
      console.log("InputNr["+i+"], InputId["+field+"]");
      if (inputs[i].type == "checkbox") {
        value = document.getElementById(field).checked;
      } else {
        value = document.getElementById(field).value;
      }
      console.log("==> name["+field+"], value["+value+"]");

      if (getBackGround(field).includes("lightgray")) {
        //then it was changes, and needs to be saved
        setBackGround(field, "white");
        console.log("Changes where made in ["+field+"]["+value+"]");
        //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
        document.getElementById("settingMessage").innerHTML = "Saving changes...";
        sendPostSetting(field, value);
      }
    }
  } // saveSettings()

    
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
      .then((response) => {
          //console.log(response.status );    //=> number 100–599
          //console.log(response.statusText); //=> String
          //console.log(response.headers);    //=> Headers
          //console.log(response.url);        //=> String
          //console.log(response.text());
          //return response.text()
          if (response.ok) {
            document.getElementById("settingMessage").innerHTML = "Saving changes... SUCCESS";
            setTimeout(function () { document.getElementById("settingMessage").innerHTML = ""; }, 2000); //and clear the message
          } else {
            document.getElementById("settingMessage").innerHTML = "Saving changes... FAILED";
          }
        }, (error) => {
          console.log("Error[" + error.message + "]"); //=> String
          return false;
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
 //============================================================================  
  function strToBool(s)
  {
      // will match one and only one of the string 'true','1', or 'on' rerardless
      // of capitalization and regardless off surrounding white-space.
      regex=/^\s*(true|1|on)\s*$/i
  
      return regex.test(s);
  } 
  
  
  var translateFields = [

    [ "hostname",                  "HostName" ]
   ,[ "mqttbroker",                "MQTT Broker IP/URL" ]
   ,[ "mqttbrokerport",            "MQTT Broker Port" ]
   ,[ "mqttuser",                  "MQTT User" ]
   ,[ "mqttpasswd",                "Password MQTT User" ]
   ,[ "mqtttoptopic",              "MQTT Top Topic" ]
   ,[ "mqttuniqueid",              "MQTT Uniqueid" ]
   ,[ "influxdbhostname",          "InfluxDB hostname"]
   ,[ "influxdbport",              "InfluxDB port (default: 8086)"]
   ,[ "influxdbdatabasename",      "InfluxDB database name"]
   ,[ "flamestatus",               "Flame status"]
   ,[ "chenable",                  "Central Heating Enabled"]
   ,[ "chmodus",                   "Central Heating Status"]
   ,[ "ch2enable",                 "Central Heating 2 Enabled"]
   ,[ "ch2modus",                  "Central Heating 2 Status"]
   ,[ "dhwenable",                 "Domestic Hot Water Enabled"]
   ,[ "dhwmode",                   "Domestic Hot Water Status"]
   ,[ "diagnosticindicator",       "Diagnostic Indicator"]
   ,[ "faultindicator",            "Fault Indicator"]
   ,[ "outsidetemperature",        "Outside Temperature"]
   ,[ "roomtemperature",           "Room Temperature"]
   ,[ "roomsetpoint",              "Room Temperature Setpoint"]
   ,[ "remoteroomsetpoint",        "Remote Room Temperature Setpoint"]
   ,[ "relmodlvl",                 "Relative Modulation Level"]
   ,[ "maxrelmodlvl",              "Max. Rel. Modulation Level"]
   ,[ "chwaterpressure",           "Central Heating Water Pressure"]
   ,[ "boilertemperature",         "Boiler Temperature"]   
   ,[ "returnwatertemperature",    "Return Water Temperature"]
   ,[ "controlsetpoint",           "Control Setpoint"]
   ,[ "maxchwatersetpoint",        "Max. CH Water Setpoint"]
   ,[ "dhwtemperature",            "Domestic Hotwater Temperature"]
   ,[ "dhwsetpoint",               "Domestic Hotwater Setpoint"]
   ,[ "oemfaultcode",              "OEM Fault Code"]
   ,[ "oemdiagnosticcode",         "OEM Diagnostic Code"]
   ,[ "coolingmodus",              "Cooling Enabled"]
   ,[ "coolingactive",             "Cooling Status"]
   ,[ "otcactive",                 "Outside Temp Compensation"]
   ,[ "servicerequest",            "Service Request"]
   ,[ "lockoutreset",              "Lockout Reset"]
   ,[ "lowwaterpressure",          "Low Water Pressure"]
   ,[ "gasflamefault",             "Gas/Flame Fault"]
   ,[ "airtemp",                   "Air Temperature"]
   ,[ "waterovertemperature",      "Water Over-Temperature"]
   ,[ "author",                     "Developer"]
   ,[ "fwversion",                  "Firmware Version"]
   ,[ "picavailable",               "PIC Available"]
   ,[ "picfwversion",               "PIC Firmware Version"]
   ,[ "picdeviceid",                "PIC Device ID"]   
   ,[ "picfwtype",                  "PIC Firmware Type"]
   ,[ "compiled",                   "Compiled on (date/time)"]
   ,[ "HostName",                   "Hostname (add .local)"]
   ,[ "ipaddress",                  "IP address"]
   ,[ "macaddress",                 "MAC address"]
   ,[ "freeheap",                   "Free Heap Mem (bytes)"]
   ,[ "maxfreeblock",               "Max. Free Mem (bytes)"]
   ,[ "chipid",                     "Unique Chip ID"]
   ,[ "coreversion",                "Arduino Core Version"]
   ,[ "sdkversion",                 "Espressif SDK Version"]
   ,[ "cpufreq",                    "CPU speed (MHz)"]
   ,[ "sketchsize",                 "Sketch Size (bytes)"]
   ,[ "freesketchspace",            "Sketch Free (bytes)"]
   ,[ "flashchipid",                "Flash ID"]   
   ,[ "flashchipsize",              "Flash Chip Size (MB)"]
   ,[ "flashchiprealsize",          "Real Flash Chip (MB)"]
   ,[ "littlefssize",               "LittleFS size (MB)"]
   ,[ "flashchipspeed",             "Flash Chip Speed (MHz)"]
   ,[ "flashchipmode",              "Flash Mode"]
   ,[ "boardtype",                  "Board Type"]
   ,[ "ssid",                       "Wifi Network (SSID)"]
   ,[ "wifirssi",                   "Wifi RX Power (dBm)"]
   ,[ "wifiquality",                "Wifi Quality (%)"]
   ,[ "wifiqualitytldr",            "Wifi Quality"]
   ,[ "lastreset",                  "Last Reset Reason"]
   ,[ "mqttconnected",              "MQTT Connected"]
   ,[ "mqttenable",                 "MQTT Enable"]
   ,[ "mqtthaprefix",               "MQTT Auto Discovery prefix"]
   ,[ "mqttharebootdetection",      "MQTT Home Assistant reboot detection"]
   ,[ "ntpenable",                  "NTP Enable"]
   ,[ "ntptimezone",                "NTP Timezone"]
   ,[ "ntphostname",                "NTP hostname"]
   ,[ "uptime",                     "Uptime Since Boot"]
   ,[ "bootcount",                  "Nr. Reboots"] 
   ,[ "ledblink",                   "Heartbeat LED (on/off)"]
   ,[ "gpiosensorsenabled",         "GPIO Sensors Enabled"]
   ,[ "gpiosensorsinterval",        "GPIO Publish Interval (sec)"]
   ,[ "gpiosensorspin",             "GPIO pin # (SD3 = GPIO10 => 10)"]
   ,[ "numberofsensors",            "Number of temperature sensors"]
   ,[ "s0counterenabled",           "S0 Counter Enabled"]
   ,[ "s0counterinterval",          "S0 Counter Interval (sec)"]
   ,[ "s0counterpin",               "S0 Counter pin # (D6 = GPIO12 => 12)"]
   ,[ "s0counterdebouncetime",      "S0 Counter debouncetime (mS)"]
   ,[ "s0counterpulsekw",           "S0 pulses per kW"]
   ,[ "s0powerkw",                  "S0 actual power (kW)"]
   ,[ "s0intervalcount",            "S0 interval pulses"]
   ,[ "s0totalcount",               "S0 total pulses"]
   ,[ "mqttotmessage",              "MQTT OT msg Enable"]
   ,[ "otgwcommandenable",          "Boot Command Enabled"]
   ,[ "otgwcommands",               "Boot Command"]
   ,[ "thermostatconnected",        "Thermostat Connected"]
   ,[ "boilerconnected",            "Boiler Connected"]
   ,[ "gatewaymode",                "Gateway/Standalone"]
   ,[ "otgwconnected",              "HA Integration"]
   ,[ "gpiooutputsenabled",         "GPIO Output Enabled"]
   ,[ "gpiooutputspin",             "GPIO pin # to switch on/off"]
   ,[ "gpiooutputstriggerbit",      "Bit X (master/slave) to trigger on (0-15)"]
   
                 ];
  
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
