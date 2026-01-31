The REST API is build for interaction with the ESP 8266 firmware, and, the OpenTherm Gateway.

# System API (v0)
## GET /api/v0/devtime 
Returns the device time

## GET /api/v0/devinfo 
Returns the device information 

# OTGW API (v0)
## GET /api/v0/otgw/{msgid}
Returns the value of the {msgid} requested. The JSON contains: Msg description, Msg value, Msg Unit (if any).
The msgid between 0 and 127 are used, 128+ are reserved according to the specification.

# OTGW API (v1)
The API v1 adds a little more than this:

## GET /api/v1/otgw/otmonitor/
Returns a JSON key value pair array, with all relevant OTGW monitor values.

## GET /api/v1/otgw/telegraf/
Returns a JSON key value pair array, compatible with telegraf, with all relevant OTGW monitor values.

## GET /api/v1/otgw/id/{msgid}
Returns the value of the {msgid} requested. 
The JSON contains: Msg description, Msg value, Msg Unit (if any).
The msgid between 0 and 127 are used, 128+ are reserved according to the specification.

## GET /api/v1/otgw/label/{msglabel}
Returns the value of the {msgid by label} that is requested. 
The JSON contains: Msg description, Msg value, Msg Unit (if any).
The msgid between 0 and 127 are used, 128+ are reserved according to the specification.

## POST /api/v1/otgw/command/{command text}
Post to this API and it will send the {command text} to the OTGW.
Should return a simple OK. 
At this time it does not check the correct receive of the command by the OTGW.  
**warning: not fully battletested, it should work**

I would like to write a Swagger API document, but that takes more time.

  