static const char indexPage[] =
R"(
<!DOCTYPE html>
<html charset="UTF-8">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">  
    <link rel="stylesheet" type="text/css" href="/index.css">
    <script src="/index.js"></script>
    <title>OTGW firmware</title>
  </head>
  <body>
    <font face="Arial">
    <!-- <div class="dev-header"> -->
    <div class="header">
      <h1>
        <span id="sysName">OTGW firmware</span> &nbsp; &nbsp; &nbsp;
        <span id="devName"    style='font-size: small;'>-</span> &nbsp;
        <span id="devVersion" style='font-size: small;'>[version]</span>
        <span id='theTime' class='nav-item nav-clock'>00:00</span>
      </h1>
    </div>
    </font>
    <div id="displayMainPage"      style="display:none">
      <div class="nav-container">
        <div class='nav-left'>
          <a id='tabDeviceInfo' class='nav-item tabButton' style="background: lightblue;">Device Info</a>
          <a id='M_FSexplorer'  class='nav-img'> <img src='/FSexplorer.png' alt='FSexplorer'></a>
          <a id='tabSettings'   class='nav-img'> <img src='/settings.png' alt='Settings'></a>
        </div>
      </div>
      <br/>
      <div id="mainPage">
        <div id="waiting">Wait for it...</div>
      </div>
    </div>
    <div id="displayDeviceInfo"  style="display:none">
      <div class="nav-container">
        <div class='nav-left'>
          <a id='D_back' class='nav-item tabButton' style="background: lightblue;">Back</a>
          <a id='D_FSexplorer' class='nav-img'> <img src='/FSexplorer.png' alt='FSexplorer'></a>
        </div>
      </div>
      <br/>
      <div id="deviceinfoPage"></div>
    </div>
    <div id="displaySettingsPage"  style="display:none">
      <div class="nav-container">
        <div class='nav-left'>
          <a id='S_back' class='nav-item tabButton' style="background: lightblue;">Back</a>
          <a id='S_saveSettings' class='nav-item tabButton' style="background: lightblue;">Save</a>
          <a id='S_FSexplorer' class='nav-img'> <img src='/FSexplorer.png' alt='FSexplorer'></a>
        </div>
      </div>
      <br/>
      <div id="settingsPage">Loading...</div>
    </div>
  
    <!-- KEEP THIS --->

    <!-- Pin to bottom right corner -->
    <div class="bottom right-0">2020 &copy; Robert van den Breemen</div>

    <!-- Pin to bottom left corner -->
    <div id="message" class="bottom left-0">-</div>
  
    <script>
       window.onload=bootsTrapMain;
    </script>

  </body>

</html>
)";

void sendIndexPage()
{
  httpServer.send(200, "text/html", indexPage);

} // sendIndexPage()

// eof
