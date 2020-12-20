# WA5ZNU fork of demcusensor

WA5ZNU updates, as running:

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
- move secrets (API key and Wifi) out to secrets.h
  you should copy secrets.h.example to secrets.h and edit
  .gitignore protects secrets.h from commit

# demcusensor
NodeMCU based PM2.5 monitoring application for PMS5003 dust sensor.

Actualy it could measure PM1, PM2.5, PM10 and AQI and push data to cloud service for visualization.
Enjoy!
