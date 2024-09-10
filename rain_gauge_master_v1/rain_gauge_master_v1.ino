#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

const char* ssid = "TP-Link_BA5A";      // Uneti WiFi SSID
const char* password = "69073953";      // Uneti WiFi password

void setupTime() {
  configTime(3600, 3600, "pool.ntp.org"); // Podešavanje za vremensku zonu Beograda (GMT+1)
  while (!time(nullptr)) {
    Serial.println("Povezivanje na NTP...");
    delay(1000);
  }
}

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);

  // Povezivanje na WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Povezivanje na WiFi...");
  }
  Serial.println("WiFi povezan.");

  setupTime();

  // Inicijalizacija LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Pokretanje LoRa nije uspelo.");
    while (1);
  }
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
    }
  }
}
