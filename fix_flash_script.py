#!/usr/bin/env python3
"""
Fix flash_otgw.sh: replace interactive prompt with auto-download
"""

# Read the current file
with open('flash_otgw.sh', 'r', encoding='utf-8') as f:
    lines = f.readlines()

output = []
i = 0
while i < len(lines):
    line = lines[i]
    
    # Find start of interactive section (around line 266)
    if 'if [ -z "$FW_FILE" ] || [ -z "$FS_FILE" ];' in line and i > 260:
        # Replace with auto-download
        output.append('if [ -z "$FW_FILE" ] || [ -z "$FS_FILE" ]; then\n')
        output.append('    info "Binaries not found locally, downloading latest release from GitHub..."\n')
        output.append('    \n')
        output.append('    RELEASE_API="https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases/latest"\n')
        output.append('    \n')
        output.append('    if command -v curl >/dev/null 2>&1; then\n')
        output.append('        RELEASE_JSON=$(curl -fSL "$RELEASE_API" 2>/dev/null) || {\n')
        output.append('            err "Failed to fetch release information from GitHub"\n')
        output.append('            err "Please check internet connection or run: ./build.sh"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    elif command -v wget >/dev/null 2>&1; then\n')
        output.append('        RELEASE_JSON=$(wget -qO- "$RELEASE_API" 2>/dev/null) || {\n')
        output.append('            err "Failed to fetch release information from GitHub"\n')
        output.append('            err "Please check internet connection or run: ./build.sh"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    else\n')
        output.append('        err "curl or wget required for downloading"\n')
        output.append('        exit 1\n')
        output.append('    fi\n')
        output.append('    \n')
        output.append('    # Parse URLs (fragile but no jq dependency)\n')
        output.append('    FW_URL=$(echo "$RELEASE_JSON" | grep -o \'"browser_download_url": "[^"]*\\.ino\\.bin"\' | head -1 | sed \'s/.*: "\\(.*\\)"/\\1/\')\n')
        output.append('    FS_URL=$(echo "$RELEASE_JSON" | grep -o \'"browser_download_url": "[^"]*\\.littlefs\\.bin"\' | head -1 | sed \'s/.*: "\\(.*\\)"/\\1/\')\n')
        output.append('    \n')
        output.append('    if [ -z "$FW_URL" ] || [ -z "$FS_URL" ]; then\n')
        output.append('        err "Could not find firmware binaries in latest GitHub release"\n')
        output.append('        err "Please run: ./build.sh"\n')
        output.append('        exit 1\n')
        output.append('    fi\n')
        output.append('    \n')
        output.append('    info "Downloading $(basename "$FW_URL")..."\n')
        output.append('    if command -v curl >/dev/null 2>&1; then\n')
        output.append('        curl -fSL -o "$SCRIPT_DIR/$(basename "$FW_URL")" "$FW_URL" || {\n')
        output.append('            err "Failed to download firmware"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    else\n')
        output.append('        wget -O "$SCRIPT_DIR/$(basename "$FW_URL")" "$FW_URL" || {\n')
        output.append('            err "Failed to download firmware"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    fi\n')
        output.append('    FW_FILE="$SCRIPT_DIR/$(basename "$FW_URL")"\n')
        output.append('    \n')
        output.append('    info "Downloading $(basename "$FS_URL")..."\n')
        output.append('    if command -v curl >/dev/null 2>&1; then\n')
        output.append('        curl -fSL -o "$SCRIPT_DIR/$(basename "$FS_URL")" "$FS_URL" || {\n')
        output.append('            err "Failed to download filesystem"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    else\n')
        output.append('        wget -O "$SCRIPT_DIR/$(basename "$FS_URL")" "$FS_URL" || {\n')
        output.append('            err "Failed to download filesystem"\n')
        output.append('            exit 1\n')
        output.append('        }\n')
        output.append('    fi\n')
        output.append('    FS_FILE="$SCRIPT_DIR/$(basename "$FS_URL")"\n')
        output.append('    \n')
        output.append('    ok "Downloaded binaries from GitHub"\n')
        output.append('fi\n')
        
        # Skip the old interactive section (up to line ~361)
        while i < len(lines):
            if lines[i].strip() == 'fi' and i > 350:
                i += 1
                break
            i += 1
        continue
    
    output.append(line)
    i += 1

# Write with Unix line endings
with open('flash_otgw.sh', 'w', encoding='utf-8', newline='\n') as f:
    f.writelines(output)

print('✓ Updated flash_otgw.sh with auto-download')
