#include <SPI.h>
#include <LoRa.h>
#include <time.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <Adafruit_BME280.h>

//Lora Ra-02
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

//Hall Sensor
#define HALL_SENSOR_PIN 33  // Podesite pin na koji je povezan Hall senzor

//BME280
#define SEALEVELPRESSURE_HPA (1013.25)

RTC_DATA_ATTR time_t slaveTime;  // RTC memorija za vreme
RTC_DATA_ATTR int hallCounter = 0;  // RTC memorija za brojanje Hall senzora
const unsigned int syncIntervalMinutes = 5;  // Sinhronizacija svakih 5 minuta

Adafruit_BME280 bme; // I2C

void setup() {
  Serial.begin(115200);

  if (!(bme.begin(0x76))) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Inicijalizacija LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Pokretanje LoRa nije uspelo.");
    while (1);
  }

  // Inicijalizacija Hall senzora
  pinMode(HALL_SENSOR_PIN, INPUT);

  // Proveravamo razlog buđenja
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    float humi = bme.readHumidity();  // read humidity
    float tempC = bme.readTemperature();  // read temperature in Celsius

    float mm = 0.058946 * hallCounter;

    Serial.println("----------------");
    Serial.print("Temperature: ");
    Serial.println(tempC);
    Serial.print("Humidity: ");
    Serial.println(humi);
    Serial.print("mm: ");
    Serial.println(mm);
    Serial.println("----------------");

    LoRa.beginPacket();   //Send LoRa packet to receiver
    LoRa.print("Temp: ");
    LoRa.println(tempC);
    LoRa.print("Humidity: ");
    LoRa.println(humi);
    LoRa.print("mm: ");
    LoRa.println(mm);
    LoRa.endPacket();
    hallCounter = 0;
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    hallCounter++;  // Ako je buđenje izazvano Hall senzorom, uvećavamo brojač
    Serial.print("Hall senzor detektovan. Broj: ");
    Serial.println(hallCounter);
    delay(500);
  }

  // Sinhronizacija vremena sa Master-om svakih 5 minuta
  syncWithMaster();

  // Izračunaj koliko vremena je ostalo do sledećih 5 minuta
  int sleepTimeInSeconds = calculateSleepTime();

  // Podesite eksterno buđenje na Hall senzor (pin HALL_SENSOR_PIN) koristeći EXT1
  esp_sleep_enable_ext1_wakeup(1ULL << HALL_SENSOR_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);  // Buđenje na HIGH vrednost Hall senzora
  esp_sleep_enable_timer_wakeup(sleepTimeInSeconds * 1000000);  // Buđenje nakon izračunatog vremena
  
  Serial.print("Prelazak u deep sleep na: ");
  Serial.print(sleepTimeInSeconds);
  Serial.println(" sekundi.");

  esp_deep_sleep_start();  // Ulazak u deep sleep
}

void loop() {
  // Ova funkcija neće biti pozvana zbog deep sleep-a
}

// Funkcija za sinhronizaciju sa Master-om
void syncWithMaster() {
  LoRa.beginPacket();
  LoRa.print("SYNC_TIME");
  LoRa.endPacket();

  Serial.println("Poslat zahtev za sinhronizaciju vremena.");

  // Čekanje na odgovor
  while (LoRa.parsePacket() == 0) {
    // Čekanje da stigne vreme
  }

  String receivedTime = "";
  while (LoRa.available()) {
    receivedTime += (char)LoRa.read();
  }

  Serial.print("Primljeno vreme sa Master-a: ");
  Serial.println(receivedTime);

  // Pretvaranje primljenog vremena u time_t format
  struct tm tm;
  if (strptime(receivedTime.c_str(), "%Y-%m-%d %H:%M:%S", &tm) != nullptr) {
    slaveTime = mktime(&tm);  // Čuvanje sinhronizovanog vremena u RTC memoriji
    Serial.println("Vreme uspešno sinhronizovano sa Master-om.");
  }
}

// Funkcija za ispis trenutnog vremena na Slave-u
void printSlaveTime() {
  struct tm *timeinfo = localtime(&slaveTime);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  Serial.print("Trenutno vreme na Slave-u: ");
  Serial.println(buffer);
}

// Funkcija za izračunavanje koliko sekundi ostaje do sledećih 5 minuta
int calculateSleepTime() {
  struct tm *timeinfo = localtime(&slaveTime);
  int minutes = timeinfo->tm_min;
  int seconds = timeinfo->tm_sec;

  // Izračunavamo koliko je prošlo od poslednjeg intervala od 5 minuta
  int minutesUntilNextSync = syncIntervalMinutes - (minutes % syncIntervalMinutes);
  
  // Izračunavamo preostalo vreme do sledećeg celokupnog 5-minutnog intervala
  int sleepTimeInSeconds = (minutesUntilNextSync * 60) - seconds;
  return sleepTimeInSeconds;
}
