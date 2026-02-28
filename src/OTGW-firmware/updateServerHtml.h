
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
        form { margin: 12px 0; padding: 12px; border: 1px solid #c7d7ea; border-radius: 6px; background: #f8fbff; }
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
        #ghSection { margin-top: 16px; padding: 12px; border: 1px solid #c7d7ea; border-radius: 6px; background: #f8fbff; }
        html.dark #ghSection { border-color: #555; background: #2b2b2b; }
        #ghSection h2 { margin: 0 0 8px; font-size: 1.1em; }
        #ghCurrentVersion { margin: 4px 0 10px; font-size: 13px; color: #555; }
        html.dark #ghCurrentVersion { color: #aaa; }
        #ghCheckBtn { padding: 8px 16px; background: #5a67d8; color: #fff; border: none; border-radius: 4px; cursor: pointer; font-weight: 700; }
        #ghCheckBtn:hover:not(:disabled) { background: #4c51bf; }
        #ghCheckBtn:disabled { background: #9aa7b5; cursor: not-allowed; }
        #ghStatus { margin: 8px 0; font-size: 13px; color: #555; min-height: 18px; }
        html.dark #ghStatus { color: #aaa; }
        .gh-table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 13px; }
        .gh-table th { text-align: left; padding: 5px 8px; background: #e8f0fe; border-bottom: 2px solid #c7d7ea; }
        html.dark .gh-table th { background: #3a3a3a; border-bottom-color: #555; color: #ddd; }
        .gh-table td { padding: 5px 8px; border-bottom: 1px solid #e0e8f0; vertical-align: middle; }
        html.dark .gh-table td { border-bottom-color: #444; color: #ddd; }
        .gh-current-row td { background: #f0fff4; }
        html.dark .gh-current-row td { background: #1a3a2a; }
        .gh-badge { display: inline-block; padding: 1px 5px; border-radius: 3px; font-size: 11px; font-weight: 700; margin-left: 4px; vertical-align: middle; }
        .gh-badge.installed { background: #38a169; color: #fff; }
        .gh-badge.update { background: #3182ce; color: #fff; }
        .gh-btn { padding: 4px 10px; color: #fff; border: none; border-radius: 3px; cursor: pointer; font-size: 12px; font-weight: 700; }
        .gh-btn.fw { background: #0a74da; }
        .gh-btn.fw:hover { background: #0864ba; }
        .gh-btn.fs { background: #2e9d57; }
        .gh-btn.fs:hover { background: #27854a; }
        .gh-btn.rb { background: #dd6b20; }
        .gh-btn.rb:hover { background: #c05621; }
        .gh-btn.both { background: #6b46c1; }
        .gh-btn.both:hover { background: #553c9a; }
        .gh-na { color: #aaa; font-size: 12px; }
     </style>
     </head>
     <body>
     <div id='pageForm'>
       <h1>OTGW firmware Flash utility</h1>
         <div id='ghSection'>
           <h2>Update from GitHub</h2>
           <div id='ghCurrentVersion'>Installed version: detecting...</div>
           <button id='ghCheckBtn' onclick='loadReleases()'>Refresh</button>
           <div id='ghStatus'></div>
           <div id='ghList' style='display:none'></div>
         </div>
        <hr>
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

         // Upload a blob to the device via XHR; returns a Promise
         function xhrUpload(action, fieldName, blob, filename) {
           return new Promise(function(resolve, reject) {
             var fd = new FormData();
             fd.append(fieldName, blob, filename);
             var xhr = new XMLHttpRequest();
             xhr.open('POST', action, true);
             xhr.timeout = 300000;
             xhr.upload.onprogress = function(ev) {
               if (ev.lengthComputable) {
                 var pct = Math.min(Math.round((ev.loaded / ev.total) * 100), 100);
                 progressBar.style.width = pct + '%';
                 progressText.textContent = 'Uploading: ' + pct + '% (' + formatBytes(ev.loaded) + ' / ' + formatBytes(ev.total) + ')';
               }
             };
             xhr.onload = function() {
               if (xhr.status >= 200 && xhr.status < 300 && (xhr.responseText || '').indexOf('Flash error') === -1) {
                 progressBar.style.width = '100%';
                 resolve();
               } else {
                 reject(new Error(xhr.responseText || 'HTTP ' + xhr.status));
               }
             };
             xhr.ontimeout = function() { reject(new Error('Upload timeout')); };
             xhr.onerror = function() { reject(new Error('Connection lost during upload')); };
             xhr.send(fd);
           });
         }

         // Wait for the device to reboot and come back online.
         // onReady: optional callback called when device is healthy.
         //          If null/undefined, performs the default labels-restore + redirect to /.
         function waitForDeviceReboot(onReady) {
           console.log('[OTA] State: Waiting for device reboot');
           var remaining = 60;
           progressText.textContent = 'Device rebooting... (' + remaining + 's)';
           progressBar.style.width = '100%';

           var checkInterval = setInterval(function() {
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
                   clearInterval(checkInterval);
                   console.log('[OTA] State: Device is healthy');
                   if (onReady) {
                     // Intermediate reboot: hand control back to caller
                     onReady();
                   } else {
                     // Final reboot: restore labels, then redirect to /
                     progressText.textContent = 'Device is back online!';
                     var labelsRestored = Promise.resolve();
                     try {
                       if (window.opener && window.opener.dallasLabelsCache) {
                         var labels = window.opener.dallasLabelsCache;
                         if (labels && typeof labels === 'object' && Object.keys(labels).length > 0) {
                           console.log('[OTA] Restoring Dallas labels from memory cache');
                           progressText.textContent = 'Restoring Dallas labels...';
                           labelsRestored = fetch('/api/v2/sensors/labels', {
                             method: 'POST',
                             headers: { 'Content-Type': 'application/json' },
                             body: JSON.stringify(labels)
                           })
                           .then(function(res) {
                             if (res.ok) {
                               console.log('[OTA] Dallas labels restored successfully');
                             } else {
                               console.error('[OTA] Label restore failed: HTTP ' + res.status);
                             }
                           })
                           .catch(function(err) {
                             console.error('[OTA] Label restore error:', err);
                           });
                         }
                       }
                     } catch(e) { console.log('[OTA] Label restore skipped:', e); }
                     labelsRestored.then(function() {
                       progressText.textContent = 'Redirecting...';
                       setTimeout(function() { window.location.href = '/'; }, 1000);
                     });
                   }
                 }
               })
               .catch(function(e) {
                 // Ignore - device still rebooting
               });

             if (remaining <= 0) {
               clearInterval(checkInterval);
               console.log('[OTA] State: Timeout reached, redirecting anyway');
               progressText.textContent = 'Redirecting...';
               window.location.href = '/';
               return;
             }

             progressText.textContent = 'Device rebooting... (' + remaining + 's)';
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

         // === GitHub Releases: one-click OTA from GitHub ===
         var ghReleases = null;
         var currentVersion = '';

         // Auto-detect installed firmware version
         fetch('/api/v2/device/info', { cache: 'no-store' })
           .then(function(r) { return r.ok ? r.json() : null; })
           .then(function(data) {
             if (data && data.device && data.device.fwversion) {
               currentVersion = data.device.fwversion;
               var el = document.getElementById('ghCurrentVersion');
               if (el) el.textContent = 'Installed: ' + currentVersion;
             }
           })
           .catch(function() {});

         function parseVersion(v) {
           if (!v) {
             return { nums: [0], pre: [] };
           }
           // Strip build metadata (+...) per SemVer
           var plusIdx = v.indexOf('+');
           if (plusIdx >= 0) {
             v = v.substring(0, plusIdx);
           }
           // Split main version and prerelease (-...)
           var main = v;
           var pre = [];
           var dashIdx = v.indexOf('-');
           if (dashIdx >= 0) {
             main = v.substring(0, dashIdx);
             var preStr = v.substring(dashIdx + 1);
             if (preStr) {
               pre = preStr.split('.');
             }
           }
           var parts = main.split('.');
           var nums = [];
           for (var i = 0; i < parts.length; i++) {
             var n = parseInt(parts[i], 10);
             nums.push(isNaN(n) ? 0 : n);
           }
           return { nums: nums, pre: pre };
         }

         function versionLt(a, b) {
           var va = parseVersion(a);
           var vb = parseVersion(b);

           // Compare main numeric parts
           var len = Math.max(va.nums.length, vb.nums.length);
           for (var i = 0; i < len; i++) {
             var na = (i < va.nums.length) ? va.nums[i] : 0;
             var nb = (i < vb.nums.length) ? vb.nums[i] : 0;
             if (na < nb) return true;
             if (na > nb) return false;
           }

           // Main versions equal: handle prerelease according to SemVer
           var aHasPre = va.pre.length > 0;
           var bHasPre = vb.pre.length > 0;

           if (!aHasPre && bHasPre) {
             // Normal version is greater than prerelease
             return false;
           }
           if (aHasPre && !bHasPre) {
             // Prerelease is less than normal version
             return true;
           }
           if (!aHasPre && !bHasPre) {
             // Completely equal
             return false;
           }

           // Both have prerelease: compare identifiers
           var preLen = Math.max(va.pre.length, vb.pre.length);
           for (var j = 0; j < preLen; j++) {
             var ida = va.pre[j];
             var idb = vb.pre[j];

             if (typeof ida === 'undefined' && typeof idb === 'undefined') {
               break;
             }
             if (typeof ida === 'undefined') {
               // a has fewer identifiers => lower precedence
               return true;
             }
             if (typeof idb === 'undefined') {
               return false;
             }

             var reNum = /^[0-9]+$/;
             var aIsNum = reNum.test(ida);
             var bIsNum = reNum.test(idb);

             if (aIsNum && bIsNum) {
               var ia = parseInt(ida, 10);
               var ib = parseInt(idb, 10);
               if (ia < ib) return true;
               if (ia > ib) return false;
             } else if (aIsNum && !bIsNum) {
               // Numeric identifiers have lower precedence than non-numeric
               return true;
             } else if (!aIsNum && bIsNum) {
               return false;
             } else {
               if (ida < idb) return true;
               if (ida > idb) return false;
             }
           }
           return false;
         }

         function escHtml(s) {
           return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
         }

         window.loadReleases = function() {
           var btn = document.getElementById('ghCheckBtn');
           var status = document.getElementById('ghStatus');
           var list = document.getElementById('ghList');
           if (btn) btn.disabled = true;
           if (status) status.textContent = 'Fetching releases from GitHub...';
           if (list) list.style.display = 'none';
           fetch('https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=10', {
             headers: { 'Accept': 'application/vnd.github.v3+json' }
           })
             .then(function(r) {
               if (!r.ok) throw new Error('GitHub API HTTP ' + r.status);
               return r.json();
             })
             .then(function(releases) {
               // Exclude draft releases — drafts may have incomplete assets
               releases = releases.filter(function(r) { return !r.draft; });
               ghReleases = releases;
               if (btn) btn.disabled = false;
               if (status) status.textContent = '';
               renderReleases(releases);
             })
             .catch(function(e) {
               if (btn) btn.disabled = false;
               if (status) status.textContent = 'Failed: ' + (e.message || String(e));
             });
         };

         function renderReleases(releases) {
           var list = document.getElementById('ghList');
           if (!list) return;
           var coreMatch = currentVersion.match(/^(\d+\.\d+\.\d+)/);
           var installedCore = coreMatch ? coreMatch[1] : '';
           if (!releases || releases.length === 0) {
             list.innerHTML = '<p>No releases found.</p>';
             list.style.display = 'block';
             return;
           }
           var html = '<table class="gh-table"><thead><tr><th>Release</th><th>Date</th><th>Firmware</th><th>Filesystem</th><th>Update</th></tr></thead><tbody>';
           for (var i = 0; i < releases.length; i++) {
             var rel = releases[i];
             var tag = (rel.tag_name || '').replace(/^v/, '');
             var tagCoreMatch = tag.match(/^(\d+\.\d+\.\d+)/);
             var tagCore = tagCoreMatch ? tagCoreMatch[1] : tag;
             var isInstalled = !!(tagCore && installedCore && installedCore === tagCore);
             var isNewer = !!(tagCore && installedCore && !isInstalled && versionLt(installedCore, tagCore));
             var isRollback = !!(tagCore && installedCore && !isInstalled && versionLt(tagCore, installedCore));
             var fwAsset = null, fsAsset = null;
             for (var j = 0; j < (rel.assets || []).length; j++) {
               var a = rel.assets[j];
               if (!fwAsset && a.name && a.name.indexOf('.ino.bin') >= 0) fwAsset = a;
               if (!fsAsset && a.name && a.name.indexOf('.littlefs.bin') >= 0) fsAsset = a;
             }
             if (!fwAsset && !fsAsset) continue;
             var nameHtml = escHtml(rel.name || rel.tag_name || '');
             if (isInstalled) nameHtml += ' <span class="gh-badge installed">Installed</span>';
             else if (isNewer) nameHtml += ' <span class="gh-badge update">Update</span>';
             var date = (rel.published_at || '').substring(0, 10);
             var fwBtnCls = 'gh-btn fw' + (isRollback ? ' rb' : '');
             var fwBtnLbl = isRollback ? 'Rollback FW' : 'Flash FW';
             var fwBtn = fwAsset
               ? '<button class="' + fwBtnCls + '" onclick="ghFlash(' + i + ',true);">' + fwBtnLbl + '</button>'
               : '<span class="gh-na">N/A</span>';
             var fsBtn = fsAsset
               ? '<button class="gh-btn fs" onclick="ghFlash(' + i + ',false);">Flash FS</button>'
               : '<span class="gh-na">N/A</span>';
             var bothBtn = (fwAsset && fsAsset)
               ? '<button class="gh-btn both" onclick="ghFlashBoth(' + i + ');">Flash Both</button>'
               : '<span class="gh-na">—</span>';
             html += '<tr' + (isInstalled ? ' class="gh-current-row"' : '') + '><td>' + nameHtml + '</td><td>' + escHtml(date) + '</td><td>' + fwBtn + '</td><td>' + fsBtn + '</td><td>' + bothBtn + '</td></tr>';
           }
           html += '</tbody></table>';
           list.innerHTML = html;
           list.style.display = 'block';
         }

         loadReleases();

         window.ghFlash = function(idx, isFirmware) {
           if (!ghReleases || idx >= ghReleases.length) return;
           var rel = ghReleases[idx];
           var asset = null;
           for (var j = 0; j < (rel.assets || []).length; j++) {
             var a = rel.assets[j];
             if (isFirmware && a.name && a.name.indexOf('.ino.bin') >= 0) { asset = a; break; }
             if (!isFirmware && a.name && a.name.indexOf('.littlefs.bin') >= 0) { asset = a; break; }
           }
           if (!asset) { alert('Asset not found in this release.'); return; }
           if (!isFirmware) {
             if (!confirm('WARNING: Flashing filesystem (LittleFS) will erase all files including settings!\nBackup first using the checkboxes above. Continue?')) return;
           }
           var downloadUrl = asset.browser_download_url;
           var filename = asset.name;
           var targetLabel = (isFirmware ? 'flash' : 'filesystem') + ' (GitHub ' + (rel.tag_name || '') + ')';
           showProgressPage();
           if (fileEl) fileEl.textContent = filename;
           if (targetEl) targetEl.textContent = targetLabel;
           progressBar.style.width = '0%';
           progressText.textContent = 'Downloading from GitHub...';
           errorEl.textContent = '';
           retryBtn.style.display = 'none';
           fetch(downloadUrl)
             .then(function(r) {
               if (!r.ok) throw new Error('Download failed: HTTP ' + r.status);
               return r.blob();
             })
             .then(function(blob) {
               progressBar.style.width = '10%';
               progressText.textContent = 'Uploading to device...';
               var action = (isFirmware ? '/update?cmd=0' : '/update?cmd=100') + '&size=' + blob.size;
               var fieldName = isFirmware ? 'firmware' : 'filesystem';
               return xhrUpload(action, fieldName, blob, filename)
                 .then(function() {
                   progressText.textContent = 'Flash complete! Device rebooting...';
                   waitForDeviceReboot(null);
                 });
             })
             .catch(function(e) {
               progressText.textContent = 'Failed';
               errorEl.textContent = (e.message || String(e)) + (String(e.message).indexOf('download') >= 0 ? ' — try manual upload instead.' : '');
               retryBtn.style.display = 'block';
             });
         };

         window.ghFlashBoth = function(idx) {
           if (!ghReleases || idx >= ghReleases.length) return;
           var rel = ghReleases[idx];
           var fwAsset = null, fsAsset = null;
           for (var j = 0; j < (rel.assets || []).length; j++) {
             var a = rel.assets[j];
             if (!fwAsset && a.name && a.name.indexOf('.ino.bin') >= 0) fwAsset = a;
             if (!fsAsset && a.name && a.name.indexOf('.littlefs.bin') >= 0) fsAsset = a;
           }
           if (!fwAsset || !fsAsset) { alert('Both firmware and filesystem assets are required.'); return; }

           var tag = rel.tag_name || '';

           showProgressPage();
           progressBar.style.width = '0%';
           progressBar.style.backgroundColor = '';
           progressText.textContent = 'Downloading firmware + filesystem from GitHub...';
           errorEl.textContent = '';
           retryBtn.style.display = 'none';

           // Step 1: Download both blobs in parallel
           Promise.all([
             fetch(fwAsset.browser_download_url).then(function(r) {
               if (!r.ok) throw new Error('Firmware download failed: HTTP ' + r.status);
               return r.blob();
             }),
             fetch(fsAsset.browser_download_url).then(function(r) {
               if (!r.ok) throw new Error('Filesystem download failed: HTTP ' + r.status);
               return r.blob();
             })
           ])
           .then(function(blobs) {
             var fwBlob = blobs[0], fsBlob = blobs[1];
             // Step 2: Backup settings + labels before filesystem flash
             progressText.textContent = 'Downloading backups...';
             return doBackups().then(function() { return [fwBlob, fsBlob]; });
           })
           .then(function(blobs) {
             var fwBlob = blobs[0], fsBlob = blobs[1];
             // Step 3: Flash firmware
             if (fileEl) fileEl.textContent = fwAsset.name;
             if (targetEl) targetEl.textContent = 'firmware (1/2) \u2014 GitHub ' + tag;
             progressBar.style.width = '0%';
             progressBar.style.backgroundColor = '';
             progressText.textContent = 'Uploading firmware (1/2)...';
             return xhrUpload('/update?cmd=0&size=' + fwBlob.size, 'firmware', fwBlob, fwAsset.name)
               .then(function() {
                 // Step 4: Firmware uploaded — wait for device to reboot, then flash filesystem
                 progressBar.style.backgroundColor = '#4CAF50';
                 progressText.textContent = 'Firmware flashed! Device rebooting... (1/2)';
                 return new Promise(function(resolve, reject) {
                   waitForDeviceReboot(function() {
                     // Step 5: Device is back — flash filesystem
                     if (fileEl) fileEl.textContent = fsAsset.name;
                     if (targetEl) targetEl.textContent = 'filesystem (2/2) \u2014 GitHub ' + tag;
                     progressBar.style.width = '0%';
                     progressBar.style.backgroundColor = '';
                     progressText.textContent = 'Uploading filesystem (2/2)...';
                     xhrUpload('/update?cmd=100&size=' + fsBlob.size, 'filesystem', fsBlob, fsAsset.name)
                       .then(function() {
                         // Step 6: Filesystem uploaded — wait for device to reboot, then redirect to /
                         progressBar.style.backgroundColor = '#4CAF50';
                         progressText.textContent = 'Filesystem flashed! Device rebooting... (2/2)';
                         waitForDeviceReboot(null);
                         resolve();
                       })
                       .catch(reject);
                   });
                 });
               });
           })
           .catch(function(e) {
             progressText.textContent = 'Failed';
             errorEl.textContent = String(e.message || e);
             retryBtn.style.display = 'block';
           });
         };
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
      %SETTINGS_MSG%
      <br/>
      <br/>Wait for the OTGW firmware to reboot and start the HTTP server
      <br/>
      <br/><span id="status" style="font-weight:bold; color: #666;">Waiting for device... (60s)</span>
      <br/>
      <br/>If nothing happens, refresh with <span style='font-size:1.3em;'><b><a href="/">this link here</a></b></span>.
      </body>
      <script>
         var remainingSeconds = 60;
         var statusEl = document.getElementById("status");
         
         var poller = setInterval(function() {
           remainingSeconds--;
           
           // Check health endpoint to verify device is fully booted
           fetch('/api/v2/health?t=' + Date.now(), { 
             method: 'GET', 
             cache: 'no-store',
             headers: { 'Accept': 'application/json' }
           })
             .then(function(res) {
               if (res.ok) {
                 return res.json();
               }
               throw new Error('HTTP ' + res.status);
             })
             .then(function(data) {
               // Validate health response - simple object access
               if (data && data.health && data.health.status === 'UP') {
                 clearInterval(poller);
                 statusEl.textContent = "Device is back online!";
                 statusEl.style.color = "green";
                 
                 // Try to restore dallas labels from parent window cache
                 var labelsRestored = Promise.resolve();
                 try {
                   if (window.opener && window.opener.dallasLabelsCache) {
                     var labels = window.opener.dallasLabelsCache;
                     if (labels && typeof labels === 'object' && Object.keys(labels).length > 0) {
                       statusEl.textContent = "Restoring Dallas labels...";
                       labelsRestored = fetch('/api/v2/sensors/labels', {
                         method: 'POST',
                         headers: { 'Content-Type': 'application/json' },
                         body: JSON.stringify(labels)
                       })
                       .then(function(res) {
                         if (res.ok) {
                           console.log('[OTA] Dallas labels restored');
                         }
                       })
                       .catch(function(err) {
                         console.error('[OTA] Label restore error:', err);
                       });
                     }
                   }
                 } catch(e) {}
                 
                 labelsRestored.then(function() {
                   statusEl.textContent = "Redirecting...";
                   setTimeout(function() {
                     window.location.href = "/";
                   }, 1000);
                 });
               }
             })
             .catch(function(e) {
               // Ignore - device still rebooting
             });
           
           if (remainingSeconds <= 0) {
             clearInterval(poller);
             statusEl.textContent = "Redirecting...";
             window.location.href = "/";
             return;
           }
           
           statusEl.textContent = "Waiting for device... (" + remainingSeconds + "s)";
           statusEl.style.color = "#666";
         }, 1000);
    </script>
    </html>)SUCCESS";
     

#endif // UPDATESERVERHTML_H
