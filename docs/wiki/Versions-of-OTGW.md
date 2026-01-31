The original hardware design was by Schelte Bron in 2009. This communicates via a serial port and does not include an ESP device, although this can be added as a separate module. https://otgw.tclcode.com/index.html#intro

This firmware runs on an ESP8266 attached to the OTGW serial port. We recommend the Nodo Shop kit which is designed to work with a WeMos D1 Mini.
https://www.nodo-shop.nl/nl/48-opentherm-

There have been several different versions made by Nodo Shop:
| PCB version  | Features | Documentation |
|---|---|---|
| <2.0 | 24V | [Dutch](https://github.com/rvdbreemen/OTGW-firmware/blob/main/hardware/Montage_aansluiten_OTGWV7.pdf)<br>[English](https://github.com/rvdbreemen/OTGW-firmware/blob/main/hardware/Assembly_and_use_OTGWV7E.pdf)  |
| 2.0  | 24V | [English](https://github.com/rvdbreemen/OTGW-firmware/blob/main/hardware/Assembly_and_use_OTGWV2.0.pdf) |
| 2.4- | 5V | [English/Dutch](https://www.nodo-shop.nl/en/index.php?controller=attachment&id_attachment=34) |

On versions before 2.3 there are jumpers that need to be set correctly. If not, the firmware will be unable to communicate with the OTGW.