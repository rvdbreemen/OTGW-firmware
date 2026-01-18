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

## Troubleshooting

*   **"File streaming is not supported by your browser":** You are likely using Firefox, Safari, or a mobile device. Please use Chrome or Edge on a desktop computer.
*   **Permission Prompt Denied:** If you accidentally clicked "Block" or "Cancel" when asked for folder permission, uncheck the box and check it again to retry.
*   **Nothing is saved:** Ensure you followed the "One-Time Setup" to enable secure context for your IP. Without that, the feature might fail silently or show an error.
