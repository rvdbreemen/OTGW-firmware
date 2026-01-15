/* 
***************************************************************************  
**  File     : StreamingTemplate.h
**  Version  : v1.0.0-rc4
**
**  Copyright (c) 2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
**
**  Template streaming processor for MQTT auto-discovery configuration.
**  
**  This class eliminates the need for large temporary buffers by streaming
**  template files directly to the MQTT client, performing token replacement
**  on-the-fly without ever loading the full message into RAM.
**
**  Benefits:
**  - Eliminates ~1200 byte buffer for message construction
**  - Prevents heap fragmentation from string operations
**  - Reduces peak RAM usage during MQTT auto-configuration
**  - Enables handling of arbitrarily large config messages
**
**  Usage:
**    StreamingTemplateProcessor::TokenReplacement tokens[] = {
**        {"%node_id%", NodeId},
**        {"%hostname%", settingHostname}
**    };
**    StreamingTemplateProcessor processor(tokens, 2);
**    
**    if (MQTTclient.beginPublish(topic, processor.calculateLength(file), true)) {
**        processor.streamToMQTT(file, MQTTclient);
**        MQTTclient.endPublish();
**    }
**
***************************************************************************      
*/

#ifndef STREAMING_TEMPLATE_H
#define STREAMING_TEMPLATE_H

#include <Arduino.h>
#include <FS.h>
#include <PubSubClient.h>

// Enable/disable streaming features
#ifndef USE_STREAMING_TEMPLATES
#define USE_STREAMING_TEMPLATES 1  // Set to 0 to disable and fall back to buffered mode
#endif

class StreamingTemplateProcessor {
public:
    // Token replacement structure
    struct TokenReplacement {
        const char* token;       // Token to search for (e.g., "[node_id]")
        const char* value;       // Replacement value
    };

    // Maximum number of tokens that can be registered
    static const uint8_t MAX_TOKENS = 16;

private:
    TokenReplacement replacements[MAX_TOKENS];
    uint8_t replacementCount;
    
    // Check if current file position matches a token
    // Returns true if matched, false otherwise
    // Updates bytesRead with number of bytes consumed
    bool matchToken(File& file, const char* token, size_t& bytesRead) {
        size_t tokenLen = strlen(token);
        size_t filePos = file.position();
        
        for (size_t i = 0; i < tokenLen; i++) {
            if (!file.available()) {
                file.seek(filePos); // Rewind on incomplete match
                return false;
            }
            char c = file.read();
            bytesRead++;
            if (c != token[i]) {
                file.seek(filePos); // Rewind - not a match
                bytesRead = 0;
                return false;
            }
        }
        return true; // Complete match
    }

public:
    // Default constructor
    StreamingTemplateProcessor() : replacementCount(0) {}
    
    // Clear all registered tokens
    void clear() {
        replacementCount = 0;
    }
    
    // Add a token/value pair for replacement
    // Returns true if added, false if table is full
    bool addToken(const __FlashStringHelper* token, const char* value) {
        if (replacementCount >= MAX_TOKENS) {
            return false;
        }
        replacements[replacementCount].token = (const char*)token;
        replacements[replacementCount].value = value;
        replacementCount++;
        return true;
    }
    
    // Stream template with token replacement directly to MQTT client
    // This is the core streaming function - NO temporary buffers needed!
    // 
    // Parameters:
    //   file     - Input file positioned at start of content to stream
    //   client   - PubSubClient to write to (must be in beginPublish state)
    //   maxBytes - Safety limit to prevent runaway reads
    //
    // Returns: true if successful, false on error
    bool streamToMQTT(File& file, PubSubClient& client, size_t maxBytes = 4096) {
        size_t bytesProcessed = 0;
        
        while (file.available() && bytesProcessed < maxBytes) {
            char c = file.peek();
            
            // Check for potential token start
            if (c == '%') {
                bool tokenFound = false;
                
                // Try to match each registered token
                for (uint8_t i = 0; i < replacementCount; i++) {
                    size_t testBytes = 0;
                    if (matchToken(file, replacements[i].token, testBytes)) {
                        // Token matched! Write replacement value directly to MQTT
                        const char* value = replacements[i].value;
                        size_t valueLen = strlen(value);
                        
                        for (size_t j = 0; j < valueLen; j++) {
                            if (!client.write((uint8_t)value[j])) {
                                return false; // Write failed
                            }
                        }
                        
                        bytesProcessed += testBytes;
                        tokenFound = true;
                        break;
                    }
                }
                
                if (!tokenFound) {
                    // '%' character didn't start a token, write it literally
                    c = file.read();
                    if (!client.write((uint8_t)c)) return false;
                    bytesProcessed++;
                }
            } else {
                // Regular character - pass through
                c = file.read();
                if (!client.write((uint8_t)c)) return false;
                bytesProcessed++;
            }
        }
        
        return true;
    }
    
    // Calculate length after token replacement
    // Required for MQTTclient.beginPublish(topic, length, retain)
    //
    // This function reads through the file once to calculate the final size,
    // then rewinds to the original position.
    //
    // Parameters:
    //   file - Input file (will be rewound to original position after calculation)
    //
    // Returns: Length in bytes after token expansion
    size_t calculateExpandedLength(File& file) {
        size_t length = 0;
        size_t originalPos = file.position();
        
        while (file.available()) {
            char c = file.peek();
            
            if (c == '%') {
                bool tokenFound = false;
                for (uint8_t i = 0; i < replacementCount; i++) {
                    size_t testBytes = 0;
                    if (matchToken(file, replacements[i].token, testBytes)) {
                        // Add replacement value length
                        length += strlen(replacements[i].value);
                        tokenFound = true;
                        break;
                    }
                }
                if (!tokenFound) {
                    // Not a token, count the '%' character
                    file.read();
                    length++;
                }
            } else {
                // Regular character
                file.read();
                length++;
            }
        }
        
        file.seek(originalPos); // Restore original position
        return length;
    }
    
    // Helper: Expand tokens in a small buffer (for topics only)
    // This is acceptable for small strings like MQTT topics (~100-200 bytes)
    // but should NOT be used for message payloads.
    //
    // Returns: true if successful, false if buffer too small
    bool expandTokensInPlace(const char* input, char* output, size_t outSize) {
        strlcpy(output, input, outSize);
        
        for (uint8_t i = 0; i < replacementCount; i++) {
            if (!replaceAllInBuffer(output, outSize, replacements[i].token, replacements[i].value)) {
                return false; // Buffer overflow
            }
        }
        return true;
    }
    
    // PROGMEM version: Calculate expanded length from a PROGMEM template string
    // Used for validation/testing
    size_t calculateExpandedLength_P(const char* template_P) {
        size_t length = 0;
        size_t pos = 0;
        
        char c;
        while ((c = pgm_read_byte(&template_P[pos])) != '\0') {
            bool tokenFound = false;
            
            // Check if this might be a token start
            if (c == '[') {
                // Try to match each registered token
                for (uint8_t i = 0; i < replacementCount; i++) {
                    const char* token = replacements[i].token;
                    size_t tokenLen = strlen_P(token);
                    
                    // Check if token matches at current position
                    bool matches = true;
                    for (size_t j = 0; j < tokenLen; j++) {
                        char tc = pgm_read_byte(&template_P[pos + j]);
                        char tk = pgm_read_byte(&token[j]);
                        if (tc != tk || tc == '\0') {
                            matches = false;
                            break;
                        }
                    }
                    
                    if (matches) {
                        // Token matched! Add replacement length
                        length += strlen(replacements[i].value);
                        pos += tokenLen;
                        tokenFound = true;
                        break;
                    }
                }
            }
            
            if (!tokenFound) {
                // Regular character
                length++;
                pos++;
            }
        }
        
        return length;
    }
    
    // PROGMEM version: Expand tokens from PROGMEM template into RAM buffer
    // Used for validation/testing
    bool expandTokensInPlace_P(const char* template_P, char* output, size_t outSize) {
        // First, copy PROGMEM template to output buffer
        size_t pos = 0;
        size_t outPos = 0;
        
        char c;
        while ((c = pgm_read_byte(&template_P[pos])) != '\0') {
            if (outPos >= outSize - 1) {
                return false; // Buffer overflow
            }
            
            bool tokenFound = false;
            
            // Check if this might be a token start
            if (c == '[') {
                // Try to match each registered token
                for (uint8_t i = 0; i < replacementCount; i++) {
                    const char* token = replacements[i].token;
                    size_t tokenLen = strlen_P(token);
                    
                    // Check if token matches at current position
                    bool matches = true;
                    for (size_t j = 0; j < tokenLen; j++) {
                        char tc = pgm_read_byte(&template_P[pos + j]);
                        char tk = pgm_read_byte(&token[j]);
                        if (tc != tk || tc == '\0') {
                            matches = false;
                            break;
                        }
                    }
                    
                    if (matches) {
                        // Token matched! Copy replacement value
                        const char* value = replacements[i].value;
                        size_t valueLen = strlen(value);
                        
                        if (outPos + valueLen >= outSize) {
                            return false; // Buffer overflow
                        }
                        
                        strcpy(&output[outPos], value);
                        outPos += valueLen;
                        pos += tokenLen;
                        tokenFound = true;
                        break;
                    }
                }
            }
            
            if (!tokenFound) {
                // Regular character
                output[outPos++] = c;
                pos++;
            }
        }
        
        output[outPos] = '\0';
        return true;
    }

private:
    // Internal helper for in-place token replacement (for small buffers only)
    bool replaceAllInBuffer(char* buffer, size_t bufSize, const char* token, const char* replacement) {
        size_t tokenLen = strlen(token);
        size_t replLen = strlen(replacement);
        
        char* pos = strstr(buffer, token);
        while (pos != nullptr) {
            size_t currentLen = strlen(buffer);
            size_t tailLen = strlen(pos + tokenLen);
            
            // Check if replacement will fit
            if (currentLen - tokenLen + replLen >= bufSize) {
                return false; // Would overflow
            }
            
            // Shift tail to make room (or close gap)
            memmove(pos + replLen, pos + tokenLen, tailLen + 1);
            
            // Copy replacement
            memcpy(pos, replacement, replLen);
            
            // Search for next occurrence
            pos = strstr(pos + replLen, token);
        }
        
        return true;
    }
};

#endif // STREAMING_TEMPLATE_H

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
****************************************************************************
*/
