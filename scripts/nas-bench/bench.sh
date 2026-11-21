#!/bin/sh
# OTGW bench captures on the NAS. Runs survive laptop reboots (setsid + nohup).
#   bench.sh start <run> <ip> <minutes> <job>...   jobs: tel  info:<s>  health:<s>  snap:<s>  mqtt:<topic> (the unit's own broker)
#   bench.sh stop <run>        bench.sh status [run]        bench.sh list
# Output: /volume1/docker/otgw-bench/runs/<run>/   (pids in <run>/pids)
BASE=/volume1/docker/otgw-bench; BIN=$BASE/bin; PY=/usr/bin/python3; DOCKER=/usr/local/bin/docker
cmd=$1; run=$2; R=$BASE/runs/$run
case "$cmd" in
start)
  ip=$3; min=$4; shift 4
  # one telnet client per unit: refuse when another live run already targets this ip
  for o in $BASE/runs/*; do [ -f "$o/pids" ] && [ "$o" != "$R" ] || continue
    grep -q "ip=$ip " "$o/run.log" 2>/dev/null || continue
    while IFS=: read -r p k; do [ "$p" != docker ] && kill -0 "$p" 2>/dev/null && { echo "REFUSED: run $(basename "$o") still active on $ip (stop it first)"; exit 2; }; done < "$o/pids"
  done
  mkdir -p "$R"; echo "$(date '+%F %T') start ip=$ip min=$min jobs=$*" >> "$R/run.log"
  for job in "$@"; do
    kind=${job%%:*}; arg=${job#*:}
    case "$kind" in
      tel)  setsid nohup $PY -I $BIN/tellog.py "$ip" "$R" "$min" >/dev/null 2>>"$R/err.log" < /dev/null & ;;
      info|health|snap) setsid nohup $PY -I $BIN/poll.py "$kind" "$ip" "$R" "$min" "$arg" >/dev/null 2>>"$R/err.log" < /dev/null & ;;
      mqtt) # The broker the unit itself uses (Robert 2026-11-21: the Home Assistant broker; host, port, user and
            # password from the unit's /settings.ini; a .local host is resolved by mDNS in mqttcfg.py). The env file
            # holds the password: it is removed once docker has read it, so it never lands in the evidence tar.
            $PY -I $BIN/mqttcfg.py "$ip" "$R" >> "$R/run.log" 2>>"$R/err.log" || { echo "mqtt: no broker config (see err.log)"; continue; }
            name="otgwbench-$run-mqtt"; $DOCKER run -d --rm --name "$name" --network host --env-file "$R/.mqtt.env" -e TZ=Europe/Amsterdam \
              eclipse-mosquitto:latest sh -c 'exec timeout "$1" mosquitto_sub -h "$MQTT_HOST" -p "$MQTT_PORT" ${MQTT_USER:+-u "$MQTT_USER"} ${MQTT_PASS:+-P "$MQTT_PASS"} -t "$2" -v -F "%I %t %p"' \
              sh "$((min*60))" "$arg" > /dev/null; rm -f "$R/.mqtt.env"
            ( $DOCKER logs -f "$name" >> "$R/mqtt.log" 2>&1 & ); echo "docker:$name" >> "$R/pids"; continue ;;
      *) echo "unknown job $job"; continue ;;
    esac
    echo "$!:$kind" >> "$R/pids"
  done; cat "$R/pids" ;;
stop)
  [ -f "$R/pids" ] || { echo "no run $run"; exit 1; }
  while IFS=: read -r p k; do
    if [ "$p" = docker ]; then $DOCKER stop "$k" >/dev/null 2>&1; else kill "$p" 2>/dev/null; fi
  done < "$R/pids"; echo "$(date '+%F %T') stop" >> "$R/run.log"; mv "$R/pids" "$R/pids.stopped" ;;
status)
  for d in ${run:+$R} ${run:-$BASE/runs/*}; do [ -f "$d/pids" ] || continue; echo "== $(basename "$d")"
    while IFS=: read -r p k; do
      if [ "$p" = docker ]; then echo "$k $($DOCKER inspect -f '{{.State.Status}}' "$k" 2>/dev/null || echo gone)"
      else kill -0 "$p" 2>/dev/null && echo "$p $k running" || echo "$p $k ended"; fi
    done < "$d/pids"; ls -la "$d" | awk 'NR>3{print "   ",$5,$9}'; done ;;
list) ls -1 $BASE/runs ;;
*) sed -n 2,5p "$0" ;;
esac
