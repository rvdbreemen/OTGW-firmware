With release 0.9.1 a list of set commands have been added to the MQTT set topic list. So from now on you can control your OTGW through MQTT using a list of logical names "set command" topics. 

In the table below is the list of all the set commands available to you, and how they translate to the OTGW commands. 

|settopic  | OTGW command send  | value   |
|-----|---|---|
|command | <any command>  | string  |
|setpoint|TT|temp = [number e.g. 20.5]|
|constant|TC|temp = [number e.g. 20.5]|
|outside|OT|temp = [number e.g. 20.5]|
|hotwater|HW|0 = off, 1 = on, P = Push, A or other = Auto|
|gatewaymode|GW| on = 1 or off = R or 0|
|setback|SB|temp = [number e.g. 20.5]|
|maxchsetpt|SH|temp = [number e.g. 20.5]|
|maxdhwsetpt|SW|temp = [number e.g. 20.5]|
|maxmodulation|MM|level = [0-100]%|
|ctrlsetpt|CS|temp = [number e.g. 20.5]|
|ctrlsetpt2|C2|temp = [number e.g. 20.5]|
|chenable|CH|on = 1 or off = 0|
|chenable2|H2|on = 1 or off = 0|
|ventsetpt|VS|level = [0-100]%|
|temperaturesensor|TS| function|
|addalternative|AA|function|
|delalternative|DA|function|
|unknownid|UI|function|
|knownid|KI|function|
|priomsg|PM|function|
|setresponse|SR|function|
|clearrespons|CR|function|
|resetcounter|RS|function|
|ignoretransitations|IT|function|
|overridehb|OH|function|
|forcethermostat|FT|function|
|voltageref|VR|function|
|debugptr|DP|function|

To find out what all commands mean you should read Schelte's firmware command page:
https://otgw.tclcode.com/firmware.html

So from 0.9.1 onward you can simply send the right command, using a right set command.

topic: `OTGW/set/otgw-8CAAB55908D6/setpoint`<br>
value: `21.5`

The firmware will receive the MQTT message from the set topic and forward it to the OTGW this: `TT=21.5`

The generic command set topic is still available, to make sure it's backward compatible to all pre-0.9.1 releases.

topic: `OTGW/set/otgw-8CAAB55908D6/command`<br>
value: `TT=21.5`

The firmware will convert this to the same message for the OTGW: `TT=21.5`

This way your automation in Node-red or YAML will be more readable. 