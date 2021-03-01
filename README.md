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
- calculateAQI: Add new field aqiValue
  Pick one of the supplied AQI calculations on pm2_5Value.
  None of them are great - the field is ill-defined in industry.
  <https://arduinosensor.tumblr.com/post/157881702335/formula-to-calculate-caqi-index>
  is the one I used.
- make sensor power-down optional by #define POWER_OFF_SENSOR
  since it intoduces instability, default on
- change sleep time to 1 minute, but there are both explicit and
  transitively embedded sleeps, so it's more like 1:40, guess?
- setup: call powerOnSensor()
- debug in powerOnSensor, since with a bad power supply it
  might fail before the print. it's workign now, though.
- loop: Take Wifi.disconnect out of of powerOffSensor, and only call
  setupWiFi() if it's not connected.  This might improve
  wifi reliability, or might not.  it's working, though.
- powerOffSensor: move delay out to loop,
  to reduce embedded delays, which are harder to account for.
- moved secrets to secrets.h
- sensor validation no longer excludes zeros, only all zeros
- added ifdef-controlled MQTT or HTTP support

## References
- https://www.instructables.com/A-Low-cost-IoT-Air-Quality-Monitor-Based-on-Raspbe/
- https://randomnerdtutorials.com/esp32-mqtt-publish-bme280-arduino/
- https://medium.com/@chardorn/set-up-a-nodemcu-to-communicate-with-thingspeak-using-mqtt-fbad7556a5bc
- https://github.com/knolleary/pubsubclient/blob/v2.8/examples/mqtt_esp8266/mqtt_esp8266.ino
- https://gist.github.com/kd8bxp/3af6508b72b2db83c2fe93228e2b6fa2
