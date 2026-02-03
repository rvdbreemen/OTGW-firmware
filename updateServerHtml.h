
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
         var lastWsMessageTime = 0;
         var wsWatchdogTimer = null;
         var wsActive = false;
         var lastDeviceStatus = null;
         var lastDeviceStatusTime = 0;
         var uploadRetryTimer = null;
         var uploadRetryPending = false;
         var uploadRetryAttempts = 0;
         var uploadRetryMax = 0;
         var uploadRetryIsFilesystem = false;
         var uploadRetryBaseMs = 2000;
         var uploadRetryMaxDelayMs = 15000;
         var wsReconnectAttempts = 0;
         var wsReconnectMaxDelay = 10000;
        var wsConnectCount = 0;
        var wsDisconnectCount = 0;
         var flashingInProgress = false;
         var flashStartTime = 0;
         var flashPollingActivated = false;
         var wsInstance = null; // Track WebSocket instance for resource management
        var wsReconnectTimer = null;
        var wsReconnectBaseDelay = 500;
        var wsReconnectDelayMs = 0;
        var lastStatusState = null;
        var lastFlashPct = -1;
        var lastUploadLogPct = -1;
        var lastLoggedPollDelayMs = 0;
        var uploadStartMs = 0;
        var uploadLastRateMs = 0;
        var uploadLastRateBytes = 0;
        var flashLogStartMs = Date.now();
        var flashOfflineGraceMs = 60000;
        var flashAssumeSuccessMinMs = 5000;
        var lastFlashStateTime = 0;
        var offlineCountdownStart = 0;
        var offlineCountdownTimer = null;
        var offlineCountdownDurationMs = 60000;
        var offlineCountdownStartDelayMs = 5000;

        logFlash('Flash UI initialized');

         function hasJsonContentType(response, contentType) {
           var ct = contentType || '';
           if (!ct && response && response.headers) {
             try { ct = response.headers.get('content-type') || ''; } catch (e) { ct = ''; }
           }
           return ct.indexOf('application/json') !== -1;
         }

         function parseJsonSafe(text) {
           if (!text) return null;
           var t = ('' + text).trim();
           if (!t.length) return null;
           if (t[0] !== '{' && t[0] !== '[') return null;
           try { return JSON.parse(t); } catch (e) { return null; }
         }

         function logFlash(message, detail) {
           try {
            var nowMs = Date.now();
            var relMs = nowMs - flashLogStartMs;
            var prefix = '[+' + relMs + 'ms]';
             if (typeof detail !== 'undefined') {
              console.log('[Flash]', prefix, message, detail);
             } else {
              console.log('[Flash]', prefix, message);
             }
           } catch (e) {}
         }

         function fetchText(url, timeoutMs, customHeaders) {
           return new Promise(function(resolve, reject) {
             if (window.fetch) {
               var controller = null;
               var timeoutId = null;
               var options = { 
                 cache: 'no-store',
                 headers: customHeaders || {}
               };
               if (window.AbortController && timeoutMs && timeoutMs > 0) {
                 controller = new AbortController();
                 options.signal = controller.signal;
                 timeoutId = setTimeout(function() { controller.abort(); }, timeoutMs);
               }
               fetch(url, options)
                 .then(function(response) {
                   // Clear timeout immediately after response headers received
                    // Safari-specific: Clearing timeout here prevents AbortError during response.text()
                    // Safari can abort the fetch if timeout expires while reading response body
                   // This prevents Safari from aborting while reading response body
                   if (timeoutId) {
                     clearTimeout(timeoutId);
                     timeoutId = null;
                   }
                   return response.text().then(function(text) {
                     resolve({
                       ok: response.ok,
                       status: response.status,
                       text: text,
                       contentType: (response.headers && response.headers.get) ? (response.headers.get('content-type') || '') : ''
                     });
                   });
                 })
                 .catch(function(err) { 
                   if (timeoutId) {
                     clearTimeout(timeoutId);
                     timeoutId = null;
                   }
                   reject(err); 
                 });
             } else {
               var xhr = new XMLHttpRequest();
               xhr.open('GET', url, true);
               xhr.timeout = timeoutMs || 5000;
               try { 
                 xhr.setRequestHeader('Cache-Control', 'no-store');
                 // Add custom headers if provided
                 if (customHeaders) {
                   for (var key in customHeaders) {
                     if (customHeaders.hasOwnProperty(key)) {
                       xhr.setRequestHeader(key, customHeaders[key]);
                     }
                   }
                 }
               } catch (e) {}
               xhr.onreadystatechange = function() {
                 if (xhr.readyState === 4) {
                   resolve({
                     ok: (xhr.status >= 200 && xhr.status < 300),
                     status: xhr.status,
                     text: xhr.responseText || '',
                     contentType: xhr.getResponseHeader('content-type') || ''
                   });
                 }
               };
               xhr.onerror = function() { reject(new Error('XHR error')); };
               xhr.ontimeout = function() { reject(new Error('XHR timeout')); };
               xhr.send();
             }
           });
         }

         function showProgressPage(title) {
           pageForm.style.display = 'none';
           pageProgress.style.display = 'block';
           if (retryBtn) retryBtn.style.display = 'none';
           if (title) progressTitle.textContent = title;
         }

         function cancelUploadRetry() {
           if (uploadRetryTimer) {
             clearTimeout(uploadRetryTimer);
             uploadRetryTimer = null;
           }
           uploadRetryPending = false;
         }

         window.retryFlash = function() {
          logFlash('User requested retry');
            pageProgress.style.display = 'none';
            pageForm.style.display = 'block';
            if (retryBtn) retryBtn.style.display = 'none';
            if (errorEl) errorEl.textContent = '';
            if (formErrorEl) formErrorEl.textContent = '';
            uploadInFlight = false;
            localUploadDone = false;
            cancelUploadRetry();
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
          logFlash('Reboot countdown started');
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
                   logFlash('Device back online during countdown');
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
          lastDeviceStatus = status;
          lastDeviceStatusTime = Date.now();
          var state = status.state || 'idle';
          var target = status.target || '';

          if (state !== lastStatusState) {
            logFlash('Status state changed: ' + state + (target ? (' (target: ' + target + ')') : ''));
            lastStatusState = state;
          }

           // Track flashing state for fallback logic
           if (state === 'start' || state === 'write') {
             if (!flashingInProgress) {
               flashingInProgress = true;
               flashStartTime = Date.now();
               console.log('Flash operation started, activating enhanced monitoring');
             }
             lastFlashStateTime = Date.now();
           } else if (state === 'end' || state === 'error' || state === 'abort') {
             flashingInProgress = false;
             flashPollingActivated = false;
           }
           if (offlineCountdownTimer) {
             clearInterval(offlineCountdownTimer);
             offlineCountdownTimer = null;
           }
           offlineCountdownStart = 0;
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

           if (flashTotal > 0) {
             var pct = Math.round((flashWritten / flashTotal) * 100);
             if (pct > 100) pct = 100;
             if (pct !== lastFlashPct) {
               lastFlashPct = pct;
               logFlash('Flash progress: ' + pct + '% (' + flashWritten + ' / ' + flashTotal + ')');
             }
           }
           
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
             logFlash('Flash completed successfully');
             if (!successShown) {
               successShown = true;
               if (successPanel) successPanel.style.display = 'block';
               if (target === 'filesystem') {
                 if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Rebooting filesystem...';
                 startSuccessCountdown();
               } else {
                 if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Rebooting device...';
                 startSuccessCountdown();
               }
             }
             stopPolling();
              flashingInProgress = false;
             
           } else if (state === 'error') {
             progressTitle.textContent = 'Flashing error';
             logFlash('Flash error: ' + (status.error || 'unknown'));
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'block';
             stopPolling();
             localUploadDone = false;
              flashingInProgress = false;
             
           } else if (state === 'abort') {
             progressTitle.textContent = 'Flashing aborted';
             logFlash('Flash aborted');
             successShown = false;
             if (successPanel) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'block';
             stopPolling();
             localUploadDone = false;
              flashingInProgress = false;
             
           } else {
             // In progress (start, write)
             progressTitle.textContent = 'Flashing in progress';
             if (successPanel && !successShown) successPanel.style.display = 'none';
             if (retryBtn) retryBtn.style.display = 'none';
           }

           // If device rebooted and status resets to idle, assume success when upload finished
           if (state === 'idle' && localUploadDone && !successShown && flashStartTime > 0) {
             var elapsed = Date.now() - flashStartTime;
             if (elapsed >= flashAssumeSuccessMinMs) {
               logFlash('Device returned with idle status after flash; assuming success');
               progressTitle.textContent = 'Flashing complete';
               successShown = true;
               if (successPanel) successPanel.style.display = 'block';
               if (successMessageEl) successMessageEl.textContent = 'Flashing successful. Rebooting device...';
               startSuccessCountdown();
               stopPolling();
               flashingInProgress = false;
             }
           }
         }

         function fetchStatus(timeoutMs) {
           logFlash('Status fetch start (timeout ' + timeoutMs + 'ms)');
            return fetchText('/status', timeoutMs)
             .then(function(res) {
               if (!res.ok) {
                 throw new Error('HTTP ' + res.status);
               }
               if (!hasJsonContentType(null, res.contentType)) {
                 throw new Error('Unexpected content-type');
               }
               var parsed = parseJsonSafe(res.text);
               if (!parsed) {
                 throw new Error('Invalid JSON');
               }
               return parsed;
             })
             .then(function(json) {
               logFlash('Status fetch OK: ' + (json.state || 'n/a') + (json.target ? (' target=' + json.target) : '') +
                        ' flash=' + (json.flash_written || 0) + '/' + (json.flash_total || 0));
               updateDeviceStatus(json);
               return true;
             })
             .catch(function(e) {
               // Safari-specific: AbortError handling
               // AbortError occurs when fetch is aborted due to timeout
               // This is normal during flash operations when device is busy
               if (e && e.name === 'AbortError') {
                 // During flash, AbortError is expected - device is busy writing
                 if (flashingInProgress || localUploadDone) {
                   console.log('Fetch aborted during flash (expected) - device is busy');
                   logFlash('Status fetch aborted during flash (expected)');
                   if (flashInfoEl && localUploadDone) {
                     flashInfoEl.textContent = 'Device busy writing flash... waiting for reboot';
                   }
                   return false; // Don't treat as error
                 }
                 console.log('Fetch aborted - may indicate timeout');
                 logFlash('Status fetch aborted');
               } else if (e && e.message && e.message.indexOf('NetworkError') !== -1) {
                 // Network errors can occur during flash when device is overloaded
                 if (flashingInProgress || localUploadDone) {
                   console.log('Network error during flash (expected) - device is busy');
                   logFlash('Status fetch network error during flash (expected)');
                   if (flashInfoEl && localUploadDone) {
                     flashInfoEl.textContent = 'Device busy writing flash... waiting for reboot';
                   }
                   return false; // Don't treat as error
                 }
                 console.log('Network error:', e.message);
                 logFlash('Status fetch network error: ' + e.message);
               } else if (e) {
                 // Log other errors only if not routine
                 console.log('Fetch status error:', e.name || 'Unknown', e.message || '');
                 logFlash('Status fetch error: ' + (e.name || 'Unknown') + ' ' + (e.message || ''));
                 // Propagate non-routine errors so polling logic can react
                 throw e;
               }
               return false;
             });
         }

        function scheduleNextPoll(delayMs) {
          if (!pollActive) return;
          if (pollTimer) clearTimeout(pollTimer);
          if (delayMs !== lastLoggedPollDelayMs) {
            lastLoggedPollDelayMs = delayMs;
            logFlash('Polling scheduled in ' + delayMs + 'ms');
          }
          pollTimer = setTimeout(runPoll, delayMs);
        }

        function updateOfflineCountdown(nowMs) {
          if (!flashingInProgress && !localUploadDone) return;
          if (lastDeviceStatusTime > 0 && (nowMs - lastDeviceStatusTime) < offlineCountdownStartDelayMs) {
            return;
          }
          if (offlineCountdownStart === 0) {
            offlineCountdownStart = nowMs;
            logFlash('No status received, starting offline countdown');
          }
          if (!offlineCountdownTimer) {
            offlineCountdownTimer = setInterval(function() {
              var now = Date.now();
              var elapsed = now - offlineCountdownStart;
              var remainingMs = Math.max(0, offlineCountdownDurationMs - elapsed);
              var remainingSec = Math.ceil(remainingMs / 1000);
              if (flashInfoEl) {
                flashInfoEl.textContent = 'No response from device. Waiting up to ' + remainingSec + 's...';
              }
              if (remainingMs <= 0) {
                clearInterval(offlineCountdownTimer);
                offlineCountdownTimer = null;
              }
            }, 1000);
          }
        }

        function runPoll() {
          if (!pollActive || pollInFlight) return;
          pollTimer = null;
          pollInFlight = true;
          // During flash operations, ESP is busy writing - use longer timeout to prevent aborts
          // When upload is in flight or device is actively flashing, allow up to 60 seconds
          var isFlashWait = uploadInFlight || flashingInProgress || localUploadDone || (lastUploadTotal > 0 && !localUploadDone);
          var baseTimeout = isFlashWait ? 60000 : pollIntervalMs * 4;
          var timeoutMs = Math.max(500, baseTimeout);
          fetchStatus(timeoutMs)
            .then(function(ok) {
              if (ok) {
                var next = Math.max(pollMinMs, Math.floor(pollIntervalMs / 2));
                if (next !== pollIntervalMs) {
                  logFlash('Polling interval decreased to ' + next + 'ms');
                }
                pollIntervalMs = next;
              } else {
                var nextWait = Math.min(pollMaxMs, Math.max(pollIntervalMs, 2000));
                if (nextWait !== pollIntervalMs) {
                  logFlash('Polling interval held at ' + nextWait + 'ms (device busy)');
                }
                pollIntervalMs = nextWait;
              }
            })
            .catch(function() {
              var next = Math.min(pollMaxMs, pollIntervalMs * 2);
              if (next !== pollIntervalMs) {
                logFlash('Polling interval increased to ' + next + 'ms');
              }
              pollIntervalMs = next;
            })
            .finally(function() {
              updateOfflineCountdown(Date.now());
              pollInFlight = false;
              scheduleNextPoll(pollIntervalMs);
            });
        }

        function startPolling() {
          pollActive = true;
          pollIntervalMs = pollMinMs;
          logFlash('Polling started');
          scheduleNextPoll(0);
        }

        function stopPolling() {
          pollActive = false;
          logFlash('Polling stopped');
          if (pollTimer) {
            clearTimeout(pollTimer);
            pollTimer = null;
          }
        }

        function startWsWatchdog() {
          if (wsWatchdogTimer) clearTimeout(wsWatchdogTimer);
          
          // Shorter timeout during active flash operations (5s vs 15s)
          var watchdogDelay = flashingInProgress ? 5000 : 15000;
          logFlash('WebSocket watchdog armed (' + watchdogDelay + 'ms)');
          
          wsWatchdogTimer = setTimeout(function() {
            var now = Date.now();
            
            // If flashing and WebSocket is silent, activate polling fallback immediately
            if (flashingInProgress && !pollActive) {
              console.log('WebSocket silent during flash operation, activating polling fallback');
              logFlash('WebSocket silent during flash, enabling polling fallback');
              wsActive = false;
              flashPollingActivated = true;
              startPolling();
            }
            // WebSocket has been silent for timeout period - restart polling as fallback
            else if (!pollActive) {
              console.log('WebSocket silent for ' + (watchdogDelay/1000) + 's, restarting polling as fallback');
              logFlash('WebSocket silent, enabling polling fallback');
              wsActive = false;
              startPolling();
            }
          }, watchdogDelay);
        }


        // Keep WebSocket open during uploads to preserve flash progress updates

        // WebSocket Support for Real-time Progress
        function setupWebSocket() {
            logFlash('WebSocket setup start');
            if (!window.WebSocket) {
              logFlash('WebSocket not supported, switching to polling');
              console.log('WebSocket not supported, using polling');
              if (!pollActive) startPolling();
              return;
            }
            
            // Use ws:// protocol (never wss:// - this firmware uses HTTP only)
            var wsScheme = 'ws://';
            var wsUrl = wsScheme + window.location.hostname + ':81/';
            logFlash('WebSocket URL: ' + wsUrl);
            console.log('Connecting to WebSocket:', wsUrl);
            var ws;
            
            try {
              ws = new WebSocket(wsUrl);
              wsInstance = ws; // Store instance for resource management
              logFlash('WebSocket created');
            } catch (e) {
              logFlash('WebSocket create failed');
              console.log('WebSocket create failed:', e);
              wsReconnectAttempts++;
              if (!pollActive) startPolling();
              
              // Dynamic backoff with jitter for reconnection
              var reconnectDelay = getWsReconnectDelay();
              console.log('Will retry WebSocket in ' + (reconnectDelay/1000) + 's (attempt ' + wsReconnectAttempts + ')');
              scheduleWsReconnect(reconnectDelay);
              return;
            }
            
            // Safari: Set a connection timeout
            // Safari can hang indefinitely on WebSocket connections
            var wsConnectionTimer = null;
            var isSafari = false;
            if (typeof navigator !== 'undefined' && navigator.userAgent) {
              var ua = navigator.userAgent;
              isSafari = (ua.indexOf('Safari') >= 0 && ua.indexOf('Chrome') < 0 && ua.indexOf('Chromium') < 0 && ua.indexOf('Android') < 0);
            }
            if (isSafari && !flashingInProgress && !uploadInFlight) {
              wsConnectionTimer = setTimeout(function() {
                if (ws.readyState === WebSocket.CONNECTING) {
                  console.log('WebSocket connection timeout (Safari workaround), closing and using polling');
                  try {
                    ws.close();
                  } catch (e) {
                    console.log('Error closing timed-out WebSocket:', e);
                  }
                  if (!pollActive) startPolling();
                }
              }, 10000); // 10 second timeout for connection
            }
            
            ws.onopen = function() {
              logFlash('WebSocket connected');
              wsConnectCount += 1;
              logFlash('WebSocket connect count: ' + wsConnectCount);
                console.log('WebSocket connected successfully');
                clearTimeout(wsConnectionTimer);
                wsReconnectAttempts = 0; // Reset backoff counter on successful connection
                wsReconnectDelayMs = 0;
                if (wsReconnectTimer) {
                  clearTimeout(wsReconnectTimer);
                  wsReconnectTimer = null;
                }
                
                // If we're in the middle of flashing and polling was activated and is currently active,
                // we expect to keep both running (see ws.onmessage for the actual decision logic)
                if (flashingInProgress && flashPollingActivated && pollActive) {
                  console.log('Flash in progress with active polling - keeping both WebSocket and polling active');
                }
            };
            
            ws.onmessage = function(e) {
                if (!e || typeof e.data !== 'string') return;
                var msg = parseJsonSafe(e.data);
                // Ignore non-JSON messages (like raw logs)
                if (!msg || typeof msg.state === 'undefined') return;

              logFlash('WebSocket status: ' + msg.state + (msg.target ? (' target=' + msg.target) : '') +
                   ' flash=' + (msg.flash_written || 0) + '/' + (msg.flash_total || 0));
                
                lastWsMessageTime = Date.now();
                
                // First WebSocket message received - stop polling if not flashing
                if (!wsActive) {
                  wsActive = true;
                  // Only stop polling if we're not in the middle of a flash operation
                  // During flash, keep polling as backup in case WS drops
                  if (!flashingInProgress && pollActive) {
                    stopPolling();
                    console.log('WebSocket active, polling stopped');
                  } else if (flashingInProgress) {
                    console.log('WebSocket active during flash - keeping polling as backup');
                  }
                }
                
                // Reset watchdog - if WS goes silent, polling will restart
                startWsWatchdog();
                updateDeviceStatus(msg);
            };
            
            ws.onerror = function(e) {
              logFlash('WebSocket error');
                console.log('WebSocket error:', e);
                clearTimeout(wsConnectionTimer);
                wsInstance = null; // Clear instance reference
                wsActive = false;
                wsReconnectAttempts++;
                if (!pollActive) startPolling();
              var reconnectDelay = getWsReconnectDelay();
              console.log('WebSocket error, retrying in ' + (reconnectDelay/1000) + 's (attempt ' + wsReconnectAttempts + ')');
              scheduleWsReconnect(reconnectDelay);
            };
            
            ws.onclose = function() {
              wsDisconnectCount += 1;
              logFlash('WebSocket disconnect count: ' + wsDisconnectCount);
              logFlash('WebSocket closed');
                console.log('WebSocket closed');
                clearTimeout(wsConnectionTimer);
                wsInstance = null; // Clear instance reference
                
                // WebSocket closed - restart polling immediately as fallback
                wsActive = false;
                if (wsWatchdogTimer) {
                  clearTimeout(wsWatchdogTimer);
                  wsWatchdogTimer = null;
                }
                if (!pollActive) {
                  console.log('WebSocket closed, starting polling fallback');
                  startPolling();
                }
                
                // Reconnect with dynamic backoff
                wsReconnectAttempts++;
                var reconnectDelay = getWsReconnectDelay();
                console.log('WebSocket will retry in ' + (reconnectDelay/1000) + 's (attempt ' + wsReconnectAttempts + ')');
                logFlash('WebSocket reconnect scheduled in ' + reconnectDelay + 'ms (attempt ' + wsReconnectAttempts + ')');
                scheduleWsReconnect(reconnectDelay);
            };
        }
        setupWebSocket();

        function getWsReconnectDelay() {
          // Decorrelated jitter backoff (AWS-style): sleep = min(cap, random(base, sleep*3))
          // Spreads reconnects across clients and avoids thundering herd.
          var minDelay = wsReconnectBaseDelay;
          var maxDelay = wsReconnectMaxDelay;
          var prev = wsReconnectDelayMs > 0 ? wsReconnectDelayMs : minDelay;
          var next = Math.min(maxDelay, Math.floor(minDelay + Math.random() * (prev * 3 - minDelay)));
          wsReconnectDelayMs = next;
          return next;
        }

        function scheduleWsReconnect(delayMs) {
          if (wsReconnectTimer) clearTimeout(wsReconnectTimer);
          wsReconnectTimer = setTimeout(function() {
            wsReconnectTimer = null;
            setupWebSocket();
          }, delayMs);
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
               if (input.files && input.files.length > 0) {
                 logFlash('File selected for ' + targetName + ': ' + (input.files[0].name || '-') + ' size=' + input.files[0].size);
               }
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
             logFlash('Upload submit: target=' + targetName + ' file=' + (input.files[0].name || '-') + ' size=' + input.files[0].size);
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

              // Keep WebSocket open during upload to preserve flash progress updates
              
              // Start polling immediately - don't rely on WebSocket during upload
              if (!pollActive) {
                console.log('Safari: Activating polling before upload (avoiding WebSocket resource contention)');
                 logFlash('Polling activated before upload');
                flashPollingActivated = true;
                startPolling();
              }

             var action = form.action;
             var preFlight = Promise.resolve();
             cancelUploadRetry();
             uploadRetryAttempts = 0;
             uploadRetryMax = (targetName === 'filesystem') ? 3 : 0;
             uploadRetryIsFilesystem = (targetName === 'filesystem');

             // --- Custom logic: backup settings before filesystem flash ---
             if (formId === 'fsForm') {
                var chk = document.getElementById('chkPreserve');
                if(chk && chk.checked) {
                logFlash('Filesystem flash: starting settings backup download');
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
                                logFlash('Settings backup download started');
                             return new Promise(function(resolve) { setTimeout(resolve, 1000); });
                        })
                        .catch(function(e) {
                                logFlash('Settings backup failed');
                             console.log('Backup error', e);
                             if(!confirm("Could not backup settings.ini. Continue anyway?")) {
                                 throw e;
                             }
                        });
                }
             }
             // ------------------------------------------

             preFlight.then(function() {
                 logFlash('Preflight complete, preparing upload');
                 if (action.indexOf('size=') === -1) {
                   action += (action.indexOf('?') === -1 ? '?' : '&') + 'size=' + encodeURIComponent(input.files[0].size);
                 }

                 var scheduleUploadRetry = function(reason) {
                     if (!uploadRetryIsFilesystem || uploadRetryMax <= 0) return false;
                     if (uploadRetryAttempts >= uploadRetryMax) return false;
                     if (uploadRetryPending) return true;
                     if (lastDeviceStatus && lastDeviceStatus.state && lastDeviceStatus.state !== 'idle') {
                       console.log('Auto-retry skipped: device is flashing');
                       logFlash('Upload retry skipped: device is flashing');
                       return false;
                     }
                     var delayMs = Math.min(uploadRetryMaxDelayMs, uploadRetryBaseMs * Math.pow(2, uploadRetryAttempts));
                     var attempt = uploadRetryAttempts + 1;
                     uploadRetryPending = true;
                     if (retryBtn) retryBtn.style.display = 'none';
                     logFlash('Scheduling upload retry in ' + Math.round(delayMs / 1000) + 's (attempt ' + attempt + '/' + uploadRetryMax + ')');
                     errorEl.textContent = (reason || 'Upload failed') + ' Retrying in ' + Math.round(delayMs / 1000) + 's (' + attempt + '/' + uploadRetryMax + ')...';
                     if (!pollActive) startPolling();
                     uploadRetryTimer = setTimeout(function() {
                       uploadRetryPending = false;
                       uploadRetryAttempts += 1;
                       performUpload();
                     }, delayMs);
                     return true;
                 };

                 var performUpload = function() {
                     cancelUploadRetry();
                     uploadInFlight = true;
                   lastUploadLogPct = -1;
                   uploadStartMs = Date.now();
                   uploadLastRateMs = uploadStartMs;
                   uploadLastRateBytes = 0;
                   logFlash('Upload started: ' + action);
                     var xhr = new XMLHttpRequest();
                     xhr.open('POST', action, true);
                     
                     // Set long timeout for firmware flash operations (5 minutes)
                     // Firmware writes can block for 10-20 seconds per chunk
                     xhr.timeout = 300000;
                     
                     xhr.upload.onprogress = function(ev) {
                       var total = ev.lengthComputable ? ev.total : 0;
                       var pct = total > 0 ? Math.round((ev.loaded / total) * 100) : 0;
                       if (pct > 100) pct = 100;
                       if (pct !== lastUploadLogPct) {
                         lastUploadLogPct = pct;
                         logFlash('Upload progress: ' + pct + '% (' + ev.loaded + ' / ' + total + ')');
                       }
                       var nowMs = Date.now();
                       if (nowMs - uploadLastRateMs >= 1000) {
                         var deltaBytes = ev.loaded - uploadLastRateBytes;
                         var deltaMs = nowMs - uploadLastRateMs;
                         if (deltaMs > 0) {
                           var rate = Math.round((deltaBytes * 1000) / deltaMs);
                           logFlash('Upload rate: ' + rate + ' bytes/s');
                         }
                         uploadLastRateMs = nowMs;
                         uploadLastRateBytes = ev.loaded;
                       }
                       lastUploadLoaded = ev.loaded;
                       lastUploadTotal = total;
                       setUploadProgress(ev.loaded, total);
                     };
                     xhr.onload = function() {
                       console.log('Upload finished, status:', xhr.status);
                       logFlash('Upload finished with status ' + xhr.status);
                       if (uploadStartMs > 0) {
                         logFlash('Upload duration: ' + Math.round((Date.now() - uploadStartMs) / 1000) + 's');
                       }
                       if (xhr.status >= 200 && xhr.status < 300) {
                         var responseText = xhr.responseText || '';
                         logFlash('Upload response length: ' + responseText.length);
                        if (responseText.indexOf('Flash error') !== -1) {
                           logFlash('Upload response indicates flash error');
                           // Status line remains hidden or used for error text
                           errorEl.textContent = responseText;
                           progressTitle.textContent = 'Flashing error';
                           successShown = false;
                           if (successPanel) successPanel.style.display = 'none';
                           if (retryBtn) retryBtn.style.display = 'block';
                         } else {
                           logFlash('Upload response OK, waiting for flash completion');
                           setUploadProgress(lastUploadTotal, lastUploadTotal); // Ensure 100%
                           localUploadDone = true;
                           cancelUploadRetry();
                         }
                         uploadInFlight = false;
                       } else {
                         uploadInFlight = false;
                         if (!scheduleUploadRetry('Upload failed: ' + xhr.status + '.')) {
                           errorEl.textContent = 'Upload failed: ' + xhr.status;
                           if (retryBtn) retryBtn.style.display = 'block';
                         }
                       }
                     };
                     xhr.ontimeout = function() {
                       console.log('Upload timeout - flash may still be in progress, check WebSocket status');
                       logFlash('Upload timeout: monitoring flash status');
                       // Don't show error yet - WebSocket might show completion
                       // Just mark upload as done and let WebSocket status take over
                       uploadInFlight = false;
                       if (!scheduleUploadRetry('Connection timeout.')) {
                         localUploadDone = true;
                         errorEl.textContent = 'Connection timeout - monitoring flash progress via WebSocket...';
                       }
                     };
                     xhr.onerror = function() {
                       console.log('Upload XHR error - checking if flash is proceeding via WebSocket');
                       logFlash('Upload error: monitoring flash status');
                       // Upload may have succeeded but response timed out
                       // WebSocket will show actual flash status
                       uploadInFlight = false;
                       if (!scheduleUploadRetry('Upload connection lost.')) {
                         localUploadDone = true;
                         errorEl.textContent = 'Upload connection lost - monitoring flash status...';
                         // Don't show retry button yet - wait for WebSocket status
                       }
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
             logFlash('Leaving flash page');
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
            fetchText('/api/v1/health?t=' + Date.now(), 5000)
                .then(function(res) {
                    if (!res.ok) {
                      throw new Error('HTTP ' + res.status);
                    }
                    var parsed = parseJsonSafe(res.text);
                    if (!parsed) {
                      throw new Error('Invalid JSON');
                    }
                    return parsed;
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
