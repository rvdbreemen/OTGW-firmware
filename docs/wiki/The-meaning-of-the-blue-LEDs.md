- LED1 - Near the Antenna - GPIO2
- LED2 - On NodeMCU board - GPIO16

OTGW version 2.1 onwards uses WeMos D1 mini which only has LED1 (GPIO2).

There are two distinct phases and behaviours of the blue LEDs. 

# 1. Setup phase
During the setup phase, there is a distinct order and meaning to the behaviour of the LEDs.

The normal setup phase is as follows:
1. Both LEDs turn on during reboot/reset/power-on.
2. Wifi starts up, if connected successfully LED1 blinks 3 times, and then turns OFF. 
3. Other setup processes then occur: Telnet (debug), NTP time sync, MQTT connection, Bonjour, OTGW reset, OTGW Stream and finally the Watchdog. 
4. When all setup is done, LED2 blinks 3 times and then turns OFF.

This process takes 1-3 seconds. It is designed to be very fast, so if it needs to recover for any reason, then it only takes a few seconds.

Abnormal LED behaviours:
1. Both LEDs 1 and 2 are solid blue for more than a few seconds. 
This indicated that the Wi-fi is not connected and has moved into Wi-fi configuration mode. If this happens, you can easily set up your wi-fi, read more here: [Configure WiFi settings using the Web UI]. The configuration mode happens when you boot the very first time, but also when your wi-fi changes SSID, you change your wifi password. 

2. LED1 turns off, after more than a few seconds (up to 10 seconds).
Most likely you are having wifi connection issues, either your wifi is bad or you are far away from your wifi. Under normal good wifi conditions, you should really connect fast, but if it takes a long time to connect, then this indicates you have wifi issues. This in turn can cause all kinds of weird behaviour.

3. LED2 never turns OFF
During the second phase of setup (it might even reboot on watchdog resets in a bootloop) the NTP time sync is happening. If for any reason you have prevented your device from accessing the internet, then it cannot resolve DNS to external NTP source. Then you might see the LED2 stay solid for a long time, seconds, timeout is long (>30s). This does not mean it will not start, but it will wait for the timeout to happen. If this happens you should turn OFF NTP sync in the UI settings screen.


# 2. Normal operational behaviour
So under normal conditions the LED1 and LED2 blink regularly. They both respond to different processes that happen in the firmware:

1. LED1 blinks based on resetting the watchdog timer. This should happen about every 1 second. It depends on some internal processes, so it will vary a tiny bit, but never longer than 3 seconds. If longer, then the external Watchdog will reset the nodeMCU and everything is reset.
2. LED2 blinks based on the receive of OT messages from OTGW gateway. The frequency totally depends on the external messages from the boiler and the thermostat, so it should blink all the time when both are connected.


# Boot loops
Sometimes it happens that you see a pattern known as a boot loop. The LEDs turn on, turn off blinking 3x, then a few seconds and the whole process repeats. The only way to troubleshoot these situations is to clearly describe the LEDs behaviour when you report the issue, perhaps even record a video. But also please capture the debug logs from port 23 using a remote telnet session. 

In a boot loop it might be hard to catch the messages, if you are in a "short" bootloop, like every 3-5 seconds. If this occurs, disconnect your OTGW first, and try connecting the NodeMCU using USB, and open op the USB Serial port, capture that log first. You will see some debug logs, like Wifi connecting, and IP port number. Then when you have that, you can try to capture debug logs on port 23. Still disconnected from the OTGW hardware. 

If you have captured some logs, and you can't explain what is going on, then open up an issue, or come and join us on the Discord server.