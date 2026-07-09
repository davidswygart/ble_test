#include <Arduino.h>
#include <NimBLEDevice.h>

// ----------------------------------------------------
// Configuration
// ----------------------------------------------------

/// ADC read of voltage 
const int ADC_PIN = 9;
const int n_avg = 1000;
const int wait = 1; //How long (ms) to wait between samples for average
const uint32_t SAMPLE_INTERVAL_MS = 1000 * 60 * 5; // How long to wait between readings (5 minutes)

/// Saving historical log
const float max_mV = 3300.0;
const float min_mV = 1300.0;
const size_t BUFFER_LENGTH = 512; //Can't go over 512 bytes for normal Characteristic

/// Output
const int OUT_PIN = 4;
const float ON_mV = 2150.0;
const float OFF_mv = 2100.0;

/// BLE stuff
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_UUID_HISTORY   "6e400004-b5a3-f393-e0a9-e50e24dcca9e"
static const uint32_t CLIENT_TIMEOUT_MS = 1000*60*10; // Disconnect client after 10 minutes

// ----------------------------------------------------
// Global variables and flags
// ----------------------------------------------------
bool isON;
uint8_t linearBuffer[BUFFER_LENGTH] = {0};
NimBLECharacteristic* historyChar;
NimBLEServer* bleServer = nullptr;
NimBLEAdvertising* bleAdvertising = nullptr;
bool clientConnected = false;
uint16_t clientConnId = 0;
uint32_t clientConnectedSinceMs = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    clientConnected = true;
    clientConnId = connInfo.getConnHandle();
    clientConnectedSinceMs = millis();
    Serial.println("Client connected");
    if (bleAdvertising != nullptr) {
      bleAdvertising->stop();
    }
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    clientConnected = false;
    clientConnId = 0;
    Serial.println("Client disconnected");
    if (bleAdvertising != nullptr) {
      bleAdvertising->start();
    }
  }
};

// ----------------------------------------------------
// Helper functions
// ----------------------------------------------------
float readAvg() {
  uint32_t sum = 0;
  for (int i=0; i<n_avg; i++){
    sum += analogReadMilliVolts(ADC_PIN);
    delay(wait);
    }
  float avg = sum / n_avg;
  Serial.printf("ADC: %f mV\n", avg);
  return avg;
}

uint8_t rescale_mV_to_byte(float mV) {
  static const float multiplier = 255.0f / (max_mV - min_mV);
  if (mV <= min_mV) return 0;
  if (mV >= max_mV) return 255;
  return static_cast<uint8_t>(((mV - min_mV) * multiplier) + 0.5f);
}

void addToRecord(float mV) {
  // Shift the buffer to the left to make room for the new value
  for (int i = 1; i < BUFFER_LENGTH; i++) {
      linearBuffer[i-1] = linearBuffer[i];
  }

  // Set the last element of the buffer to the new sensor reading
  linearBuffer[BUFFER_LENGTH - 1] = rescale_mV_to_byte(mV);
  
  // Update the BLE characteristic with the new buffer value (we could only do this onRead, but this is simpler)
  historyChar->setValue(linearBuffer, BUFFER_LENGTH);
}

// ----------------------------------------------------
// Setup and Main loops
// ----------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(OUT_PIN, OUTPUT);
    digitalWrite(OUT_PIN, LOW);
    isON = false;

    NimBLEDevice::init("XIAO-ESP32S3-History");
    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());
    NimBLEService* service = bleServer->createService(SERVICE_UUID);
    historyChar = service->createCharacteristic(
        CHAR_UUID_HISTORY,
        NIMBLE_PROPERTY::READ
    );
    historyChar->setValue(linearBuffer, BUFFER_LENGTH);
    service->start();
    bleAdvertising = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData adData;
    adData.setName("XIAO-ESP32S3-History");
    adData.addServiceUUID(SERVICE_UUID);
    bleAdvertising->setAdvertisementData(adData);
    bleAdvertising->start();

    Serial.println("BLE ready, advertising...");
}

void loop() {
  float mV = readAvg();

  if (!isON & (mV > ON_mV)){
    digitalWrite(OUT_PIN, HIGH);
    Serial.println("Turning ON");
    isON = true;
  }

  if (isON & (mV < OFF_mv)){
    digitalWrite(OUT_PIN, LOW);
    Serial.println("Turning OFF");
    isON = false;
  }

  addToRecord(mV);

  if (clientConnected && (bleServer != nullptr) && (millis() - clientConnectedSinceMs >= CLIENT_TIMEOUT_MS)) {
    Serial.printf("Client timeout reached (%lu ms), disconnecting...\n", CLIENT_TIMEOUT_MS);
    bleServer->disconnect(clientConnId);
  }

  static const int loop_delay = SAMPLE_INTERVAL_MS-(n_avg*wait);
  delay(loop_delay);
}
