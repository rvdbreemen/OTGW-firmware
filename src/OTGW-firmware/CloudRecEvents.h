/*
***************************************************************************
**  Program  : CloudRecEvents.h
**  TASK-1473 (branch research/cloud-recording only): the capture points of the cloud recorder's events section
**  (CloudRecCodec.h, SEC_EVT). Each sits next to the matching diagRec capture point (ADR-213), or where the
**  firmware had none: sensors, network, MQTT connection, REST results, telnet keys, webhook, firmware events,
**  the port 25238 bridge. In a build without the cloud recorder they are empty inline functions, so the call
**  sites need no #if. The functions return at once when the recorder is not logging, and never block for more
**  than the recorder's 10 ms lock wait.
***************************************************************************
*/
#pragma once

#include <stddef.h>
#include <stdint.h>

// EV_FW kinds; the same numbers as crec::FwEventKind in CloudRecCodec.h, here so call sites build without it.
enum CloudRecFwKind : uint8_t {
  crec_FW_PIC_START = 0, crec_FW_PIC_DONE = 1, crec_FW_OTA_START = 2, crec_FW_OTA_END = 3, crec_FW_FILE_UPLOAD = 4,
  crec_FW_FILE_DELETE = 5
};

#if HAS_CLOUD_REC
void cloudRecEvPicCmd(const char *text, size_t n);
void cloudRecEvOtdCmd(const char *text, size_t n);
void cloudRecEvResponse(const char *text, size_t n);
void cloudRecEvMqttPub(const char *topic, const char *value);
void cloudRecEvMqttIn(const char *topic, const char *value, size_t n);
void cloudRecEvRestIn(uint8_t route, uint8_t method, const char *body, size_t n);
void cloudRecEvRest(uint8_t route, uint8_t method, int status);
void cloudRecEvWeather(float temperature, float humidity, float wind);
void cloudRecEvBle(uint8_t slot, float temperature, float humidity, uint8_t battery, int8_t rssi);
void cloudRecEvSetting(const char *field, const char *value);
void cloudRecEvTimeSync(uint32_t epoch);
void cloudRecEvDallas(uint8_t index, const char *address, float tempC);
void cloudRecEvS0(uint32_t pulses, uint32_t total, uint32_t durationMs);
void cloudRecEvNet(uint8_t kind, int8_t rssi, uint32_t ip);
void cloudRecEvMqttConn(bool up, uint8_t reason);
void cloudRecEvTelnetKey(char key);
void cloudRecEvWebhook(bool state, int status);
void cloudRecEvFw(uint8_t kind, int result, const char *text);
void cloudRecEvMode(uint8_t kind, uint32_t value);
void cloudRecEvBridge(const char *text, size_t n, bool refused);
#else
inline void cloudRecEvPicCmd(const char *, size_t) {}
inline void cloudRecEvOtdCmd(const char *, size_t) {}
inline void cloudRecEvResponse(const char *, size_t) {}
inline void cloudRecEvMqttPub(const char *, const char *) {}
inline void cloudRecEvMqttIn(const char *, const char *, size_t) {}
inline void cloudRecEvRestIn(uint8_t, uint8_t, const char *, size_t) {}
inline void cloudRecEvRest(uint8_t, uint8_t, int) {}
inline void cloudRecEvWeather(float, float, float) {}
inline void cloudRecEvBle(uint8_t, float, float, uint8_t, int8_t) {}
inline void cloudRecEvSetting(const char *, const char *) {}
inline void cloudRecEvTimeSync(uint32_t) {}
inline void cloudRecEvDallas(uint8_t, const char *, float) {}
inline void cloudRecEvS0(uint32_t, uint32_t, uint32_t) {}
inline void cloudRecEvNet(uint8_t, int8_t, uint32_t) {}
inline void cloudRecEvMqttConn(bool, uint8_t) {}
inline void cloudRecEvTelnetKey(char) {}
inline void cloudRecEvWebhook(bool, int) {}
inline void cloudRecEvFw(uint8_t, int, const char *) {}
inline void cloudRecEvMode(uint8_t, uint32_t) {}
inline void cloudRecEvBridge(const char *, size_t, bool) {}
#endif
