#include <Arduino.h>
#include <NimBLEDevice.h>

// ----------------------------------------------------
// Configuration
// ----------------------------------------------------
static const size_t SAMPLE_SIZE = 2;          // int16_t
static const size_t BUFFER_LENGTH = 2048;     // bytes
static const uint32_t SAMPLE_INTERVAL_MS = 5000;

uint8_t circBuffer[BUFFER_LENGTH];
size_t writeIndex = 0;

// ----------------------------------------------------
// BLE UUIDs
// ----------------------------------------------------
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_UUID_HISTORY   "6e400004-b5a3-f393-e0a9-e50e24dcca9e"

NimBLECharacteristic* historyChar;

// ----------------------------------------------------
// Fake sensor
// ----------------------------------------------------
int16_t readSensor() {
    return (millis() / 100) % 1000;
}

// ----------------------------------------------------
// Circular buffer write
// ----------------------------------------------------
void addSample(int16_t value) {
    circBuffer[writeIndex] = value & 0xFF;
    writeIndex = (writeIndex + 1) % BUFFER_LENGTH;

    circBuffer[writeIndex] = (value >> 8) & 0xFF;
    writeIndex = (writeIndex + 1) % BUFFER_LENGTH;
}

// ----------------------------------------------------
// Update BLE characteristic with full buffer
// ----------------------------------------------------
void updateBLEBlob() {
    historyChar->setValue(circBuffer, BUFFER_LENGTH);
}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------
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

    // Initial empty buffer
    historyChar->setValue(circBuffer, BUFFER_LENGTH);

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);

    // No scan response needed
    adv->start();

    Serial.println("BLE ready, advertising...");
}

// ----------------------------------------------------
// Main loop
// ----------------------------------------------------
void loop() {
    int16_t sample = readSensor();
    addSample(sample);
    updateBLEBlob();

    delay(SAMPLE_INTERVAL_MS);
}
