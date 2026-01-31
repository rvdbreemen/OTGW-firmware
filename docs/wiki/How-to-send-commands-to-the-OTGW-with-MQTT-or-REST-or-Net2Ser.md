To control the OpenTherm Gateway you have three options:
1. MQTT 
2. REST API calls over HTTP
3. Net2Serial

# Sending commands over MQTT
To control the OTGW you can publish commands to this topic `<top topic>/set/<node id>/command`.
Default top topic: 'OTGW'

Example topics: 
`OTGW/set/otgw-1234567890AB/command`

So sending Outside Temperature to your OTGW with a local measurement of the outside temperature then it is this:
topic: `OTGW/set/otgw-1234567890AB/command`
value: OT=21.5

The messages are put in a command queue. The command queue will try to deliver the message and wait for a valid response from the OTGW PIC. If it gets a confirmation, then it will remove the command from the queue. If it does not respond, then the command is resent after 5 seconds. This continues for a maximum of 5 retries. After 5 retries, the command is always removed from the queue. 

# Sending commands over REST
To control the OTGW using REST calls you can send the commands to `/api/v1/otgw/command/{any command}`.

To test this you can simply use cURL to PUT or POST a command:
`curl -X PUT http://otgw.local/api/v1/otgw/command/OT=10`

This will send sent the Toutside = 10C to your boiler.

Commands are added to the command queue, just like with MQTT and handled the same way. 

# Sending commands over the network through serial
The final option to control the OTGW is through the network2serial interface. Just connect to port 25238. This will give you access to the raw serial data interface over the network. The communication is bidirectional. This can be used to connect the orginal OTmonitor application or the Domoticz net2ser component. 

