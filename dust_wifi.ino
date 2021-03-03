//******************************
// PMS5003 Dust sensor application
// That reads PM1, pm2.5, pm10 values and sends to cloud application
// wia WIFI connection
//******************************
#include <Arduino.h>
#include <ESP8266WiFi.h>

// config
#include "secrets.h"

#if USE_MQTT
#include <PubSubClient.h>
#endif

#if DEBUG
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

#define POWER_OFF_SENSOR (1)

#define MAX_UNSIGNED_INT     65535
#define MSG_LENGTH 31   //0x42 + 31 bytes equal to PMS5003 serial message packet length
#define HTTP_TIMEOUT 20000 //maximum http response wait period, sensor disconects if no response
#define MIN_WARM_TIME 30000 //warming-up period requred for sensor to enable fan and prepare air chamber
unsigned char buf[MSG_LENGTH];

unsigned int pm01Value = 0;   //define PM1.0 value of the air detector module
unsigned int pm2_5Value = 0;  //define pm2.5 value of the air detector module
unsigned int pm10Value = 0;   //define pm10 value of the air detector module
unsigned int pmRAW25 = 0;
unsigned int aqiValue = 0;    // AQI value calculated

unsigned long timeout = 0;

const int   SLEEP_TIME = 1 * 60 * 1000;

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

boolean readSensorData() {
  DEBUG_PRINTLN("readSensorData start");
  
  unsigned char ch;
  unsigned char high;  
  unsigned int value = 0;
  pmsMessage.receivedSum = 0;
  
  for(int count = 0; count < 32 && Serial.available(); count ++){    
    ch = Serial.read();
    if ((count == 0 && ch != 0x42) || (count == 1 && ch != 0x4d)) {
      pmsMessage.receivedSum = 0;
      DEBUG_PRINTLN("message failed");
      break;
    }else if((count % 2) == 0){
      high = ch;
    }else{
      value = 256 * high + ch;
    }
    
    if (count == 5) { 
      pmsMessage.pm1tsi = value; //PM 1.0 [ug/m3] (TSI standard)
    } else if (count == 7) {
      pmsMessage.pm25tsi = value; //PM 2.5 [ug/m3] (TSI standard)
    } else if (count == 9) {
      pmsMessage.pm10tsi = value; //PM 10. [ug/m3] (TSI standard)
    } else if (count == 11) {
      pmsMessage.pm1atm = value; //PM 1.0 [ug/m3] (std. atmosphere)
    } else if (count == 13) {
      pmsMessage.pm25atm = value; //PM 2.5 [ug/m3] (std. atmosphere)
    } else if (count == 15) {
      pmsMessage.pm10atm = value; //PM 10. [ug/m3] (std. atmosphere)
    } else if (count == 17) {
      pmsMessage.raw03um = value; //num. particles with diameter > 0.3 um in 100 cm3 of air
    } else if (count == 19) {
      pmsMessage.raw05um = value; //num. particles with diameter > 0.5 um in 100 cm3 of air
    } else if (count == 21) {
      pmsMessage.raw10um = value; //num. particles with diameter > 1.0 um in 100 cm3 of air
    } else if (count == 23) {
      pmsMessage.raw25um = value; //num. particles with diameter > 2.5 um in 100 cm3 of air
    } else if (count == 25) {
      pmsMessage.raw50um = value; //num. particles with diameter > 5.0 um in 100 cm3 of air
    } else if (count == 27) {
      pmsMessage.raw100um = value; //num. particles with diameter > 10. um in 100 cm3 of air
    } else if (count == 29) {
      pmsMessage.version = 256 * high; //version & error code
      pmsMessage.errorCode = ch;      
    }
            
    if (count < 30){
      pmsMessage.receivedSum += ch; //calculate checksum for all bytes except last two      
    } else if (count == 31) {
      pmsMessage.checkSum = value; // last two bytes contains checksum from device
    }
  }

  //read data that is not usefull
  while (Serial.available()){
    Serial.read();
    DEBUG_PRINT(".");
  }

  DEBUG_PRINT("data ready:");
  DEBUG_PRINTLN(pmsMessage.receivedSum);
  bool checksum_match = (pmsMessage.receivedSum == pmsMessage.checkSum);
  DEBUG_PRINTLN(checksum_match ? "checksum match" : "checksum fail");
  return checksum_match;
}


void printInfo() {
  //debug printing
#if DEBUG
  DEBUG_PRINT("pm1tsi=");
  DEBUG_PRINT(pmsMessage.pm1tsi);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("pm25tsi=");
  DEBUG_PRINT(pmsMessage.pm25tsi);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("pm10tsi=");
  DEBUG_PRINT(pmsMessage.pm10tsi);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("pm1atm=");
  DEBUG_PRINT(pmsMessage.pm1atm);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("pm25atm=");
  DEBUG_PRINT(pmsMessage.pm25atm);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("pm10atm=");
  DEBUG_PRINT(pmsMessage.pm10atm);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw03um=");
  DEBUG_PRINT(pmsMessage.raw03um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw05um=");
  DEBUG_PRINT(pmsMessage.raw05um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw10um=");
  DEBUG_PRINT(pmsMessage.raw10um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw25um=");
  DEBUG_PRINT(pmsMessage.raw25um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw50um=");
  DEBUG_PRINT(pmsMessage.raw50um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("raw100um=");
  DEBUG_PRINT(pmsMessage.raw100um);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("version=");
  DEBUG_PRINT(pmsMessage.version);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("errorCode=");
  DEBUG_PRINT(pmsMessage.errorCode);
  DEBUG_PRINTLN();
  
  DEBUG_PRINT("receivedSum=");
  DEBUG_PRINT(pmsMessage.receivedSum);
  DEBUG_PRINTLN();
  
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
  digitalWrite(D0, LOW);
  // DEBUG_PRINTLN("going to sleep zzz...");
  //ESP.deepSleep(SLEEP_TIME * 1000000);
  //deep sleep in microseconds, unfortunately doesn't work properly
}

void setupWIFI() {
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  DEBUG_PRINT("connecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");

    if (millis() - timeout > MIN_WARM_TIME) {
      //can't connect to WIFI for a long time
      //disable sensor to save lazer
      digitalWrite(D0, LOW);
      timeout = millis(); //reset timer
    }
  }
  //enable sensor just in case if was disabled
  digitalWrite(D0, HIGH);

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
}

#if USE_MQTT
void getESPID(char *id, int n) {
   uint32_t chipid=ESP.getChipId();
   snprintf(id, n,"%s%08X", CLIENT_NAME_PREFIX, chipid);
}

boolean connectWiFi(const char *host, int port) {
  DEBUG_PRINTLN("connectWifi");
  // Use WiFiClient class to create TCP connections
  if (! wifiClient.connect(host, port)) {
    DEBUG_PRINT("connectWifi failed: ");
    DEBUG_PRINT(host);
    DEBUG_PRINT(" ");
    DEBUG_PRINTLN(port);
    return false;
  } else {
    return true;
  }
}

// void closeWifi() {
//   DEBUG_PRINTLN("closeWifi");
//   wifiClient.stop();
// }
  
boolean connectMQTT() {
  char esp_id[MAX_ID_LEN];
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  getESPID(esp_id, MAX_ID_LEN);
  DEBUG_PRINT("espid ");
  DEBUG_PRINTLN(esp_id);

  if (mqttClient.connect(esp_id)) {
    Serial.println("MQTT connected");
    return true;
  } else {
    Serial.print("failed with state ");
    Serial.println(mqttClient.state());
    delay(2000);
    return false;
  }
}

void sendDataToCloudMQTT() {
  DEBUG_PRINT("emb pm2.5: ");
  DEBUG_PRINTLN(pm2_5Value);
  DEBUG_PRINT("RAW pm2.5: ");
  DEBUG_PRINTLN(pmRAW25);

  boolean ok = false;

  if (connectWiFi(MQTT_HOST, MQTT_PORT)) {
    ok = true;
  } else if (connectWiFi(MQTT_HOST_BACKUP, MQTT_PORT_BACKUP)) {
    ok = true;
  }
  if (ok && connectMQTT()) {
    publishMQTT();
  }
}

void publishMQTT() {
  DEBUG_PRINTLN("publishMQTT");
  String payload =
    "&field1=" + String(pm01Value) +
    "&field2=" + String(pm2_5Value) +
    "&field3=" + String(pm10Value) +
    "&field4=" + String(aqiValue) +
    "&field7=" + String(pmRAW25);
  String topic = String("channels/") +
    String(MQTT_CHANNEL_ID) +
    String("/publish/") +
    String(THINGSPEAK_WRITE_API_KEY);
  DEBUG_PRINTLN("mqttClient.loop");
  mqttClient.loop();
  DEBUG_PRINT("publish ");
  DEBUG_PRINT(topic);
  DEBUG_PRINT(" ");
  DEBUG_PRINTLN(payload);
  mqttClient.publish(topic.c_str(), payload.c_str());
  DEBUG_PRINTLN("DONE");
}
#endif

#if USE_HTTP_API
void sendDataToCloudAPI() {
  DEBUG_PRINT("emb pm2.5: ");
  DEBUG_PRINTLN(pm2_5Value);
  DEBUG_PRINT("RAW pm2.5: ");
  DEBUG_PRINTLN(pmRAW25);
  
  DEBUG_PRINTLN("sendDataToCloud start");
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(CLOUD_API_HOST, CLOUD_API_PORT)) {
    DEBUG_PRINT("sendDataToCloud failed: ");
    DEBUG_PRINT(CLOUD_API_PORT);
    DEBUG_PRINT(" ");
    DEBUG_PRINTLN(CLOUD_API_PORT)
    client.stop();
    return;
  }

  //create URI for request
  String url = String(CLOUD_APPLICATION_ENDPOINT) +
    "&api_key=" + THINGSPEAK_WRITE_API_KEY +
    "&field1=" + String(pm01Value) +
    "&field2=" + String(pm2_5Value) +
    "&field3=" + String(pm10Value) +
    "&field4=" + String(aqiValue) +
    "&field7=" + String(pmRAW25);

  // logs api key if you care
  DEBUG_PRINTLN("Requesting GET: " + url);
  // This will send the request to the server
  client.print(String("GET /") + url + " HTTP/1.1\r\n" +
               "Host: " + CLOUD_API_HOST + "\r\n" +
               "Accept: */*\r\n" +
               "User-Agent: Mozilla/4.0 (compatible; esp8266 Lua; Windows NT 5.1)\r\n" + // Why this complex UA?
               "Connection: close\r\n" +
               "\r\n");
  client.flush();
  delay(10);
  DEBUG_PRINTLN("wait for response");
  timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > HTTP_TIMEOUT) {
      DEBUG_PRINTLN(">>> Client Timeout !");
      client.stop();
      DEBUG_PRINTLN("closing connection by timeout");
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    DEBUG_PRINT(line);
  }

  client.stop();
  DEBUG_PRINTLN();
  DEBUG_PRINTLN("closing connection");
}
#endif

#if USE_MQTT
#endif


void setup() {
  Serial.begin(9600);   //shared between reading module and writing DEBUG
#if DEBUG
  Serial.println(" Init started: DEBUG MODE");
#else
  Serial.println(" Init started: NO DEBUG MODE");
#endif
  Serial.setTimeout(1500);//set the Timeout to 1500ms, longer than the data transmission time of the sensor
  pinMode(D0, OUTPUT);
  powerOnSensor();
  setupWIFI();

  DEBUG_PRINTLN("Initialization finished");
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
  unsigned int maxPm01Value =0;
  unsigned int minPm01Value =0;
    
  unsigned int maxPm2_5Value =0;
  unsigned int minPm2_5Value =0;
    
  unsigned int maxPm10Value =0;
  unsigned int minPm10Value =0;
  
  unsigned int maxRAW25 =0;
  unsigned int minRAW25 =0;

  pm01Value = 0;   //define PM1.0 value of the air detector module
  pm2_5Value = 0;  //define pm2.5 value of the air detector module
  pm10Value = 0;   //define pm10 value of the air detector module
  pmRAW25 = 0;   
  int count = 0;
  
  for (int i = 0; i < 35 && count < 8; i++) {
    if (readSensorData()){
      if (pmsMessage.pm25atm == 0 &&
	  pmsMessage.pm1atm == 0 &&
	  pmsMessage.pm10atm == 0 &&
	  pmsMessage.raw25um == 0) {
          // only skip on all zeros
          DEBUG_PRINT("all zero - skip loop:");
          DEBUG_PRINTLN(i);
	  printInfo();
          delay(1000);
          continue;
        }

        //***********************************
        //find max dust per sample
        if (pmsMessage.pm1atm > maxPm01Value) {
          maxPm01Value = pmsMessage.pm1atm;
        }
        //find min dust per sample
        if (pmsMessage.pm1atm < minPm01Value) {
          minPm01Value = pmsMessage.pm1atm;
        }
        pm01Value += pmsMessage.pm1atm;
        //***********************************
        //find max dust per sample
        if (pmsMessage.pm25atm > maxPm2_5Value) {
          maxPm2_5Value = pmsMessage.pm25atm;
        }
        //find min dust per sample
        if (pmsMessage.pm25atm < minPm2_5Value) {
          minPm2_5Value = pmsMessage.pm25atm;
        }
        pm2_5Value += pmsMessage.pm25atm;
        //***********************************
        //find max dust per sample
        if (pmsMessage.pm10atm > maxPm10Value) {
          maxPm10Value = pmsMessage.pm10atm;
        }
        //find min dust per sample
        if (pmsMessage.pm10atm < minPm10Value) {
          minPm10Value = pmsMessage.pm10atm;
        }
        pm10Value += pmsMessage.pm10atm;
        //***********************************        
        //findRAW  max dust per sample
        if (pmsMessage.pm10atm > maxRAW25) {
          maxRAW25 = pmsMessage.raw25um;
        }
        //find min dust per sample
        if (pmsMessage.pm10atm > 0 && pmsMessage.pm10atm < minRAW25) {
          minRAW25 = pmsMessage.raw25um;
        }
        pmRAW25 += pmsMessage.raw25um;
        //***********************************
        printInfo();
        delay(500);
        count++;
    } else {
      delay(1000);//data read failed
    }
  }

  if (count > 2) {
    if (pm2_5Value == 0) {
      pm01Value += minPm01Value;
      pm2_5Value += minPm2_5Value;
      pm10Value += minPm10Value;
      pmRAW25 += minRAW25;
    }

    //remove max & min records from calculations
    pm01Value -= maxPm01Value;
    pm01Value -= minPm01Value;
    //get mid value
    pm01Value = pm01Value / (count - 1);

    //remove max & min records from calculations
    pm2_5Value -= maxPm2_5Value;
    pm2_5Value -= minPm2_5Value;
    //get mid value
    pm2_5Value = pm2_5Value / (count - 1);

    //remove max & min records from calculations
    pm10Value -= maxPm10Value;
    pm10Value -= minPm10Value;
    pm10Value = pm10Value / (count - 1);

    //removve max & min records from calculations
    pmRAW25 -= maxRAW25;
    pmRAW25 -= minRAW25;
    //get mid value
    pmRAW25 = pmRAW25 / (count - 1);
    
    // Calculate AQI
    aqiValue = calculateAQI_25(pm2_5Value);
    DEBUG_PRINT("pm2_5Value="); DEBUG_PRINT(pm2_5Value); DEBUG_PRINT(" aqiValue="); DEBUG_PRINTLN(aqiValue);

#if USE_HTTP_API
    sendDataToCloudAPI();
#endif
#if USE_MQTT
    sendDataToCloudMQTT();
#endif
  }

  if (POWER_OFF_SENSOR) {
    powerOffSensor();
  } else {
    DEBUG_PRINTLN("Skipping powerOffSensor");
  }
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

