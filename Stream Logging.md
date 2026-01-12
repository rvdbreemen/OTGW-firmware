# How to Use Stream Logging in OTGW Firmware

The **Stream to File** feature allows you to automatically save all OpenTherm log data directly to a file on your computer in real-time. This is useful for long-term debugging or data collection without overloading the OTGW's internal memory.

## 1. Requirements

This feature uses the modern **File System Access API** for efficient, real-time file streaming with automatic backpressure management.

### Supported Browsers:
*   **Google Chrome** (Desktop, version 86+)
*   **Microsoft Edge** (Desktop, version 86+)
*   **Opera** (Desktop, version 72+)
*   **Safari** (Desktop, version 15.2+, limited support)
*   ❌ **Firefox** - Not yet supported (in development)
*   ❌ **Mobile browsers** - Not supported (Android/iOS)

### Connection Security:
*   This feature **requires a Secure Context** (`HTTPS` or `localhost`).
*   Since your OTGW likely runs on a local IP address (e.g., `http://192.168.1.50`), which is considered "insecure" by browsers, you must perform a **one-time browser configuration** (see below).

## 2. One-Time Browser Setup (Important!)

If you are accessing your OTGW via an IP address (like `192.168.x.x`), you must configure your browser to treat it as a "secure context" to enable file streaming.

### For Chrome or Edge:

1.  Open your browser
2.  In the address bar, type:
    *   **Chrome:** `chrome://flags/#unsafely-treat-insecure-origin-as-secure`
    *   **Edge:** `edge://flags/#unsafely-treat-insecure-origin-as-secure`
3.  Find the flag named **"Insecure origins treated as secure"**
4.  Change the setting from `Disabled` to **`Enabled`**
5.  In the text box below it, enter the URL of your OTGW:
    *   Example: `http://192.168.1.50` (replace with your actual IP address)
    *   **Important:** Include the `http://` prefix
6.  Click the **Relaunch** button at the bottom to restart your browser

> **Why is this necessary?**  
> Browsers restrict file system access to secure contexts (HTTPS) to protect users from malicious websites. Since OTGW runs on HTTP (local network only), this configuration tells your browser to trust your specific OTGW device. This is safe because the device is on your local network and not exposed to the internet.

## 3. How to Start Logging

1.  Open the OTGW web interface
2.  Go to the **Home** page (where you see the OpenTherm Monitor section)
3.  Look for the **"Stream"** checkbox in the log controls area
4.  Check the **Stream** checkbox
5.  Your browser will display a dialog asking you to **"View files"** or **"Save files to this folder"**
6.  **Select a folder** on your computer where you want logs to be saved
7.  Click **"Select Folder"** (you may need to click **"Allow"** or **"View Files"** if prompted again)
8.  The checkbox will stay checked, and you'll see "**Log: ot-monitor-DD-MM-YYYY.log**" appear

## 4. How it Works

### Automatic File Management
*   **Daily Files:** A new log file is created each day with the naming format: `ot-monitor-DD-MM-YYYY.log`
*   **Real-time Streaming:** Log messages are written to the file in batches every 10 seconds
*   **Automatic Rotation:** At midnight, the current file is closed and a new one is created for the new day
*   **Session Markers:** Each streaming session is marked with timestamps in the log file

### Performance & Backpressure
The implementation includes intelligent queue management to prevent memory issues:

*   **Write Queue:** Messages are queued and written in batches to minimize disk I/O
*   **Backpressure Control:** If the queue grows beyond 10,000 lines, new messages are dropped to prevent memory exhaustion
*   **Queue Monitoring:** The UI shows queue status when it grows large (e.g., "Queue: 5000/10000 ⚠️")
*   **Batch Processing:** Up to 5,000 lines are written at once to balance memory and performance

### Stopping the Stream
Simply uncheck the **Stream** checkbox to stop logging. Any queued messages will be written to the file before stopping.

## 5. Troubleshooting

### "File streaming is not supported by your browser"
*   **Solution:** You are likely using Firefox, Safari (older version), or a mobile browser. Use Chrome, Edge, or Opera on a desktop computer.

### "This feature requires a Secure Context (HTTPS or localhost)"
*   **Solution:** You haven't completed the one-time browser setup (see Section 2). Enable the "Insecure origins treated as secure" flag and add your OTGW's IP address.

### Permission prompt denied or "Permission denied"
*   **Solution:** If you clicked "Block" or "Cancel" when asked for folder permission:
    1.  Uncheck the Stream checkbox
    2.  Check it again to get a new permission prompt
    3.  This time, click "Allow" or "View Files"

### "Queue: XXXX/10000 ⚠️" appears in the log filename
*   **Meaning:** The write queue is growing because messages are arriving faster than they can be written to disk
*   **Solution:** This is usually temporary and will resolve itself. If it persists:
    *   Your disk may be slow (external drive, network drive, etc.)
    *   Consider using a local SSD for better performance
    *   The system will automatically drop messages if the queue reaches 10,000 lines to prevent crashes

### "File stream write failed" errors
*   **Disk Full:** Free up disk space in the selected folder
*   **Permission Revoked:** The browser may have lost permission to the folder. Stop streaming and start again, re-granting permission
*   **File Locked:** Another program may have the log file open. Close any text editors or log viewers that have the file open

### Nothing is being written to the file
*   Verify the Stream checkbox is checked and shows the current log filename
*   Check browser console (F12) for any error messages
*   Try stopping and restarting the stream
*   Verify you completed the one-time browser setup correctly

## 6. Technical Details

### Security Considerations
*   **Local Network Only:** The OTGW WebSocket server is unauthenticated and should only be used on trusted local networks
*   **User Consent Required:** Browsers require explicit user permission before allowing file system access
*   **Permission Persistence:** Modern browsers remember your folder choice using the "otmonitor-logs" permission identifier

### Browser Compatibility Notes
*   **Standard WebSocket:** The implementation uses standard WebSocket (not WebSocketStream) for broad compatibility
*   **No Native Backpressure:** Since WebSocket.pause() is not available, backpressure is managed via queue limits
*   **Fallback Option:** If streaming is not supported, use the "Download" button to manually save logs as a fallback

### Performance Characteristics
*   **Write Frequency:** Every 10 seconds (configurable in code)
*   **Batch Size:** Up to 5,000 lines per write operation
*   **Queue Limit:** 10,000 lines maximum (older messages dropped if exceeded)
*   **Memory Usage:** Approximately 200-500 bytes per queued line
*   **Disk I/O:** Minimized through batching and append-only operations

For more information about the File System Access API, see:
https://developer.mozilla.org/en-US/docs/Web/API/File_System_Access_API
