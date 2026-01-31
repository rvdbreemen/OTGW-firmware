De REST API is gebouwd voor interactie met de ESP 8266-firmware en de OpenTherm Gateway.

# Systeem-API (v0)
## `GET /api/v0/devtime`
Geeft de apparaattijd, epoch en bericht door.

```
{"devtime":[
  {"name": "dateTime", "value": "2021-02-28 20:22:07"},
  {"name": "epoch", "value": 1614543727},
  {"name": "message", "value": "No OTGW connected!"}
]}
```

## `GET /api/v0/devinfo`
Retourneert de apparaatinformatie

```
{"devinfo":[
  {"name": "author", "value": "Robert van den Breemen"},
  {"name": "fwversion", "value": "0.7.8+718 (27-02-2021)"},
  {"name": "picfwversion", "value": "No OTGW connected!"},
  {"name": "compiled", "value": "Feb 27 2021 23:33:37"},
  {"name": "hostname", "value": "OTGW"},
  {"name": "ipaddress", "value": "192.168.178.24"},
  {"name": "macaddress", "value": "8C:AA:B5:59:08:D6"},
  {"name": "freeheap", "value": 18592},
  {"name": "maxfreeblock", "value": 13224},
  {"name": "chipid", "value": "5908d6"},
  {"name": "coreversion", "value": "2_7_4"},
  {"name": "sdkversion", "value": "2.2.2-dev(38a443e)"},
  {"name": "cpufreq", "value": 160},
  {"name": "sketchsize", "value": 495.625},
  {"name": "freesketchspace", "value": 1552.000},
  {"name": "flashchipid", "value": "001640D8"},
  {"name": "flashchipsize", "value": 4.000},
  {"name": "flashchiprealsize", "value": 4.000},
  {"name": "LittleFSsize", "value": 2.000},
  {"name": "flashchipspeed", "value": 40.000},
  {"name": "flashchipmode", "value": "DIO"},
  {"name": "boardtype", "value": "ESP8266_NODEMCU"},
  {"name": "ssid", "value": "Koekie"},
  {"name": "wifirssi", "value": -83},
  {"name": "mqttconnected", "value": "false"},
  {"name": "ntpenabled", "value": "true"},
  {"name": "ntptimezone", "value": "Europe/Amsterdam"},
  {"name": "uptime", "value": "0(d)-00:59(H:m)"},
  {"name": "lastreset", "value": "External System"},
  {"name": "bootcount", "value": 1}
]}
``` 


# OTGW API (v0)
## 'GET /api/v0/otgw/{msgid}'
Retourneert de waarde van de aangevraagde {msgid}. De JSON bevat: Msg-beschrijving, Msg-waarde, Msg-eenheid (indien aanwezig).
Het msgid tussen 0 en 127 wordt gebruikt, 128+ zijn gereserveerd volgens de specificatie.

```
{
  "label": "Status",
  "value": 0,
  "unit": ""
}
```

# OTGW API (v1)
De API v1 voegt iets meer toe dan dit:

## `GET /api/v1/otgw/otmonitor/`
Retourneert een array van een JSON-sleutelwaardepaar, met alle relevante OTGW-monitorwaarden.

```
{"otmonitor":[
  {"name": "flamestatus", "value": "Off", "unit": "", "epoch": 0},
  {"name": "chmodus", "value": "Off", "unit": "", "epoch": 0},
  {"name": "chenable", "value": "Off", "unit": "", "epoch": 0},
  {"name": "ch2modus", "value": "Off", "unit": "", "epoch": 0},
  {"name": "ch2enable", "value": "Off", "unit": "", "epoch": 0},
  {"name": "dhwmode", "value": "Off", "unit": "", "epoch": 0},
  {"name": "dhwenable", "value": "Off", "unit": "", "epoch": 0},
  {"name": "diagnosticindicator", "value": "Off", "unit": "", "epoch": 0},
  {"name": "faultindicator", "value": "Off", "unit": "", "epoch": 0},
  {"name": "coolingmodus", "value": "Off", "unit": "", "epoch": 0},
  {"name": "coolingactive", "value": "Off", "unit": "", "epoch": 0},
  {"name": "otcactive", "value": "Off", "unit": "", "epoch": 0},
  {"name": "servicerequest", "value": "Off", "unit": "", "epoch": 0},
  {"name": "lockoutreset", "value": "Off", "unit": "", "epoch": 0},
  {"name": "lowwaterpressure", "value": "Off", "unit": "", "epoch": 0},
  {"name": "gasflamefault", "value": "Off", "unit": "", "epoch": 0},
  {"name": "airtemp", "value": "Off", "unit": "", "epoch": 0},
  {"name": "waterovertemperature", "value": "Off", "unit": "", "epoch": 0},
  {"name": "outsidetemperature", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "roomtemperature", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "roomsetpoint", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "remoteroomsetpoint", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "controlsetpoint", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "relmodlvl", "value": 0.000, "unit": "%", "epoch": 0},
  {"name": "maxrelmodlvl", "value": 0.000, "unit": "%", "epoch": 0},
  {"name": "boilertemperature", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "returnwatertemperature", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "dhwtemperature", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "dhwsetpoint", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "maxchwatersetpoint", "value": 0.000, "unit": "°C", "epoch": 0},
  {"name": "chwaterpressure", "value": 0.000, "unit": "bar", "epoch": 0},
  {"name": "oemfaultcode", "value": 0, "unit": "", "epoch": 0}
]}
```

## `GET /api/v1/otgw/id/{msgid}`
Retourneert de waarde van de aangevraagde {msgid}.
De JSON bevat: Msg-beschrijving, Msg-waarde, Msg-eenheid (indien aanwezig).
Het msgid tussen 0 en 127 wordt gebruikt, 128+ zijn gereserveerd volgens de specificatie.

```
{
  "label": "TSet",
  "value": 0,
  "unit": "°C"
}
```

## `GET /api/v1/otgw/label/{msglabel}` 
Retourneert de waarde van de aangevraagde {msgid by label}.
De JSON bevat: Msg-beschrijving, Msg-waarde, Msg-eenheid (indien aanwezig).
Het msgid tussen 0 en 127 wordt gebruikt, 128+ zijn gereserveerd volgens de specificatie.

```
{
  "label": "Tr",
  "value": 0,
  "unit": "°C"
}
```

## `POST /api/v1/otgw/command/{command text}`
Post naar deze API en het zal de {command text} naar de OTGW sturen.
Moet een simpele OK retourneren.
Op dit moment controleert het niet of het commando correct is ontvangen door de OTGW.
** waarschuwing: niet volledig getest, het zou moeten werken **

Ik zou graag een Swagger API-document willen schrijven, maar dat kost meer tijd.