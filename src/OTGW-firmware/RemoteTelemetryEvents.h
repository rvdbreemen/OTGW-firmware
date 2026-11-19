/*
***************************************************************************
**  Program  : RemoteTelemetryEvents.h
**  TASK-1473 (branch research/cloud-recording only): the capture points of the remote telemetry recorder's events section
**  (RemoteTelemetryCodec.h, SEC_EVT). Each sits next to the matching diagRec capture point (ADR-213), or where the
**  firmware had none: sensors, network, MQTT connection, REST results, telnet keys, webhook, firmware events,
**  the port 25238 bridge. In a build without the remote telemetry recorder they are empty inline functions, so the call
**  sites need no #if. The functions return at once when the recorder is not logging, and never block for more
**  than the recorder's 10 ms lock wait.
***************************************************************************
*/
#pragma once

#include <stddef.h>
#include <stdint.h>

// EV_FW kinds; the same numbers as rtel::FwEventKind in RemoteTelemetryCodec.h, here so call sites build without it.
enum RemoteTelemetryFwKind : uint8_t {
  rtel_FW_PIC_START = 0, rtel_FW_PIC_DONE = 1, rtel_FW_OTA_START = 2, rtel_FW_OTA_END = 3, rtel_FW_FILE_UPLOAD = 4,
  rtel_FW_FILE_DELETE = 5
};

#if HAS_REMOTE_TELEMETRY
void remoteTelemetryEvPicCmd(const char *text, size_t n);
void remoteTelemetryEvOtdCmd(const char *text, size_t n);
void remoteTelemetryEvResponse(const char *text, size_t n);
void remoteTelemetryEvMqttPub(const char *topic, const char *value);
void remoteTelemetryEvMqttIn(const char *topic, const char *value, size_t n);
void remoteTelemetryEvRestIn(uint8_t route, uint8_t method, const char *body, size_t n);
void remoteTelemetryEvRest(uint8_t route, uint8_t method, int status);
void remoteTelemetryEvWeather(float temperature, float humidity, float wind);
void remoteTelemetryEvBle(uint8_t slot, float temperature, float humidity, uint8_t battery, int8_t rssi);
void remoteTelemetryEvSetting(const char *field, const char *value);
void remoteTelemetryEvTimeSync(uint32_t epoch);
void remoteTelemetryEvDallas(uint8_t index, const char *address, float tempC);
void remoteTelemetryEvS0(uint32_t pulses, uint32_t total, uint32_t durationMs);
void remoteTelemetryEvNet(uint8_t kind, int8_t rssi, uint32_t ip);
void remoteTelemetryEvMqttConn(bool up, uint8_t reason);
void remoteTelemetryEvTelnetKey(char key);
void remoteTelemetryEvWebhook(bool state, int status);
void remoteTelemetryEvFw(uint8_t kind, int result, const char *text);
void remoteTelemetryEvMode(uint8_t kind, uint32_t value);
void remoteTelemetryEvBridge(const char *text, size_t n, bool refused);
#else
inline void remoteTelemetryEvPicCmd(const char *, size_t) {}
inline void remoteTelemetryEvOtdCmd(const char *, size_t) {}
inline void remoteTelemetryEvResponse(const char *, size_t) {}
inline void remoteTelemetryEvMqttPub(const char *, const char *) {}
inline void remoteTelemetryEvMqttIn(const char *, const char *, size_t) {}
inline void remoteTelemetryEvRestIn(uint8_t, uint8_t, const char *, size_t) {}
inline void remoteTelemetryEvRest(uint8_t, uint8_t, int) {}
inline void remoteTelemetryEvWeather(float, float, float) {}
inline void remoteTelemetryEvBle(uint8_t, float, float, uint8_t, int8_t) {}
inline void remoteTelemetryEvSetting(const char *, const char *) {}
inline void remoteTelemetryEvTimeSync(uint32_t) {}
inline void remoteTelemetryEvDallas(uint8_t, const char *, float) {}
inline void remoteTelemetryEvS0(uint32_t, uint32_t, uint32_t) {}
inline void remoteTelemetryEvNet(uint8_t, int8_t, uint32_t) {}
inline void remoteTelemetryEvMqttConn(bool, uint8_t) {}
inline void remoteTelemetryEvTelnetKey(char) {}
inline void remoteTelemetryEvWebhook(bool, int) {}
inline void remoteTelemetryEvFw(uint8_t, int, const char *) {}
inline void remoteTelemetryEvMode(uint8_t, uint32_t) {}
inline void remoteTelemetryEvBridge(const char *, size_t, bool) {}
#endif
