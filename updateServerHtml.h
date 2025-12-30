
static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <style type='text/css'>
        body {background-color: lightblue;}
     </style>
     <body>
     <h1>OTGW firmware Flash utility</h1>
     <div id="updateStatus">Idle</div>
     <progress id="updateProgress" value="0" max="100" style="width: 100%; max-width: 420px;"></progress>
     <div id="updateDetail"></div>
     <form method='POST' action='?cmd=0' enctype='multipart/form-data'>
          Select a "<b>.ino.bin</b>" file to flash<br/>
          <input type='file' accept='ino.bin' name='firmware'>
          <input type='submit' value='Flash Firmware'>
      </form>
      <form method='POST' action='?cmd=100' enctype='multipart/form-data'> 
          Select a "<b>.littlefs.bin</b>" file to flash<br/>
          <input type='file' accept='littlefs.bin' name='filesystem'>
          <input type='submit' value='Flash LittleFS'>
      </form>
      <hr>
      <br/><font color='red'>Warning!!!</font>
      <br/>You will lose all your files when flashing LittleFS. 
      <br/>Backup your files first to your local filesystem (using FSexplorer) 
      and upload them after the flashing of your LittleFS.
      <hr>
      <br/>
      <!--
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
     </script>-->
     <script>
       var pollIntervalId = null;  // Track polling interval
       var sseRetryCount = 0;
       var sseMaxRetries = 3;
       var sseRetryDelay = 1000;  // Start with 1 second
       var maxRetryDelay = 8000;  // Cap exponential backoff at 8 seconds
       
       function applyUpdateStatus(data) {
         var pct = data.percent || 0;
         document.getElementById('updateProgress').value = pct;
         document.getElementById('updateStatus').textContent = data.message || 'Idle';
         if (data.total > 0) {
           document.getElementById('updateDetail').textContent =
             data.transferred + ' / ' + data.total + ' bytes (' + pct + '%)';
         } else {
           document.getElementById('updateDetail').textContent = '';
         }
       }

       function pollUpdateStatus() {
         fetch('/api/v0/update/status')
           .then(function(response) { return response.json(); })
           .then(applyUpdateStatus)
           .catch(function(err) {
             if (window.console && console.error) {
               console.error('pollUpdateStatus error:', err);
             }
           });
       }
       
       function startPolling() {
         // Clear any existing interval before creating a new one
         if (pollIntervalId !== null) {
           clearInterval(pollIntervalId);
         }
         pollIntervalId = setInterval(pollUpdateStatus, 500);
       }

       function startSse() {
         if (!window.EventSource) return false;
         
         var es = new EventSource('/api/v0/update/events');
         
         es.addEventListener('update', function(e) {
           try { 
             applyUpdateStatus(JSON.parse(e.data));
             sseRetryCount = 0;  // Reset retry count on successful message
           } catch (err) {
             if (window.console && console.error) {
               console.error('SSE message parse error:', err, 'Data:', e.data);
             }
           }
         });
         
         es.onerror = function() {
           try {
             es.close();
           } catch (err) {
             // EventSource may already be closed, ignore errors
           }
           
           // Implement exponential backoff retry logic
           if (sseRetryCount < sseMaxRetries) {
             sseRetryCount++;
             // Exponential backoff: 1s, 2s, 4s (capped at maxRetryDelay)
             var delay = Math.min(sseRetryDelay * Math.pow(2, sseRetryCount - 1), maxRetryDelay);
             if (window.console && console.log) {
               console.log('SSE connection failed, retrying in ' + delay + 'ms (attempt ' + sseRetryCount + '/' + sseMaxRetries + ')');
             }
             setTimeout(function() {
               startSse();
             }, delay);
           } else {
             // Permanent fallback to polling after retries exhausted
             if (window.console && console.log) {
               console.log('SSE connection failed permanently, falling back to polling');
             }
             startPolling();
           }
         };
         
         return true;
       }

       if (!startSse()) {
         startPolling();
       }
       
       // Initial status fetch with error handling
       try {
         pollUpdateStatus();
       } catch (err) {
         if (window.console && console.error) {
           console.error('Initial pollUpdateStatus call failed:', err);
         }
       }
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
     
