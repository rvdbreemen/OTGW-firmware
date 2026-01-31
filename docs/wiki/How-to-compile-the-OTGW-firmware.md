# Using Arduino-CLI
Thanks to @DaveDavenport and @hvxl for making it work on Linux.

## Windows
1. At an administrator command prompt type 
`wsl --install -d ubuntu`
2. Create a username and password when prompted to do so
3. Type Win-R then `ubuntu` to open a shell window

Then follow the Linux instructions below.

## Linux
If this is the first time of building, set up the build tools:
```
sudo apt update
sudo apt install build-essential make git
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
arduino-cli version
```
If `arduino-cli` cannot be found try logging in again to add your local `bin` directory to your path.

Fetch the source code: 
```
git clone https://github.com/rvdbreemen/OTGW-firmware
```

Now compile it:
```
cd OTGW-firmware
make
make filesystem
```

You should find `littlefs.bin` and `OTGW-firmware.ino.bin` under the `build` directory. 

# Manually compiling the firmware and filesystem
To compile the OTGW firmware there are several things you will need.

1. Install <a href=https://www.arduino.cc/>Arduino IDE</a>

2. Unzip the <a href=https://github.com/rvdbreemen/OTGW-firmware/archive/main.zip>source</a> into a local folder on your PC called `OTGW-firmware`

3. Open Arduino IDE then use **File -> Open** to load **OTGW-firmware.ino**

4. Use **Tools -> Manage Libraries** to obtain these additional libraries:

| TYPE| NAME| VERSION | REPO |COMMIT|
|-|-|-|-|-|
| package | ESP8266 board | v3.0.2 | | esp8266/Arduino@2843a5ac |
| library | ArduinoJson | v6.17.2 | https://arduinojson.org/| bblanchon/ArduinoJson@1360b6a396e096365c917a5518cf29aeac6c9444 |
| library | pubsubclient | v2.8.0 |https://github.com/knolleary/pubsubclient | knolleary/pubsubclient@2d228f2f862a95846c65a8518c79f48dfc8f188c |
| library | WiFiManager | 2.0.15-rc.1 | https://github.com/tzapu/WiFiManager | tzapu/WiFiManager@16bd0f4f3cf63f495b6e4fc14a7a5867eab328a1 |
| library | TelnetStream | v1.2.2 | https://github.com/jandrassy/TelnetStream |jandrassy/TelnetStream@16c07e97ee83f2a094706044d1e24d7fdb1c6265 |
| library | Acetime | v2.0.1 | https://github.com/bxparks/AceTime | bxparks/AceTime@5ad947f3df2af87508772a2157a5d8c52ab3ebf6 |
| library | OneWire | v2.3.6 | https://github.com/PaulStoffregen/OneWire | PaulStoffregen/OneWire@c61ed2389f7b748cda056053da8accbbc580218a |
| library | DallasTemperature | v3.9.0 | https://github.com/milesburton/Arduino-Temperature-Control-Library | milesburton/Arduino-Temperature-Control-Library@d66fec48c33f58af816abe5299863bc444078d6a |

5. Add ESP8266 to Arduino IDE: https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/

6. Compile the project using **Sketch -> Verify**

7. If the ESP8266 isn't already running, connect it to your computer with a MicroUSB lead then upload using **Sketch -> Upload**. 
   Subsequent software updates can be performed by entering **FSexplorer** and selecting **Update Firmware**.

8. After uploading the code you need to upload the files in the data directory. [[How to upload files to LittleFS]]

9. Once you have Arduino working you might wish to install <a href=https://code.visualstudio.com/download>Visual Studio Code</a> and the <a href="https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino">Arduino extension</a>.

_If you want to autogenerate the build numbers (like I do in my IDE). Then add you can find out how I auto increment my projects using some handy scripting. Just check out my other repo on that: https://github.com/rvdbreemen/autoinc-semver_