
static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <style type='text/css'>
        body {background-color: lightblue; font-family: sans-serif;}
        #pageProgress { display: none; }
        #updatePanel { margin-top: 10px; padding: 10px; background: #e8f4ff; border: 1px solid #7aaad6; max-width: 520px; }
        #updateProgress { width: 100%; height: 18px; }
        #updateError { color: #b00020; font-weight: bold; }
        .small { font-size: 0.9em; }
     </style>
     <body>
     <div id='pageForm'>
       <h1>OTGW firmware Flash utility</h1>
       <form id='fwForm' method='POST' action='?cmd=0' enctype='multipart/form-data'>
            Select a "<b>.ino.bin</b>" file to flash<br/>
            <input type='file' accept='ino.bin' name='firmware' required>
            <input id='fwSubmit' type='submit' value='Flash Firmware' disabled>
        </form>
        <form id='fsForm' method='POST' action='?cmd=100' enctype='multipart/form-data'> 
            Select a "<b>.littlefs.bin</b>" file to flash<br/>
            <input type='file' accept='littlefs.bin' name='filesystem' required>
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
         <div id='uploadState'>Upload: idle</div>
         <div id='deviceState'>Device: idle</div>
         <div class='small'>File: <span id='updateFilename'>-</span></div>
         <div class='small'>Target: <span id='updateTarget'>-</span></div>
         <progress id='updateProgress' value='0' max='100'></progress>
         <div id='updateProgressText'>Upload: 0% (0 B / 0 B)</div>
         <div id='updateDeviceBytes' class='small'></div>
         <div id='updateError'></div>
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
         var deviceBytesEl = document.getElementById('updateDeviceBytes');
         var errorEl = document.getElementById('updateError');
         var pollTimer = null;
         var uploadInFlight = false;
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

         function updateDeviceStatus(status) {
           if (!status) return;
           var state = status.state || 'idle';
           if (state !== 'idle') {
             showProgressPage('Flashing in progress');
           }
           deviceStateEl.textContent = 'Device: ' + state;
           if (status.filename) fileEl.textContent = status.filename;
           if (status.target) targetEl.textContent = status.target;
           if (status.error && status.error.length) {
             errorEl.textContent = status.error;
           } else {
             errorEl.textContent = '';
           }
           if (!uploadInFlight && status.received && status.total) {
             setUploadProgress(status.received, status.total);
           } else if (!uploadInFlight && status.received) {
             setUploadProgressUnknown(status.received);
           }
           if (status.received && status.total) {
             deviceBytesEl.textContent = 'Device bytes (HTTP): ' + formatBytes(status.received) + ' / ' + formatBytes(status.total);
           } else if (status.received) {
             deviceBytesEl.textContent = 'Device bytes (HTTP): ' + formatBytes(status.received);
           } else {
             deviceBytesEl.textContent = '';
           }
           if (state === 'end') {
             progressTitle.textContent = 'Update finished';
           } else if (state === 'error') {
             progressTitle.textContent = 'Update error';
           } else if (state === 'abort') {
             progressTitle.textContent = 'Update aborted';
           } else {
             progressTitle.textContent = 'Flashing in progress';
           }
         }

         function fetchStatus() {
           fetch('/update/status', { cache: 'no-store' })
             .then(function(response) { return response.json(); })
             .then(function(json) { updateDeviceStatus(json); })
             .catch(function() {});
         }

         function startPolling() {
           if (pollTimer) return;
           pollTimer = setInterval(fetchStatus, 1000);
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
             xhr.open('POST', form.action, true);
             xhr.upload.onprogress = function(ev) {
               var total = ev.lengthComputable ? ev.total : 0;
               setUploadProgress(ev.loaded, total);
               if (total > 0) {
                 uploadStateEl.textContent = 'Upload: ' + Math.round((ev.loaded / total) * 100) + '%';
               } else {
                 uploadStateEl.textContent = 'Upload: ' + formatBytes(ev.loaded);
               }
             };
             xhr.onload = function() {
               if (xhr.status >= 200 && xhr.status < 300) {
                 uploadStateEl.textContent = 'Upload: complete';
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
         startPolling();
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
      <br/>Wait <span style='font-size: 1.3em;' id="waitSeconds">120</span> seconds ..
      <br>If nothing happend, then wait for count down to zero and then refresh with <span style='font-size:1.3em;'><b><a href="/">this link here</a></b></span>!
      </body>
      <script>
         var seconds = document.getElementById("waitSeconds").textContent;
         var countdown = setInterval(function() {
           seconds--;
           document.getElementById('waitSeconds').textContent = seconds;
           if (seconds <= 0) {
              clearInterval(countdown);
              window.location.assign("/")
           }
         }, 1000);
     </script>
     </html>)";
     
