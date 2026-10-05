/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include "ClientSync.h"
#include <lwip/sockets.h>  // socket options

namespace espMqttClientInternals {

ClientSync::ClientSync()
: client() {
  // empty
}

bool ClientSync::connect(IPAddress ip, uint16_t port) {
  bool ret = client.connect(ip, port);  // implicit conversion of return code int --> bool
  if (ret) {
    #if defined(ARDUINO_ARCH_ESP8266)
    client.setNoDelay(true);
    #elif defined(ARDUINO_ARCH_ESP32)
    // Set TCP option directly to bypass lack of working setNoDelay for WiFiClientSecure (for consistency also here)
    int val = true;
    client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
    #endif
  }
  return ret;
}

bool ClientSync::connect(const char* host, uint16_t port) {
  bool ret = client.connect(host, port);  // implicit conversion of return code int --> bool
  if (ret) {
    #if defined(ARDUINO_ARCH_ESP8266)
    client.setNoDelay(true);
    #elif defined(ARDUINO_ARCH_ESP32)
    // Set TCP option directly to bypass lack of working setNoDelay for WiFiClientSecure (for consistency also here)
    int val = true;
    client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
    #endif
  }
  return ret;
}

size_t ClientSync::write(const uint8_t* buf, size_t size) {
  #if defined(ARDUINO_ARCH_ESP32)
  // OTGW-firmware patch (ADR-186, TASK-1213), offered upstream as
  // https://github.com/bertmelis/espMqttClient/pull/191: never block the caller.
  // NetworkClient::write() waits in select() until a slow broker takes the data, which
  // stalled the Arduino loop task that pumps this client (UseInternalTask::NO) for seconds.
  // Hand the socket only what it accepts now: MqttClient::_sendPacket() adds the count to
  // _bytesSent, and _checkOutbox() stops on 0 and continues on the next loop() call.
  int fd = client.fd();
  if (fd < 0) return 0;
  ssize_t sent = ::send(fd, buf, size, MSG_DONTWAIT);
  if (sent < 0) return 0;  // EAGAIN/EWOULDBLOCK: buffer full; a real error shows up as a disconnect
  return static_cast<size_t>(sent);
  #else
  return client.write(buf, size);
  #endif
}

int ClientSync::read(uint8_t* buf, size_t size) {
  return client.read(buf, size);
}

void ClientSync::stop() {
  client.stop();
}

bool ClientSync::connected() {
  return client.connected();
}

bool ClientSync::disconnected() {
  return !client.connected();
}

}  // namespace espMqttClientInternals

#endif
