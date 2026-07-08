#include <Arduino.h>
#include <NimBLEDevice.h>

// ----------------------------------------------------
// Configuration
// ----------------------------------------------------
static const size_t BUFFER_LENGTH = 512;
static const uint32_t SAMPLE_INTERVAL_MS = 2000;
uint8_t linearBuffer[BUFFER_LENGTH] = {0};
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_UUID_HISTORY   "6e400004-b5a3-f393-e0a9-e50e24dcca9e"

NimBLECharacteristic* historyChar;

// ----------------------------------------------------
// Fake sensor
// ----------------------------------------------------
uint8_t counter = 0;
uint8_t readSensor() {
    return (counter++); // Simulate a sensor reading (0-255)
}

void addSample() {
    // Shift the buffer to the left to make room for the new value
    for (int i = 1; i < BUFFER_LENGTH; i++) {
        linearBuffer[i-1] = linearBuffer[i];
    }

    // Set the last element of the buffer to the new sensor reading
    linearBuffer[BUFFER_LENGTH - 1] = readSensor();
    
    // Update the BLE characteristic with the new buffer value (we could only do this onRead, but this is simpler)
    historyChar->setValue(linearBuffer, BUFFER_LENGTH);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    NimBLEDevice::init("XIAO-ESP32S3-History");

    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(SERVICE_UUID);

    historyChar = service->createCharacteristic(
        CHAR_UUID_HISTORY,
        NIMBLE_PROPERTY::READ
    );

    historyChar->setValue(linearBuffer, BUFFER_LENGTH);

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData adData;
    adData.setName("XIAO-ESP32S3-History");
    adData.addServiceUUID(SERVICE_UUID);

    adv->setAdvertisementData(adData);
    adv->start();

    Serial.println("BLE ready, advertising...");
}

// ----------------------------------------------------
// Main loop
// ----------------------------------------------------
void loop() {
    addSample();
    delay(SAMPLE_INTERVAL_MS);
}
