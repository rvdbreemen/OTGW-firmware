# API Documentation Structure

This guide explains how the new REST API v3 documentation integrates with the existing wiki.

## Documentation Organization

### Legacy API Documentation (v0/v1)
- **[[How to use the REST API]]** - Original documentation for API v0 and v1
- Location: `wiki_backup/How-to-use-the-REST-API.md`
- Audience: Users on older versions
- Status: Maintained for backward compatibility

### Modern API Documentation (v3)

The new v3 documentation provides a complete, modern REST API implementation following Richardson Maturity Level 3.

#### Quick Navigation Structure

```
1. Getting Started
   ├─ [[1-API-v3-Overview|1-API-v3-Overview]] (What's new in v3)
   └─ [[2-Quick-Start-Guide|2-Quick-Start-Guide]] (30-second setup)

2. Using the API
   ├─ [[3-Complete-API-Reference|3-Complete-API-Reference]] (All endpoints)
   ├─ [[7-HATEOAS-Navigation-Guide|7-HATEOAS-Navigation-Guide]] (Discovery)
   └─ [[6-Error-Handling-Guide|6-Error-Handling-Guide]] (Error codes)

3. Integration Paths
   ├─ [[4-Migration-Guide|4-Migration-Guide]] (From v0/v1/v2)
   ├─ [[5-Changelog|5-Changelog]] (What changed)
   └─ [[8-Troubleshooting|8-Troubleshooting]] (Common issues)

4. Code Examples
   └─ `example-api/` directory in repository
      ├─ `v1/` - Stable v1 API examples (curl, JavaScript, Python)
      ├─ `v2/` - Enhanced v2 API examples (curl, JavaScript, Python)
      └─ `v3/` - Modern v3 API examples
```

## File Descriptions

### 1-API-v3-Overview.md
**Purpose:** Introduction to REST API v3 architecture and capabilities

**Contains:**
- Feature overview and improvements from v0/v1/v2
- Architecture principles (Richardson Level 3, HATEOAS)
- Key capabilities (ETag caching, pagination, filtering, rate limiting)
- Quick feature comparison table
- Links to detailed guides

**Audience:** All users considering v3, decision makers
**Length:** ~500 lines
**Cross-references:** Points to Quick Start Guide, Migration Guide

### 2-Quick-Start-Guide.md
**Purpose:** Get started with API v3 in 30 seconds

**Contains:**
- Minimal setup example
- First API call (health check)
- Common patterns (reading values, sending commands)
- Available client libraries
- Next steps for different use cases

**Audience:** Developers wanting quick start
**Length:** ~400 lines
**Cross-references:** Points to Complete API Reference, code examples

### 3-Complete-API-Reference.md
**Purpose:** Comprehensive documentation of all v3 endpoints

**Contains:**
- All 31 endpoints organized by category:
  - System endpoints (/api/v1/health, /api/v1/devinfo)
  - Configuration endpoints (/api/v1/settings/*)
  - OpenTherm endpoints (/api/v1/otgw/*)
  - PIC firmware endpoints (/api/v1/pic/*)
  - Sensor endpoints (/api/v1/sensors/*)
  - Export endpoints (/api/v1/export/*)
- Request/response examples for each endpoint
- Status codes and error conditions
- Authentication and rate limiting details

**Audience:** Developers integrating with the API
**Length:** ~2000+ lines
**Cross-references:** Links to Error Handling Guide, HATEOAS Guide

### 4-Migration-Guide.md
**Purpose:** Help users upgrade from older API versions

**Contains:**
- Version comparison table (v0 vs v1 vs v2 vs v3)
- Breaking changes (if any)
- Endpoint mapping for each version
- Code examples showing before/after
- Performance improvements
- Step-by-step migration process
- Common pitfalls and solutions

**Audience:** Existing users with v0/v1/v2 implementations
**Length:** ~1200+ lines
**Cross-references:** Links to API Reference, Error Handling Guide

### 5-Changelog.md
**Purpose:** Document evolution of REST API across versions

**Contains:**
- v0 → v1 changes
- v1 → v2 changes
- v2 → v3 changes (major)
- Feature additions per version
- Deprecations and removals
- Security improvements
- Performance optimizations

**Audience:** Users understanding version history
**Length:** ~800+ lines
**Cross-references:** Links to Migration Guide

### 6-Error-Handling-Guide.md
**Purpose:** Comprehensive error handling reference

**Contains:**
- HTTP status codes and meanings
- API error response format
- Common error scenarios
- Error recovery strategies
- Debugging tips
- Code examples for error handling
- Rate limiting and backoff strategies

**Audience:** Developers handling edge cases
**Length:** ~600 lines
**Cross-references:** Links to API Reference, Troubleshooting

### 7-HATEOAS-Navigation-Guide.md
**Purpose:** Guide to using HATEOAS for API discovery

**Contains:**
- HATEOAS principles explanation
- How the API exposes links in responses
- Using links for discovery instead of hardcoding URLs
- Benefits and best practices
- Code examples (curl, JavaScript, Python)
- Advanced navigation patterns

**Audience:** Developers wanting self-documenting API discovery
**Length:** ~700 lines
**Cross-references:** Links to API Reference, code examples

### 8-Troubleshooting.md
**Purpose:** Common issues and solutions

**Contains:**
- Connection issues
- Authentication failures
- Rate limiting exceeded
- Unexpected response formats
- WebSocket connection problems
- MQTT integration issues
- Home Assistant integration issues
- Performance issues
- Debugging techniques
- Where to get help

**Audience:** Users troubleshooting integration
**Length:** ~600 lines
**Cross-references:** Links to Error Handling Guide, related wiki pages

## Code Examples Location

All code examples are in the `example-api/` directory (repository root, not wiki):

```
example-api/
├── README.md                  # Master index for all examples
├── QUICK_REFERENCE.md         # Quick lookup table
├── SUMMARY.md                 # Creation summary and statistics
├── v1/                        # API v1 examples (stable)
│   ├── README.md
│   ├── curl_examples.sh       # 600+ lines, 13 bash examples
│   ├── javascript_examples.js # 700+ lines, OTGWClientV1 class
│   └── python_examples.py     # 600+ lines, OTGWClientV1 class
├── v2/                        # API v2 examples (enhanced)
│   ├── README.md
│   ├── curl_examples.sh       # 450+ lines with v1 compatibility
│   ├── javascript_examples.js # 550+ lines, OTGWClientV2 class
│   └── python_examples.py     # 550+ lines, OTGWClientV2 class
└── v3/                        # API v3 examples (modern)
    ├── curl_examples.sh
    ├── javascript_examples.js
    └── python_examples.py
```

**Link from wiki:** When referencing code examples in documentation, use:
- "See `example-api/v3/curl_examples.sh` in the repository"
- "The `OTGWClientV3` class in `example-api/v3/javascript_examples.js`"

## Integration with Existing Wiki

### Preserved Content
- **All existing documentation remains unchanged** - Legacy guides still work
- **Original "How to use the REST API"** - Linked as legacy documentation
- **Installation, debugging, MQTT guides** - Supplementary to new v3 docs

### Enhanced Structure
- **Home.md updated** - New "REST API Documentation" section
- **Clear version guidance** - Recommends v3 for new projects
- **Migration path** - Easy upgrade path for existing users

### Cross-References
New v3 documentation links back to existing wiki where relevant:
- Error Handling Guide → How to send commands (MQTT alternative)
- Quick Start Guide → How to use with Home Assistant
- Troubleshooting → How to debug the firmware
- HATEOAS Guide → How to use with OTmonitor

## Synchronization with GitHub Wiki

The `sync-wiki.yml` GitHub Action automatically:

1. **On push to `docs/wiki/`:**
   - Clones the GitHub wiki repository
   - Copies all markdown files from `docs/wiki/` to wiki
   - Commits and pushes changes with proper attribution
   - Updates the wiki with latest documentation

2. **Preserves git history:** Each sync creates a new commit with timestamp

3. **Enables PR workflow:** Changes can be reviewed via pull requests before syncing to wiki

## Adding New Documentation

When adding new API documentation:

1. **Create in `docs/wiki/` folder**
   - Use descriptive filenames (kebab-case)
   - Add content in Markdown with wiki-style links

2. **Update Home.md**
   - Add link to new document in appropriate section
   - Use `[[filename|Display Text]]` wiki syntax

3. **Cross-reference related docs**
   - Add "See also" section with related pages
   - Update this structure guide if adding new categories

4. **Push to GitHub**
   - Create a pull request
   - Let workflow auto-sync to the GitHub wiki

## Browser Navigation in Wiki

GitHub wiki sidebar auto-generates from:
- **Home.md** - Main entry point (appears as "Home")
- **All .md files** - Listed in alphabetical order in sidebar
- **Links in content** - Clickable navigation within pages

Users will navigate primarily through **Home.md** links, with sidebar as backup.

## Version-Specific Links

Different API versions documented at:

| Version | Legacy Doc | New Doc | Code Examples |
|---------|-----------|---------|----------------|
| v0 | How to use the REST API | [[1-API-v3-Overview]] | N/A |
| v1 | How to use the REST API | [[4-Migration-Guide]] | `example-api/v1/` |
| v2 | How to use the REST API | [[4-Migration-Guide]] | `example-api/v2/` |
| v3 | N/A | [[1-API-v3-Overview]] | `example-api/v3/` |

## Related Repository Documentation

This wiki documentation complements repository docs:

- **`docs/api/v3/API_REFERENCE.md`** - Detailed API specification (repo)
- **`example-api/README.md`** - Example index (repo)
- **`EVALUATION.md`** - Code quality metrics (repo)
- **GitHub wiki** - User-facing documentation (this folder)

## Maintenance Notes

- Update v3 documentation when API changes
- Add migration guides for new features
- Keep examples in sync with latest endpoints
- Review and update links during major releases

