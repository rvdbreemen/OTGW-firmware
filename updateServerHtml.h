
#ifndef UPDATESERVERHTML_H
#define UPDATESERVERHTML_H

static const char UpdateServerIndex[] PROGMEM =
  R"(<!DOCTYPE html>
<html charset="UTF-8">
     <head>
     <script>
      (function() {
        var css = "/index.css";
        var isDark = false;
        try {
          var storedTheme = localStorage.getItem('theme');
          if (storedTheme === 'dark') {
            css = "/index_dark.css";
            isDark = true;
            document.documentElement.className = 'dark';
          }
        } catch (e) { console.error(e); }
        document.write('<link rel="stylesheet" type="text/css" href="' + css + '" id="theme-style">');
      })();
     </script>
     <style type='text/css'>
        body { font-family: sans-serif; max-width: 600px; margin: 20px auto; }
        #pageProgress { display: none; }
        #updatePanel { margin-top: 10px; padding: 15px; background: #e8f4ff; border: 1px solid #7aaad6; color: black; }
        html.dark #updatePanel { background: #333; border: 1px solid #555; color: white; }
        
        progress { width: 100%; height: 24px; margin: 8px 0; }
        .status-text { font-size: 1.1em; font-weight: bold; margin: 12px 0; }
        
        #updateError { color: #b00020; font-weight: bold; margin: 12px 0; }
        html.dark #updateError { color: #ff5555; }
        #retryButton { margin-top: 10px; background-color: #d32f2f; color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 4px; font-weight: bold; }
        #retryButton:hover { background-color: #b71c1c; }
     </style>
     </head>
     <body>
     <div id='pageForm'>
       <h1>OTGW firmware Flash utility</h1>
       <form id='fwForm' method='POST' action='?cmd=0' enctype='multipart/form-data'>
            Select a "<b>.ino.bin</b>" file to flash<br/>
            <input type='file' accept='.ino.bin' name='firmware' required>
            <br/>
            <input id='fwSubmit' type='submit' value='Flash Firmware' disabled>
        </form>
        <form id='fsForm' method='POST' action='?cmd=100' enctype='multipart/form-data'> 
            Select a "<b>.littlefs.bin</b>" file to flash<br/>
            <input type='file' accept='.littlefs.bin' name='filesystem' required>
            <br/>
            <label><input type="checkbox" id="chkPreserve" checked autocomplete="off"> Download settings backup (settings auto-restore from memory)</label>
            <br/>
            <input id='fsSubmit' type='submit' value='Flash LittleFS' disabled>
        </form>
        <div id='formError' style='color: #b00020; font-weight: bold; margin-top: 8px;'></div>
        <hr>
        <br/><font color='red'>Warning!!!</font>
        <br/>You will lose all your files when flashing LittleFS. 
        <br/>Backup your files first to your local filesystem (using FSexplorer) 
        and upload them after the flashing of your LittleFS.
        <hr>
     </div>
     <div id='pageProgress'>
       <h1>OTGW firmware Flash utility</h1>
       <div id='updatePanel'>
         <div>File: <strong><span id='updateFilename'>-</span></strong></div>
         <div>Target: <strong><span id='updateTarget'>-</span></strong></div>
         <div class="status-text" id='statusText'>Preparing...</div>
         <progress id='mainProgress' value='0' max='100'></progress>
         <div id='updateError'></div>
         <button id='retryButton' onclick='retryFlash()' style='display:none;'>Try Again</button>
       </div>
     </div>
     <script>
       (function() {
         // Simple XHR-based OTA update (no WebSocket, no polling during flash)
         // Based on ESP8266 best practices for reliable updates
         
         var pageForm = document.getElementById('pageForm');
         var pageProgress = document.getElementById('pageProgress');
         var fileEl = document.getElementById('updateFilename');
         var targetEl = document.getElementById('updateTarget');
         var statusText = document.getElementById('statusText');
         var progressBar = document.getElementById('mainProgress');
         var errorEl = document.getElementById('updateError');
         var retryBtn = document.getElementById('retryButton');
         var formErrorEl = document.getElementById('formError');
         
         function showProgressPage() {
           console.log('[OTA] State: Showing progress page');
           pageForm.style.display = 'none';
           pageProgress.style.display = 'block';
           retryBtn.style.display = 'none';
           errorEl.textContent = '';
         }
         
         window.retryFlash = function() {
           pageProgress.style.display = 'none';
           pageForm.style.display = 'block';
           retryBtn.style.display = 'none';
           errorEl.textContent = '';
           formErrorEl.textContent = '';
         };
         
         function waitForDeviceReboot() {
           var maxAttempts = 60; // 60 seconds
           var remainingSeconds = maxAttempts;
           var redirectCountdown = 5;
           
           console.log('[OTA] State: Waiting for device reboot');
           statusText.textContent = 'Waiting for device restart... (' + remainingSeconds + 's)';
           progressBar.value = 100;
           
           var checkInterval = setInterval(function() {
             remainingSeconds--;
             
             // Update countdown display
             if (remainingSeconds > 0) {
               statusText.textContent = 'Waiting for device restart... (' + remainingSeconds + 's)';
             }
             
             // Try to reach health endpoint
             var xhr = new XMLHttpRequest();
             xhr.open('GET', '/api/v1/health?t=' + Date.now(), true);
             xhr.timeout = 3000;
             xhr.onload = function() {
               if (xhr.status === 200) {
                 try {
                   var data = JSON.parse(xhr.responseText);
                   if (data.status === 'UP') {
                     console.log('[OTA] State: Device is UP (after ' + (maxAttempts - remainingSeconds) + ' seconds)');
                     clearInterval(checkInterval);
                     
                     // Start 5-second countdown before redirect
                     console.log('[OTA] State: Starting redirect countdown (5 seconds)');
                     var countdownInterval = setInterval(function() {
                       statusText.textContent = 'Device is online! Redirecting in ' + redirectCountdown + ' seconds...';
                       redirectCountdown--;
                       if (redirectCountdown < 0) {
                         console.log('[OTA] State: Redirecting to /');
                         clearInterval(countdownInterval);
                         window.location.href = '/';
                       }
                     }, 1000);
                   }
                 } catch (e) {}
               }
             };
             xhr.onerror = function() {};
             xhr.ontimeout = function() {};
             xhr.send();
             
             // Countdown reached 0 - redirect anyway
             if (remainingSeconds <= 0) {
               console.log('[OTA] State: Countdown reached 0, redirecting to /');
               clearInterval(checkInterval);
               statusText.textContent = 'Redirecting...';
               window.location.href = '/';
             }
           }, 1000);
         }
         
         function formatBytes(bytes) {
           if (!bytes || bytes < 0) return '0 B';
           if (bytes < 1024) return bytes + ' B';
           if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
           return (bytes / 1024 / 1024).toFixed(2) + ' MB';
         }
         
         function initUploadForm(formId, targetName) {
           var form = document.getElementById(formId);
           if (!form) return;
           var input = form.querySelector('input[type="file"]');
           var submit = form.querySelector('input[type="submit"]');
           if (input && submit) {
             submit.disabled = !input.files || input.files.length === 0;
             input.addEventListener('change', function() {
               submit.disabled = !input.files || input.files.length === 0;
               if (formErrorEl) formErrorEl.textContent = '';
             });
           }
           
           form.addEventListener('submit', function(e) {
             if (!input || !input.files || input.files.length === 0) {
               if (formErrorEl) formErrorEl.textContent = 'Select a file first.';
               e.preventDefault();
               return;
             }
             
             e.preventDefault();
             console.log('[OTA] State: Form submitted for ' + targetName);
             console.log('[OTA] File: ' + input.files[0].name + ' (' + formatBytes(input.files[0].size) + ')');
             formErrorEl.textContent = '';
             showProgressPage();
             fileEl.textContent = input.files[0].name || '-';
             targetEl.textContent = targetName;
             progressBar.value = 0;
             statusText.textContent = 'Preparing upload...';
             errorEl.textContent = '';
             
             if (!window.FormData || !window.XMLHttpRequest) {
               errorEl.textContent = 'Browser not supported';
               retryBtn.style.display = 'block';
               return;
             }
             
             var preFlight = Promise.resolve();
             
             // Backup settings before filesystem flash
             if (formId === 'fsForm') {
               var chk = document.getElementById('chkPreserve');
               if (chk && chk.checked) {
                 console.log('[OTA] State: Downloading settings backup before filesystem flash');
                 statusText.textContent = 'Downloading settings backup...';
                 preFlight = fetch('/settings.ini')
                   .then(function(resp) { return resp.blob(); })
                   .then(function(blob) {
                     var now = new Date();
                     var stamp = now.toISOString().replace(/[:.]/g, '-');
                     var filename = 'settings-' + stamp + '.ini';
                     var url = window.URL.createObjectURL(blob);
                     var a = document.createElement('a');
                     a.style.display = 'none';
                     a.href = url;
                     a.download = filename;
                     document.body.appendChild(a);
                     a.click();
                     window.URL.revokeObjectURL(url);
                     document.body.removeChild(a);
                     console.log('[OTA] State: Settings backup downloaded as ' + filename);
                     return new Promise(function(resolve) { setTimeout(resolve, 1000); });
                   })
                   .catch(function(e) {
                     if (!confirm('Could not backup settings.ini. Continue anyway?')) {
                       throw e;
                     }
                   });
               }
             }
             
             preFlight.then(function() {
               var action = form.action;
               if (action.indexOf('size=') === -1) {
                 action += (action.indexOf('?') === -1 ? '?' : '&') + 'size=' + encodeURIComponent(input.files[0].size);
               }
               
               console.log('[OTA] State: Starting XHR upload to ' + action);
               var xhr = new XMLHttpRequest();
               xhr.open('POST', action, true);
               xhr.timeout = 300000; // 5 minutes for flash operations
               
               xhr.upload.onprogress = function(ev) {
                 if (ev.lengthComputable) {
                   var pct = Math.round((ev.loaded / ev.total) * 100);
                   if (pct > 100) pct = 100;
                   progressBar.value = pct;
                   statusText.textContent = 'Uploading: ' + pct + '% (' + formatBytes(ev.loaded) + ' / ' + formatBytes(ev.total) + ')';
                 }
               };
               
               xhr.onload = function() {
                 if (xhr.status >= 200 && xhr.status < 300) {
                   var responseText = xhr.responseText || '';
                   if (responseText.indexOf('Flash error') !== -1) {
                     statusText.textContent = 'Flash error';
                     errorEl.textContent = responseText;
                     retryBtn.style.display = 'block';
                   } else {
                     // Upload complete at 100%
                     console.log('[OTA] State: Upload complete at 100%, backend is flashing');
                     progressBar.value = 100;
                     statusText.textContent = 'Flashing...';
                     
                     // Wait a moment for flash to complete
                     setTimeout(function() {
                       console.log('[OTA] State: Flash complete, device rebooting');
                       statusText.textContent = 'Done! Device rebooting...';
                       waitForDeviceReboot();
                     }, 2000);
                   }
                 } else {
                   statusText.textContent = 'Upload failed';
                   errorEl.textContent = 'Upload failed: HTTP ' + xhr.status;
                   retryBtn.style.display = 'block';
                 }
               };
               
               xhr.ontimeout = function() {
                 console.error('[OTA] Error: Upload timeout after 5 minutes');
                 statusText.textContent = 'Upload timeout';
                 errorEl.textContent = 'Connection timeout - flash may still be in progress. Wait 60 seconds and check device manually.';
                 retryBtn.style.display = 'block';
               };
               
               xhr.onerror = function() {
                 console.error('[OTA] Error: Upload connection lost');
                 statusText.textContent = 'Upload error';
                 errorEl.textContent = 'Upload connection lost - flash may still be in progress. Wait 60 seconds and check device manually.';
                 retryBtn.style.display = 'block';
               };
               
               console.log('[OTA] State: Sending FormData via XHR');
               xhr.send(new FormData(form));
             }).catch(function(e) {
               console.error('[OTA] Error: Upload cancelled or pre-flight failed', e);
               statusText.textContent = 'Cancelled';
               errorEl.textContent = 'Cancelled';
               retryBtn.style.display = 'block';
             });
           });
         }
         
         initUploadForm('fwForm', 'flash');
         initUploadForm('fsForm', 'filesystem');
       })();
     </script>
     </html>)";

static const char UpdateServerSuccess[] PROGMEM = 
  R"(<html charset="UTF-8">
      <head>
      <script>
        (function() {
          var css = "/index.css";
          try {
            var storedTheme = localStorage.getItem('theme');
            if (storedTheme === 'dark') {
              css = "/index_dark.css";
              document.documentElement.className = 'dark';
            }
          } catch (e) { console.error(e); }
          document.write('<link rel="stylesheet" type="text/css" href="' + css + '" id="theme-style">');
        })();
      </script>
      <style type='text/css'>
        body { font-family: sans-serif; }
      </style>
      </head>
      <body>
      <h1>OTGW firmware Flash utility</h1>
      <br/>
      <h2>Flashing successful!</h2>
      %SETTINGS_MSG%
      <br/>
      <br/>Wait for the OTGW firmware to reboot and start the HTTP server
      <br/>
      <br/><span id="status" style="font-weight:bold; color: #666;">Waiting for device...</span>
      <br/>
      <br/>Wait <span style='font-size: 1.3em;' id="waitSeconds">60</span> seconds ..
      <br>If nothing happend, then wait for count down to zero and then refresh with <span style='font-size:1.3em;'><b><a href="/">this link here</a></b></span>!
      </body>
      <script>
         var seconds = 60;
         var statusEl = document.getElementById("status");
         
         function checkHealth() {
            fetch('/api/v1/health?t=' + Date.now(), { 
              method: 'GET', 
              cache: 'no-store',
              timeout: 5000
            })
            .then(function(res) {
              if (!res.ok) throw new Error('HTTP ' + res.status);
              return res.json();
            })
            .then(function(d) {
              var s = d.status || (d.health && d.health.status);
              if (s === 'UP') {
                statusEl.textContent = "Device is UP! Redirecting...";
                statusEl.style.color = "green";
                window.location.href = "/";
              }
            })
            .catch(function(e) {
              // Still waiting
            });
         }

         var countdown = setInterval(function() {
           seconds--;
           document.getElementById('waitSeconds').textContent = seconds;
           checkHealth();
           if (seconds <= 0) {
              clearInterval(countdown);
              window.location.href = "/";
           }
         }, 1000);
     </script>
     </html>)";
     

#endif // UPDATESERVERHTML_H
