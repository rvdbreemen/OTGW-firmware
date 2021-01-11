
static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <style type='text/css'>
        body {background-color: lightblue;}
     </style>
     <body>
     <h1>OTGW firmware Flash utility</h1>
     <form method='POST' action='?cmd=0' enctype='multipart/form-data'>
          Select a "<b>.ino.bin</b>" file to flash<br/>
          <input type='file' accept='ino.bin' name='firmware'>
          <input type='submit' value='Flash Firmware'>
      </form>
      <form method='POST' action='?cmd=100' enctype='multipart/form-data'> 
          Select a "<b>.spiffs.bin</b>" file to flash<br/>
          <input type='file' accept='spiffs.bin' name='filesystem'>
          <input type='submit' value='Flash Spiffs'>
      </form>
      <hr>
      <br/><font color='red'>Warning!!!</font>
      <br/>You will lose all your files when flassing SPIFF. 
      <br/>Basckup your files first to your local filesystem (using FSexplorer) 
      and upload them after the flassing of your SPIFF.
      <hr>
      <br/>
      <br/>Wait <span style='font-size: 1.3em;' id="waitSeconds">60</span> seconds ..
      <br>If nothing happend, then wait for count down to zero and then refresh with this linkspan style='font-size:1.3em;'><b><a href="/">hier</a></b></span>!
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
      <br>If nothing happend, then wait for count down to zero and then refresh with this linkspan style='font-size:1.3em;'><b><a href="/">hier</a></b></span>!
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
     
