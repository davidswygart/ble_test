#include <NimBLEDevice.h>

// Standard Nordic UART Service (NUS) UUIDs
#define DEVICE_NAME "STREAM_TEST"
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define TARGET_MTU 512


NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;

// Declared volatile to enforce cross-core execution safety
bool new_write = false;
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
        if (pCharacteristic->getValue().length() > 0 && deviceConnected) {
            new_write = true;
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
        pServer->setDataLen(connInfo.getConnHandle(), 251);
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
                        NIMBLE_PROPERTY::NOTIFY
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
  if (new_write){
    new_write = false;

    Serial.println("Starting bulk transfer");
    sendHistoricalData();
  }
}



typedef struct {
    uint16_t seq_num : 15; 
    uint16_t fin     : 1;  
} __attribute__((packed)) ble_header_t;

void send_ble_packet(uint8_t is_fin, uint16_t seq, uint8_t* payload, size_t payload_len) {
    ble_header_t header;
    header.seq_num = seq;
    header.fin = is_fin;

    size_t header_len = sizeof(ble_header_t); // 2 bytes
    size_t total_len = header_len + payload_len;
    uint8_t tx_buffer[total_len];

    // Copy the header into the front of the array
    memcpy(tx_buffer, &header, header_len);
    
    // Copy the payload data immediately after the header
    if (payload_len > 0 && payload != nullptr) {
      memcpy(tx_buffer + header_len, payload, payload_len);
    }

    // 5. Send via NimBLE
    pTxCharacteristic->setValue(tx_buffer, total_len);
    pTxCharacteristic->notify(); 
}

void sendHistoricalData() {
  uint16_t negotiatedMtu = NimBLEDevice::getMTU(); // Local configuration baseline
  
  if (pServer->getConnectedCount() > 0) {
      negotiatedMtu = pServer->getPeerInfoByHandle(activeConnHandle).getMTU();
  }

  int maxPayloadBytes = negotiatedMtu - 3 - sizeof(ble_header_t); 
  int itemSize = sizeof(DataPoint); 
  int itemsPerPacket = maxPayloadBytes / itemSize;
  
  Serial.printf("[BLE] Pipeline Configuration -> Negotiated MTU: %d | Items per chunk: %d\n", negotiatedMtu, itemsPerPacket);

  int currentIndex = 0;
  uint16_t sequence_number = 0;
  unsigned long startTime = millis();
  while (currentIndex < TOTAL_POINTS && deviceConnected) {
    int num_remaining = TOTAL_POINTS - currentIndex;
    uint8_t is_fin = (itemsPerPacket >= num_remaining) ? 1 : 0;
    int itemsToSend = (is_fin) ? num_remaining : itemsPerPacket;
    int payload_len = itemsToSend * itemSize;

    uint8_t* payloadPtr = (uint8_t*)&sensorHistory[currentIndex];

    new_write = false;
    Serial.printf("[BLE] Sending packet %d\n", sequence_number);
    send_ble_packet(is_fin, sequence_number, payloadPtr, payload_len);
    while (!new_write && deviceConnected){delay(1);} // Wait for confirmation
    new_write = false;

    currentIndex += itemsToSend;
    sequence_number++;
  }
  
  unsigned long duration = millis() - startTime;
  Serial.printf("[BLE] Bulk transaction accomplished! Delivered %d datapoints, in %d packets, in %lu ms\n", currentIndex, sequence_number, duration);
}

