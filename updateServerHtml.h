
static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <style type='text/css'>
        body {background-color: lightblue; font-family: sans-serif;}
        #pageProgress { display: none; }
        #updatePanel { margin-top: 10px; padding: 10px; background: #e8f4ff; border: 1px solid #7aaad6; max-width: 520px; }
        #updateProgress, #flashProgress { width: 100%; height: 18px; }
        #updateError { color: #b00020; font-weight: bold; }
        .small { font-size: 0.9em; }
     </style>
     <body>
     <div id='pageForm'>
       <h1>OTGW firmware Flash utility</h1>
       <form id='fwForm' method='POST' action='?cmd=0' enctype='multipart/form-data'>
            Select a "<b>.ino.bin</b>" file to flash<br/>
            <input type='file' accept='.ino.bin' name='firmware' required>
            <input id='fwSubmit' type='submit' value='Flash Firmware' disabled>
        </form>
        <form id='fsForm' method='POST' action='?cmd=100' enctype='multipart/form-data'> 
            Select a "<b>.littlefs.bin</b>" file to flash<br/>
            <input type='file' accept='.littlefs.bin' name='filesystem' required>
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
         <div >Flashing: <span id='updateTarget'>-</span></div>
         <div id='uploadState'>Flash Status: idle</div>
         <div id='deviceState'>Action: idle</div>
         <progress id='updateProgress' value='0' max='100'></progress>
         <div id='updateProgressText'>Upload: 0% (0 B / 0 B)</div>
         <progress id='flashProgress' value='0' max='100'></progress>
         <div id='flashProgressText'>Flashing: 0% (0 B / 0 B)</div>
         <div id='updateError'></div>
       </div>
       <div id='successPanel' class='small' style='display: none;'>
         <div id='successMessage'>Update successful. Rebooting device...</div>
         <div>Wait <span id='successCountdown'>60</span> seconds. <a id='successLink' href='/'>Go back to main page</a></div>
       </div>
       <div class='small'>If the device reboots, refresh this page after it comes back.</div>
     </div>
     <script>
       (function() {
         var pageForm = document.getElementById('pageForm');
         var pageProgress = document.getElementById('pageProgress');
         var progressTitle = document.getElementById('progressTitle');
         var uploadStateEl = document.getElementById('uploadState');
         var deviceStateEl = document.getElementById('deviceState');
         var fileEl = document.getElementById('updateFilename');
         var targetEl = document.getElementById('updateTarget');
         var progressEl = document.getElementById('updateProgress');
         var progressTextEl = document.getElementById('updateProgressText');
         var flashProgressEl = document.getElementById('flashProgress');
         var flashProgressTextEl = document.getElementById('flashProgressText');
         var errorEl = document.getElementById('updateError');
         var successPanel = document.getElementById('successPanel');
         var successMessageEl = document.getElementById('successMessage');
         var successCountdownEl = document.getElementById('successCountdown');
         var eventSource = null;
         var pollTimer = null;
         var reconnectTimer = null;
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
           if (title) progressTitle.textContent = title;
         }

         function formatBytes(bytes) {
           if (!bytes || bytes < 0) return '0 B';
           if (bytes < 1024) return bytes + ' B';
           if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
           return (bytes / 1024 / 1024).toFixed(2) + ' MB';
         }

         function setUploadProgress(loaded, total) {
           var pct = 0;
           if (total > 0) {
             pct = Math.round((loaded / total) * 100);
             if (pct > 100) pct = 100;
           }
           if (!progressEl.hasAttribute('value')) {
             progressEl.setAttribute('value', '0');
           }
           progressEl.value = pct;
           progressTextEl.textContent = 'Upload: ' + pct + '% (' + formatBytes(loaded) + ' / ' + (total > 0 ? formatBytes(total) : '?') + ')';
         }

         function setUploadProgressUnknown(loaded) {
           if (progressEl.hasAttribute('value')) {
             progressEl.removeAttribute('value');
           }
           progressTextEl.textContent = 'Upload: ? (' + formatBytes(loaded) + ' / ?)';
         }

         function setFlashProgress(loaded, total) {
           var pct = 0;
           if (total > 0) {
             pct = Math.round((loaded / total) * 100);
             if (pct > 100) pct = 100;
           }
           if (!flashProgressEl.hasAttribute('value')) {
             flashProgressEl.setAttribute('value', '0');
           }
           flashProgressEl.value = pct;
           flashProgressTextEl.textContent = 'Flashing: ' + pct + '% (' + formatBytes(loaded) + ' / ' + (total > 0 ? formatBytes(total) : '?') + ')';
         }

         function setFlashProgressUnknown(loaded) {
           if (flashProgressEl.hasAttribute('value')) {
             flashProgressEl.removeAttribute('value');
           }
           flashProgressTextEl.textContent = 'Flashing: ? (' + formatBytes(loaded) + ' / ?)';
         }

        function startSuccessCountdown() {
          if (!successCountdownEl || successTimer) return;
          var remaining = 60;
          successCountdownEl.textContent = remaining;
          successTimer = setInterval(function() {
             remaining -= 1;
             if (remaining <= 0) {
               clearInterval(successTimer);
               successTimer = null;
               successCountdownEl.textContent = '0';
               if (successMessageEl) {
                 successMessageEl.textContent = 'Update successful. Redirecting...';
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
          if (successMessageEl) successMessageEl.textContent = 'Update successful. Rebooting device...';
        }

        function updateDeviceStatus(status) {
          console.log('Device status:', status);
          if (!status) return;
          var state = status.state || 'idle';
          if (state !== 'idle') {
            showProgressPage('Flashing in progress');
          }
           
          var stateMap = {
             'start': 'starting flash...',
             'write': 'writing to flash...',
             'end': 'flashing complete',
             'error': 'error flashing',
             'abort': 'flashing aborted',
             'idle': 'idle'
           };
           deviceStateEl.textContent = 'Device: ' + (stateMap[state] || state);

           if (status.filename) fileEl.textContent = status.filename;
           if (status.target) targetEl.textContent = status.target;
           if (status.error && status.error.length) {
             errorEl.textContent = status.error;
           } else {
             errorEl.textContent = '';
           }
           var uploadReceived = status.upload_received || status.received || 0;
           var uploadTotal = status.upload_total || status.total || 0;
           var flashWritten = status.flash_written || status.received || 0;
           var flashTotal = status.flash_total || status.total || 0;
           if (!uploadInFlight && uploadReceived && uploadTotal) {
             if (localUploadDone) {
               setUploadProgress(uploadTotal, uploadTotal);
             } else {
               setUploadProgress(uploadReceived, uploadTotal);
             }
           } else if (!uploadInFlight && uploadReceived) {
             setUploadProgressUnknown(uploadReceived);
           }
           if (flashWritten && flashTotal) {
             setFlashProgress(flashWritten, flashTotal);
           } else if (flashWritten) {
             setFlashProgressUnknown(flashWritten);
           }
           if (state === 'end') {
             progressTitle.textContent = 'Update finished';
             successShown = true;
             if (successPanel) successPanel.style.display = 'block';
             startSuccessCountdown();
             stopEvents();
             localUploadDone = false;
           } else if (state === 'error') {
             progressTitle.textContent = 'Update error';
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             stopEvents();
             localUploadDone = false;
           } else if (state === 'abort') {
             progressTitle.textContent = 'Update aborted';
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             stopEvents();
             localUploadDone = false;
           } else {
             progressTitle.textContent = 'Flashing in progress';
             if (successPanel && !successShown) successPanel.style.display = 'none';
           }
         }

         function fetchStatus() {
           fetch('/status', { cache: 'no-store' })
             .then(function(response) { return response.json(); })
             .then(function(json) { updateDeviceStatus(json); })
             .catch(function(e) { console.log('Fetch status error:', e); });
         }

         function startEvents() {
           if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
           if (!!window.WebSocket) {
             if (eventSource) return;
             var protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
             var wsUrl = protocol + window.location.host + '/events';
             eventSource = new WebSocket(wsUrl);
             
             eventSource.onopen = function(e) {
               console.log("WS Connected");
             };
             
             eventSource.onmessage = function(e) {
               // console.log("WS Message", e.data);
               try {
                 var json = JSON.parse(e.data);
                 updateDeviceStatus(json);
               } catch (err) {
                 console.log("WS JSON Error", err);
               }
             };
             
             eventSource.onerror = function(e) {
               console.log("WS Error", e);
             };
             
             eventSource.onclose = function(e) {
               console.log("WS Disconnected");
               eventSource = null;
               // Try to reconnect after 1s
               reconnectTimer = setTimeout(startEvents, 1000);
             };
           } else if (!!window.EventSource) {
             if (eventSource) return;
             eventSource = new EventSource('/events');
             eventSource.addEventListener('open', function(e) {
               console.log("Events Connected");
             }, false);
             eventSource.addEventListener('error', function(e) {
               if (e.target.readyState != EventSource.OPEN) {
                 console.log("Events Disconnected");
               }
             }, false);
             eventSource.addEventListener('status', function(e) {
               var json = JSON.parse(e.data);
               updateDeviceStatus(json);
             }, false);
           } else {
             if (!pollTimer) pollTimer = setInterval(fetchStatus, 1000);
           }
         }

         function stopEvents() {
           if (reconnectTimer) {
             clearTimeout(reconnectTimer);
             reconnectTimer = null;
           }
           if (eventSource) {
             // Prevent onclose/onerror from triggering fallback when we intentionally close
             if (eventSource instanceof WebSocket) {
                eventSource.onclose = null;
                eventSource.onerror = null;
             }
             eventSource.close();
             eventSource = null;
           }
           if (pollTimer) {
             clearInterval(pollTimer);
             pollTimer = null;
           }
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

             if (formErrorEl) formErrorEl.textContent = '';
             resetSuccessPanel();
             showProgressPage('Flashing in progress');
             fileEl.textContent = input.files[0].name || '-';
             targetEl.textContent = targetName;
             uploadStateEl.textContent = 'Upload: starting...';
             errorEl.textContent = '';
             if (!window.FormData || !window.XMLHttpRequest) {
               return;
             }
             e.preventDefault();
             uploadInFlight = true;

             var xhr = new XMLHttpRequest();
             var action = form.action;
             if (action.indexOf('size=') === -1) {
               action += (action.indexOf('?') === -1 ? '?' : '&') + 'size=' + encodeURIComponent(input.files[0].size);
             }
             xhr.open('POST', action, true);
             xhr.setRequestHeader('X-File-Size', input.files[0].size);
             xhr.upload.onprogress = function(ev) {
               console.log('Upload progress:', ev.loaded, ev.total);
               var total = ev.lengthComputable ? ev.total : 0;
               lastUploadLoaded = ev.loaded;
               lastUploadTotal = total;
               setUploadProgress(ev.loaded, total);
               if (total > 0) {
                 var pct = Math.round((ev.loaded / total) * 100);
                 if (pct >= 100) {
                   uploadStateEl.textContent = 'Upload: complete upload...';
                 } else {
                   uploadStateEl.textContent = 'Upload: ' + pct + '%';
                 }
               } else {
                 uploadStateEl.textContent = 'Upload: ' + formatBytes(ev.loaded);
               }
             };
             xhr.onload = function() {
               console.log('Upload finished, status:', xhr.status);
               if (xhr.status >= 200 && xhr.status < 300) {
                 var responseText = xhr.responseText || '';
                 if (responseText.indexOf('Update error') !== -1) {
                   uploadStateEl.textContent = 'Upload: failed';
                   errorEl.textContent = responseText;
                   progressTitle.textContent = 'Update error';
                   successShown = false;
                   if (successPanel) successPanel.style.display = 'none';
                 } else {
                   uploadStateEl.textContent = 'Upload: complete upload...';
                   localUploadDone = true;
                   if (lastUploadTotal > 0) {
                     setFlashProgress(lastUploadTotal, lastUploadTotal);
                   } else if (lastUploadLoaded > 0) {
                     setFlashProgressUnknown(lastUploadLoaded);
                   }
                   progressTitle.textContent = 'Update finished';
                   successShown = true;
                   if (successPanel) successPanel.style.display = 'block';
                   startSuccessCountdown();
                 }
                 uploadInFlight = false;
               } else {
                 uploadStateEl.textContent = 'Upload: failed';
                 errorEl.textContent = 'Upload failed: ' + xhr.status;
                 uploadInFlight = false;
               }
             };
             xhr.onerror = function() {
               uploadStateEl.textContent = 'Upload: error';
               errorEl.textContent = 'Upload error';
               uploadInFlight = false;
             };
             xhr.send(new FormData(form));
           });
         }

         initUploadForm('fwForm', 'flash');
         initUploadForm('fsForm', 'filesystem');
         fetchStatus();
         startEvents();
       })();
     </script>
     </html>)";

static const char UpdateServerSuccess[] PROGMEM = 
  R"(<html charset="UTF-8">
      <style type='text/css'>
        body {background-color: lightgray;}
      </style>
      <body>
      <h1>OTGW firmware Flash utility</h1>
      <br/>
      <h2>Update successful!</h2>
      <br/>
      <br/>Wait for the OTGW firmware to reboot and start the HTTP server
      <br/>
      <br>
      <br/>Wait <span style='font-size: 1.3em;' id="waitSeconds">60</span> seconds ..
      <br>If nothing happend, then wait for count down to zero and then refresh with <span style='font-size:1.3em;'><b><a href="/">this link here</a></b></span>!
      </body>
      <script>
         var seconds = parseInt(document.getElementById("waitSeconds").textContent);
         var countdown = setInterval(function() {
           seconds--;
           document.getElementById('waitSeconds').textContent = seconds;
           if (seconds <= 0) {
              clearInterval(countdown);
              window.location.href = "/";
           }
         }, 1000);
     </script>
     </html>)";
     
