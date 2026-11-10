# PIC not detected on an OTGW Classic with an S3 Mini (2.x firmware)

You put a LOLIN S3 Mini on your OTGW Classic, flashed 2.0.0, and there is no OpenTherm traffic. Open `http://<ip-of-your-otgw>/api/v2/device/info`. If you see `"hardwaremode": "Degraded"` and `"otcommandinterface": "None"`, the firmware looked for the PIC at boot and got no answer.

## Try this first

1. **Seating.** The S3 Mini has two pin rows per side. Only the **outer** rows go into the Wemos socket; the inner rows stay outside. Check from the side that every outer pin is fully in and none is bent or shifted by a row.
2. **Full power cycle.** Power off for ten seconds, then on. A web UI reboot restarts only the ESP.
3. **Board mode = Auto-detect.** Settings, System, Board mode, Auto-detect, save, reboot. Never pick *OT-Direct (no PIC)* on a Classic.
4. **Old module.** If the D1 mini with 1.x still sees the PIC, PIC and board are fine; look at the S3 Mini and the socket.

## Read the boot record

Boot detection runs before Telnet starts, so a Telnet capture cannot show it. Download `/bootdetect.log` from the FSexplorer (`http://<ip>/FSexplorer.html`). Newest boot on top:

```text
boot#13 t=6851ms eMode=3 pic=0 mode=0 pro=0 RST=12 RX=44 TX=43 I2C=35/36
```

`eMode=1 pic=1` means the PIC was found. `eMode=3 pic=0` means no answer. With `RST=12 RX=44 TX=43` the firmware used the right S3 Mini pins, so look at the hardware, not at a setting.

## Asking for help

Post in Discord or a GitHub issue with: `/bootdetect.log`, the `/api/v2/device/info` output, a capture with the capture script from the flash zip (start it, reboot the gateway from the web UI, let it run a few minutes, stop with `Q`), side photos of both pin rows, the firmware version and the PCB revision.

More detail: the user manual, chapter 7, section 7.1a ([EN](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/manuals/en/ch07-troubleshooting.md), [NL](https://github.com/rvdbreemen/OTGW-firmware/blob/dev/docs/manuals/nl/h07-probleemoplossing.md)).
