
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
        body { font-family: sans-serif; max-width: 920px; margin: 20px auto; padding: 0 12px; box-sizing: border-box; }
        form { margin: 12px 0; padding: 12px; border: 1px solid #c7d7ea; border-radius: 6px; background: #f8fbff; box-sizing: border-box; }
        html.dark form { border-color: #555; background: #2b2b2b; }
        input[type='file'] { margin: 8px 0 10px; }
        #fwSubmit, #fsSubmit {
          min-width: 170px;
          padding: 10px 14px;
          border: 1px solid #9aa7b5;
          border-radius: 6px;
          font-weight: 700;
          background: #d7dde3;
          color: #6c757d;
          cursor: not-allowed;
          transition: background-color 0.15s ease, border-color 0.15s ease, transform 0.1s ease;
        }
        #fwSubmit:enabled, #fsSubmit:enabled {
          color: #fff;
          cursor: pointer;
          transform: translateY(0);
        }
        #fwSubmit:enabled,
        #fsSubmit:enabled {
          background: #2e9d57;
          border-color: #207240;
        }
        #fwSubmit:enabled:hover,
        #fsSubmit:enabled:hover { background: #27854a; }
        #preserveWrap { margin-top: 10px; padding: 8px 10px; border-left: 3px solid #7aaad6; background: #eef6ff; border-radius: 4px; }
        html.dark #preserveWrap { border-left-color: #4f89c1; background: #3a3a3a; }
        #preserveHelp { margin-top: 4px; font-size: 12px; opacity: 0.85; }
        #pageProgress { display: none; }
        #updatePanel { margin-top: 10px; padding: 15px; background: #e8f4ff; border: 1px solid #7aaad6; color: black; }
        html.dark #updatePanel { background: #333; border: 1px solid #555; color: white; }
        
        /* Progress bar container */
        #progressContainer { position: relative; width: 100%; height: 40px; background-color: #e0e0e0; border: 1px solid #999; border-radius: 4px; margin: 12px 0; overflow: hidden; }
        html.dark #progressContainer { background-color: #444; border-color: #666; }
        
        /* Progress bar fill */
        #progressBar { height: 100%; background-color: #2196F3; transition: width 0.3s ease, background-color 0.5s ease; width: 0%; }
        
        /* Progress text overlay */
        #progressText { position: absolute; top: 0; left: 0; width: 100%; height: 100%; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 14px; color: #fff; text-shadow: 0 0 4px rgba(0,0,0,0.9); }
        html.dark #progressText { color: #fff; text-shadow: 0 0 3px rgba(0,0,0,0.8); }
        
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
            <input id='fsSubmit' type='submit' value='Flash LittleFS' disabled>
            <div id='preserveWrap'>
              <label><input type="checkbox" id="chkPreserve" checked autocomplete="off"> Download backups (settings.ini + dallas_labels.ini if available)</label>
              <div id='preserveHelp'>Recommended before LittleFS flash so you can restore settings and labels quickly.</div>
            </div>
        </form>
        <div id='formError' style='color: #b00020; font-weight: bold; margin-top: 8px;'></div>
     </div>
     <div id='pageProgress'>
       <h1>OTGW firmware Flash utility</h1>
       <div id='updatePanel'>
         <div>File: <strong><span id='updateFilename'>-</span></strong></div>
         <div>Target: <strong><span id='updateTarget'>-</span></strong></div>
         <div id='progressContainer'>
           <div id='progressBar'></div>
           <div id='progressText'>Preparing...</div>
         </div>
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
         var progressBar = document.getElementById('progressBar');
         var progressText = document.getElementById('progressText');
         var errorEl = document.getElementById('updateError');
         var retryBtn = document.getElementById('retryButton');
         var formErrorEl = document.getElementById('formError');

         function restoreDallasLabelsFromOpener(onStatus) {
           var labelsRestored = Promise.resolve();
           try {
             if (window.opener && window.opener.dallasLabelsCache) {
               var labels = window.opener.dallasLabelsCache;
               if (labels && typeof labels === 'object' && Object.keys(labels).length > 0) {
                 if (onStatus) onStatus('Restoring Dallas labels...');
                 labelsRestored = fetch('/api/v2/sensors/labels', {
                   method: 'POST',
                   headers: { 'Content-Type': 'application/json' },
                   body: JSON.stringify(labels)
                 })
                 .then(function(res) {
                   if (res.ok) {
                     console.log('[OTA] Dallas labels restored');
                   } else {
                     console.error('[OTA] Label restore failed: HTTP ' + res.status);
                   }
                 })
                 .catch(function(err) {
                   console.error('[OTA] Label restore error:', err);
                 });
               }
             }
           } catch (e) {
             console.log('[OTA] Label restore skipped:', e);
           }
           return labelsRestored;
         }

         function redirectToHome(delayMs) {
           setTimeout(function() {
             window.location.href = '/';
           }, delayMs || 1000);
         }

         function pollUntilHealthy(options) {
           var remaining = options.timeoutSeconds || 60;
           if (options.onTick) {
             options.onTick(remaining);
           }

           var timer = setInterval(function() {
             remaining--;

             console.log('[OTA] Health check: GET /api/v2/health?t=' + Date.now());
             fetch('/api/v2/health?t=' + Date.now(), {
               method: 'GET',
               cache: 'no-store',
               headers: { 'Accept': 'application/json' }
             })
               .then(function(res) {
                 if (res.ok) return res.json();
                 throw new Error('HTTP ' + res.status);
               })
               .then(function(data) {
                 if (data && data.health && data.health.status === 'UP') {
                   clearInterval(timer);
                   console.log('[OTA] State: Device is healthy');
                   options.onHealthy();
                 }
               })
               .catch(function() {
                 // Ignore - device still rebooting
               });

             if (remaining <= 0) {
               clearInterval(timer);
               if (options.onTimeout) {
                 options.onTimeout();
               }
               return;
             }

             if (options.onTick) {
               options.onTick(remaining);
             }
           }, 1000);

           return timer;
         }
         
         function showProgressPage() {
           console.log('[OTA] State: Showing progress page');
           pageForm.style.display = 'none';
           pageProgress.style.display = 'block';
           retryBtn.style.display = 'none';
           errorEl.textContent = '';
           progressBar.style.backgroundColor = '';
         }
         
         window.retryFlash = function() {
           pageProgress.style.display = 'none';
           pageForm.style.display = 'block';
           retryBtn.style.display = 'none';
           errorEl.textContent = '';
           formErrorEl.textContent = '';
         };
         
         // Download a single file from the device and trigger a browser download
         function downloadBackup(url, prefix) {
           return fetch(url)
             .then(function(resp) {
               if (!resp.ok && resp.status === 404) return null;
               if (!resp.ok) throw new Error('HTTP ' + resp.status);
               return resp.blob();
             })
             .then(function(blob) {
               if (!blob) return; // File doesn't exist
               var now = new Date();
               var stamp = now.toISOString().replace(/[:.]/g, '-');
               var filename = prefix + '-' + stamp + '.ini';
               var objUrl = window.URL.createObjectURL(blob);
               var a = document.createElement('a');
               a.style.display = 'none';
               a.href = objUrl;
               a.download = filename;
               document.body.appendChild(a);
               a.click();
               window.URL.revokeObjectURL(objUrl);
               document.body.removeChild(a);
               console.log('[OTA] State: Backup downloaded as ' + filename);
               return new Promise(function(resolve) { setTimeout(resolve, 500); });
             });
         }

         // Back up settings.ini and dallas_labels.ini to the browser
         function doBackups() {
           var settingsBackup = downloadBackup('/settings.ini', 'settings')
             .catch(function(e) {
               if (!confirm('Could not backup settings.ini. Continue anyway?')) {
                 throw e;
               }
             });
           var labelsBackup = downloadBackup('/dallas_labels.ini', 'dallas_labels')
             .catch(function(e) {
               console.log('[OTA] Dallas labels backup skipped (non-fatal)', e);
             });
           return Promise.all([settingsBackup, labelsBackup])
             .then(function() {
               console.log('[OTA] State: All backups complete');
               return new Promise(function(resolve) { setTimeout(resolve, 500); });
             });
         }

         // Wait for the device to reboot and come back online.
         // onReady: optional callback called when device is healthy.
         //          If null/undefined, performs the default labels-restore + redirect to /.
         function waitForDeviceReboot(onReady) {
           console.log('[OTA] State: Waiting for device reboot');
           progressBar.style.width = '100%';

           pollUntilHealthy({
             timeoutSeconds: 60,
             onTick: function(remaining) {
               progressText.textContent = 'Device rebooting... (' + remaining + 's)';
             },
             onHealthy: function() {
               if (onReady) {
                 onReady();
                 return;
               }

               progressText.textContent = 'Device is back online!';
               restoreDallasLabelsFromOpener(function(status) {
                 console.log('[OTA] ' + status);
                 progressText.textContent = status;
               }).then(function() {
                 progressText.textContent = 'Redirecting...';
                 redirectToHome(1000);
               });
             },
             onTimeout: function() {
               console.log('[OTA] State: Timeout reached, redirecting anyway');
               progressText.textContent = 'Redirecting...';
               window.location.href = '/';
             }
           });
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
             progressBar.style.width = '0%';
             progressText.textContent = 'Preparing upload...';
             errorEl.textContent = '';

             if (!window.FormData || !window.XMLHttpRequest) {
               errorEl.textContent = 'Browser not supported';
               retryBtn.style.display = 'block';
               return;
             }

             var preFlight = Promise.resolve();

             // Backup settings and dallas labels before filesystem flash
             if (formId === 'fsForm') {
               var chk = document.getElementById('chkPreserve');
               if (chk && chk.checked) {
                 console.log('[OTA] State: Downloading backups before filesystem flash');
                 progressText.textContent = 'Downloading backups...';
                 preFlight = doBackups();
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
                   console.log('[OTA] Progress: ' + pct + '% (' + ev.loaded + '/' + ev.total + ')');
                   progressBar.style.width = pct + '%';
                   progressText.textContent = 'Uploading: ' + pct + '% (' + formatBytes(ev.loaded) + ' / ' + formatBytes(ev.total) + ')';
                 }
               };

               xhr.onload = function() {
                 if (xhr.status >= 200 && xhr.status < 300) {
                   var responseText = xhr.responseText || '';
                   if (responseText.indexOf('Flash error') !== -1) {
                     progressText.textContent = 'Flash error';
                     errorEl.textContent = responseText;
                     retryBtn.style.display = 'block';
                   } else {
                     console.log('[OTA] State: Flash complete (backend confirmed), device rebooting');
                     progressBar.style.width = '100%';
                     progressBar.style.backgroundColor = '#4CAF50';
                     progressText.textContent = 'Flash complete! Device rebooting...';
                     waitForDeviceReboot(null);
                   }
                 } else {
                   progressText.textContent = 'Upload failed';
                   errorEl.textContent = 'Upload failed: HTTP ' + xhr.status;
                   retryBtn.style.display = 'block';
                 }
               };

               xhr.ontimeout = function() {
                 console.error('[OTA] Error: Upload timeout after 5 minutes');
                 progressText.textContent = 'Upload timeout';
                 errorEl.textContent = 'Connection timeout - flash may still be in progress. Wait 60 seconds and check device manually.';
                 retryBtn.style.display = 'block';
               };

               xhr.onerror = function() {
                 console.error('[OTA] Error: Upload connection lost');
                 progressText.textContent = 'Upload error';
                 errorEl.textContent = 'Upload connection lost - flash may still be in progress. Wait 60 seconds and check device manually.';
                 retryBtn.style.display = 'block';
               };

               console.log('[OTA] State: Sending FormData via XHR');
               xhr.send(new FormData(form));
             }).catch(function(e) {
               console.error('[OTA] Error: Upload cancelled or pre-flight failed', e);
               progressText.textContent = 'Cancelled';
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
  R"SUCCESS(<html charset="UTF-8">
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
      <br/>
      <br/>Wait for the OTGW firmware to reboot and start the HTTP server
      <br/>
      <br/><span id="status" style="font-weight:bold; color: #666;">Waiting for device... (60s)</span>
      <br/>
      <br/>If nothing happens, refresh with <span style='font-size:1.3em;'><b><a href="/">this link here</a></b></span>.
      </body>
      <script>
         function restoreDallasLabelsFromOpener(onStatus) {
           var labelsRestored = Promise.resolve();
           try {
             if (window.opener && window.opener.dallasLabelsCache) {
               var labels = window.opener.dallasLabelsCache;
               if (labels && typeof labels === 'object' && Object.keys(labels).length > 0) {
                 if (onStatus) onStatus('Restoring Dallas labels...');
                 labelsRestored = fetch('/api/v2/sensors/labels', {
                   method: 'POST',
                   headers: { 'Content-Type': 'application/json' },
                   body: JSON.stringify(labels)
                 })
                 .then(function(res) {
                   if (res.ok) {
                     console.log('[OTA] Dallas labels restored');
                   } else {
                     console.error('[OTA] Label restore failed: HTTP ' + res.status);
                   }
                 })
                 .catch(function(err) {
                   console.error('[OTA] Label restore error:', err);
                 });
               }
             }
           } catch (e) {
             console.log('[OTA] Label restore skipped:', e);
           }
           return labelsRestored;
         }

         function redirectToHome(delayMs) {
           setTimeout(function() {
             window.location.href = '/';
           }, delayMs || 1000);
         }

         function pollUntilHealthy(options) {
           var remaining = options.timeoutSeconds || 60;
           if (options.onTick) {
             options.onTick(remaining);
           }

           var timer = setInterval(function() {
             remaining--;

             fetch('/api/v2/health?t=' + Date.now(), {
               method: 'GET',
               cache: 'no-store',
               headers: { 'Accept': 'application/json' }
             })
               .then(function(res) {
                 if (res.ok) return res.json();
                 throw new Error('HTTP ' + res.status);
               })
               .then(function(data) {
                 if (data && data.health && data.health.status === 'UP') {
                   clearInterval(timer);
                   options.onHealthy();
                 }
               })
               .catch(function() {
                 // Ignore - device still rebooting
               });

             if (remaining <= 0) {
               clearInterval(timer);
               if (options.onTimeout) {
                 options.onTimeout();
               }
               return;
             }

             if (options.onTick) {
               options.onTick(remaining);
             }
           }, 1000);

           return timer;
         }

         var remainingSeconds = 60;
         var statusEl = document.getElementById("status");

         pollUntilHealthy({
           timeoutSeconds: 60,
           onTick: function(remaining) {
             remainingSeconds = remaining;
             statusEl.textContent = "Waiting for device... (" + remainingSeconds + "s)";
             statusEl.style.color = "#666";
           },
           onHealthy: function() {
             statusEl.textContent = "Device is back online!";
             statusEl.style.color = "green";
             restoreDallasLabelsFromOpener(function(status) {
               statusEl.textContent = status;
             }).then(function() {
               statusEl.textContent = "Redirecting...";
               redirectToHome(1000);
             });
           },
           onTimeout: function() {
             statusEl.textContent = "Redirecting...";
             window.location.href = "/";
           }
         });
    </script>
    </html>)SUCCESS";
     

#endif // UPDATESERVERHTML_H
