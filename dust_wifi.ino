//******************************
// PMS5003 Dust sensor application
// That reads PM1, pm2.5, pm10 values and sends to cloud application
// wia WIFI connection
//******************************
#include <Arduino.h>
#include <ESP8266WiFi.h>

// config
#include "secrets.h"

#include <PubSubClient.h>

#if DEBUG
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)
#define DEBUG_PUTC(c)  debug_putc(c)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#define DEBUG_PUTC(c)
#endif

#define MAX_UNSIGNED_INT     65535
#define MSG_LENGTH 31   //0x42 + 31 bytes equal to PMS5003 serial message packet length
#define WIFI_STA_DELAY 1000 // wifi warmup time after entering STA mode.
#define MIN_WARM_TIME 30000 //warming-up period requred for sensor to enable fan and prepare air chamber

unsigned int pm01 = 0;   //define PM1.0 value of the air detector module
unsigned int pm2_5 = 0;  //define pm2.5 value of the air detector module
unsigned int pm10 = 0;   //define pm10 value of the air detector module
unsigned int pm2_5raw = 0;
unsigned int aqi = 0;    // AQI value calculated

unsigned long timeout = 0;

unsigned long checksum_errors = 0;
unsigned long tcp_connect_errors = 0;
unsigned long mqtt_connect_errors = 0;
unsigned long mqtt_publish_errors = 0;

char esp_id[MAX_ID_LEN];
const int SLEEP_TIME = 60 * 1000;

// too many issues
const bool POWER_OFF_SENSOR = false;

// PMS5003 Message Structure
struct PMSMessage {
    unsigned int pm1tsi;
    unsigned int pm25tsi;
    unsigned int pm10tsi;
    unsigned int pm1atm;
    unsigned int pm25atm;
    unsigned int pm10atm;
    unsigned int raw03um;
    unsigned int raw05um;
    unsigned int raw10um;
    unsigned int raw25um;
    unsigned int raw50um;
    unsigned int raw100um;
    unsigned int version;
    unsigned int errorCode;    
    unsigned int receivedSum;
    unsigned int checkSum;
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
PMSMessage pmsMessage;

void debug_putc(char c) {
  if (c >= ' ' && c <= '~') {
    DEBUG_PRINT(c);
  } else {
    DEBUG_PRINT("<");
    DEBUG_PRINT((int)c);
    DEBUG_PRINT(">");
  }
}

boolean syncFrame() {
  DEBUG_PRINT("Syncing frame: ");
  char c = '\0';
  int i = 0;
  while (c != 'M') {
    do {
      c = Serial.read();
      DEBUG_PUTC(c);
      if ((i++ % 64) == 0) {
        yield(); mqttClient.loop();
      }
    } while (c != 'B');
    c = Serial.read();
    DEBUG_PUTC(c);
  }
  DEBUG_PRINTLN();
  return true;
}

boolean readSensorData() {
  DEBUG_PRINTLN("readSensorData start");
  
  unsigned char ch;
  unsigned char high;  
  unsigned int value = 0;
  pmsMessage.receivedSum = 0;
  
  // Look for 'BM' at beginning of first whole lessage
  if (! syncFrame()) {
    return false;
  }
  pmsMessage.receivedSum += 'B' + 'M';
  delay(100);			// delay 100ms to wait for chars to show up

  // Depends on Serial.available with timeout -- does that work?
  for (int count = 2; count < 32 && Serial.available(); count++) {
    ch = Serial.read();
    if ((count % 2) == 0) {
      high = ch;
    } else {
      value = 256 * high + ch;
    }
    
    //calculate checksum for all bytes except last two
    if (count < 30) {
      pmsMessage.receivedSum += ch;
    }

    switch(count) {
    case 5:
      pmsMessage.pm1tsi = value; //PM 1.0 [ug/m3] (TSI standard)
      break;
    case 7: pmsMessage.pm25tsi = value; //PM 2.5 [ug/m3] (TSI standard)
      break;
    case 9: pmsMessage.pm10tsi = value; //PM 10. [ug/m3] (TSI standard)
      break;
    case 11: pmsMessage.pm1atm = value; //PM 1.0 [ug/m3] (std. atmosphere)
      break;
    case 13: pmsMessage.pm25atm = value; //PM 2.5 [ug/m3] (std. atmosphere)
      break;
    case 15: pmsMessage.pm10atm = value; //PM 10. [ug/m3] (std. atmosphere)
      break;
    case 17: pmsMessage.raw03um = value; //num. particles with diameter > 0.3 um in 100 cm3 of air
      break;
    case 19: pmsMessage.raw05um = value; //num. particles with diameter > 0.5 um in 100 cm3 of air
      break;
    case 21: pmsMessage.raw10um = value; //num. particles with diameter > 1.0 um in 100 cm3 of air
      break;
    case 23: pmsMessage.raw25um = value; //num. particles with diameter > 2.5 um in 100 cm3 of air
      break;
    case 25: pmsMessage.raw50um = value; //num. particles with diameter > 5.0 um in 100 cm3 of air
      break;
    case 27: pmsMessage.raw100um = value; //num. particles with diameter > 10. um in 100 cm3 of air
      break;
    case 29: pmsMessage.version = 256 * high; pmsMessage.errorCode = ch; //version & error code
      break;
    case 31: pmsMessage.checkSum = value; // last two bytes contains checksum from device
      break;
    }
  }

  DEBUG_PRINT("data ready: receivedSum=");
  DEBUG_PRINT(pmsMessage.receivedSum);
  DEBUG_PRINT(" expectedSum=");
  DEBUG_PRINT(pmsMessage.checkSum);
  bool checksum_match = (pmsMessage.receivedSum == pmsMessage.checkSum);
  DEBUG_PRINTLN(checksum_match ? " checksum match" : " checksum fail");
  if (! checksum_match) {
    checksum_errors++;
    DEBUG_PRINT("FAIL: ");
    printInfo();
  }
  return checksum_match;
}


void printInfo() {
  //debug printing
#if DEBUG
  DEBUG_PRINT("pm1tsi=");
  DEBUG_PRINT(pmsMessage.pm1tsi);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("pm25tsi=");
  DEBUG_PRINT(pmsMessage.pm25tsi);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("pm10tsi=");
  DEBUG_PRINT(pmsMessage.pm10tsi);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("pm1atm=");
  DEBUG_PRINT(pmsMessage.pm1atm);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("pm25atm=");
  DEBUG_PRINT(pmsMessage.pm25atm);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("pm10atm=");
  DEBUG_PRINT(pmsMessage.pm10atm);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw03um=");
  DEBUG_PRINT(pmsMessage.raw03um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw05um=");
  DEBUG_PRINT(pmsMessage.raw05um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw10um=");
  DEBUG_PRINT(pmsMessage.raw10um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw25um=");
  DEBUG_PRINT(pmsMessage.raw25um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw50um=");
  DEBUG_PRINT(pmsMessage.raw50um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("raw100um=");
  DEBUG_PRINT(pmsMessage.raw100um);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("version=");
  DEBUG_PRINT(pmsMessage.version);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("errorCode=");
  DEBUG_PRINT(pmsMessage.errorCode);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("receivedSum=");
  DEBUG_PRINT(pmsMessage.receivedSum);
  DEBUG_PRINT(" ");
  
  DEBUG_PRINT("checkSum=");
  DEBUG_PRINT(pmsMessage.checkSum);
  DEBUG_PRINTLN(); 
#endif
}

void powerOnSensor() {
  digitalWrite(D0, HIGH);
  DEBUG_PRINT("sensor warm-up: ");
  delay(MIN_WARM_TIME);
  DEBUG_PRINT("done");
}

void powerOffSensor() {
  DEBUG_PRINTLN("powerOffSensor");
  digitalWrite(D0, LOW);
  //ESP.deepSleep(SLEEP_TIME * 1000000);
  //deep sleep in microseconds, unfortunately doesn't work properly
}

void setupWIFI() {
  WiFi.mode(WIFI_STA);
  delay(WIFI_STA_DELAY);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  DEBUG_PRINT("\nConnecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");

    if (POWER_OFF_SENSOR) {
      if (millis() - timeout > MIN_WARM_TIME) {
	//can't connect to WIFI for a long time
	//disable sensor to save lazer
	digitalWrite(D0, LOW);
	timeout = millis(); //reset timer
      }
    }
  }
  //enable sensor just in case if was disabled
  digitalWrite(D0, HIGH);

  DEBUG_PRINT("\nWiFi connected: IP=");
  DEBUG_PRINTLN(WiFi.localIP());

  getESPID(esp_id, MAX_ID_LEN);
  DEBUG_PRINT("esp_id = ");
  DEBUG_PRINTLN(esp_id);
}

void getESPID(char *id, int n) {
   uint32_t chipid=ESP.getChipId();
   snprintf(id, n,"%s%08X", CLIENT_NAME_PREFIX, chipid);
}

boolean connectTCP(const char *host, int port) {
  boolean ok = false;
  DEBUG_PRINT("connectTCP: ");
  for (int i = 0; i<10 && ! ok; i++) {
    ok = wifiClient.connect(host, port);
    if (! ok) {
      DEBUG_PRINT("failed: ");
      wifiClient.stop();
      delay(1000);
    }
    DEBUG_PRINT(host);
    DEBUG_PRINT(" ");
    DEBUG_PRINTLN(port);
  }
  return ok;
}
  
boolean connectMQTT() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  DEBUG_PRINT(String("connectMQTT host=") + MQTT_HOST + ":" + String(MQTT_PORT) + " espid=" + esp_id + ": ");
  if (mqttClient.connect(esp_id)) {
    Serial.println("connected");
    return true;
  } else {
    Serial.print("failed; state=");
    Serial.println(mqttClient.state());
    mqtt_connect_errors++;
    return false;
  }
}

boolean connectAndPublishMQTT() {
  if (! connectTCP(MQTT_HOST, MQTT_PORT)) {
    tcp_connect_errors++;
    return false;
  }
  if (! connectMQTT()) {
    mqtt_connect_errors++;
    return false;
  }
  if (! publishMQTT()) {
    mqtt_publish_errors++;
    return false;
  }
  return true;
}

boolean publishMQTT() {
  boolean ok = false;
  {
    DEBUG_PRINT("publishMQTT sensor: ");
    char topic[MAX_TOPIC_LEN];
    char payload[MAX_PAYLOAD_LEN];
    snprintf(topic, MAX_TOPIC_LEN,"%s/%s", MQTT_SENSOR_TOPIC_PREFIX, esp_id);
    snprintf(payload, MAX_PAYLOAD_LEN, "pm01=%d;pm2_5=%d;pm10=%d;aqi=%d;pm2_5raw=%d",
	     pm01, pm2_5, pm10, aqi, pm2_5raw);
    DEBUG_PRINT("topic="); DEBUG_PRINT(topic); DEBUG_PRINT(" payload="); DEBUG_PRINTLN(payload);
    ok = mqttClient.publish(topic, payload);
  }

  {
    DEBUG_PRINT("publishMQTT telemetry: ");
    char topic[MAX_TOPIC_LEN];
    char payload[MAX_PAYLOAD_LEN];
    snprintf(topic, MAX_TOPIC_LEN,"%s/%s", MQTT_STATS_TOPIC_PREFIX, esp_id);
    snprintf(payload, MAX_PAYLOAD_LEN, "checksum_errors=0;tcp_connect_errors=0;mqtt_connect_errors=0;mqtt_publish_errors=0",
	     checksum_errors, tcp_connect_errors, mqtt_connect_errors, mqtt_publish_errors);
    DEBUG_PRINT("topic="); DEBUG_PRINT(topic); DEBUG_PRINT(" payload="); DEBUG_PRINTLN(payload);
    ok = mqttClient.publish(topic, payload);
  }  
  return ok;
}

void setup() {
  // Serial shared between reading module and writing DEBUG, so it must be 9600.
  Serial.begin(9600);   
#if DEBUG
  Serial.println(" Init started: DEBUG MODE");
#else
  Serial.println(" Init started: NO DEBUG MODE");
#endif
  //set the Timeout to 1500ms, longer than the data transmission time of the sensor
  Serial.setTimeout(1500);
  pinMode(D0, OUTPUT);
  powerOnSensor();
  setupWIFI();

  DEBUG_PRINTLN("setup done");
}

void loop() {
  DEBUG_PRINTLN("loop start");
  timeout = millis();

  if (WiFi.status() != WL_CONNECTED) {
    setupWIFI();
  }

  if (POWER_OFF_SENSOR) {
    powerOnSensor();
  }

//Max & Min values are used to filter noise data
  unsigned int maxPm01 =0;
  unsigned int minPm01 =0;
    
  unsigned int maxPM2_5 =0;
  unsigned int minPM2_5 =0;
    
  unsigned int maxPM10 =0;
  unsigned int minPM10 =0;
  
  unsigned int maxRAW25 =0;
  unsigned int minRAW25 =0;

  pm01 = 0;   //define PM1.0 value of the air detector module
  pm2_5 = 0;  //define pm2.5 value of the air detector module
  pm10 = 0;   //define pm10 value of the air detector module
  pm2_5raw = 0;   
  int count = 0;
  
  for (int i = 0; i < 35 && count < 8; i++) {
    if (readSensorData()) {
      if (pmsMessage.pm25atm == 0 &&
          pmsMessage.pm1atm == 0 &&
          pmsMessage.pm10atm == 0 &&
          pmsMessage.raw25um == 0) {
          // only skip on all zeros
          DEBUG_PRINT("sensor data all zero - skip loop:");
          DEBUG_PRINTLN(i);
          DEBUG_PRINTLN("mqttClient.loop"); mqttClient.loop();
          continue;
        }

        //***********************************
        //find max dust per sample
        if (pmsMessage.pm1atm > maxPm01) {
          maxPm01 = pmsMessage.pm1atm;
        }
        //find min dust per sample
        if (pmsMessage.pm1atm < minPm01) {
          minPm01 = pmsMessage.pm1atm;
        }
        pm01 += pmsMessage.pm1atm;
        //***********************************
        //find max dust per sample
        if (pmsMessage.pm25atm > maxPM2_5) {
          maxPM2_5 = pmsMessage.pm25atm;
        }
        //find min dust per sample
        if (pmsMessage.pm25atm < minPM2_5) {
          minPM2_5 = pmsMessage.pm25atm;
        }
        pm2_5 += pmsMessage.pm25atm;
        //***********************************
        //find max dust per sample
        if (pmsMessage.pm10atm > maxPM10) {
          maxPM10 = pmsMessage.pm10atm;
        }
        //find min dust per sample
        if (pmsMessage.pm10atm < minPM10) {
          minPM10 = pmsMessage.pm10atm;
        }
        pm10 += pmsMessage.pm10atm;
        //***********************************        
        //findRAW  max dust per sample
        if (pmsMessage.pm10atm > maxRAW25) {
          maxRAW25 = pmsMessage.raw25um;
        }
        //find min dust per sample
        if (pmsMessage.pm10atm > 0 && pmsMessage.pm10atm < minRAW25) {
          minRAW25 = pmsMessage.raw25um;
        }
        pm2_5raw += pmsMessage.raw25um;
        //***********************************
        printInfo();
        DEBUG_PRINTLN("mqttClient.loop"); mqttClient.loop();
        count++;
    } else {
      DEBUG_PRINTLN("mqttClient.loop"); mqttClient.loop();
    }
  }

  if (count > 2) {
    if (pm2_5 == 0) {
      pm01 += minPm01;
      pm2_5 += minPM2_5;
      pm10 += minPM10;
      pm2_5raw += minRAW25;
    }

    //remove max & min records from calculations
    pm01 -= maxPm01;
    pm01 -= minPm01;
    //get mid value
    pm01 = pm01 / (count - 1);

    //remove max & min records from calculations
    pm2_5 -= maxPM2_5;
    pm2_5 -= minPM2_5;
    //get mid value
    pm2_5 = pm2_5 / (count - 1);

    //remove max & min records from calculations
    pm10 -= maxPM10;
    pm10 -= minPM10;
    pm10 = pm10 / (count - 1);

    //remove max & min records from calculations
    pm2_5raw -= maxRAW25;
    pm2_5raw -= minRAW25;
    //get mid value
    pm2_5raw = pm2_5raw / (count - 1);
    
    // Calculate AQI
    aqi = calculateAQI_25(pm2_5);

    connectAndPublishMQTT();
  }

  if (POWER_OFF_SENSOR) {
    powerOffSensor();
  }
  DEBUG_PRINTLN("mqttClient.loop"); mqttClient.loop();
  DEBUG_PRINT("Sleeping ");
  DEBUG_PRINTLN(SLEEP_TIME);
  delay(SLEEP_TIME);
}

// USA AQI Standard
// AQI formula: https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
int toAQI(int I_high, int I_low, int C_high, int C_low, int C) {
  float f_I_high = I_high;
  float f_I_low = I_low;
  float f_C_high = C_high;
  float f_C_low = C_low;
  float f_C = C;
  int aqiResult = (int)(round(I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low);
  return aqiResult;
}

// https://arduinosensor.tumblr.com/post/157881702335/formula-to-calculate-caqi-index
int calculateAQI_25(int density_25)  {
  int dx10 = density_25 * 10;
  
  if (dx10 <= 0) {
    return  0;
  } else if (dx10 <= 120) {
    return  toAQI(50, 0, 120, 0, dx10);
  } else if (dx10 <= 354) {
    return  toAQI(100, 51, 354, 121, dx10);
  } else if (dx10 <= 554) {
    return  toAQI(150, 101, 554, 355, dx10);
  } else if (dx10 <= 1504) {
    return  toAQI(200, 151, 1504, 555, dx10);
  } else if (dx10 <= 2504) {
    return  toAQI(300, 201, 2504, 1505, dx10);
  } else if (dx10 <= 3504) {
    return  toAQI(400, 301, 3504, 2505, dx10);
  } else if (dx10 <= 5004) {
    return  toAQI(500, 401, 5004, 3505, dx10);
  } else if (dx10 <= 10000) {
    return toAQI(1000, 501, 10000, 5005, dx10);
  } else {
    return 1001;
  }
}

