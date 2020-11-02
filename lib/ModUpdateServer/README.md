# ModUpdateServer
A modified version of the ESP8266HTTPUpdateServer

### Why
I needed a more friendly looking update server for my projects.

Two extra functions:

```
    void setIndexPage(const char *indexPage);
    void setSuccessPage(const char *succesPage);
```

The `indexPage[]` and `successPage[]` const char need 
to be defined and can be tailered to your needs.

```
 static const char updateServerIndex[] PROGMEM =
  R"(<html charset="UTF-8">
     <body>
     <h1>ESP8266 Flash utility</h1>
     <form method='POST' action='?cmd=0' enctype='multipart/form-data'>
        <br/>Select firmware to flash<br/>
            <input type='file' accept='ino.bin' name='firmware'>
            <input type='submit' value='Flash Firmware'>
      </form>
      <form method='POST' action='?cmd=100' enctype='multipart/form-data'> 
        <br/>Select spiffs to flash<br/>
                  <input type='file' accept='spiffs.bin' name='filesystem'>
                  <input type='submit' value='Flash Spiffs'>
      </form>
     </html>)";

static const char updateServerSuccess[] PROGMEM = 
  "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update <b>Success</b>!<br>Wait for ESP8266 to reboot...";
```

Right after

```
  httpUpdater.setup(&httpServer);
```

you need to set these pages:

```
  httpUpdater.setIndexPage(updateServerIndex);
  httpUpdater.setSuccessPage(updateServerSuccess);
```
