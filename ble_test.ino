#include <NimBLEDevice.h>

// Standard Nordic UART Service (NUS) UUIDs
#define DEVICE_NAME "STREAM_TEST"
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define TARGET_MTU 256


NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;

// Declared volatile to enforce cross-core execution safety
bool should_send = false;
volatile bool deviceConnected = false;
volatile uint16_t activeConnHandle = 0;

// Data point saved in packed struct (avoid padding confusion)
struct __attribute__((packed)) DataPoint {
  uint16_t timestamp; 
  uint16_t batt_mV;
  uint8_t duty;
};
static_assert(sizeof(DataPoint) == 5, "DataPoint must be 5 bytes");

const int TOTAL_POINTS = 10000;
DataPoint sensorHistory[TOTAL_POINTS];

void sendHistoricalData();

class MyCharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0 && deviceConnected) {
            Serial.println("[BLE] Valid transfer command recognized. Initiating bulk dump...");
            should_send = true;
        }
    }
};

class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        activeConnHandle = connInfo.getConnHandle(); 
        Serial.printf("[BLE] Client successfully connected! Connection Handle: %d\n", activeConnHandle);
        
        // PERFORMANCE INJECTION: Request optimized 15ms window directly via server reference
        pServer->updateConnParams(activeConnHandle, 12, 24, 0, 400);

        // Request our desired MTU
        NimBLEClient* pClient = pServer->getClient(activeConnHandle); 
        if (pClient != nullptr) {
            pClient->exchangeMTU(); 
        } 
    }
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        activeConnHandle = 0;
        Serial.printf("[BLE] Client disconnected cleanly. Reason code: %d\n", reason);
        Serial.println("[BLE] Resetting stack advertising profile...");
        pServer->startAdvertising(); 
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo& connInfo) override {
        Serial.printf("MTU negotiation complete. Current MTU size: %d\n", mtu);
    }
};

void setup() {
  Serial.begin(115200);
  delay(1500); 
  Serial.println("[System] Booting up hardware stack...");

  // Generate fake data 
  for (uint16_t i = 0; i < TOTAL_POINTS; i++) {
    sensorHistory[i].timestamp = i*1; 
    sensorHistory[i].batt_mV = i*2;
    sensorHistory[i].duty = i;
  }

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(TARGET_MTU);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        NIMBLE_PROPERTY::INDICATE
                      );
  // pTxCharacteristic->setCallbacks(new txCallbacks());
  NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                           CHARACTERISTIC_UUID_RX,
                                           NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
                                         );                                       
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  
  // Package advertisement profiles
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(DEVICE_NAME);
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  
  Serial.println("[BLE] Infrastructure operational. Awaiting incoming client connection...");
}

void loop() {
  if (should_send){
    // sendDebug();
    sendHistoricalData();
    should_send = false;
  }
}

void sendHistoricalData() {
  uint16_t negotiatedMtu = NimBLEDevice::getMTU(); // Local configuration baseline
  
  if (pServer->getConnectedCount() > 0) {
      negotiatedMtu = pServer->getPeerInfoByHandle(activeConnHandle).getMTU();
  }

  int maxPayloadBytes = negotiatedMtu - 3; 
  int itemSize = sizeof(DataPoint); 
  Serial.println(itemSize);
  int itemsPerPacket = maxPayloadBytes / itemSize;
  
  Serial.printf("[BLE] Pipeline Configuration -> Negotiated MTU: %d | Items per chunk: %d\n", negotiatedMtu, itemsPerPacket);

  int currentIndex = 0;
  unsigned long startTime = millis();

  while (currentIndex < TOTAL_POINTS && deviceConnected) {
    int itemsToSend = min(itemsPerPacket, TOTAL_POINTS - currentIndex);
    int bytesToSend = itemsToSend * itemSize;

    // Zero-copy reference mapping points straight to index memory slices
    uint8_t* payloadPtr = (uint8_t*)&sensorHistory[currentIndex];

    pTxCharacteristic->setValue(payloadPtr, bytesToSend);
    pTxCharacteristic->notify();

    currentIndex += itemsToSend;
    
    // Controlled hardware delay protects the ESP32-S3 network ring buffers from overflows
    delay(10); 
  }
  
  unsigned long duration = millis() - startTime;
  Serial.printf("[BLE] Bulk transaction accomplished! Delivered %d datapoints in %lu ms\n", currentIndex, duration);
}

