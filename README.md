# WA5ZNU fork of demcusensor

ESP8266/NodeMCU 1.0 PM2.5/PM10/AQI monitoring application for PMS5003 dust sensor.

# Setup
- Configure your secrets (API key and Wifi), etc.
```
  cp secrets.h.example to secrets.h
  ed secrets.h
```


## WA5ZNU updates, as running:
- fork from https://github.com/vlytsus/demcusensor
- Changed from HTTP to MQTT
- Added primary and backup MQTT broker (e.g. home lab, cloud)
- calculateAQI: Add new field aqiValue
  Pick one of the supplied AQI calculations on pm2_5Value.
  None of them are great - the field is ill-defined in industry.
  <https://arduinosensor.tumblr.com/post/157881702335/formula-to-calculate-caqi-index>
  is the one I used.
- make sensor power-down optional by POWER_OFF_SENSOR=false
  since it intoduces instability.
- change sleep time to 1 minute if values are changing quickly,
  and 5 minutes if not.
  transitively embedded sleeps, so it's more like 1:40, guess?
- moved secrets to secrets.h
- sensor validation no longer excludes zeros, only all zeros
- changed frame sync logic
- added more calls to yield and mqttClient.loop(), perhaps too many.
- general code simplification (switch/case, etc)


## References
- https://www.instructables.com/A-Low-cost-IoT-Air-Quality-Monitor-Based-on-Raspbe/
- https://randomnerdtutorials.com/esp32-mqtt-publish-bme280-arduino/
- https://medium.com/@chardorn/set-up-a-nodemcu-to-communicate-with-thingspeak-using-mqtt-fbad7556a5bc
- https://github.com/knolleary/pubsubclient/blob/v2.8/examples/mqtt_esp8266/mqtt_esp8266.ino
- https://gist.github.com/kd8bxp/3af6508b72b2db83c2fe93228e2b6fa2
