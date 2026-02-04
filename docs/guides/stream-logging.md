# How to Use Stream Logging in OTGW Firmware

The **Stream to File** feature allows you to automatically save all OpenTherm log data directly to a file on your computer in real-time. This is useful for long-term debugging or data collection without overloading the OTGW's internal memory.

## 1. Requirements

Because this feature uses advanced browser capabilities (the *File System Access API*), it has specific requirements:

*   **Supported Browsers:**
    *   Google Chrome (Desktop)
    *   Microsoft Edge (Desktop)
    *   Opera (Desktop)
    *   *Note: Firefox and Safari are NOT supported.*
    *   *Note: Mobile browsers (Android/iOS) are NOT supported.*

*   **Connection Security:**
    *   This feature is designed for Secure Contexts (`HTTPS` or `localhost`).
    *   Since your OTGW likely runs on a local IP address (e.g., `http://192.168.1.50`) which is technically "insecure", you must perform a one-time configuration in your browser.

## 2. One-Time Setup (Important!)

If you are accessing your OTGW via an IP address (like `192.168.x.x`), you must tell your browser to treat it as "secure" to enable file access.

1.  Open your browser (Chrome or Edge).
2.  In the address bar, type:
    *   **Chrome:** `chrome://flags/#unsafely-treat-insecure-origin-as-secure`
    *   **Edge:** `edge://flags/#unsafely-treat-insecure-origin-as-secure`
3.  Find the flag named **"Insecure origins treated as secure"**.
4.  Change the setting from `Disabled` to **`Enabled`**.
5.  In the text box below it, enter the URL of your OTGW (e.g., `http://192.168.1.50` replace with your actual IP).
6.  Click the **Relaunch** button at the bottom of the screen to restart your browser.

> **Why is this necessary?** Browsers block websites from writing to your hard drive unless the connection is secure. This bypass allows you to grant that permission specifically for your local OTGW device.

## 3. How to Start Logging

1.  Open the OTGW web interface.
2.  Go to the **OT Monitor** tab (or the main dashboard).
3.  Look for the **"Stream to File"** checkbox (usually near the other log controls).
4.  Check the box.
5.  Your browser will pop up a window asking you to **View and save files to a folder** or **Select a directory**.
6.  Choose the folder on your computer where you want the logs to be saved.
7.  Click **Select Folder** (and **Allow** if prompted again).
8.  The checkbox will stay checked, and a "Current Log File" indicator may appear.

## 4. How it Works

*   **Automatic Files:** The system creates a new log file every day named `ot-monitor-DD-MM-YYYY.log`.
*   **Real-time:** Every line received from the OpenTherm Gateway is immediately added to the file.
*   **Rotation:** At midnight, it automatically closes the old file and starts a new one for the next day.
*   **Stopping:** Simply uncheck the box to stop logging.

## 5. Troubleshooting

*   **"File streaming is not supported by your browser":** You are likely using Firefox, Safari, or a mobile device. Please use Chrome or Edge on a desktop computer.
*   **Permission Prompt Denied:** If you accidentally clicked "Block" or "Cancel" when asked for folder permission, uncheck the box and check it again to retry.
*   **Nothing is saved:** Ensure you followed the "One-Time Setup" to enable secure context for your IP. Without that, the feature might fail silently or show an error.

---

## Technical Details

### Implementation Architecture

The Stream to File feature uses the modern **File System Access API** to write log data directly to your local filesystem. Here's how it works under the hood:

#### Write Queue and Batching

To prevent overwhelming your disk with continuous small writes, the implementation uses a smart batching system:

*   **Write Queue:** Incoming log lines are added to an in-memory queue rather than written immediately
*   **Batch Processing:** Every 10 seconds, all queued lines are written to disk in a single operation
*   **Backpressure Handling:** The queue prevents data loss if the filesystem temporarily can't keep up with incoming data
*   **Non-blocking:** Writes happen asynchronously, so the UI remains responsive even during large batch writes

This approach balances real-time logging with system performance, ensuring your computer doesn't slow down from constant disk access.

#### File Rotation

The system automatically creates a new log file each day:

*   **Naming Convention:** Files are named `ot-monitor-DD-MM-YYYY.log` (e.g., `ot-monitor-12-01-2026.log`)
*   **Date Change Check:** Every minute, the system checks if the date has changed since the last check
*   **Graceful Transition:** When a date change is detected (typically at midnight), the current queue is flushed, the old file is closed, and a new file is opened
*   **Session Markers:** Each new file starts with a "Start of logging" marker for easy identification

#### Memory vs. Streaming

**Important:** While this feature is called "streaming," it uses **buffered writes** rather than true streaming:

*   **Buffer-based:** Log lines are queued in memory (typically 10-20 seconds worth of data) before being written
*   **Why buffering?** Direct streaming to disk on every WebSocket message would cause excessive disk I/O and poor performance
*   **Memory usage:** Minimal - the queue is flushed every 10 seconds, so memory usage stays low even with high log rates
*   **Browser compatibility:** This approach works reliably across all supported browsers (Chrome, Edge, Opera)

For true streaming without any buffering, you would need the experimental `WebSocketStream` API with `pipeTo()`, which is not yet widely supported.

### Browser Compatibility

The File System Access API is a modern web standard with growing but not universal support:

| Browser | Desktop Support | Mobile Support | Notes |
|---------|----------------|----------------|-------|
| Chrome  | ✅ Yes (v86+)  | ❌ No          | Full support |
| Edge    | ✅ Yes (v86+)  | ❌ No          | Full support (Chromium-based) |
| Opera   | ✅ Yes         | ❌ No          | Full support (Chromium-based) |
| Safari  | 🔶 Limited (v15.2+) | ❌ No     | Experimental support - may require enabling feature flags |
| Firefox | ⏳ In Development | ❌ No      | Not yet available in current versions |

**For unsupported browsers:** The "Stream to File" checkbox will be disabled, and you'll see a tooltip explaining why. You can still use the **Download** button to save logs manually.

### Security and Permissions

The File System Access API has strict security requirements to protect your computer:

#### HTTPS Requirement

*   **Secure Context Only:** The API only works on pages loaded via HTTPS or `localhost`
*   **Local IP addresses:** By default, `http://192.168.x.x` is considered "insecure"
*   **Browser Flag Workaround:** The one-time setup (Step 2 above) tells your browser to treat your OTGW's IP as secure
*   **Why?** Allowing arbitrary websites to write to your disk would be a massive security risk

#### Permission Model

*   **User Prompt:** You must explicitly grant permission by selecting a directory
*   **Per-session:** Permissions are granted per browser session - you may need to re-grant after browser restart
*   **Revocable:** You can revoke access at any time by closing the browser or unchecking the Stream checkbox
*   **Directory-scoped:** The firmware can only write to the directory you selected, nowhere else on your system

#### Error Handling

The implementation includes robust error handling:

*   **Permission Denied:** If you click "Cancel" on the permission prompt, the checkbox automatically unchecks
*   **Write Errors:** If a write fails (e.g., disk full, permission revoked), streaming stops automatically and you're notified
*   **`DOMException` Handling:** All File System Access API calls are wrapped in try-catch blocks to handle browser security exceptions gracefully

### Fallback for Older Browsers

If your browser doesn't support the File System Access API, you have two alternatives:

1.  **Manual Download:** Click the "Download" button to save the current log buffer as a text file
2.  **Auto-Download:** Enable the "Auto" checkbox next to Download to automatically save the log every 15 minutes

These methods work in all browsers but don't provide true real-time streaming - they save a snapshot of the log buffer at a specific moment.

### Advanced: Future Improvements

The current implementation could be enhanced with:

*   **WebSocketStream API:** Once widely supported, this would enable true streaming with automatic backpressure handling via `pipeTo()`
*   **Web Workers:** For extremely high log rates (1000+ lines/second), processing could be offloaded to a Web Worker to prevent UI lag
*   **Origin Private File System:** For temporary storage, this would provide synchronous write access without HTTPS requirements
*   **IndexedDB Fallback:** For browsers without File System Access API, logs could be stored in IndexedDB and downloaded periodically

These are not currently implemented to keep the code simple and maintainable.
