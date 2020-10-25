
static const char UpdateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <style type='text/css'>
        body {background-color: lightblue;}
     </style>
     <body>
     <h1>OTGW firmware Flash utility</h1>
     <form method='POST' action='?cmd=0' enctype='multipart/form-data'>
          Selecteer een "<b>.ino.bin</b>" bestand<br/>
          <input type='file' accept='ino.bin' name='firmware'>
          <input type='submit' value='Flash Firmware'>
      </form>
      <form method='POST' action='?cmd=100' enctype='multipart/form-data'> 
          Selecteer een "<b>.spiffs.bin</b>" bestand<br/>
          <input type='file' accept='spiffs.bin' name='filesystem'>
          <input type='submit' value='Flash Spiffs'>
      </form>
      <hr>
      <br/><font color='red'>Let op!!!</font>
      <br/>Bij het flashen van SPIFFS raakt u uw bestanden kwijt. 
      <br/>Maak daarom eerst een kopie van deze bestanden (met de FSexplorer) 
      en zet deze na het flashen van SPIFFS weer terug.
      <hr>
      <br/>
      <br/>Wacht nog <span style='font-size: 1.3em;' id="waitSeconds">60</span> seconden ..
      <br>Als het lijkt of er niets gebeurd, wacht dan tot de teller
           op 'nul' staat en klik daarna <span style='font-size:1.3em;'><b><a href="/">hier</a></b></span>!
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
     <br/>Wacht nog <span style='font-size: 1.3em;' id="waitSeconds">60</span> seconden ..
     <br/>Als het lijkt of er niets gebeurd, wacht dan tot de teller
          op 'nul' staat en klik daarna <span style='font-size:1.3em;'><b><a href="/">hier</a></b></span>!
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
     
