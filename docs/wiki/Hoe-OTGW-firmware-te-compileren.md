Om de OTGW-firmware te compileren, zijn er verschillende dingen die u nodig heeft. 

1. Installeer Arduino IDE (https://www.arduino.cc/) of Visual Code (https://maker.pro/arduino/tutorial/how-to-use-visual-studio-code-for-arduino). 

2. Haal de bron van Github op naar een lokale map op je computer. 

3. Open ** OTGW-firmware.ino ** in Arduino IDE 

4. Gebruik ** Extra -> Bibliotheken beheren ** om deze aanvullende bibliotheken te verkrijgen: <ul> <li> <a href=https://github.com/ropg/ezTime> ezTime </a> <li> <a href=https://github.com/jandrassy/TelnetStream> TelnetStream </a> <li> <a href=https://arduinojson.org/> ArduinoJson </a> <li> <a href=https://github.com/tzapu/WiFiManager> WiFiManager </a> <li> <a href=https://github.com/knolleary/pubsubclient> Pubsubclient </a> </ul> 

6. Voeg NodeMCU toe aan IDE: https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/ 

7. Schakel de gateway uit, koppel de NodeMCU los en sluit hem vervolgens met een MicroUSB-kabel aan op uw computer. 

8. U zou nu moeten kunnen compileren en uploaden met ** Sketch -> Upload **. 

9. Na het uploaden van de code moet u ook de bestanden uploaden vanuit de datadirectory. [[Hoe bestanden uploaden naar LittleFS]] _Als u de buildnummers automatisch wilt genereren (zoals ik doe in mijn IDE). Voeg vervolgens toe dat u kunt zien hoe ik mijn projecten automatisch vergroot met behulp van een aantal handige scripts. Bekijk gewoon mijn andere repo hierover: https://github.com/rvdbreemen/autoinc-semver_