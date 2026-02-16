# CI Build Fix: Transient Network Error Handling

## Problem

The CI build was failing intermittently with errors like:

```
Downloading index: package_esp8266com_index.json Download failed: server returned 502 Bad Gateway
Error initializing instance: Some indexes could not be updated.
make: *** [Makefile:66: update_indexes] Error 1
```

**Reference**: https://github.com/rvdbreemen/OTGW-firmware/actions/runs/22051453682/job/63710333502#step:4:1

## Root Cause

The Makefile's `update_indexes` target was not resilient to transient network errors. When the ESP8266 package index server at GitHub returned temporary errors (502 Bad Gateway), the entire build would fail immediately without retry.

This is problematic because:
1. Network errors are transient and often resolve within seconds
2. The package indexes might already be cached from previous builds
3. A single network hiccup should not fail the entire CI pipeline

## Solution

Added **exponential backoff retry logic** to the `update_indexes` target in the Makefile:

- **3 retry attempts** for each index update (core and library indexes)
- **Exponential backoff**: 2s after first failure, 4s after second failure
- **Clear user feedback**: Shows attempt number and wait time
- **Graceful failure**: Only fails after all 3 attempts are exhausted

### Code Changes

Modified the `update_indexes` target in `Makefile`:

```makefile
update_indexes: | $(CFGFILE)
    @echo "Updating package indexes..."
    @for i in 1 2 3; do \
        if $(CLICFG) core update-index; then \
            echo "✓ Core index updated successfully"; \
            break; \
        else \
            if [ $$i -lt 3 ]; then \
                wait_time=$$((2 ** $$i)); \
                echo "⚠ Core index update failed (attempt $$i/3), retrying in $${wait_time}s..."; \
                sleep $$wait_time; \
            else \
                echo "✗ Core index update failed after 3 attempts"; \
                exit 1; \
            fi; \
        fi; \
    done
    # Similar retry logic for lib update-index...
```

### Retry Schedule

| Attempt | Wait Before | Total Wait |
|---------|-------------|------------|
| 1st     | 0s          | 0s         |
| 2nd     | 2s          | 2s         |
| 3rd     | 4s          | 6s         |
| Fail    | -           | -          |

## Benefits

1. **Resilience**: Handles transient network errors gracefully
2. **Minimal impact**: Only adds ~6 seconds in worst case (all retries needed)
3. **Clear feedback**: Users can see retry attempts in CI logs
4. **Same behavior**: Still fails after 3 attempts if server is truly down
5. **No code changes**: Pure Makefile change, no source code modifications

## Testing

The fix has been validated:
- ✅ Makefile syntax is correct (`make -n` passes)
- ✅ Shell retry logic works correctly
- ✅ Minimal code change (30 lines added)
- ✅ No changes to source code or build artifacts

## Monitoring

To verify the fix in CI:
1. Watch for "Updating package indexes..." messages in build logs
2. If retries occur, you'll see: "⚠ Core index update failed (attempt X/3)..."
3. Successful builds will show: "✓ Core index updated successfully"

## Future Improvements

If this doesn't fully resolve the issue, consider:
1. Increasing retry attempts from 3 to 5
2. Adding longer backoff delays
3. Implementing cache fallback (skip update if cached indexes exist)
4. Using a CDN mirror for package indexes

## Related Issues

- GitHub Actions run: https://github.com/rvdbreemen/OTGW-firmware/actions/runs/22051453682
- ESP8266 Arduino package index: https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json
