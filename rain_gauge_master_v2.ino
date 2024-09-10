#include <SPI.h>
#include <LoRa.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "TP-Link_BA5A"
// WiFi password
#define WIFI_PASSWORD "69073953"

#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "pzUDi0__-78xPbdAnQ4lDdpLVC7sigw9I7gwrhmbcOh0dLny5TY_vAhHdL3v1tb4dAXwhUYNsueI8rMGmiHmUA=="
#define INFLUXDB_ORG "d17da67d9b7c53bd"
#define INFLUXDB_BUCKET "Rain_Gauge"

// Time zone info
#define TZ_INFO "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define DEVICE "ESP32_Rain_Gauge_1"

//LoRa
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

WiFiMulti wifiMulti;  

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("Rain_Gauge_measurements_v2");

String getFormattedTime() {
time_t now = time(nullptr);
struct tm *timeinfo = localtime(&now);
char buffer[80];
strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
return String(buffer);
}

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Accurate time is necessary for certificate validation and writing in batches
  // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Inicijalizacija LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
  Serial.println("Pokretanje LoRa nije uspelo.");
  while (1);
  }

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  // Add tags to the data point
  sensor.addTag("device", DEVICE);
}

void loop() {
  
  // Provera da li je stigao zahtev od Slave-a
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedMessage = "";
    while (LoRa.available()) {
      receivedMessage += (char)LoRa.read();
    }

    Serial.print("Primljen zahtev: ");
    Serial.println(receivedMessage);

    // Ako je zahtev za vreme, šalje tačno vreme nazad
    if (receivedMessage == "SYNC_TIME") {
      String currentTime = getFormattedTime();
      LoRa.beginPacket();
      LoRa.print(currentTime);
      LoRa.endPacket();

      Serial.print("Poslato vreme: ");
      Serial.println(currentTime);
    } else {
      // Razdvajanje podataka na osnovu zareza
      char dataArray[receivedMessage.length() + 1];
      receivedMessage.toCharArray(dataArray, receivedMessage.length() + 1);
  
        char* tempValue = strtok(dataArray, ",");
      char* humiValue = strtok(NULL, ",");
      char* mmValue = strtok(NULL, ",");
  
        // Pretvaranje stringova u brojeve ako je potrebno
      float tempC = atof(tempValue);
      float humi = atof(humiValue);
      float mm = atof(mmValue);

      // Clear fields for reusing the point. Tags will remain the same as set above.
      sensor.clearFields();

      // Store measured value into point
      sensor.addField("Temperature", tempC);
      sensor.addField("Humidity", humi);
      sensor.addField("mm", mm);

      // Print what are we exactly writing
      Serial.print("Writing: ");
      Serial.println(sensor.toLineProtocol());
      
      // Write point
      if (!client.writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
      }
    }
  }
  
  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }


}
