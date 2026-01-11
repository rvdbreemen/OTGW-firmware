/*
***************************************************************************  
**  Program  : OTGW-MonUpdateServer.h
**  Modified to work with OTGW Nodoshop Hardware Watchdog
** 
**  This is the ESP8266HTTPUpdateServer.h file 
**  Created and modified by Ivan Grokhotkov, Miguel Angel Ajo, Earle Philhower and many others 
**  see: https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPUpdateServer
**
**  ... and then modified by Willem Aandewiel
**
** License and credits
** Arduino IDE is developed and maintained by the Arduino team. The IDE is licensed under GPL.
**
** ESP8266 core includes an xtensa gcc toolchain, which is also under GPL.
**
***************************************************************************
*/

#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <ESP8266WebServer.h>
#include <WiFiClient.h>

namespace esp8266httpupdateserver {
using namespace esp8266webserver;

template <typename ServerType>
class ESP8266HTTPUpdateServerTemplate
{
  public:
    ESP8266HTTPUpdateServerTemplate(bool serial_debug=false);

    void setup(ESP8266WebServerTemplate<ServerType> *server)
    {
      setup(server, emptyString, emptyString);
    }

    void setup(ESP8266WebServerTemplate<ServerType> *server, const String& path)
    {
      setup(server, path, emptyString, emptyString);
    }

    void setup(ESP8266WebServerTemplate<ServerType> *server, const String& username, const String& password)
    {
      setup(server, "/update", username, password);
    }

    void setup(ESP8266WebServerTemplate<ServerType> *server, const String& path, const String& username, const String& password);

    void updateCredentials(const String& username, const String& password)
    {
      _username = username;
      _password = password;
    }
    
    void setIndexPage(const char *indexPage);
    void setSuccessPage(const char *succesPage);

  protected:
    void _setUpdaterError();
    void _resetStatus();
    void _setStatus(uint8_t phase, const String &target, size_t received, size_t total, const String &filename, const String &error);
    void _sendStatusJson();
    void _jsonEscape(const String &in, char *out, size_t outSize);
    const char *_phaseToString(uint8_t phase);

  private:
    enum UpdatePhase : uint8_t {
      UPDATE_IDLE = 0,
      UPDATE_START,
      UPDATE_WRITE,
      UPDATE_END,
      UPDATE_ERROR,
      UPDATE_ABORT
    };

    struct UpdateStatus {
      UpdatePhase phase;
      String target;
      size_t received;
      size_t total;
      size_t upload_received;
      size_t upload_total;
      size_t flash_written;
      size_t flash_total;
      String filename;
      String error;
    };

    bool _serial_output;
    ESP8266WebServerTemplate<ServerType> *_server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
    const char *_serverIndex;
    const char *_serverSuccess;
    String _savedSettings;
    size_t _lastFeedbackBytes;
    unsigned long _lastFeedbackTime;
    unsigned long _lastDogFeedTime;
    int _lastProgressPerc;
    UpdateStatus _status;
};

};

#include "OTGW-ModUpdateServer-impl.h"


using ESP8266HTTPUpdateServer = esp8266httpupdateserver::ESP8266HTTPUpdateServerTemplate<WiFiServer>;

namespace BearSSL {
using ESP8266HTTPUpdateServerSecure = esp8266httpupdateserver::ESP8266HTTPUpdateServerTemplate<WiFiServerSecure>;
};

namespace axTLS {
using ESP8266HTTPUpdateServerSecure = esp8266httpupdateserver::ESP8266HTTPUpdateServerTemplate<WiFiServerSecure>;
};

#endif
