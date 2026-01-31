Om de OTGW-firmware te compileren, zijn er verschillende dingen die u nodig heeft. 

1. Installeer [Arduino IDE](https://www.arduino.cc/). 

2. Haal de bron van Github op naar een lokale map op je computer. 

3. Open **OTGW-firmware.ino** in Arduino IDE 

4. Gebruik **Extra -> Bibliotheken beheren** om deze aanvullende bibliotheken te verkrijgen: 

| TYPE| NAME| VERSION | REPO |COMMIT|
|-|-|-|-|-|
| package | ESP8266 board | v2.7.4 | | esp8266/Arduino@2843a5ac |
| library | ArduinoJson | v6.17.2 | https://arduinojson.org/| bblanchon/ArduinoJson@1360b6a396e096365c917a5518cf29aeac6c9444 |
| library | pubsubclient | v2.8 |https://github.com/knolleary/pubsubclient | knolleary/pubsubclient@2d228f2f862a95846c65a8518c79f48dfc8f188c |
| library | WiFiManager | v2.04-beta | https://github.com/tzapu/WiFiManager | tzapu/WiFiManager@16bd0f4f3cf63f495b6e4fc14a7a5867eab328a1 |
| library | TelnetStream | v1.2.1 | https://github.com/jandrassy/TelnetStream |jandrassy/TelnetStream@16c07e97ee83f2a094706044d1e24d7fdb1c6265 |
| library | Acetime | v1.9.0 | https://github.com/bxparks/AceTime | bxparks/AceTime@5ad947f3df2af87508772a2157a5d8c52ab3ebf6 |
| library | Timelib | v1.6.1 | https://github.com/PaulStoffregen/Time | PaulStoffregen/Time@a18e50dcea4ee17285d732d39e7bc559482d1d3d |


6. Voeg ESP8266 board toe aan de Adruino IDE, lees hier hoe: https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/ 

7. Schakel de gateway uit, koppel de ESP8266 los en sluit hem vervolgens met een MicroUSB-kabel aan op uw computer. 

8. Complieer de firmware en uploaden het met **Sketch -> Upload**. 

9. Na het uploaden van de code moet u ook de bestanden uploaden vanuit de datadirectory. [[NL:Hoe bestanden uploaden naar LittleFS]] 

10. Zodra je Arduino aan het werk hebt, wil je misschien <a href=https://code.visualstudio.com/download>Visual Studio Code</a> en de <a href="https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino">Arduino-extensie</a> installeren.

_Als u de buildnummers automatisch wilt genereren (zoals ik doe in mijn IDE). Voeg vervolgens toe dat u kunt zien hoe ik mijn projecten automatisch vergroot met behulp van een aantal handige scripts. Bekijk gewoon mijn andere repo hierover: https://github.com/rvdbreemen/autoinc-semver_