/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <PubSubClient.h>           // MQTT client publish and subscribe functionality
#include <ctype.h>
#include <pgmspace.h>
#include "OTGW-Core.h"              // Core OpenTherm data structures and functions

// MQTT Streaming Mode - ALWAYS ENABLED
// Large auto-discovery messages are sent in 128-byte chunks instead of
// requiring a large buffer resize. This prevents heap fragmentation on ESP8266.
// Similar to ESPHome's chunked MQTT publishing strategy.

#define MQTTDebugTln(...) ({ if (state.debug.bMQTT) DebugTln(__VA_ARGS__);    })
#define MQTTDebugln(...)  ({ if (state.debug.bMQTT) Debugln(__VA_ARGS__);    })
#define MQTTDebugTf(...)  ({ if (state.debug.bMQTT) DebugTf(__VA_ARGS__);    })
#define MQTTDebugf(...)   ({ if (state.debug.bMQTT) Debugf(__VA_ARGS__);    })
#define MQTTDebugT(...)   ({ if (state.debug.bMQTT) DebugT(__VA_ARGS__);    })
#define MQTTDebug(...)    ({ if (state.debug.bMQTT) Debug(__VA_ARGS__);    })

void doAutoConfigure();

// Declare some variables within global scope

static IPAddress  MQTTbrokerIP;
static char       MQTTbrokerIPchar[20];
constexpr size_t  MQTT_ID_MAX_LEN = 96;
constexpr size_t  MQTT_NAMESPACE_MAX_LEN = 192;
constexpr size_t  MQTT_TOPIC_MAX_LEN = 200;
constexpr size_t  MQTT_CLIENT_BUFFER_SIZE = 384;
constexpr size_t  MQTT_PROGMEM_STAGE_LEN = 63;
constexpr size_t  MQTT_MSG_MAX_LEN = 1200;
constexpr size_t  MQTT_CFG_LINE_MAX_LEN = 1200;

struct MQTTAutoConfigLineView {
  byte id;
  char *topicTemplate;
  char *msgTemplate;
};

struct MQTTAutoConfigTemplateContext {
  const char *nodeId;
  const char *sensorId;
  const char *hostname;
  const char *version;
  const char *mqttPubTopic;
  const char *mqttSubTopic;
  const char *sourceSuffix;
  const char *sourceName;
  const char *sourceTopicSegment;
};

// MQTT autoconfig scratch buffers (ADR-053 two-buffer design):
//   cMsg[CMSG_SIZE=512] — global general-purpose scratch, reused as sTopic (rendered MQTT
//                          discovery topic, ≤200 bytes) during autoconfig.  cMsg is safe for
//                          sTopic because template pointers (topicTemplate/msgTemplate) point
//                          into sLine, not cMsg.
//   sLine[SLINE_SIZE=1200] — global, exclusively holds raw config file lines (≤900 bytes from
//                          mqttha.cfg).  Only used by MQTT autoconfig; guarded by
//                          mqttAutoConfigInProgress.
// feedWatchDog() is used (not doBackgroundTasks()) inside expandAndPublishSourceTemplates()
// and in the per-line loop to prevent cMsg or sLine from being overwritten by HTTP/MQTT
// callbacks between the three per-source renders within a single iteration (ADR-053).

// Guard shared MQTT autoconfig buffers (cMsg for sTopic, sLine for config lines) against
// accidental re-entry.  Acquiring this lock is the exclusive gate for using sLine; it also
// guarantees cMsg is held for sTopic since feedWatchDog() (not doBackgroundTasks()) is the
// only yield used during autoconfig, so no HTTP/MQTT callback can overwrite cMsg mid-use.
// Current firmware is effectively single-threaded, but this protects future
// callback/timer refactors from clobbering the shared workspace.
// Not volatile: ESP8266 is cooperative single-threaded; no ISR enters this path.
static bool mqttAutoConfigInProgress = false;

struct MQTTAutoConfigSessionLock {
  bool locked = false;
  MQTTAutoConfigSessionLock() {
    if (!mqttAutoConfigInProgress) {
      mqttAutoConfigInProgress = true;
      locked = true;
    }
  }
  ~MQTTAutoConfigSessionLock() {
    if (locked) mqttAutoConfigInProgress = false;
  }
  // Prevent accidental copy/move which would cause double-release or leaked lock.
  MQTTAutoConfigSessionLock(const MQTTAutoConfigSessionLock&) = delete;
  MQTTAutoConfigSessionLock& operator=(const MQTTAutoConfigSessionLock&) = delete;
};

static            PubSubClient MQTTclient(wifiClient);

int8_t            reconnectAttempts = 0;
char              lastMQTTtimestamp[15] = "";

enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_WAIT_CONNECTION_ATTEMPT, MQTT_STATE_WAIT_FOR_RECONNECT, MQTT_STATE_ERROR };
enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

static char       MQTTclientId[MQTT_ID_MAX_LEN];
static char       MQTTPubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       MQTTSubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       NodeId[MQTT_ID_MAX_LEN];

// Trim whitespace on both sides in-place
static void trimInPlace(char *buffer) {
  if (buffer == nullptr) return;
  size_t len = strlen(buffer);
  while (len > 0 && isspace(static_cast<unsigned char>(buffer[len - 1]))) {
    buffer[--len] = '\0';
  }
  size_t start = 0;
  while (buffer[start] != '\0' && isspace(static_cast<unsigned char>(buffer[start]))) {
    start++;
  }
  if (start > 0) {
    memmove(buffer, buffer + start, len - start + 1);
  }
}

static bool parseAutoConfigLine(char *sIn, char del, void *viewPtr) {
  MQTTAutoConfigLineView &view = *static_cast<MQTTAutoConfigLineView*>(viewPtr);
  if (!sIn) return false;
  char *comment = strstr(sIn, "//");
  if (comment) *comment = '\0';

  trimInPlace(sIn);
  if (strlen(sIn) <= 3) return false;

  char *firstDel = strchr(sIn, del);
  if (firstDel == nullptr || firstDel == sIn || *(firstDel + 1) == '\0') return false;
  char *secondDel = strchr(firstDel + 1, del);
  if (secondDel == nullptr || secondDel <= firstDel + 1 || *(secondDel + 1) == '\0') return false;

  *firstDel = '\0';
  *secondDel = '\0';

  trimInPlace(sIn);
  char *topicTemplate = firstDel + 1;
  char *msgTemplate = secondDel + 1;
  trimInPlace(topicTemplate);
  trimInPlace(msgTemplate);

  if (strlen(sIn) == 0 || strlen(topicTemplate) == 0 || strlen(msgTemplate) == 0) return false;

  view.id = static_cast<byte>(strtoul(sIn, nullptr, 10));
  view.topicTemplate = topicTemplate;
  view.msgTemplate = msgTemplate;
  return true;
}

static bool tryGetTemplateReplacement(const char *cursor, const void *ctxPtr, const char *&replacement, size_t &tokenLen)
{
  const MQTTAutoConfigTemplateContext &ctx = *static_cast<const MQTTAutoConfigTemplateContext*>(ctxPtr);
  if (strncmp(cursor, "%mqtt_sub_topic%", 16) == 0) {
    replacement = ctx.mqttSubTopic;
    tokenLen = 16;
    return true;
  }
  if (strncmp(cursor, "%mqtt_pub_topic%", 16) == 0) {
    replacement = ctx.mqttPubTopic;
    tokenLen = 16;
    return true;
  }
  if (strncmp(cursor, "%source_topic_segment%", 22) == 0) {
    replacement = ctx.sourceTopicSegment;
    tokenLen = 22;
    return true;
  }
  if (strncmp(cursor, "%source_suffix%", 15) == 0) {
    replacement = ctx.sourceSuffix;
    tokenLen = 15;
    return true;
  }
  if (strncmp(cursor, "%source_name%", 13) == 0) {
    replacement = ctx.sourceName;
    tokenLen = 13;
    return true;
  }
  if (strncmp(cursor, "%sensor_id%", 11) == 0) {
    replacement = ctx.sensorId;
    tokenLen = 11;
    return true;
  }
  if (strncmp(cursor, "%hostname%", 10) == 0) {
    replacement = ctx.hostname;
    tokenLen = 10;
    return true;
  }
  if (strncmp(cursor, "%node_id%", 9) == 0) {
    replacement = ctx.nodeId;
    tokenLen = 9;
    return true;
  }
  if (strncmp(cursor, "%version%", 9) == 0) {
    replacement = ctx.version;
    tokenLen = 9;
    return true;
  }
  replacement = nullptr;
  tokenLen = 0;
  return false;
}

static bool renderTemplateToBuffer(const char *templateStr, char *dest, size_t destSize, const void *ctxPtr)
{
  const MQTTAutoConfigTemplateContext &ctx = *static_cast<const MQTTAutoConfigTemplateContext*>(ctxPtr);
  if (!templateStr || !dest || destSize == 0) return false;
  dest[0] = '\0';

  size_t destLen = 0;
  const char *cursor = templateStr;
  while (*cursor != '\0') {
    const char *replacement = nullptr;
    size_t tokenLen = 0;
    if (tryGetTemplateReplacement(cursor, &ctx, replacement, tokenLen)) {
      size_t replacementLen = strlen(replacement);
      if (destLen + replacementLen >= destSize) return false;
      memcpy(dest + destLen, replacement, replacementLen);
      destLen += replacementLen;
      dest[destLen] = '\0';
      cursor += tokenLen;
      continue;
    }

    if (destLen + 1 >= destSize) return false;
    dest[destLen++] = *cursor++;
    dest[destLen] = '\0';
  }

  return true;
}

static size_t measureRenderedTemplate(const char *templateStr, const void *ctxPtr)
{
  const MQTTAutoConfigTemplateContext &ctx = *static_cast<const MQTTAutoConfigTemplateContext*>(ctxPtr);
  size_t renderedLen = 0;
  const char *cursor = templateStr;
  while (*cursor != '\0') {
    const char *replacement = nullptr;
    size_t tokenLen = 0;
    if (tryGetTemplateReplacement(cursor, &ctx, replacement, tokenLen)) {
      renderedLen += strlen(replacement);
      cursor += tokenLen;
      continue;
    }

    renderedLen++;
    cursor++;
  }
  return renderedLen;
}

static bool writeMqttChunk(const char *data, size_t len)
{
  const size_t CHUNK_SIZE = 128;
  size_t pos = 0;
  while (pos < len) {
    size_t chunkLen = (len - pos) > CHUNK_SIZE ? CHUNK_SIZE : (len - pos);
    size_t written = MQTTclient.write(reinterpret_cast<const uint8_t*>(data + pos), chunkLen);
    if (written != chunkLen) {
      PrintMQTTError();
      return false;
    }
    pos += chunkLen;
    feedWatchDog();
  }
  return true;
}

static bool writeMqttProgmemChunk(PGM_P data, size_t len)
{
  char stage[MQTT_PROGMEM_STAGE_LEN + 1];
  size_t pos = 0;
  while (pos < len) {
    size_t chunkLen = (len - pos) > MQTT_PROGMEM_STAGE_LEN ? MQTT_PROGMEM_STAGE_LEN : (len - pos);
    for (size_t i = 0; i < chunkLen; i++) {
      stage[i] = static_cast<char>(pgm_read_byte(data + pos + i));
    }
    size_t written = MQTTclient.write(reinterpret_cast<const uint8_t*>(stage), chunkLen);
    if (written != chunkLen) {
      PrintMQTTError();
      return false;
    }
    pos += chunkLen;
    feedWatchDog();
  }
  return true;
}

static bool beginMqttPublish(const char *topic, size_t len, bool retain)
{
  if (!MQTTclient.beginPublish(topic, len, retain)) {
    PrintMQTTError();
    return false;
  }
  return true;
}

static bool sendMQTTTemplateStreaming(const char *topic, const char *templateStr, const void *ctxPtr)
{
  const MQTTAutoConfigTemplateContext &ctx = *static_cast<const MQTTAutoConfigTemplateContext*>(ctxPtr);
  if (!settings.mqtt.bEnable) return false;
  if (!MQTTclient.connected()) { DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return false; }
  if (!isValidIP(MQTTbrokerIP)) { DebugTln(F("Error: MQTT broker IP not valid.")); return false; }
  if (!canPublishMQTT()) return false;

  size_t renderedLen = measureRenderedTemplate(templateStr, ctxPtr);
  MQTTDebugTf(PSTR("Sending MQTT (template streaming): server %s:%d => TopicId [%s] (len=%d bytes)\r\n"),
              settings.mqtt.sBroker, settings.mqtt.iBrokerPort, topic, renderedLen);

  if (!MQTTclient.beginPublish(topic, renderedLen, true)) {
    PrintMQTTError();
    return false;
  }

  const char *cursor = templateStr;
  while (*cursor != '\0') {
    const char *replacement = nullptr;
    size_t tokenLen = 0;
    if (tryGetTemplateReplacement(cursor, &ctx, replacement, tokenLen)) {
      if (!writeMqttChunk(replacement, strlen(replacement))) {
        MQTTclient.endPublish();
        return false;
      }
      cursor += tokenLen;
      continue;
    }

    const char *literalStart = cursor;
    while (*cursor != '\0') {
      if (tryGetTemplateReplacement(cursor, &ctx, replacement, tokenLen)) break;
      cursor++;
    }

    size_t literalLen = static_cast<size_t>(cursor - literalStart);
    if (literalLen > 0 && !writeMqttChunk(literalStart, literalLen)) {
      MQTTclient.endPublish();
      return false;
    }
  }

  if (!MQTTclient.endPublish()) {
    PrintMQTTError();
    return false;
  }

  feedWatchDog();
  return true;
}

static void buildNamespace(char *dest, size_t destSize, const char *base, const char *segment, const char *node) {
  if (!dest) return;
  dest[0] = '\0';
  if (!base || !segment || !node) return;
  strlcpy(dest, base, destSize);
  size_t len = strlen(dest);
  if (len > 0 && dest[len - 1] == '/') dest[len - 1] = '\0';
  strlcat(dest, "/", destSize);
  strlcat(dest, segment, destSize);
  strlcat(dest, "/", destSize);
  strlcat(dest, node, destSize);
}

static size_t copyMQTTPayloadToBuffer(const byte *payload, unsigned int length, char *dest, size_t destSize) {
  if (!dest || destSize == 0) return 0;
  dest[0] = '\0';
  if (!payload || length == 0) return 0;

  const size_t copyLen = (length < (destSize - 1)) ? length : (destSize - 1);
  memcpy(dest, payload, copyLen);
  dest[copyLen] = '\0';
  return copyLen;
}

static bool readMQTTTopicToken(const char *&cursor, char *token, size_t tokenSize) {
  if (!cursor || !token || tokenSize == 0) return false;

  while (*cursor == '/') {
    cursor++;
  }
  if (*cursor == '\0') {
    token[0] = '\0';
    return false;
  }

  size_t len = 0;
  while (*cursor != '\0' && *cursor != '/') {
    if (len < (tokenSize - 1)) {
      token[len++] = *cursor;
    }
    cursor++;
  }
  token[len] = '\0';
  return (len > 0);
}

//set command list
// Move strings to PROGMEM to save RAM
const char s_raw[] PROGMEM = "raw";
const char s_temp[] PROGMEM = "temp";
const char s_on[] PROGMEM = "on";
const char s_level[] PROGMEM = "level";
const char s_function[] PROGMEM = "function";
const char s_empty[] PROGMEM = "";

const char s_cmd_command[] PROGMEM = "command";
const char s_cmd_setpoint[] PROGMEM = "setpoint";
const char s_cmd_constant[] PROGMEM = "constant";
const char s_cmd_outside[] PROGMEM = "outside";
const char s_cmd_hotwater[] PROGMEM = "hotwater";
const char s_cmd_gatewaymode[] PROGMEM = "gatewaymode";
const char s_cmd_setback[] PROGMEM = "setback";
const char s_cmd_maxchsetpt[] PROGMEM = "maxchsetpt";
const char s_cmd_maxdhwsetpt[] PROGMEM = "maxdhwsetpt";
const char s_cmd_maxmodulation[] PROGMEM = "maxmodulation";
const char s_cmd_ctrlsetpt[] PROGMEM = "ctrlsetpt";
const char s_cmd_ctrlsetpt2[] PROGMEM = "ctrlsetpt2";
const char s_cmd_chenable[] PROGMEM = "chenable";
const char s_cmd_chenable2[] PROGMEM = "chenable2";
const char s_cmd_ventsetpt[] PROGMEM = "ventsetpt";
const char s_cmd_temperaturesensor[] PROGMEM = "temperaturesensor";
const char s_cmd_addalternative[] PROGMEM = "addalternative";
const char s_cmd_delalternative[] PROGMEM = "delalternative";
const char s_cmd_unknownid[] PROGMEM = "unknownid";
const char s_cmd_knownid[] PROGMEM = "knownid";
const char s_cmd_priomsg[] PROGMEM = "priomsg";
const char s_cmd_setresponse[] PROGMEM = "setresponse";
const char s_cmd_clearrespons[] PROGMEM = "clearrespons";
const char s_cmd_resetcounter[] PROGMEM = "resetcounter";
const char s_cmd_ignoretransitations[] PROGMEM = "ignoretransitations";
const char s_cmd_overridehb[] PROGMEM = "overridehb";
const char s_cmd_forcethermostat[] PROGMEM = "forcethermostat";
const char s_cmd_voltageref[] PROGMEM = "voltageref";
const char s_cmd_debugptr[] PROGMEM = "debugptr";

// ADR-009: Keep source-template discovery literals in PROGMEM.
const char s_mqtt_source_suffix_token[] PROGMEM = "%source_suffix%";
const char s_mqtt_source_name_token[] PROGMEM = "%source_name%";
const char s_mqtt_source_topic_segment_token[] PROGMEM = "%source_topic_segment%";

// RAM copies of the above tokens — initialized once by initSourceTokens().
// Made module-level static so both doAutoConfigure and doAutoConfigureMsgid share them
// without re-copying from PROGMEM on every call.
static char s_sourceSuffixToken[16];
static char s_sourceNameToken[16];
static char s_sourceTopicSegmentToken[24];

static void initSourceTokens() {
  static bool initialized = false;
  if (initialized) return;
  strncpy_P(s_sourceSuffixToken,       s_mqtt_source_suffix_token,         sizeof(s_sourceSuffixToken) - 1);
  s_sourceSuffixToken[sizeof(s_sourceSuffixToken) - 1] = '\0';
  strncpy_P(s_sourceNameToken,         s_mqtt_source_name_token,           sizeof(s_sourceNameToken) - 1);
  s_sourceNameToken[sizeof(s_sourceNameToken) - 1] = '\0';
  strncpy_P(s_sourceTopicSegmentToken, s_mqtt_source_topic_segment_token,  sizeof(s_sourceTopicSegmentToken) - 1);
  s_sourceTopicSegmentToken[sizeof(s_sourceTopicSegmentToken) - 1] = '\0';
  initialized = true;
}

const char s_mqtt_src_suffix_thermostat[] PROGMEM = "_thermostat";
const char s_mqtt_src_suffix_boiler[] PROGMEM = "_boiler";
const char s_mqtt_src_suffix_gateway[] PROGMEM = "_gateway";
const char s_mqtt_src_key_thermostat[] PROGMEM = "thermostat";
const char s_mqtt_src_key_boiler[] PROGMEM = "boiler";
const char s_mqtt_src_key_gateway[] PROGMEM = "gateway";
const char s_mqtt_src_name_thermostat[] PROGMEM = "Thermostat";
const char s_mqtt_src_name_boiler[] PROGMEM = "Boiler";
const char s_mqtt_src_name_gateway[] PROGMEM = "Gateway";

const char* const mqttSourceKeys[] PROGMEM = {
  s_mqtt_src_key_thermostat,
  s_mqtt_src_key_boiler,
  s_mqtt_src_key_gateway
};

const char* const mqttSourceSuffixes[] PROGMEM = {
  s_mqtt_src_suffix_thermostat,
  s_mqtt_src_suffix_boiler,
  s_mqtt_src_suffix_gateway
};

const char* const mqttSourceNames[] PROGMEM = {
  s_mqtt_src_name_thermostat,
  s_mqtt_src_name_boiler,
  s_mqtt_src_name_gateway
};

const char s_otgw_TT[] PROGMEM = "TT";
const char s_otgw_TC[] PROGMEM = "TC";
const char s_otgw_OT[] PROGMEM = "OT";
const char s_otgw_HW[] PROGMEM = "HW";
const char s_otgw_GW[] PROGMEM = "GW";
const char s_otgw_SB[] PROGMEM = "SB";
const char s_otgw_SH[] PROGMEM = "SH";
const char s_otgw_SW[] PROGMEM = "SW";
const char s_otgw_MM[] PROGMEM = "MM";
const char s_otgw_CS[] PROGMEM = "CS";
const char s_otgw_C2[] PROGMEM = "C2";
const char s_otgw_CH[] PROGMEM = "CH";
const char s_otgw_H2[] PROGMEM = "H2";
const char s_otgw_VS[] PROGMEM = "VS";
const char s_otgw_TS[] PROGMEM = "TS";
const char s_otgw_AA[] PROGMEM = "AA";
const char s_otgw_DA[] PROGMEM = "DA";
const char s_otgw_UI[] PROGMEM = "UI";
const char s_otgw_KI[] PROGMEM = "KI";
const char s_otgw_PM[] PROGMEM = "PM";
const char s_otgw_SR[] PROGMEM = "SR";
const char s_otgw_CR[] PROGMEM = "CR";
const char s_otgw_RS[] PROGMEM = "RS";
const char s_otgw_IT[] PROGMEM = "IT";
const char s_otgw_OH[] PROGMEM = "OH";
const char s_otgw_FT[] PROGMEM = "FT";
const char s_otgw_VR[] PROGMEM = "VR";
const char s_otgw_DP[] PROGMEM = "DP";

struct MQTT_set_cmd_t
{
    PGM_P setcmd;
    PGM_P otgwcmd;
    PGM_P ottype;
};


const MQTT_set_cmd_t setcmds[] PROGMEM = {
  {   s_cmd_command, s_empty, s_raw },
  {   s_cmd_setpoint, s_otgw_TT, s_temp },
  {   s_cmd_constant, s_otgw_TC, s_temp },
  {   s_cmd_outside, s_otgw_OT, s_temp },
  {   s_cmd_hotwater, s_otgw_HW, s_on },  // HW=0 (off), HW=1 (on), HW=P (DHW push), HW=<other> (auto)
  {   s_cmd_gatewaymode, s_otgw_GW, s_on },
  {   s_cmd_setback, s_otgw_SB, s_temp },
  {   s_cmd_maxchsetpt, s_otgw_SH, s_temp },
  {   s_cmd_maxdhwsetpt, s_otgw_SW, s_temp },
  {   s_cmd_maxmodulation, s_otgw_MM, s_level },        
  {   s_cmd_ctrlsetpt, s_otgw_CS, s_temp },        
  {   s_cmd_ctrlsetpt2, s_otgw_C2, s_temp },        
  {   s_cmd_chenable, s_otgw_CH, s_on },        
  {   s_cmd_chenable2, s_otgw_H2, s_on },        
  {   s_cmd_ventsetpt, s_otgw_VS, s_level },
  {   s_cmd_temperaturesensor, s_otgw_TS, s_function },
  {   s_cmd_addalternative, s_otgw_AA, s_function },
  {   s_cmd_delalternative, s_otgw_DA, s_function },
  {   s_cmd_unknownid, s_otgw_UI, s_function },
  {   s_cmd_knownid, s_otgw_KI, s_function },
  {   s_cmd_priomsg, s_otgw_PM, s_function },
  {   s_cmd_setresponse, s_otgw_SR, s_function },
  {   s_cmd_clearrespons, s_otgw_CR, s_function },
  {   s_cmd_resetcounter, s_otgw_RS, s_function },
  {   s_cmd_ignoretransitations, s_otgw_IT, s_function },
  {   s_cmd_overridehb, s_otgw_OH, s_function },
  {   s_cmd_forcethermostat, s_otgw_FT, s_function },
  {   s_cmd_voltageref, s_otgw_VR, s_function },
  {   s_cmd_debugptr, s_otgw_DP, s_function },
} ;

const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);

static int findMQTTSetCommandIndex(const char *topicToken)
{
  if (!topicToken) return -1;

  for (int i = 0; i < nrcmds; i++) {
    PGM_P pSetCmd = (PGM_P)pgm_read_ptr(&setcmds[i].setcmd);
    if (strcasecmp_P(topicToken, pSetCmd) == 0) {
      return i;
    }

    PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[i].otgwcmd);
    if (strlen_P(pOtgwCmd) > 0 && strcasecmp_P(topicToken, pOtgwCmd) == 0) {
      return i;
    }
  }

  return -1;
}

//===========================================================================================
void startMQTT() 
{
  if (!settings.mqtt.bEnable) return;
  
  // Outbound publishes now stream via beginPublish/write/endPublish.
  // Keep only enough client buffer for inbound subscribed topics and payloads.
  MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
  
  stateMQTT = MQTT_STATE_INIT;
  //setup for mqtt discovery
  clearMQTTConfigDone();
  strlcpy(NodeId, CSTR(settings.mqtt.sUniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settings.mqtt.sTopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settings.mqtt.sTopTopic), "set", NodeId);
  handleMQTT(); //initialize the MQTT statemachine
  // handleMQTT(); //then try to connect to MQTT
  // handleMQTT(); //now you should be connected to MQTT ready to send
}

bool bHAcycle = false;

// handles MQTT subscribe incoming stuff
void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  if (state.debug.bMQTT) {
    DebugT(F("Message arrived on topic [")); Debug(topic); Debug(F("] = ["));
    for (unsigned int i = 0; i < length; i++) {
      Debug((char)payload[i]);
    }
    Debug(F("] (")); Debug(length); Debug(F(")")); Debugln(); DebugFlush();
  }  

  //detect home assistant going down...
  char msgPayload[128];
  copyMQTTPayloadToBuffer(payload, length, msgPayload, sizeof(msgPayload));

  // Check if payload was truncated — refuse to forward truncated commands to PIC
  if (length >= sizeof(msgPayload)) {
    DebugTf(PSTR("WARNING: MQTT payload truncated (%u > %u), skipping command\r\n"),
            length, (unsigned int)(sizeof(msgPayload) - 1));
    return;
  }

  if (strcasecmp_P(topic, PSTR("homeassistant/status")) == 0) {
    //incoming message on status, detect going down
    if (!settings.mqtt.bHaRebootDetect) {
      //So if the HA reboot detection is turned of, we will just look for HA going online.
      //This means everytime there is "online" message, we will restart MQTT configuration, including the HA Auto Discovery. 
      bHAcycle = true; 
    }
    if (strcasecmp_P(msgPayload, PSTR("offline")) == 0){
      //home assistant went down
      DebugTln(F("Home Assistant went offline!"));
      bHAcycle = true; //set flag, so it triggers when it goes back online
    } else if ((strcasecmp_P(msgPayload, PSTR("online")) == 0) && bHAcycle){
      DebugTln(F("Home Assistant went online!"));
      bHAcycle = false; //clear flag, so it does not trigger again
      // HA restart does not affect the MQTT broker connection; no reconnect needed.
      // Clearing the discovery bitfield is sufficient: JIT (Path B) re-publishes
      // discovery configs as OpenTherm messages arrive (ADR-041).
      clearMQTTConfigDone();
    } else {
      DebugTf(PSTR("Home Assistant Status=[%s] and HA cycle status [%s]\r\n"), msgPayload, CBOOLEAN(bHAcycle)); 
    }
  }


  // parse the incoming topic and execute commands
  char otgwcmd[51]={0};
  static char topicToken[MQTT_ID_MAX_LEN];
  topicToken[0] = '\0';
  const char *topicCursor = topic;

  //first check toptopic part, it can include the seperator, e.g. "myHome/OTGW" or "OTGW""
  const size_t topTopicLen = strlen(CSTR(settings.mqtt.sTopTopic));
  if (strncmp(topicCursor, CSTR(settings.mqtt.sTopTopic), topTopicLen) != 0) {
    MQTTDebugln(F("MQTT: wrong top topic"));
    return;
  } else {
    //remove the top topic part
    MQTTDebugTf(PSTR("Parsing topic: %s/"), CSTR(settings.mqtt.sTopTopic));
    topicCursor += topTopicLen;
    while (*topicCursor == '/') {
      topicCursor++;
    }
  }
  // naming convention /set/<node id>/<command>
  if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
    MQTTDebugln(F("MQTT: missing 'set' token"));
    return;
  }
  MQTTDebugf(PSTR("%s/"), topicToken);
  if (strcasecmp_P(topicToken, PSTR("set")) == 0) {
    if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
      MQTTDebugln(F("MQTT: missing node-id token"));
      return;
    }
    MQTTDebugf(PSTR("%s/"), topicToken);
    if (strcasecmp(topicToken, NodeId) == 0) {
      if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
        MQTTDebugln(F("MQTT: missing command token"));
        return;
      }
      MQTTDebugf(PSTR("%s"), topicToken);
      if (topicToken[0] != '\0') {
        // --- SAT MQTT commands: set/<nodeId>/sat/<sub-command> ---
        if (strcasecmp_P(topicToken, PSTR("sat")) == 0) {
          char satSubCmd[20];
          if (readMQTTTopicToken(topicCursor, satSubCmd, sizeof(satSubCmd))) {
            MQTTDebugf(PSTR("/%s [%s]\r\n"), satSubCmd, msgPayload);
            if (strcasecmp_P(satSubCmd, PSTR("target")) == 0) {
              satHandleTargetTemp(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("indoor_temp")) == 0) {
              satHandleExternalTemp(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("outdoor_temp")) == 0) {
              satHandleExternalOutdoor(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("enabled")) == 0) {
              satHandleEnabled(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("control_mode")) == 0) {
              satHandleControlMode(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("overshoot_margin")) == 0) {
              updateSetting("SATovershootmargin", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("heating_system")) == 0) {
              updateSetting("SATsystem", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("manufacturer")) == 0) {
              updateSetting("SATmanufacturer", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("max_modulation")) == 0) {
              updateSetting("SATmaxmodulation", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("dhw_setpoint")) == 0) {
              updateSetting("SATdhwsetpoint", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("dhw_enabled")) == 0) {
              updateSetting("SATdhwenabled", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("interval")) == 0) {
              updateSetting("SATinterval", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("preset")) == 0) {
              satHandlePreset(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("ovp_start")) == 0) {
              satOvpStartCalibration();
            } else if (strcasecmp_P(satSubCmd, PSTR("ovp_stop")) == 0) {
              satOvpStopCalibration();
            } else if (strcasecmp_P(satSubCmd, PSTR("ovp_value")) == 0) {
              updateSetting("SATovpvalue", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("ovp_enabled")) == 0) {
              updateSetting("SATovpenabled", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("push_setpoint")) == 0) {
              updateSetting("SATpushsetpoint", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("flame_off_offset")) == 0) {
              updateSetting("SATflameoffset", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("reset_integral")) == 0) {
              satResetIntegral();
            } else if (strcasecmp_P(satSubCmd, PSTR("window")) == 0) {
              bool isOpen = (strcasecmp_P(msgPayload, PSTR("open")) == 0 ||
                            strcasecmp_P(msgPayload, PSTR("1")) == 0 ||
                            strcasecmp_P(msgPayload, PSTR("ON")) == 0);
              satHandleWindow(isOpen);
            } else if (strcasecmp_P(satSubCmd, PSTR("force_pwm")) == 0) {
              updateSetting("SATforcepwm", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("flow_offset")) == 0) {
              updateSetting("SATflowoffset", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("summer_simmer")) == 0) {
              updateSetting("SATsummersimmer", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("summer_threshold")) == 0) {
              updateSetting("SATsummerthreshold", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("summer_min_hours")) == 0) {
              updateSetting("SATsummerminhours", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("humidity")) == 0) {
              satHandleHumidity(msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("comfort_adjust")) == 0) {
              updateSetting("SATcomfortadjust", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("comfort_humidity")) == 0) {
              updateSetting("SATcomforthumidity", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("comfort_max_offset")) == 0) {
              updateSetting("SATcomfortmaxoffset", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("simulation")) == 0) {
              updateSetting("SATsimulation", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("ble_enable")) == 0) {
              updateSetting("SATbleenable", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("ble_mac")) == 0) {
              updateSetting("SATblemac", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("ble_interval")) == 0) {
              updateSetting("SATbleinterval", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("preset_sync")) == 0) {
              updateSetting("SATpresetsync", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("preset_sync_topic")) == 0) {
              updateSetting("SATpresetsynctopic", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("multi_area")) == 0) {
              updateSetting("SATmultiarea", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("auto_tune")) == 0) {
              updateSetting("SATautotune", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("auto_tune_rate")) == 0) {
              updateSetting("SATautotunerate", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("multi_area_count")) == 0) {
              updateSetting("SATmultiareacount", msgPayload);
            } else if (strcasecmp_P(satSubCmd, PSTR("area")) == 0) {
              // sat/area/<index> — push area temperature
              char areaIdx[4];
              if (readMQTTTopicToken(topicCursor, areaIdx, sizeof(areaIdx))) {
                int idx = atoi(areaIdx);
                if (idx >= 0 && idx < 4) {
                  satHandleAreaTemp((uint8_t)idx, msgPayload);
                } else {
                  MQTTDebugTf(PSTR("SAT: area index out of range [%s]\r\n"), areaIdx);
                }
              }
            } else {
              MQTTDebugTf(PSTR("SAT: unknown sub-command [%s]\r\n"), satSubCmd);
            }
          } else {
            MQTTDebugln(F(" SAT: missing sub-command"));
          }
          return;
        }
        if (!hasOTCommandInterface()) {
          MQTTDebugln(F(" MQTT command ignored: no OT command interface detected"));
          return;
        }
        const int cmdIndex = findMQTTSetCommandIndex(topicToken);
        if (cmdIndex >= 0) {
          PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[cmdIndex].otgwcmd);
          PGM_P pOtType = (PGM_P)pgm_read_ptr(&setcmds[cmdIndex].ottype);

          if (pOtType == s_raw){
            //raw command
            snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s"), msgPayload);
            MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
            addCommandToQueue(otgwcmd, strlen(otgwcmd), true);
          } else {
            //all other commands are <otgwcmd>=<payload message>
            // Copy command string from Flash to temp buffer for snprintf
            char cmdBuf[10];
            strncpy_P(cmdBuf, pOtgwCmd, sizeof(cmdBuf));
            cmdBuf[sizeof(cmdBuf)-1] = 0; // Ensure null termination

            snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), cmdBuf, msgPayload);
            MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
            addCommandToQueue(otgwcmd, strlen(otgwcmd), true);
          }
        } else {
          //no match found
          MQTTDebugln();
          MQTTDebugTf(PSTR("No match found for command: [%s]\r\n"), topicToken);
        }
      }
    }
  }
}

//===========================================================================================
// sendMQTT - streaming version for large messages (auto-discovery)
//===========================================================================================
// sendMQTT - streaming version for large messages (auto-discovery)
// Sends the message in small chunks to avoid buffer reallocation
// This prevents heap fragmentation on ESP8266
//===========================================================================================
void sendMQTT(const char* topic, const char *json);

// Forward declaration; implementation is provided later in this file
void sendMQTTStreaming(const char* topic, const char *json, const size_t len);

void handleMQTT() 
{  
  if (!settings.mqtt.bEnable) return;
  DECLARE_TIMER_SEC(timerMQTTwaitforconnect, 42, CATCH_UP_MISSED_TICKS);   // wait before trying to connect again
  DECLARE_TIMER_SEC(timerMQTTwaitforretry, 3, CATCH_UP_MISSED_TICKS);     // wait for retry

  //State debug timers
  DECLARE_TIMER_SEC(timerMQTTdebugwaitforreconnect, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugerrorstate, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugwaitconnectionattempt, 1);
  DECLARE_TIMER_SEC(timerMQTTdebugisconnected, 60);
  
  if (MQTTclient.connected()) MQTTclient.loop();  //always do a MQTTclient.loop() first

  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
      MQTTDebugTln(F("MQTT State: MQTT Initializing")); 
      WiFi.hostByName(CSTR(settings.mqtt.sBroker), MQTTbrokerIP);  // lookup the MQTTbroker convert to IP
      snprintf_P(MQTTbrokerIPchar, sizeof(MQTTbrokerIPchar), PSTR("%d.%d.%d.%d"), MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        MQTTDebugTf(PSTR("[%s] => setServer(%s, %d)\r\n"), CSTR(settings.mqtt.sBroker), MQTTbrokerIPchar, settings.mqtt.iBrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settings.mqtt.iBrokerPort);
        MQTTclient.setCallback(handleMQTTcallback);
        MQTTclient.setSocketTimeout(15);  // Increased from 4 to 15 seconds for better stability
        MQTTclient.setKeepAlive(60);      // Set to 60 seconds (default was 15) to reduce reconnections
        uint8_t mac[6]{0};
        WiFi.macAddress(mac);
        snprintf_P(MQTTclientId, sizeof(MQTTclientId), PSTR("%s%02X%02X%02X%02X%02X%02X"), _HOSTNAME, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        //skip try to connect
        reconnectAttempts =0;
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        MQTTDebugTf(PSTR("ERROR: [%s] => is not a valid URL\r\n"), CSTR(settings.mqtt.sBroker));
        stateMQTT = MQTT_STATE_ERROR;
        //DebugTln(F("Next State: MQTT_STATE_ERROR"));
      }    
      RESTART_TIMER(timerMQTTwaitforconnect); 
    break;

    case MQTT_STATE_TRY_TO_CONNECT:
      MQTTDebugTln(F("MQTT State: MQTT try to connect"));
      MQTTDebugTf(PSTR("MQTT server is [%s], IP[%s]\r\n"), settings.mqtt.sBroker, MQTTbrokerIPchar);
      
      MQTTDebugT(F("Attempting MQTT connection .. "));
      reconnectAttempts++;

      //If no username, then anonymous connection to broker, otherwise assume username/password.
      if (strlen(settings.mqtt.sUser) == 0) 
      {
        MQTTDebug(F("without a Username/Password "));
        if(!MQTTclient.connect(MQTTclientId, MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      } 
      else 
      {
        MQTTDebugf(PSTR("Username [%s] "), CSTR(settings.mqtt.sUser));
        if(!MQTTclient.connect(MQTTclientId, CSTR(settings.mqtt.sUser), CSTR(settings.mqtt.sPasswd), MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      }

      //If connection was made succesful, move on to next state...
      if (MQTTclient.connected())
      {
        reconnectAttempts = 0;  
        MQTTDebugln(F(" .. connected\r"));
        Debugln(F("MQTT connected"));	
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        MQTTDebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
        // birth message, sendMQTT retains  by default
        sendMQTT(MQTTPubNamespace, "online");

        // Reset discovery state on every connect so JIT (Path B) re-publishes
        // discovery configs as OpenTherm messages arrive. This handles both
        // normal reconnects and broker restarts (lost retained messages) without
        // flooding the broker with configs for message IDs never seen in the wild.
        clearMQTTConfigDone();
        requestMQTTRepublishAll();

        //Subscribe to topics
        char topic[MQTT_TOPIC_MAX_LEN];
        strlcpy(topic, MQTTSubNamespace, sizeof(topic));
        strlcat(topic, "/#", sizeof(topic));
        MQTTDebugTf(PSTR("Subscribe to MQTT: TopicId [%s]\r\n"), topic);
        if (MQTTclient.subscribe(topic)){
          MQTTDebugTf(PSTR("MQTT: Subscribed successfully to TopicId [%s]\r\n"), topic);
        }
        else
        {
          MQTTDebugTf(PSTR("MQTT: Subscribe TopicId [%s] FAILED! \r\n"), topic);
          PrintMQTTError();
        }
        MQTTclient.subscribe("homeassistant/status"); //start monitoring the status of homeassistant, if it goes down, then force a restart after it comes back online.
        sendMQTTversioninfo();
        publishAllPICsettings();  // Republish any cached PIC settings on (re)connect
      }
      else
      { // no connection, try again, do a non-blocking wait for 3 seconds.
        MQTTDebugln(F(" .. \r"));
        MQTTDebugTf(PSTR("failed, retrycount=[%d], rc=[%d] ..  try again in 3 seconds\r\n"), reconnectAttempts, MQTTclient.state());
        RESTART_TIMER(timerMQTTwaitforretry);
        stateMQTT = MQTT_STATE_WAIT_CONNECTION_ATTEMPT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_CONNECTION_ATTEMPT"));
      }
      
      //After 5 attempts... go wait for a while.
      if (reconnectAttempts >= 5)
      {
        MQTTDebugTln(F("5 attempts have failed. Retry wait for next reconnect in 10 minutes\r"));
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }   
    break;
    
    case MQTT_STATE_IS_CONNECTED:
      if DUE(timerMQTTdebugisconnected) MQTTDebugTln(F("MQTT State: MQTT is Connected"));
      if (MQTTclient.connected()) 
      { //if the MQTT client is connected, then please do a .loop call...
        MQTTclient.loop();
      }
      else
      { //else go and wait 10 minutes, before trying again.
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }  
    break;

    case MQTT_STATE_WAIT_CONNECTION_ATTEMPT:
      //do non-blocking wait for 3 seconds
      if  DUE(timerMQTTdebugwaitconnectionattempt) MQTTDebugTln(F("MQTT State: MQTT_WAIT_CONNECTION_ATTEMPT"));
      if (DUE(timerMQTTwaitforretry))
      {
        //Try again... after waitforretry non-blocking delay
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;
    
    case MQTT_STATE_WAIT_FOR_RECONNECT:
      //do non-blocking wait for 10 minutes, then try to connect again. 
      if DUE(timerMQTTdebugwaitforreconnect) MQTTDebugTln(F("MQTT State: MQTT wait for reconnect"));
      if (DUE(timerMQTTwaitforconnect))
      {
        //remember when you tried last time to reconnect
        RESTART_TIMER(timerMQTTwaitforretry);
        reconnectAttempts = 0; 
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;

    case MQTT_STATE_ERROR:
      if DUE(timerMQTTdebugerrorstate) MQTTDebugTln(F("MQTT State: MQTT ERROR, wait for 10 minutes, before trying again"));
      //wait for next retry
      RESTART_TIMER(timerMQTTwaitforconnect);
      stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
      MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
    break;

    default:
      MQTTDebugTln(F("MQTT State: default, this should NEVER happen!"));
      //do nothing, this state should not happen
      stateMQTT = MQTT_STATE_INIT;
      DebugTln(F("Next State: MQTT_STATE_INIT"));
    break;
  }
  state.mqtt.bConnected = MQTTclient.connected();
  if (state.mqtt.bConnected) state.mqtt.iLastConnectedMs = millis();
} // handleMQTT()

void PrintMQTTError(){
  MQTTDebugln();
  switch (MQTTclient.state())
  {
    case MQTT_CONNECTION_TIMEOUT     : MQTTDebugTln(F("Error: MQTT connection timeout"));break;
    case MQTT_CONNECTION_LOST        : MQTTDebugTln(F("Error: MQTT connections lost"));break;
    case MQTT_CONNECT_FAILED         : MQTTDebugTln(F("Error: MQTT connection failed"));break;
    case MQTT_DISCONNECTED           : MQTTDebugTln(F("Error: MQTT disconnected"));break;
    case MQTT_CONNECTED              : MQTTDebugTln(F("Error: MQTT connected"));break;
    case MQTT_CONNECT_BAD_PROTOCOL   : MQTTDebugTln(F("Error: MQTT connect bad protocol"));break;
    case MQTT_CONNECT_BAD_CLIENT_ID  : MQTTDebugTln(F("Error: MQTT connect bad client id"));break;
    case MQTT_CONNECT_UNAVAILABLE    : MQTTDebugTln(F("Error: MQTT connect unavailable"));break;
    case MQTT_CONNECT_BAD_CREDENTIALS: MQTTDebugTln(F("Error: MQTT connect bad credentials"));break;
    case MQTT_CONNECT_UNAUTHORIZED   : MQTTDebugTln(F("Error: MQTT connect unauthorized"));break;
    default: MQTTDebugTln(F("Error: MQTT unknown error"));
  }
}

/* 
  topic:  <string> , sensor topic, will be automatically prefixed with <mqtt topic>/value/<node_id>
  json:   <string> , payload to send
  retain: <bool> , retain mqtt message  
*/
void sendMQTTData(const char* topic, const char *json, const bool retain) 
{
  if (!settings.mqtt.bEnable) return;
  if (!mqttPublishAllowed) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  char full_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topic, sizeof(full_topic));
  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n"), settings.mqtt.sBroker, settings.mqtt.iBrokerPort, full_topic, json);
  const size_t payloadLen = strlen(json);
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return;
  if (!writeMqttChunk(json, payloadLen)) {
    MQTTclient.endPublish();
    return;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return; }
  // Publish succeeded — confirm any pending throttle slot updates
  confirmMQTTPublishSlot();
  confirmMQTTPublishBitSlot();
  confirmMQTTPublishByteSlot();
  feedWatchDog();//feed the dog
} // sendMQTTData()

void sendMQTTData(const __FlashStringHelper *topic, const char *json, const bool retain)
{
  char topicBuf[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  sendMQTTData(topicBuf, json, retain);
}

void sendMQTTData(const __FlashStringHelper *topic, const __FlashStringHelper *json, const bool retain)
{
  if (!settings.mqtt.bEnable) return;
  if (!mqttPublishAllowed) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;}
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;}
  if (!canPublishMQTT()) return;

  char topicBuf[MQTT_TOPIC_MAX_LEN];
  char full_topic[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topicBuf, sizeof(full_topic));

  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message ["),
              settings.mqtt.sBroker,
              settings.mqtt.iBrokerPort,
              full_topic);
  MQTTDebug(json);
  MQTTDebugln(F("]"));

  PGM_P payload = reinterpret_cast<PGM_P>(json);
  const size_t payloadLen = strlen_P(payload);
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return;
  if (!writeMqttProgmemChunk(payload, payloadLen)) {
    MQTTclient.endPublish();
    return;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return; }
  confirmMQTTPublishSlot();
  confirmMQTTPublishBitSlot();
  confirmMQTTPublishByteSlot();
  feedWatchDog();
}

//===================[ Send useful information to MQTT ]======================

void sendMQTTuptime(){
  DebugTf(PSTR("Uptime seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
  char uptimeBuf[11] = {0};
  snprintf_P(uptimeBuf, sizeof(uptimeBuf), PSTR("%lu"), (unsigned long)state.uptime.iSeconds);
  sendMQTTData(F("otgw-firmware/uptime"), uptimeBuf, false);
}

/*
Publish usefull firmware version information to MQTT broker.
*/
void sendMQTTversioninfo(){
  char rebootCountBuf[12];
  snprintf_P(rebootCountBuf, sizeof(rebootCountBuf), PSTR("%lu"), static_cast<unsigned long>(state.uptime.iRebootCount));
  sendMQTTData("otgw-firmware/version", _SEMVER_FULL);
  sendMQTTData("otgw-firmware/reboot_count", rebootCountBuf);
  sendMQTTData("otgw-firmware/reboot_reason", lastReset);
  if (isPICEnabled()) {
    sendMQTTData("otgw-pic/version", state.pic.sFwversion);
    sendMQTTData("otgw-pic/deviceid", state.pic.sDeviceid);
    sendMQTTData("otgw-pic/firmwaretype", state.pic.sType);
  }
  sendMQTTData("otgw-pic/picavailable", CCONOFF(state.pic.bAvailable));

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  // OT-direct (OTGW32) status — parallel to otgw-pic/ topics
  sendMQTTData(F("otgw-otdirect/available"), CCONOFF(isOTDirectEnabled()));
  if (isOTDirectEnabled()) {
    // Operating mode (text string for HA)
    {
      const char* modeStr = "gateway";
      if (state.otd.eMode == OTD_MODE_MONITOR) modeStr = "monitor";
      else if (state.otd.eMode == OTD_MODE_BYPASS) modeStr = "bypass";
      else if (state.otd.eMode == OTD_MODE_MASTER) modeStr = "master";
      else if (state.otd.eMode == OTD_MODE_LOOPBACK) modeStr = "loopback";
      sendMQTTData(F("otgw-otdirect/mode"), modeStr);
    }
    sendMQTTData(F("otgw-otdirect/bypass"), CCONOFF(state.otd.bBypassActive));
    sendMQTTData(F("otgw-otdirect/monitor_mode"), CCONOFF(state.otd.bMonitorMode));
    sendMQTTData(F("otgw-otdirect/master_mode"), CCONOFF(state.otd.bMasterMode));
    sendMQTTData(F("otgw-otdirect/stepup"), CCONOFF(state.otd.bStepUpEnabled));
    sendMQTTData(F("otgw-otdirect/thermostat_connected"), CCONOFF(state.otd.bThermostatConnected));
    sendMQTTData(F("otgw-otdirect/setback_active"), CCONOFF(state.otd.bSetbackActive));
    char buf[8];
    snprintf_P(buf, sizeof(buf), PSTR("%u"), state.otd.iScheduleActive);
    sendMQTTData(F("otgw-otdirect/schedule_active"), buf);
    snprintf_P(buf, sizeof(buf), PSTR("%u"), state.otd.iScheduleDisabled);
    sendMQTTData(F("otgw-otdirect/schedule_disabled"), buf);
    snprintf_P(buf, sizeof(buf), PSTR("%u"), state.otd.iOverrideCount);
    sendMQTTData(F("otgw-otdirect/overrides_active"), buf);
  }
#endif

  // Hardware platform info
  sendMQTTData(F("otgw-firmware/board"), boardName());
  sendMQTTData(F("otgw-firmware/hardware_mode"), hardwareModeName());
  sendMQTTData(F("otgw-firmware/network_mode"), networkModeName());
}

static void publishBoilerConnectedState()
{
  sendMQTTData(F("boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  }
#endif
}

static void publishThermostatConnectedState()
{
  sendMQTTData(F("thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  }
#endif
}

static void publishOTGWConnectedState()
{
  sendMQTTData(F("otgw_connected"), CCONOFF(state.otBus.bOnline));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/otgw_connected"), CCONOFF(state.otBus.bOnline));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/ot_online"), CCONOFF(state.otBus.bOnline));
  }
#endif
  sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otBus.bOnline));
}

/*
Publish state information of PIC firmware version information to MQTT broker.
*/
void sendMQTTstateinformation(){
  publishBoilerConnectedState();
  publishThermostatConnectedState();
  if (state.otBus.bGatewayModeKnown) {
    sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(state.otBus.bGatewayMode));
  }
  publishOTGWConnectedState();
}

/*
* topic:  <string> , topic will be used as is (no prefixing), retained = true
* json:   <string> , payload to send
*/
//===========================================================================================
// Streaming version - sends message in 128-byte chunks to avoid buffer reallocation
// This prevents heap fragmentation on ESP8266 (similar to ESPHome's approach)
void sendMQTT(const char* topic, const char *json) {
  sendMQTTStreaming(topic, json, strlen(json));
}

void sendMQTTStreaming(const char* topic, const char *json, const size_t len) 
{
  if (!settings.mqtt.bEnable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  MQTTDebugTf(PSTR("Sending MQTT (streaming): server %s:%d => TopicId [%s] (len=%d bytes)\r\n"), 
              settings.mqtt.sBroker, settings.mqtt.iBrokerPort, topic, len);

  // Use beginPublish which tells PubSubClient the total length upfront
  // This allows it to use its buffer efficiently without reallocation
  if (!beginMqttPublish(topic, len, true)) return;

  // Write message in small chunks to avoid buffer overflow
  // PubSubClient's write() method handles buffering internally
  const size_t CHUNK_SIZE = 128; // Small chunks fit comfortably in 256-byte buffer
  size_t pos = 0;
  
  while (pos < len) {
    size_t chunkLen = (len - pos) > CHUNK_SIZE ? CHUNK_SIZE : (len - pos);
    
    // Write chunk as a bulk block instead of byte-by-byte
    size_t written = MQTTclient.write((const uint8_t*)(json + pos), chunkLen);
    if (written != chunkLen) {
      PrintMQTTError();
      MQTTclient.endPublish(); // Clean up even on error
      return;
    }
    
    pos += chunkLen;
    feedWatchDog(); // Feed watchdog during long write operations
  }
  
  if (!MQTTclient.endPublish()) {
    PrintMQTTError();
  }

  feedWatchDog();
} // sendMQTTStreaming()

//===========================================================================================
// Helper functions to reduce duplicated MQTT topic building patterns
//===========================================================================================

/**
 * Publish ON/OFF value to MQTT topic
 * Reduces duplicate pattern of boolean-to-string conversion
 */
void publishMQTTOnOff(const char* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
}

void publishMQTTOnOff(const __FlashStringHelper* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
}

/**
 * Publish numeric value as string to MQTT topic
 * Reduces duplicate pattern of number-to-string conversion with static buffer
 */
void publishMQTTNumeric(const char* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

void publishMQTTNumeric(const __FlashStringHelper* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

/**
 * Publish integer value as string to MQTT topic
 */
void publishMQTTInt(const char* topic, int value) {
  static char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

void publishMQTTInt(const __FlashStringHelper* topic, int value) {
  static char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

// Shared source mapping for source-separated MQTT topics / HA discovery expansion.
// Returns false for parity errors / unknown types (caller should not publish).
static bool resolveSourceIndex(byte rsptype, uint8_t &sourceIndex) {
  switch (rsptype) {
    case OTGW_THERMOSTAT:        sourceIndex = 0; return true;
    case OTGW_BOILER:            sourceIndex = 1; return true;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT: sourceIndex = 2; return true;
    default: return false;
  }
}

static bool copySourceTableEntry(const char* const table[], uint8_t sourceIndex, char *dest, size_t destSize)
{
  if (!dest || destSize == 0 || sourceIndex >= 3) return false;
  PGM_P pValue = (PGM_P)pgm_read_ptr(&table[sourceIndex]);
  if (!pValue) { dest[0] = '\0'; return false; }
  strncpy_P(dest, pValue, destSize - 1);
  dest[destSize - 1] = '\0';
  return true;
}

void publishToSourceTopic(const char* topic, const char* json, byte rsptype)
{
  if (!settings.mqtt.bSeparateSources || !topic || !json) return;
  // Re-entrancy guard: sendMQTTData may yield via feedWatchDog, allowing
  // a second processOT call to overwrite the static buffer mid-publish.
  static bool inUse = false;
  if (inUse) return;
  inUse = true;
  uint8_t sourceIndex = 0;
  if (!resolveSourceIndex(rsptype, sourceIndex)) { inUse = false; return; }
  static char sourceTopic[MQTT_TOPIC_MAX_LEN];
  char sourceKeyBuf[16];
  if (!copySourceTableEntry(mqttSourceKeys, sourceIndex, sourceKeyBuf, sizeof(sourceKeyBuf))) { inUse = false; return; }
  snprintf(sourceTopic, sizeof(sourceTopic), "%s/%s", topic, sourceKeyBuf);
  sendMQTTData(sourceTopic, json, false);
  inUse = false;
}

//===========================================================================================
// resetMQTTBufferSize() - fixed inbound buffer strategy
//
// INTENTIONALLY A NO-OP.
//
// Current strategy:
// - PubSubClient buffer is allocated once at startup
// - Outbound publishes stream via beginPublish/write/endPublish
// - Buffer is never resized at runtime, avoiding heap churn
// - Function is kept for API compatibility with existing discovery call sites
//
// Memory impact: fixed MQTT client buffer of MQTT_CLIENT_BUFFER_SIZE bytes.
//===========================================================================================
void resetMQTTBufferSize()
{
  if (!settings.mqtt.bEnable) return;
  // Intentionally empty - buffer is fixed for app lifetime
}
//===========================================================================================
bool getMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  uint32_t result = bitRead(MQTTautoConfigMap[group], index);
  MQTTDebugTf(PSTR("Reading bit %d from group %d for MSGid %d: result = %d\r\n"), index, group, MSGid, result);
  if (result > 0) {
    return true;
  } else {
    return false;
  }
}
//===========================================================================================
void setMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  MQTTDebugTf(PSTR("Setting bit %d from group %d for MSGid %d\r\n"), index, group, MSGid);
  MQTTDebugTf(PSTR("Value before setting bit %d\r\n"), MQTTautoConfigMap[group]);
  bitSet(MQTTautoConfigMap[group], index);
  MQTTDebugTf(PSTR("Value after setting bit  %d\r\n"), MQTTautoConfigMap[group]);
}
//===========================================================================================
void clearMQTTConfigDone()
{
  memset(MQTTautoConfigMap, 0, sizeof(MQTTautoConfigMap));
}
//===========================================================================================
// expandAndPublishSourceTemplates()
// Expands a source-template discovery line into 3 per-source variants and publishes each.
// Renders topic variants into renderedTopic (cMsg global, passed as sTopic pointer) and
// stream-renders the JSON payload from the original template (in sLine global).
// feedWatchDog() is used (not doBackgroundTasks()) between per-source publishes to prevent
// cMsg from being overwritten by HTTP/MQTT callbacks mid-iteration: topicTemplate/msgTemplate
// point into sLine; renderedTopic points into cMsg — keeping the two buffers disjoint.
// Returns true if at least one variant was successfully published.
static bool expandAndPublishSourceTemplates(byte msgid,
                                           const char *logLabel,
                                           const char *topicTemplate,
                                           const char *msgTemplate,
                                           const void *baseCtxPtr,
                                           char *renderedTopic)
{
  const MQTTAutoConfigTemplateContext &baseCtx = *static_cast<const MQTTAutoConfigTemplateContext*>(baseCtxPtr);
  if (!topicTemplate || !msgTemplate || !renderedTopic) return false;

  bool published = false;
  for (uint8_t i = 0; i < 3; i++) {
    char srcSuffixBuf[16];
    char srcKeyBuf[16];
    char srcNameBuf[16];
    if (!copySourceTableEntry(mqttSourceSuffixes, i, srcSuffixBuf, sizeof(srcSuffixBuf))) continue;
    if (!copySourceTableEntry(mqttSourceKeys,     i, srcKeyBuf,    sizeof(srcKeyBuf)))    continue;
    if (!copySourceTableEntry(mqttSourceNames,    i, srcNameBuf,   sizeof(srcNameBuf)))   continue;
    MQTTAutoConfigTemplateContext variantCtx = baseCtx;
    variantCtx.sourceSuffix = srcSuffixBuf;
    variantCtx.sourceName = srcNameBuf;
    variantCtx.sourceTopicSegment = srcKeyBuf;
    if (!renderTemplateToBuffer(topicTemplate, renderedTopic, MQTT_TOPIC_MAX_LEN, &variantCtx)) continue;
    MQTTDebugTf(PSTR("MQTT source discovery (%s) msgid %d -> %s\r\n"), logLabel, (int)msgid, renderedTopic);
    if (!sendMQTTTemplateStreaming(renderedTopic, msgTemplate, &variantCtx)) continue;
    published = true;
    feedWatchDog();
  }
  return published;
}
//===========================================================================================
void doAutoConfigure(){
  // Force-publishes HA discovery configs for all message IDs in mqttha.cfg.
  // Intended only as an explicit utility (Serial 'F', REST API); never called
  // automatically. Use shared static buffers (zero heap allocation).
  // Lock scope is limited to the file-parse/publish loop so it is released
  // before configSensors() is called.  configSensors() -> sensorAutoConfigure()
  // -> doAutoConfigureMsgid() can then acquire the lock independently.

  if (!settings.mqtt.bEnable) return;

  {
    // Lock released at end of this block, before configSensors() is called.
    MQTTAutoConfigSessionLock sessionLock;
    if (!sessionLock.locked) {
      MQTTDebugTln(F("MQTT autoconfig already running, skipping doAutoConfigure()"));
      return;
    }

    char *sTopic = cMsg;                         // cMsg reused as rendered topic (≤200 bytes, fits in CMSG_SIZE)
    initSourceTokens();
    bool sourceTemplateSchemaLogged = false;

    // 2. Open File ONCE
    LittleFS.begin();
    if (!LittleFS.exists(F("/mqttha.cfg"))) {
      DebugTln(F("Error: configuration file not found.")); 
      return;
    } 

    File fh = LittleFS.open(F("/mqttha.cfg"), "r");
    if (!fh) {
      DebugTln(F("Error: could not open configuration file.")); 
      return;
    } 

    byte lineID = 0;
    
    // 3. Loop through file once
    while (fh.available()) {
      feedWatchDog(); // Keep the dog happy during IO
      
      // Read line into sLine (the dedicated global config-line buffer, guarded by mqttAutoConfigInProgress)
      size_t len = fh.readBytesUntil('\n', sLine, SLINE_SIZE - 1);
      sLine[len] = '\0';
      MQTTAutoConfigLineView lineView;
      
      // Parse Line
      if (!parseAutoConfigLine(sLine, ';', &lineView)) continue;
      lineID = lineView.id;

      // 4. Decision: Do we send this line?
      // Skip Dallas sensors - they need per-sensor addresses from configSensors()
      if (lineID == OTGWdallasdataid) continue;
      // Skip PIC-specific discovery entries when no PIC is detected.
      // Relies on the "otgw-pic/" topic prefix convention — update if topics are renamed.
      if (!isPICEnabled() && strstr_P(sLine, PSTR("otgw-pic/"))) continue;
      // Skip OT-direct discovery entries when OT-direct is not active (ESP8266/PIC builds).
      if (!isOTDirectEnabled() && strstr_P(sLine, PSTR("otgw-otdirect/"))) continue;

      {

         MQTTDebugTf(PSTR("Processing AutoConfig for ID %d\r\n"), lineID);

         MQTTAutoConfigTemplateContext renderCtx;
         renderCtx.nodeId = NodeId;
         renderCtx.sensorId = "";
         renderCtx.hostname = CSTR(settings.sHostname);
         renderCtx.version = _VERSION;
         renderCtx.mqttPubTopic = MQTTPubNamespace;
         renderCtx.mqttSubTopic = MQTTSubNamespace;

         if (!renderTemplateToBuffer(lineView.topicTemplate, sTopic, MQTT_TOPIC_MAX_LEN, &renderCtx)) continue;
         if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%homeassistant%", CSTR(settings.mqtt.sHaprefix))) continue;

         // ADR-040 source templates expand to 3 discovery entries from one line.
         // This is used by manual/bulk discovery paths (doAutoConfigure()).
         bool hasSourceSuffixToken = (strstr(lineView.topicTemplate, s_sourceSuffixToken) || strstr(lineView.msgTemplate, s_sourceSuffixToken));
         bool hasSourceNameToken = (strstr(lineView.topicTemplate, s_sourceNameToken) || strstr(lineView.msgTemplate, s_sourceNameToken));
         bool hasSourceTopicSegmentToken = (strstr(lineView.topicTemplate, s_sourceTopicSegmentToken) || strstr(lineView.msgTemplate, s_sourceTopicSegmentToken));
         if (hasSourceSuffixToken || hasSourceNameToken || hasSourceTopicSegmentToken) {
           if (!sourceTemplateSchemaLogged) {
             if (hasSourceTopicSegmentToken) MQTTDebugTln(F("MQTT source template schema: nested %source_topic_segment% placeholders detected (likely updated mqttha.cfg)"));
             else MQTTDebugTln(F("MQTT source template schema: legacy source placeholders only (older flashed mqttha.cfg may still be in use)"));
             sourceTemplateSchemaLogged = true;
           }
           if (settings.mqtt.bSeparateSources) {
             if (expandAndPublishSourceTemplates(lineID, "bulk", lineView.topicTemplate, lineView.msgTemplate, &renderCtx, sTopic)) {
               setMQTTConfigDone(lineID);
             }
           }
           continue; // source template handled (or intentionally skipped when disabled)
         }

         if (!sendMQTTTemplateStreaming(sTopic, lineView.msgTemplate, &renderCtx)) continue;
         setMQTTConfigDone(lineID);

         feedWatchDog(); // Keep the dog happy between publishes (not doBackgroundTasks — cMsg is live as sTopic)
      }
    }

    fh.close();
    
    // Note: cMsg (sTopic) and sLine are globals; they persist across calls.
    // No cleanup needed; guard (mqttAutoConfigInProgress) is released by sessionLock dtor.
    resetMQTTBufferSize();
  } // Lock released here - configSensors() can now acquire it independently

  // Trigger Dallas configuration separately as it requires specific sensor addresses
  if (settings.mqtt.bEnable && !getMQTTConfigDone(OTGWdallasdataid)) {
    configSensors();
  }
}
//===========================================================================================
bool doAutoConfigureMsgid(byte OTid)
{ 
  // check if foney dataid is called to do autoconfigure for temp sensors, call configsensors instead 
  if (OTid == OTGWdallasdataid) {
    MQTTDebugTf(PSTR("Sending auto configuration for temp sensors %d\r\n"), OTid);
    configSensors() ;
    return true;
  }  
  return doAutoConfigureMsgid(OTid, nullptr);
}

bool doAutoConfigureMsgid(byte OTid, const char *cfgSensorId) {
  return doAutoConfigureMsgid(OTid, cfgSensorId, nullptr);
}

bool doAutoConfigureMsgid(byte OTid, const char *cfgSensorId, const char *baseMqttTopic)
{
  bool _result = false;
  const char *sensorId = (cfgSensorId != nullptr) ? cfgSensorId : "";

  MQTTAutoConfigSessionLock sessionLock;
  if (!sessionLock.locked) {
    MQTTDebugTln(F("MQTT autoconfig already running, skipping doAutoConfigureMsgid()"));
    return _result;
  }
  if (!settings.mqtt.bEnable) {
    return _result;
  }
  if (!MQTTclient.connected()) {
    DebugTln(F("Error: MQTT broker not connected.")); 
    return _result;
  } 
  if (!isValidIP(MQTTbrokerIP)) {
    DebugTln(F("Error: MQTT broker IP not valid.")); 
    return _result;
  } 

  // Workspace (ADR-053 two-buffer design):
  //   sLine[SLINE_SIZE=1200] — global, holds raw config file lines (≤900 bytes). Guarded by
  //                            mqttAutoConfigInProgress (acquired above via sessionLock).
  //   sTopic = cMsg — cMsg global reused as rendered topic (≤200 bytes). Safe because
  //                   topicTemplate/msgTemplate point into sLine, not cMsg.
  //   feedWatchDog() is the only yield — prevents HTTP/MQTT callbacks from overwriting cMsg.
  char *sTopic = cMsg;                         // cMsg reused for rendered topic (≤200 bytes, fits in CMSG_SIZE)
  initSourceTokens();
  byte lineID = 39; // 39 is unused in OT protocol so is a safe value

  //Let's open the MQTT autoconfig file
  File fh; //filehandle
  // const char *cfgFilename = "/mqttha.cfg"; // moved to usage
  LittleFS.begin();

  if (!LittleFS.exists(F("/mqttha.cfg"))) {
    DebugTln(F("Error: configuration file not found.")); 
    return _result;
  } 

  fh = LittleFS.open(F("/mqttha.cfg"), "r");

  if (!fh) {
    DebugTln(F("Error: could not open configuration file.")); 
    return _result;
  } 

  //Lets go read the config and send it out to MQTT line by line
  while (fh.available())
  {
    //read file line by line, split and send to MQTT (topic, msg)
    feedWatchDog(); //start with feeding the dog
    
    // Read line into sLine (global config-line buffer, guarded by mqttAutoConfigInProgress)
    size_t len = fh.readBytesUntil('\n', sLine, SLINE_SIZE - 1);
    sLine[len] = '\0';
    MQTTAutoConfigLineView lineView;
    if (!parseAutoConfigLine(sLine, ';', &lineView)) {  // parseAutoConfigLine() also filters comments
      continue;
    }
    lineID = lineView.id;

    // check if this is the specific line we are looking for
    // Old config dump method (dumping all lines) is no longer used - we now fetch specific lines by ID
    if (lineID != OTid) continue;

    MQTTDebugTf(PSTR("Found line in config file for %d: [%d][%s] \r\n"), OTid, lineID, lineView.topicTemplate);

    MQTTAutoConfigTemplateContext renderCtx;
    renderCtx.nodeId = NodeId;
    renderCtx.sensorId = sensorId;
    renderCtx.hostname = CSTR(settings.sHostname);
    renderCtx.version = _VERSION;
    renderCtx.mqttPubTopic = MQTTPubNamespace;
    renderCtx.mqttSubTopic = MQTTSubNamespace;

    // discovery topic prefix (rendered into cMsg via sTopic pointer)
    MQTTDebugTf(PSTR("sTopic[%s]==>"), lineView.topicTemplate); 
    if (!renderTemplateToBuffer(lineView.topicTemplate, sTopic, MQTT_TOPIC_MAX_LEN, &renderCtx)) { MQTTDebugTln(F("MQTT: topic template rendering overflow")); continue; }
    if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%homeassistant%", CSTR(settings.mqtt.sHaprefix))) { MQTTDebugTln(F("MQTT: topic replacement overflow")); continue; }

    MQTTDebugf(PSTR("[%s]\r\n"), sTopic); 
    /// ----------------------
    MQTTDebugTf(PSTR("sMsg template len[%d]\r\n"), (int)strlen(lineView.msgTemplate));
    DebugFlush();
    
    // ADR-040: Source template detection — entries with source placeholders in mqttha.cfg
    // are emitted 3× (thermostat/boiler/gateway).
    bool hasSourceSuffixToken = (strstr(lineView.topicTemplate, s_sourceSuffixToken) || strstr(lineView.msgTemplate, s_sourceSuffixToken));
    bool hasSourceNameToken = (strstr(lineView.topicTemplate, s_sourceNameToken) || strstr(lineView.msgTemplate, s_sourceNameToken));
    bool hasSourceTopicSegmentToken = (strstr(lineView.topicTemplate, s_sourceTopicSegmentToken) || strstr(lineView.msgTemplate, s_sourceTopicSegmentToken));
    if (hasSourceSuffixToken || hasSourceNameToken || hasSourceTopicSegmentToken) {
      if (settings.mqtt.bSeparateSources && baseMqttTopic != nullptr) {
        if (expandAndPublishSourceTemplates(OTid, "jit", lineView.topicTemplate, lineView.msgTemplate, &renderCtx, sTopic)) _result = true;
      }
      continue; // skip regular single-send below (source templates on or off)
    }

    if (sendMQTTTemplateStreaming(sTopic, lineView.msgTemplate, &renderCtx)) {
      _result = true;
    }

  } // while available()

  fh.close();

  // Note: cMsg (sTopic) and sLine are globals; they persist across calls.
  // No cleanup needed; guard (mqttAutoConfigInProgress) is released by sessionLock dtor.
  resetMQTTBufferSize();

  return _result;

}

void sensorAutoConfigure(byte dataid, bool finishflag , const char *cfgSensorId = nullptr) {
 // Special version of Autoconfigure for sensors
 // dataid is a foney id, not used by OT 
 // check wheter MQTT topic needs to be configured
 // cfgNodeId can be set to alternate NodeId to allow for multiple temperature sensors, should normally be NodeId
 // When finishflag is true, check on dataid is already done and complete the config.  On false do the config and leave completion to caller
 if(getMQTTConfigDone(dataid)==false or !finishflag) {
   MQTTDebugTf(PSTR("Need to set MQTT config for sensor id(%d)\r\n"),dataid);
   bool success = doAutoConfigureMsgid(dataid,cfgSensorId);
   if(success) {
     MQTTDebugTf(PSTR("Successfully sent MQTT config for sensor id(%d)\r\n"),dataid);
     if (finishflag) setMQTTConfigDone(dataid);
     } else {
       MQTTDebugTf(PSTR("Not able to complete MQTT configuration for sensor id(%d)\r\n"),dataid);
     }
   } else {
   // MQTTDebugTf(PSTR("No need to set MQTT config for sensor id(%d)\r\n"),dataid);
   }
 }




/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
