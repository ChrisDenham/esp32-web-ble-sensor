/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-web-ble-sensor-visualization/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <DHT.h>

#define DHTPIN 5     // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

// BLE UUIDs 
// Environmental Sensing Service
#define SERVICE_UUID              "181A"
// Temperature
#define TEMP_CHARACTERISTIC_UUID  "2A6E"
// Humidity
#define HUM_CHARACTERISTIC_UUID   "2A6F"
// Pressure
#define PRESS_CHARACTERISTIC_UUID "2A6D"

// BLE variables
BLEServer*         pServer            = nullptr;
BLECharacteristic* pTempChar          = nullptr;
BLECharacteristic* pHumChar           = nullptr;
BLECharacteristic* pPressChar         = nullptr;
bool               deviceConnected    = false;
bool               oldDeviceConnected = false;

// Send a reading every 5 seconds
const unsigned long SAMPLE_INTERVAL_MS = 5000;
unsigned long lastSampleMs = 0;

// BLE server callbacks
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE client connected.");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected.");
  }
};

// Format a float to one decimal place and set it on a BLE characteristic,
// then call notify() so connected clients receive it immediately
void notifyFloat(BLECharacteristic* pChar, float value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", value);
  pChar->setValue(buf);
  pChar->notify();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 DHT11 BLE Server starting...");

  // Initialize DHT11 Sensor
  dht.begin();

  // ESP32 BLE init
  BLEDevice::init("ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Temperature characteristic — READ + NOTIFY
  pTempChar = pService->createCharacteristic(
    TEMP_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pTempChar->addDescriptor(new BLE2902());

  // Humidity characteristic — READ + NOTIFY
  pHumChar = pService->createCharacteristic(
    HUM_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pHumChar->addDescriptor(new BLE2902());

  // Pressure characteristic — READ + NOTIFY
  pPressChar = pService->createCharacteristic(
    PRESS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pPressChar->addDescriptor(new BLE2902());

  // Set initial values
  pTempChar->setValue("0.0");
  pHumChar->setValue("0.0");
  pPressChar->setValue("0.0");

  // Start the service
  pService->start();

  // Start BLE Device Advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising started. Waiting for client...");
}

void loop() {
  unsigned long now = millis();

  // Sample and notify on schedule
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;

    float tempC     = dht.readTemperature();   // °C
    float humidity  = dht.readHumidity();      // %
    float pressurePa  = 0.0;    // Pa
    float pressureHPa = pressurePa / 100.0F;   // hPa

    Serial.printf("Temp: %.1f °C  Hum: %.1f %%  Press: %.1f hPa\n",
                   tempC, humidity, pressureHPa);

      // Notify BLE client with sensor readings if a BLE client is connected
      if (deviceConnected) {
        notifyFloat(pTempChar,  tempC);
        notifyFloat(pHumChar,   humidity);
        notifyFloat(pPressChar, pressureHPa);
      }
  }

  // Handle BLE reconnect after unexpected disconnect
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restarted advertising.");
    oldDeviceConnected = false;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
    Serial.println("BLE client connected.");
  }
}
