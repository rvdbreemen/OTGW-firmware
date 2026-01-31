/* 
***************************************************************************  
**  Program  : restAPI_v3_advanced
**  Version  : v1.1.0
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.
**
**  REST API v3 Advanced Features
**  - ETag caching support
**  - Pagination helpers
**  - Query parameter filtering
**  - Rate limiting (optional)
**
**  Architectural Decision Record: docs/adr/ADR-025-rest-api-v3-design.md
***************************************************************************      
*/

//=======================================================================
// ETag Generation and Caching (Task 3.1)
// Supports If-None-Match conditional requests for 304 responses
//=======================================================================

/**
 * Simple FNV-1a hash for ETag generation
 * Works with PROGMEM and RAM strings
 */
static uint32_t fnv1a_hash(const char *data, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    hash ^= static_cast<uint8_t>(data[i]);
    hash *= 16777619u;
  }
  return hash;
}

/**
 * Generate ETag string from resource data and timestamp
 * @param resource Resource identifier (e.g., "config/device")
 * @param lastModified Timestamp of last modification
 * @return ETag string in format: "hash-timestamp"
 */
String generateETagV3(const char *resource, uint32_t lastModified) {
  char etag[32];
  uint32_t resourceHash = fnv1a_hash(resource, strlen(resource));
  snprintf_P(etag, sizeof(etag), PSTR("\"%08x-%08x\""), resourceHash, lastModified);
  return String(etag);
}

/**
 * Check If-None-Match header and return true if client has current version
 * @param expectedETag The current ETag value
 * @return true if client's version matches (should return 304), false otherwise
 */
static bool clientHasCurrentVersion(const String &expectedETag) {
  if (!httpServer.hasHeader(F("If-None-Match"))) {
    return false;
  }
  
  String clientETag = httpServer.header(F("If-None-Match"));
  // Handle multiple ETags (comma-separated) and wildcard *
  if (clientETag == F("*")) {
    return true;
  }
  
  // Simple comparison for single ETag
  return clientETag == expectedETag;
}

/**
 * Send 304 Not Modified response with ETag header
 */
static void sendNotModified(const String &etag) {
  httpServer.sendHeader(F("ETag"), etag);
  httpServer.sendHeader(F("Cache-Control"), F("public, max-age=3600"));
  httpServer.send(304);
}

/**
 * Send response with ETag header
 * @param statusCode HTTP status code (200, etc.)
 * @param contentType Content-Type header value
 * @param content Response body
 * @param etag ETag value to include
 * @param cacheControl Cache-Control header value (optional)
 */
static void sendWithETag(int statusCode, const char *contentType, 
                        const String &content, const String &etag,
                        const char *cacheControl = nullptr) {
  httpServer.sendHeader(F("ETag"), etag);
  
  if (cacheControl == nullptr) {
    httpServer.sendHeader(F("Cache-Control"), F("public, max-age=3600"));
  } else {
    httpServer.sendHeader(F("Cache-Control"), cacheControl);
  }
  
  httpServer.send(statusCode, contentType, content);
}

//=======================================================================
// Pagination Support (Task 3.2)
// Handles pagination for large collections
//=======================================================================

/**
 * Pagination parameters extracted from query string
 */
struct PaginationParams {
  uint16_t page;        // Current page (0-indexed)
  uint16_t perPage;     // Items per page
  uint16_t totalItems;  // Total items in collection
  
  // Calculated values
  uint16_t totalPages;
  uint16_t offset;
  uint16_t limit;
  
  PaginationParams() : page(0), perPage(20), totalItems(0), 
                       totalPages(0), offset(0), limit(20) {}
};

/**
 * Parse pagination parameters from query string
 * @param totalItems Total number of items in collection
 * @param defaultPerPage Default items per page (default 20)
 * @param maxPerPage Maximum allowed items per page (default 50)
 * @return PaginationParams with page/perPage from query or defaults
 */
static PaginationParams parsePaginationParams(uint16_t totalItems, 
                                             uint16_t defaultPerPage = 20,
                                             uint16_t maxPerPage = 50) {
  PaginationParams params;
  params.totalItems = totalItems;
  params.perPage = defaultPerPage;
  
  // Parse page parameter
  if (httpServer.hasArg(F("page"))) {
    int p = atoi(httpServer.arg(F("page")).c_str());
    if (p >= 0) params.page = p;
  }
  
  // Parse per_page parameter
  if (httpServer.hasArg(F("per_page"))) {
    int pp = atoi(httpServer.arg(F("per_page")).c_str());
    if (pp > 0 && pp <= maxPerPage) {
      params.perPage = pp;
    }
  }
  
  // Calculate derived values
  params.limit = params.perPage;
  params.offset = params.page * params.perPage;
  params.totalPages = (totalItems + params.perPage - 1) / params.perPage;
  
  // Clamp page to valid range
  if (params.page >= params.totalPages && params.totalPages > 0) {
    params.page = params.totalPages - 1;
    params.offset = params.page * params.perPage;
  }
  
  return params;
}

/**
 * Add pagination metadata and links to JSON object
 * @param root JSON object to modify
 * @param params Pagination parameters
 * @param baseUrl Base URL for pagination links (e.g., "/api/v3/otgw/messages")
 */
static void addPaginationLinks(JsonObject &root, const PaginationParams &params, 
                              const char *baseUrl) {
  JsonObject pagination = root.createNestedObject(F("pagination"));
  pagination[F("page")] = params.page;
  pagination[F("per_page")] = params.perPage;
  pagination[F("total")] = params.totalItems;
  pagination[F("total_pages")] = params.totalPages;
  
  JsonObject links = root.createNestedObject(F("_links"));
  
  // Self link
  char selfUrl[128];
  snprintf_P(selfUrl, sizeof(selfUrl), PSTR("%s?page=%d&per_page=%d"), 
             baseUrl, params.page, params.perPage);
  JsonObject selfLink = links.createNestedObject(F("self"));
  selfLink[F("href")] = selfUrl;
  
  // First page link
  if (params.page > 0) {
    char firstUrl[128];
    snprintf_P(firstUrl, sizeof(firstUrl), PSTR("%s?page=0&per_page=%d"), 
               baseUrl, params.perPage);
    JsonObject firstLink = links.createNestedObject(F("first"));
    firstLink[F("href")] = firstUrl;
  }
  
  // Previous page link
  if (params.page > 0) {
    char prevUrl[128];
    snprintf_P(prevUrl, sizeof(prevUrl), PSTR("%s?page=%d&per_page=%d"), 
               baseUrl, params.page - 1, params.perPage);
    JsonObject prevLink = links.createNestedObject(F("prev"));
    prevLink[F("href")] = prevUrl;
  }
  
  // Next page link
  if (params.page < params.totalPages - 1) {
    char nextUrl[128];
    snprintf_P(nextUrl, sizeof(nextUrl), PSTR("%s?page=%d&per_page=%d"), 
               baseUrl, params.page + 1, params.perPage);
    JsonObject nextLink = links.createNestedObject(F("next"));
    nextLink[F("href")] = nextUrl;
  }
  
  // Last page link
  if (params.page < params.totalPages - 1) {
    char lastUrl[128];
    snprintf_P(lastUrl, sizeof(lastUrl), PSTR("%s?page=%d&per_page=%d"), 
               baseUrl, params.totalPages - 1, params.perPage);
    JsonObject lastLink = links.createNestedObject(F("last"));
    lastLink[F("href")] = lastUrl;
  }
}

//=======================================================================
// Query Parameter Filtering (Task 3.3)
// Supports filtering by category, timestamp, and boolean flags
//=======================================================================

/**
 * Filter parameters structure
 */
struct FilterParams {
  char category[32];
  uint32_t updatedAfter;
  bool connectedOnly;
  
  FilterParams() : updatedAfter(0), connectedOnly(false) {
    category[0] = '\0';
  }
};

/**
 * Parse filter parameters from query string
 * @return FilterParams with values from query string
 */
static FilterParams parseFilterParams() {
  FilterParams filters;
  
  if (httpServer.hasArg(F("category"))) {
    strlcpy(filters.category, httpServer.arg(F("category")).c_str(), sizeof(filters.category));
  }
  
  if (httpServer.hasArg(F("updated_after"))) {
    filters.updatedAfter = atol(httpServer.arg(F("updated_after")).c_str());
  }
  
  if (httpServer.hasArg(F("connected"))) {
    String connected = httpServer.arg(F("connected"));
    filters.connectedOnly = (connected == F("true") || connected == F("1"));
  }
  
  return filters;
}

/**
 * Check if OpenTherm message matches filter category
 * @param msgId Message ID
 * @param category Category filter (empty = match all)
 * @return true if message matches category
 */
static bool messageMatchesCategory(uint8_t msgId, const char *category) {
  if (category == nullptr || category[0] == '\0') {
    return true; // No filter
  }
  
  // Get message type from OT specification
  // Categories: temperature, pressure, flowrate, flags, etc.
  
  if (strcmp_P(category, PSTR("temperature")) == 0) {
    // Messages 24-28: temperatures
    return (msgId >= 24 && msgId <= 28);
  } else if (strcmp_P(category, PSTR("pressure")) == 0) {
    // Message 41: CH pressure
    return (msgId == 41);
  } else if (strcmp_P(category, PSTR("flow")) == 0) {
    // Message 42: DHW flow rate
    return (msgId == 42);
  } else if (strcmp_P(category, PSTR("status")) == 0) {
    // Message 0: Status
    return (msgId == 0);
  } else if (strcmp_P(category, PSTR("setpoint")) == 0) {
    // Message 16: Room setpoint
    return (msgId == 16);
  }
  
  return false; // Unknown category
}

//=======================================================================
// Rate Limiting (Task 3.4 - Optional)
// Per-IP request rate limiting with minimal memory overhead
//=======================================================================

#define RATELIMIT_ENABLED 0  // Set to 1 to enable rate limiting
#define RATELIMIT_REQUESTS_PER_MINUTE 60
#define RATELIMIT_MAX_CLIENTS 8  // Track up to N IP addresses

#if RATELIMIT_ENABLED

struct RateLimitEntry {
  uint32_t clientIp;      // Client IP address (4 bytes)
  uint32_t windowStart;   // Start of rate limit window (Unix timestamp)
  uint8_t requestCount;   // Requests in current window
};

static RateLimitEntry rateLimitTable[RATELIMIT_MAX_CLIENTS];
static uint8_t rateLimitCount = 0;

/**
 * Check if client has exceeded rate limit
 * @param clientIp Client IP address
 * @return true if rate limit exceeded, false if within limits
 */
static bool checkRateLimit(uint32_t clientIp) {
  uint32_t now = millis() / 1000;  // Convert to seconds
  const uint32_t WINDOW_SIZE = 60;  // 1 minute window
  
  // Find or create entry for this client
  for (uint8_t i = 0; i < rateLimitCount; i++) {
    if (rateLimitTable[i].clientIp == clientIp) {
      // Check if window has expired
      if (now - rateLimitTable[i].windowStart >= WINDOW_SIZE) {
        // New window
        rateLimitTable[i].windowStart = now;
        rateLimitTable[i].requestCount = 1;
        return false;
      }
      
      // Same window
      rateLimitTable[i].requestCount++;
      return rateLimitTable[i].requestCount > RATELIMIT_REQUESTS_PER_MINUTE;
    }
  }
  
  // New client
  if (rateLimitCount >= RATELIMIT_MAX_CLIENTS) {
    // Table full, evict oldest entry
    uint32_t oldestTime = rateLimitTable[0].windowStart;
    uint8_t oldestIdx = 0;
    for (uint8_t i = 1; i < rateLimitCount; i++) {
      if (rateLimitTable[i].windowStart < oldestTime) {
        oldestTime = rateLimitTable[i].windowStart;
        oldestIdx = i;
      }
    }
    // Replace oldest entry
    rateLimitTable[oldestIdx].clientIp = clientIp;
    rateLimitTable[oldestIdx].windowStart = now;
    rateLimitTable[oldestIdx].requestCount = 1;
  } else {
    // Add new entry
    rateLimitTable[rateLimitCount].clientIp = clientIp;
    rateLimitTable[rateLimitCount].windowStart = now;
    rateLimitTable[rateLimitCount].requestCount = 1;
    rateLimitCount++;
  }
  
  return false;  // First request from new client
}

/**
 * Handle rate limit exceeded response
 */
static void sendRateLimitExceeded() {
  httpServer.sendHeader(F("Retry-After"), F("60"));
  httpServer.sendHeader(F("X-RateLimit-Limit"), PSTR("60"));
  httpServer.sendHeader(F("X-RateLimit-Remaining"), F("0"));
  sendJsonError_P(429, PSTR("RATE_LIMIT_EXCEEDED"), 
                  PSTR("Too many requests. Maximum 60 per minute."), 
                  httpServer.uri().c_str());
}

#endif  // RATELIMIT_ENABLED

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
