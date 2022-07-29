#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#define DEVICE "ESP32"
  #elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

char my_sensor[20];
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <secrets.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "DHT.h"
DHT dht;

#define TZ_INFO "<+07>-7"
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
// Data point
Point sensor("sensor_temp");

const char* modes[] = { "NULL", "STA", "AP", "STA+AP" };

unsigned long mtime = 0;


WiFiManager wm;


// TEST OPTION FLAGS
bool TEST_CP         = true; // always start the configportal, even if ap found
int  TESP_CP_TIMEOUT = 60; // test cp timeout

bool TEST_NET        = true; // do a network test after connect, (gets ntp time)
bool ALLOWONDEMAND   = true; // enable on demand
int  ONDDEMANDPIN    = 0; // gpio for button
bool WMISBLOCKING    = true; // use blocking or non blocking mode, non global params wont work in non blocking

int DELAY_TIME = 10000; // 10 sec.

void setup() {
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

  //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);

  #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
          DynamicJsonDocument json(1024);
          auto deserializeError = deserializeJson(json, buf.get());
          serializeJson(json, Serial);
          if ( ! deserializeError ) {
  #else
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
  #endif
            Serial.println("\nparsed json");
            strcpy(my_sensor, json["my_sensor"]);
          } else {
            Serial.println("failed to load json config");
          }
          configFile.close();
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }
    //end read
    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();


    Serial.println("\n Starting");
    // WiFi.setSleepMode(WIFI_NONE_SLEEP); // disable sleep, can improve ap stability

    Serial.println("Error - TEST");
    Serial.println("Information- - TEST");

    Serial.println("[ERROR]  TEST");
    Serial.println("[INFORMATION] TEST");  


    wm.setDebugOutput(true);
    wm.debugPlatformInfo();

    WiFiManagerParameter custom_text_box("my_sensor","Enter sensor name",my_sensor,20);
    wm.addParameter(&custom_text_box);
    //set config save notify callback
    wm.setSaveConfigCallback(saveConfigCallback);

    if(!wm.autoConnect("WM_AutoConnectAP","12345678")) {
      Serial.println("failed to connect and hit timeout");
    }
    else if(TEST_CP) {
      // start configportal always
      delay(1000);
      Serial.println("TEST_CP ENABLED");
      wm.setConfigPortalTimeout(TESP_CP_TIMEOUT);
      wm.startConfigPortal("VillaConnectAPNodeMCU","12345678");
    }
    else {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
    }
    
    wifiInfo();
    pinMode(ONDDEMANDPIN, INPUT_PULLUP);


    //read updated parameters
    strcpy(my_sensor, custom_text_box.getValue());
    Serial.println("The values in the file are: ");
    Serial.println("\tmy_sensor : " + String(my_sensor));
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
  #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
      DynamicJsonDocument json(1024);
  #else
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
  #endif
      json["my_sensor"] = my_sensor;
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

  #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
      serializeJson(json, Serial);
      serializeJson(json, configFile);
  #else
      json.printTo(Serial);
      json.printTo(configFile);
  #endif
      configFile.close();
      //end save
    }  
    pinMode(2,OUTPUT);  
    Serial.println();
    Serial.println("Status\tHumidity (%)\tTemperature (C)\t(F)");
    dht.setup(2); // data pin 5 gpio5 D1

    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Check server connection
    if (client.validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
        delay(2000);    
    } else {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
        delay(2000);      
    }
    sensor.addTag("device",my_sensor);    
}

void wifiInfo(){
  // can contain gargbage on esp32 if wifi is not ready yet
  Serial.println("[WIFI] WIFI INFO DEBUG");
  // WiFi.printDiag(Serial);
  Serial.println("[WIFI] SAVED: " + (String)(wm.getWiFiIsSaved() ? "YES" : "NO"));
  Serial.println("[WIFI] SSID: " + (String)wm.getWiFiSSID());
  Serial.println("[WIFI] PASS: " + (String)wm.getWiFiPass());
  Serial.println("[WIFI] HOSTNAME: " + (String)WiFi.getHostname());
}
void loop() {
    delay(dht.getMinimumSamplingPeriod());
    float humidity = dht.getHumidity(); // ดึงค่าความชื้น
    float temperature = dht.getTemperature(); // ดึงค่าอุณหภูมิ
    Serial.print(dht.getStatusString());
    Serial.print("\t");
    Serial.print(humidity, 1);
    Serial.print("\t\t");
    Serial.print(temperature, 1);
    Serial.print("\t\t");
    Serial.println(dht.toFahrenheit(temperature), 1);
    // Store measured value into point
    sensor.clearFields();
    // Report RSSI of currently connected network
    sensor.addField("rssi", WiFi.RSSI());
    sensor.addField("temperature", temperature);
    sensor.addField("humidity", humidity);     
    // Write point
    if (!client.writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
    }
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");     
    digitalWrite(2,HIGH);   
    delay(DELAY_TIME);
    digitalWrite(2,LOW);       
}
