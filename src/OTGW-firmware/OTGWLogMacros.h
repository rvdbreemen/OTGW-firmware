/*
***************************************************************************
**  Program  : OTGWLogMacros.h
**
**  Shared OT event log buffer macros.
***************************************************************************
*/

#ifndef OTGW_LOG_MACROS_H
#define OTGW_LOG_MACROS_H

#define OT_LOG_BUFFER_SIZE 512
extern char ot_log_buffer[OT_LOG_BUFFER_SIZE];
extern size_t ot_log_pos;

#define ClrLog()            ({ ot_log_buffer[0] = '\0'; ot_log_pos = 0; })
#define AddLogf(fmt, ...)   ({ if (ot_log_pos < (OT_LOG_BUFFER_SIZE - 1)) { \
                                 int _w = snprintf(ot_log_buffer + ot_log_pos, OT_LOG_BUFFER_SIZE - ot_log_pos, fmt, ##__VA_ARGS__); \
                                 if (_w > 0) { size_t _rem = OT_LOG_BUFFER_SIZE - 1 - ot_log_pos; ot_log_pos += ((size_t)_w < _rem) ? (size_t)_w : _rem; } \
                               } })
#define AddLogf_P(fmt, ...) ({ if (ot_log_pos < (OT_LOG_BUFFER_SIZE - 1)) { \
                                 int _w = snprintf_P(ot_log_buffer + ot_log_pos, OT_LOG_BUFFER_SIZE - ot_log_pos, fmt, ##__VA_ARGS__); \
                                 if (_w > 0) { size_t _rem = OT_LOG_BUFFER_SIZE - 1 - ot_log_pos; ot_log_pos += ((size_t)_w < _rem) ? (size_t)_w : _rem; } \
                               } })
#define AddLog(logstring)   ({ if (ot_log_pos < (OT_LOG_BUFFER_SIZE - 1)) { \
                                 size_t _rem = OT_LOG_BUFFER_SIZE - ot_log_pos; \
                                 size_t _src = strlcpy(ot_log_buffer + ot_log_pos, logstring, _rem); \
                                 ot_log_pos += (_src < _rem) ? _src : (_rem - 1); \
                               } })
#define AddLogln()          ({ if (ot_log_pos < (OT_LOG_BUFFER_SIZE - 2)) { \
                                 ot_log_buffer[ot_log_pos++] = '\r'; \
                                 ot_log_buffer[ot_log_pos++] = '\n'; \
                                 ot_log_buffer[ot_log_pos] = '\0'; \
                               } })

#endif
