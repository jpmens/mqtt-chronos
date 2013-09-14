# mqtt-chronos

![logo](clockface300.png)

_mqtt-chronos_ periodically publishes time information to an [MQTT] broker, on a topic branch of your choosing (default: `system/nodename/chronos/`). Long-lived time data (e.g. weekday, year, etc.) are published with a _retain_ flag set, whereas short-lived periods (minutes, second) are published without.

## Usage

_mqtt-chronos_ understands the following options:

* `-h` specify the hostname or IP address to connect to. Default is `localhost`.
* `-p` specify the TCP port number of the MQTT broker. Default is `1883`.
* `-i` specify an interval in seconds (mininum: 1) to publish information. Default: 10.
* `-t` specify a topic branch prefix. Up to three `%s` are each replaced by the system's node name. Default: `system/%s/chronos`.
* `-C` specify the path to a PEM-encoded CA certificate for connecting to a broker over TLS.
* `-U` use UTC (default: True)
* `-L` use localtime (default: False)
* `-I` use TLS PSK. This is the identity string.
* `-K` use TLS PSK. This is the key string.

## Example

Starting _mqtt-chronos_ with a 60 second interval. (Blank lines show where the breaks are.)

```
system/tiggr/chronos/uptime 0
system/tiggr/chronos/utc/tics 1379091521
system/tiggr/chronos/utc/year 2013
system/tiggr/chronos/utc/month 9
system/tiggr/chronos/utc/day 13
system/tiggr/chronos/utc/weekday Friday
system/tiggr/chronos/utc/dow 5
system/tiggr/chronos/utc/week 37
system/tiggr/chronos/utc/isodate 2013-09-13
system/tiggr/chronos/utc/hour 16
system/tiggr/chronos/utc/ampm PM
system/tiggr/chronos/utc/minute 58
system/tiggr/chronos/utc/second 41
system/tiggr/chronos/utc/time 16:58:41

system/tiggr/chronos/uptime 60
system/tiggr/chronos/utc/tics 1379091581
system/tiggr/chronos/utc/minute 59
system/tiggr/chronos/utc/time 16:59:41

system/tiggr/chronos/uptime 120
system/tiggr/chronos/utc/tics 1379091641
system/tiggr/chronos/utc/hour 17
system/tiggr/chronos/utc/ampm PM
system/tiggr/chronos/utc/minute 0
system/tiggr/chronos/utc/time 17:00:41
```

## Note

* _mqtt-chronos_ publishes a non-retained topic `uptime` specifying for how long it has been up; the value is in seconds.
* Specifying both `-U` and `-L` effectively disable all time publishes except `uptime`. This is a really, really cool feature. :-)

  [mqtt]: http://mqtt.org
  [mosquitto]: http://mosquitto.org
