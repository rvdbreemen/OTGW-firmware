
#ifndef UPDATESERVERHTML_H
#define UPDATESERVERHTML_H

static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <head>
     <link rel="stylesheet" type="text/css" href="/index.css" id="theme-style">
     <script>
      (function() {
        try {
          var storedTheme = localStorage.getItem('theme');
          if (storedTheme === 'dark') {
            document.getElementById('theme-style').href = "/index_dark.css";
            document.documentElement.className = 'dark';
          }
        } catch (e) { console.error(e); }
      })();
     </script>
     <style type='text/css'>
        body { font-family: sans-serif; }
        #pageProgress { display: none; }
        #updatePanel { margin-top: 10px; padding: 10px; background: #e8f4ff; border: 1px solid #7aaad6; max-width: 520px; color: black; }
        html.dark #updatePanel { background: #333; border: 1px solid #555; color: white; }
        
        progress { width: 100%; height: 18px; }
        .progress-section { margin-bottom: 8px; }
        
        #updateError { color: #b00020; font-weight: bold; }
        html.dark #updateError { color: #ff5555; }
        .small { font-size: 0.9em; }
        #successLink { color: #0000EE; font-weight: bold; }
        html.dark #successLink { color: #8ab4f8; }
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
        <div id='formError' class='small' style='color: #b00020; font-weight: bold;'></div>
        <hr>
        <br/><font color='red'>Warning!!!</font>
        <br/>You will lose all your files when flashing LittleFS. 
        <br/>Backup your files first to your local filesystem (using FSexplorer) 
        and upload them after the flashing of your LittleFS.
        <hr>
     </div>
     <div id='pageProgress'>
       <h1>OTGW firmware Flash utility</h1>
       <h2 id='progressTitle'>Flashing in progress</h2>
       <div id='updatePanel'>
         <div >File: <span id='updateFilename'>-</span></div>
         <div >Target: <span id='updateTarget'>-</span></div>
         
         <div class="progress-section">
            <div>Uploading: <span id='uploadInfo'>Waiting...</span></div>
            <progress id='uploadProgress' value='0' max='100'></progress>
         </div>
         
         <div class="progress-section">
            <div>Flashing: <span id='flashInfo'>Waiting...</span></div>
            <progress id='flashProgress' value='0' max='100'></progress>
         </div>

         <div id='updateStatus' style='display:none'>Status: idle</div>
         <div id='updateError'></div>
         <button id='retryButton' onclick='retryFlash()' style='display:none; margin-top: 10px; background-color: #d32f2f; color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 4px; font-weight: bold;'>Try Again</button>
         <div id='successPanel' class='small' style='display: none; border-top: 1px solid #ccc; margin-top: 10px; padding-top: 5px;'>
           <div id='successMessage'>Flashing successful. Rebooting device...</div>
           <div>Wait <span id='successCountdown'>60</span> seconds. <a id='successLink' href='/'>Go back to main page</a></div>
         </div>
       </div>
       <div class='small'>If the device reboots, refresh this page after it comes back.</div>
     </div>
     <script>
       (function() {
         // Notify parent window/opener that we're in flash mode
         try {
           if (window.opener && typeof window.opener.enterFlashMode === 'function') {
             window.opener.enterFlashMode();
           }
         
         var uploadProgressEl = document.getElementById('uploadProgress');
         var flashProgressEl = document.getElementById('flashProgress');
         var uploadInfoEl = document.getElementById('uploadInfo');
         var flashInfoEl = document.getElementById('flashInfo');
           sessionStorage.setItem('flashMode', 'true');
         } catch(e) { console.log('Could not signal flash mode:', e); }

         var pageForm = document.getElementById('pageForm');
         var pageProgress = document.getElementById('pageProgress');
         var progressTitle = document.getElementById('progressTitle');
         var statusEl = document.getElementById('updateStatus');
         var fileEl = document.getElementById('updateFilename');
         var targetEl = document.getElementById('updateTarget');
         var progressEl = document.getElementById('updateProgress');
         var errorEl = document.getElementById('updateError');
         var successPanel = document.getElementById('successPanel');
         var successMessageEl = document.getElementById('successMessage');
         var successCountdownEl = document.getElementById('successCountdown');
         var retryBtn = document.getElementById('retryButton');
        var pollTimer = null;
        var pollIntervalMs = 500;
        var pollMinMs = 500;
        var pollMaxMs = 3000;
        var pollInFlight = false;
        var pollActive = false;
         var uploadInFlight = false;
         var localUploadDone = false;
         var successTimer = null;
         var successShown = false;
         var lastUploadLoaded = 0;
         var lastUploadTotal = 0;
         var formErrorEl = document.getElementById('formError');

         function showProgressPage(title) {
           pageForm.style.display = 'none';
           pageProgress.style.display = 'block';
           if (retryBtn) retryBtn.style.display = 'none';
           if (title) progressTitle.textContent = title;
         }

         window.retryFlash = function() {
            pageProgress.style.display = 'none';
            pageForm.style.display = 'block';
            if (retryBtn) retryBtn.style.display = 'none';
            if (errorEl) errorEl.textContent = '';
            if (formErrorEl) formErrorEl.textContent = '';
            uploadInFlight = false;
            localUploadDone = false;
            startPolling(); // Restart polling in case status is waiting
         };

         function formatBytes(bytes) {
           if (!bytes || bytes < 0) return '0 B';
           if (bytes < 1024) return bytes + ' B';
           if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
           return (bytes / 1024 / 1024).toFixed(2) + ' MB';
         }

         function setUploadProgress(loaded, total) {
           var pct = 0;
           if (total > 0) pct = Math.round((loaded / total) * 100);
           if (pct > 100) pct = 100;
           
           if (total > 0) {
              uploadProgressEl.value = pct;
              if (uploadInfoEl) {
                 uploadInfoEl.textContent = pct + '% (' + formatBytes(loaded) + ' / ' + formatBytes(total) + ')';
              }
           } else {
              if (uploadInfoEl) uploadInfoEl.textContent = formatBytes(loaded);
           }
         }

         function setFlashProgress(loaded, total) {
           var pct = 0;
           if (total > 0) pct = Math.round((loaded / total) * 100);
           if (pct > 100) pct = 100;
           
           if (total > 0) {
              flashProgressEl.value = pct;
              if (flashInfoEl) {
                 flashInfoEl.textContent = pct + '% (' + formatBytes(loaded) + ' / ' + formatBytes(total) + ')';
              }
           } else {
              if (flashInfoEl) flashInfoEl.textContent = 'Waiting...';
           }
         }


        function startSuccessCountdown() {
          if (!successCountdownEl || successTimer) return;
          var remaining = 60;
          successCountdownEl.textContent = remaining;
          successTimer = setInterval(function() {
             remaining -= 1;
             
             // Active Polling: Check if device is back online
             fetch('/', { method: 'GET', cache: 'no-store' })
              .then(function(res) {
                if (res.ok) {
                   clearInterval(successTimer);
                   successTimer = null;
                   if (successMessageEl) successMessageEl.textContent = 'Device back online! Redirecting...';
                   window.location.href = "/"; 
                }
              }).catch(function(e){ /* ignore, still rebooting */ });

             if (remaining <= 0) {
               clearInterval(successTimer);
               successTimer = null;
               successCountdownEl.textContent = '0';
               if (successMessageEl) {
                successMessageEl.textContent = 'Flashing successful. Redirecting...';
               }
               window.location.href = "/";
               return;
             }
            successCountdownEl.textContent = remaining;
          }, 1000);
        }

        function resetSuccessPanel() {
          successShown = false;
          if (successTimer) {
            clearInterval(successTimer);
            successTimer = null;
          }
          if (successPanel) successPanel.style.display = 'none';
          if (successCountdownEl) successCountdownEl.textContent = '60';
          if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Rebooting device...';
        }

         function updateDeviceStatus(status) {
          console.log('Device status:', status);
          if (!status) return;
          var state = status.state || 'idle';
          var target = status.target || '';
          if (state !== 'idle') {
            showProgressPage('Flashing in progress');
          }
           
          var stateMap = {
             'start': 'Flashing',
             'write': 'Flashing',
             'end': 'Flashing complete',
             'error': 'Flash error',
             'abort': 'Flash aborted',
             'idle': 'Idle'
           };

           if (status.filename) fileEl.textContent = status.filename;
           if (status.target) targetEl.textContent = status.target;
           if (status.error && status.error.length) {
             errorEl.textContent = status.error;
           } else {
             errorEl.textContent = '';
           }
           var flashWritten = status.flash_written || status.received || 0;
           var flashTotal = status.flash_total || status.total || 0;
           
           // Handle Flash Progress
           if (state !== 'idle' && flashTotal > 0) {
             localUploadDone = (state === 'end'); 
             setFlashProgress(flashWritten, flashTotal);
           }
           
           if (!uploadInFlight && state !== 'idle') {
             if (statusEl) statusEl.textContent = stateMap[state] || 'Flashing';
           } else if (!uploadInFlight && state === 'idle') {
             if (statusEl) statusEl.textContent = 'Status: idle';
           }

           // Handle specific states
           if (state === 'end') {
             progressTitle.textContent = 'Flashing complete';
             if (!successShown) {
               successShown = true;
               if (successPanel) successPanel.style.display = 'block';
               if (target === 'filesystem') {
                 if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Settings restored. Rebooting...';
                 startSuccessCountdown();
               } else {
                 if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Rebooting device...';
                 startSuccessCountdown();
               }
             }
             stopPolling();
             
           } else if (state === 'error') {
             progressTitle.textContent = 'Flashing error';
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'block';
             stopPolling();
             localUploadDone = false;
             
           } else if (state === 'abort') {
             progressTitle.textContent = 'Flashing aborted';
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'block';
             stopPolling();
             localUploadDone = false;
             
           } else {
             // In progress (start, write)
             progressTitle.textContent = 'Flashing in progress';
             if (successPanel && !successShown) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'none';
           }
         }

         function fetchStatus(timeoutMs) {
          var controller = null;
          var timeoutId = null;
          var options = { cache: 'no-store' };
          if (window.AbortController && timeoutMs && timeoutMs > 0) {
            controller = new AbortController();
            options.signal = controller.signal;
            timeoutId = setTimeout(function() { controller.abort(); }, timeoutMs);
          }
          return fetch('/status', options)
            .then(function(response) {
              if (!response.ok) {
                throw new Error('HTTP ' + response.status);
              }
              return response.json();
            })
            .then(function(json) { updateDeviceStatus(json); })
            .catch(function(e) {
              console.log('Fetch status error:', e);
              throw e;
            })
            .finally(function() {
              if (timeoutId) clearTimeout(timeoutId);
            });
        }

        function scheduleNextPoll(delayMs) {
          if (!pollActive) return;
          if (pollTimer) clearTimeout(pollTimer);
          pollTimer = setTimeout(runPoll, delayMs);
        }

        function runPoll() {
          if (!pollActive || pollInFlight) return;
          pollTimer = null;
          pollInFlight = true;
          var timeoutMs = Math.max(500, pollIntervalMs * 4);
          fetchStatus(timeoutMs)
            .then(function() {
              pollIntervalMs = Math.max(pollMinMs, Math.floor(pollIntervalMs / 2));
            })
            .catch(function() {
              pollIntervalMs = Math.min(pollMaxMs, pollIntervalMs * 2);
            })
            .finally(function() {
              pollInFlight = false;
              scheduleNextPoll(pollIntervalMs);
            });
        }

        function startPolling() {
          pollActive = true;
          pollIntervalMs = pollMinMs;
          scheduleNextPoll(0);
        }

        function stopPolling() {
          pollActive = false;
          if (pollTimer) {
            clearTimeout(pollTimer);
            pollTimer = null;
          }
        }

        // WebSocket Support for Real-time Progress
        function setupWebSocket() {
            var wsUrl = 'ws://' + window.location.hostname + ':81/';
            console.log('Connecting to WebSocket:', wsUrl);
            var ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WS Connected');
            };
            
            ws.onmessage = function(e) {
                try {
                    // Try to parse JSON message 
                    // (Log messages are plain text and will throw, which is fine)
                    var msg = JSON.parse(e.data);
                    // Check if it's an update status message
                    if (msg && typeof msg.state !== 'undefined') {
                        // Optimization: Stop HTTP polling if we are getting live updates via WS
                        stopPolling();
                        updateDeviceStatus(msg);
                    }
                } catch(err) {
                    // Ignore non-JSON messages (like raw logs)
                }
            };
            
            ws.onerror = function(e) {
                console.log('WS Error:', e);
            };
            
            ws.onclose = function() {
                // simple reconnect logic
                if (window.location.hash !== '#done') { // Don't reconnect if we are done/navigating away
                   console.log('WS Closed, retrying in 2s');
                   setTimeout(setupWebSocket, 2000);
                }
            };
        }
        setupWebSocket();

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

             if (formErrorEl) formErrorEl.textContent = '';
             
             e.preventDefault();
             resetSuccessPanel();
             showProgressPage('Flashing in progress');
             fileEl.textContent = input.files[0].name || '-';
             targetEl.textContent = targetName;
             
             // Reset UI elements
             uploadProgressEl.value = 0;
             uploadInfoEl.textContent = 'Waiting...';
             flashProgressEl.value = 0;
             flashInfoEl.textContent = 'Waiting...';
             
             errorEl.textContent = '';
             if (!window.FormData || !window.XMLHttpRequest) {
               return;
             }
             // uploadInFlight = true; // Moved inside callback

             var action = form.action;
             var preFlight = Promise.resolve();

             // --- Custom logic: backup settings before filesystem flash ---
             if (formId === 'fsForm') {
                var chk = document.getElementById('chkPreserve');
                if(chk && chk.checked) {
                    // Trigger download as backup (settings will be auto-restored from ESP memory)
                    preFlight = fetch('/settings.ini')
                        .then(function(resp) { return resp.blob(); })
                        .then(function(blob) {
                             // Trigger Download with unique filename
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
                             
                             // Wait briefly for download to start
                             return new Promise(function(resolve) { setTimeout(resolve, 1000); });
                        })
                        .catch(function(e) {
                             console.log('Backup error', e);
                             if(!confirm("Could not backup settings.ini. Continue anyway?")) {
                                 throw e;
                             }
                        });
                }
             }
             // ------------------------------------------

             preFlight.then(function() {
                 if (action.indexOf('size=') === -1) {
                   action += (action.indexOf('?') === -1 ? '?' : '&') + 'size=' + encodeURIComponent(input.files[0].size);
                 }

                 var performUpload = function() {
                     uploadInFlight = true;
                     var xhr = new XMLHttpRequest();
                     xhr.open('POST', action, true);
                     xhr.setRequestHeader('X-File-Size', input.files[0].size);
                     xhr.upload.onprogress = function(ev) {
                       console.log('Upload progress:', ev.loaded, ev.total);
                       var total = ev.lengthComputable ? ev.total : 0;
                       lastUploadLoaded = ev.loaded;
                       lastUploadTotal = total;
                       setUploadProgress(ev.loaded, total);
                     };
                     xhr.onload = function() {
                       console.log('Upload finished, status:', xhr.status);
                       if (xhr.status >= 200 && xhr.status < 300) {
                         var responseText = xhr.responseText || '';
                        if (responseText.indexOf('Flash error') !== -1) {
                           // Status line remains hidden or used for error text
                           errorEl.textContent = responseText;
                           progressTitle.textContent = 'Flashing error';
                           successShown = false;
                           if (successPanel) successPanel.style.display = 'none';
                           if (retryBtn) retryBtn.style.display = 'block';
                         } else {
                           setUploadProgress(lastUploadTotal, lastUploadTotal); // Ensure 100%
                           localUploadDone = true;
                         }
                         uploadInFlight = false;
                       } else {
                         errorEl.textContent = 'Upload failed: ' + xhr.status;
                         uploadInFlight = false;
                       }
                     };
                     xhr.onerror = function() {
                       errorEl.textContent = 'Upload error';
                       uploadInFlight = false;
                       if (retryBtn) retryBtn.style.display = 'block';
                     };
                     xhr.send(new FormData(form));
                 };

                 performUpload();
             }).catch(function(e) {
                 errorEl.textContent = 'Cancelled';
                 if (retryBtn) retryBtn.style.display = 'block';
             });
           });
         }

         initUploadForm('fwForm', 'flash');
         initUploadForm('fsForm', 'filesystem');
        fetchStatus().catch(function() {});
        startPolling();

         // Clean up and notify when leaving flash page
         window.addEventListener('beforeunload', function() {
           try {
             sessionStorage.removeItem('flashMode');
             if (window.opener && typeof window.opener.exitFlashMode === 'function') {
               window.opener.exitFlashMode();
             }
           } catch(e) { console.log('Cleanup error:', e); }
         });
       })();
     </script>
     </html>)";

static const char UpdateServerSuccess[] PROGMEM = 
  R"(<html charset="UTF-8">
      <head>
      <link rel="stylesheet" type="text/css" href="/index.css" id="theme-style">
      <script>
        try {
          var storedTheme = localStorage.getItem('theme');
          if (storedTheme === 'dark') {
            document.getElementById('theme-style').href = "/index_dark.css";
          }
        } catch (e) { console.error(e); }
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
            fetch('/api/v1/health?t=' + Date.now())
                .then(function(r) { return r.json(); })
                .then(function(d) {
                    var s = d.status || (d.health && d.health.status);
                    if (s === 'UP') {
                        statusEl.textContent = "Device is UP! Redirecting...";
                        statusEl.style.color = "green";
                        window.location.href = "/";
                    }
                })
                .catch(function(e) {
                   // console.log("Waiting...", e);
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
