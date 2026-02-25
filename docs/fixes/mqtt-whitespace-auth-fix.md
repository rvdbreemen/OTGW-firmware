# MQTT Authentication Fix: Whitespace Trimming

## Issue Description

After upgrading to OTGW firmware version 1.0, some users experienced MQTT authentication failures with the "addons" user account (commonly used in Home Assistant). The password in `settings.ini` was correct, but MQTT login still failed. Creating a new user with a new password resolved the issue.

## Root Cause

The issue was caused by **leading or trailing whitespace** in the MQTT username or password fields stored in the `settings.ini` file. When credentials are read from the JSON settings file, they are copied using `strlcpy()` which preserves all characters including whitespace.

In version 1.0, changes to string handling (introduction of the CSTR macro with null-pointer protection) meant that credentials were passed to the MQTT broker exactly as stored, including any whitespace. For example:

- Stored username: `"addons "` (with trailing space)
- Actual username in MQTT broker: `"addons"` (no space)
- Result: Authentication fails because `"addons "` ≠ `"addons"`

## Why It Worked in Earlier Versions

Earlier firmware versions may have had different string handling or the issue may have been masked by other code paths. The specific combination of:

1. The new CSTR macro introduced in v1.0
2. Direct use of char arrays without trimming
3. Settings migrated from older versions potentially containing whitespace

Created a scenario where authentication would fail even though the credentials appeared correct to the user.

## The Fix

The fix adds whitespace trimming to MQTT username and password in two critical locations:

### 1. Reading Settings from settings.ini (`readSettings()`)

When settings are loaded from the JSON file at startup:

```cpp
strlcpy(settingMQTTuser, doc[F("MQTTuser")] | "", sizeof(settingMQTTuser));
// Trim leading/trailing whitespace from username
char* trimmedUser = trimwhitespace(settingMQTTuser);
if (trimmedUser != settingMQTTuser) {
  memmove(settingMQTTuser, trimmedUser, strlen(trimmedUser) + 1);
}
```

### 2. Updating Settings via API/Web UI (`updateSetting()`)

When settings are changed through the REST API or Web UI:

```cpp
if (strcasecmp_P(field, PSTR("MQTTuser"))==0) {
  strlcpy(settingMQTTuser, newValue, sizeof(settingMQTTuser));
  // Trim leading/trailing whitespace from username
  char* trimmedUser = trimwhitespace(settingMQTTuser);
  if (trimmedUser != settingMQTTuser) {
    memmove(settingMQTTuser, trimmedUser, strlen(trimmedUser) + 1);
  }
}
```

## Implementation Details

### trimwhitespace() Function

The existing `trimwhitespace()` function in `helperStuff.ino` handles both leading and trailing whitespace:

```cpp
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}
```

### Important: Pointer Shift Handling

The function returns a potentially **shifted pointer** if leading whitespace was removed. This is why we check if the returned pointer differs and use `memmove()` to shift the content back:

```cpp
char* trimmed = trimwhitespace(buffer);
if (trimmed != buffer) {
  // Leading whitespace was removed, pointer shifted
  memmove(buffer, trimmed, strlen(trimmed) + 1);
}
```

## Testing

The fix was validated with a C++ test program that confirmed trimming works correctly for:

- Clean credentials: `"addons"` → `"addons"`
- Trailing space: `"addons "` → `"addons"`
- Leading space: `" addons"` → `"addons"`
- Both spaces: `" addons "` → `"addons"`

All test cases resulted in the correctly trimmed string with length 6.

## Impact

This fix ensures that:

1. **Legacy settings** with whitespace will work correctly after upgrade
2. **User input errors** (accidental spaces) won't cause authentication failures
3. **MQTT authentication** is more robust and user-friendly

## Migration Notes

Users experiencing this issue should:

1. Update to the fixed firmware version
2. Either:
   - **Option A**: No action needed - the fix will automatically trim credentials on next boot
   - **Option B**: Manually edit `settings.ini` to remove any whitespace from MQTTuser and MQTTpasswd fields

The firmware will handle the trimming automatically, so manual editing is optional but can provide peace of mind.

## Related Files

- `src/OTGW-firmware/settingStuff.ino` - Settings read/write functions
- `src/OTGW-firmware/helperStuff.ino` - trimwhitespace() utility function
- `src/OTGW-firmware/MQTTstuff.ino` - MQTT connection logic

## Commit

- Commit: eba5d51
- PR: copilot/fix-thermostat-login-issue
- Date: 2026-02-10
