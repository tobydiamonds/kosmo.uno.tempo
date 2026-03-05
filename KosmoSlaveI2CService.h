#ifndef KosmoSlaveI2CService_h
#define KosmoSlaveI2CService_h

#include <Wire.h>
#include "Common.h"
#include "Models.h"

template <typename PartType>
class KosmoSlaveI2CService {
private:
  uint8_t address;
  PartType parts[PARTS];
  unsigned long lastReceive = 0;
  const size_t totalPartSize = sizeof(PartType);
  int totalChunks = 0;
  int currentTxChunk = 0;
  uint8_t rxBuffer[sizeof(PartType)] = {0};
  void (*songPartsReceivedCallback)(void) = nullptr;
  void (*partIndexChangedCallback)(const int) = nullptr;
  void (*startCallback)(void) = nullptr;
  void (*stopCallback)(void) = nullptr;
  void (*initPartCallback)(const int) = nullptr;
  void (*initPartsCallback)(void) = nullptr;
  void (*automationCallback)(const Automation) = nullptr;
  void (*resetCallback)(void) = nullptr;
  

  static KosmoSlaveI2CService* instance;
  static void staticOnReceive(int size) {
    if (instance) {
      instance->onReceive(size);
    }
  } 
  static void staticOnRequest() {
    if(instance) {
      instance->onRequest();
    }
  }

  void onRequest() {
    // send current data to master
    if(currentTxChunk < totalChunks) {
      size_t offset = currentTxChunk * I2C_MAX;
      size_t chunkSize = min(I2C_MAX, totalPartSize-offset);
      uint8_t buffer[chunkSize];
      memcpy(buffer, (uint8_t*)&current + offset, chunkSize);  

    // Serial.print("Sending chunk: ");
    // for(size_t i = 0; i < chunkSize; i++) {
    //   Serial.print(buffer[i], HEX);
    //   Serial.print(" ");
    // }
    // Serial.println();

      Wire.write(buffer, chunkSize); // send data
      currentTxChunk++;      
      if(currentTxChunk==totalChunks)
        currentTxChunk=0;
    }
  }

  void onReceive(int size) {
    uint8_t buffer[I2C_MAX] = {0};
    size_t bytesRead = Wire.readBytes(buffer, size);  
    bool handled = false;

    if(bytesRead == 0) return;

    Instruction instruction = static_cast<Instruction>(buffer[0] & 0xF0);
    uint8_t partIndex = buffer[0] & 0x0F;

    switch (instruction) {
      case SetAutomation:
        receiveAutomation(buffer + 2, bytesRead - 2);
        handled = true;
        break;
      case SetPartIndex:
        if(partIndexChangedCallback) partIndexChangedCallback(partIndex);
        handled = true;
        break;
      case SetParts:
        receivePartData(buffer + 1, bytesRead - 1, partIndex);
        handled = true;
        break;
      case Start:
        if (startCallback) startCallback();
        handled = true;
        break;
      case Stop:
        if (stopCallback) stopCallback();
        handled = true;
        break;
      case InitPart:
        if(initPartCallback) initPartCallback(partIndex);
        handled = true;
        break;
      case InitParts:
        if(initPartsCallback) initPartsCallback();
        handled = true;
        break;
      case Reset:
        if(resetCallback) resetCallback();
        handled = true;
        break;
      default:
        break;
    }

    if(!handled) {
      Serial.print("Instruction not handled: ");
      printByteln(buffer[0]);
    }

  }

  void receiveAutomation(uint8_t* buffer, int size) {
    if(size != sizeof(Automation)) return;
    if(automationCallback) {
      Automation automation;
      memcpy((uint8_t*)&automation, buffer, size);
      automationCallback(automation);
    }
  }

  void receivePartData(uint8_t* buffer, int size, uint8_t partIndex) {
    if(size < 2) return; // first byte has chunk size, second byte must exist to have the processing happen
    if(partIndex < 0 || partIndex >= PARTS) return;

    // first byte has currentChunk
    uint8_t currentChunk = buffer[0];
    size_t offset = currentChunk * I2C_CHUNK_MAX;

    lastReceive = millis();

    char s[100];
    sprintf(s, "receiving part %d  chunk %d/%d - size: %d - offset: %d", partIndex, currentChunk, totalChunks, size-1, offset);
    Serial.println(s);
    printBuffer(buffer, size);

    for(int i=1; i<size; i++) { // buffer[0] has the chunksize
      rxBuffer[offset + i-1] = buffer[i];
    }

    if(currentChunk == totalChunks-1) {
      memcpy((uint8_t*)&parts[partIndex], rxBuffer, totalPartSize);
      if(partIndex == (PARTS-1) && songPartsReceivedCallback)
        songPartsReceivedCallback();
    }
  }

public:
  KosmoSlaveI2CService(uint8_t address)
    : address(address) {
    instance = this;
    Wire.begin(address);
    //Wire.setClock(400000);
    Wire.onReceive(staticOnReceive);
    Wire.onRequest(staticOnRequest);
    totalChunks = sizeof(PartType) / I2C_CHUNK_MAX;
    if(totalChunks==0)
      totalChunks = 1;
  }

  PartType current;

  void onSongPartsReceived(void (*callback)(void)) {
    songPartsReceivedCallback = callback;
  }

  void onPartIndexChanged(void (*callback)(const int)) {
    partIndexChangedCallback = callback;
  }

  void onStart(void (*callback)(void)) {
    startCallback = callback;
  }

  void onStop(void (*callback)(void)) {
    stopCallback = callback;
  }  

  void onInitPart(void (*callback)(const int)) {
    initPartCallback = callback;
  }

  void onInitPars(void (*callback)(void)) {
    initPartsCallback = callback;
  }

  void onAutomation(void (*callback)(const Automation)) {
    automationCallback = callback;
  }

  void onReset(void (*callback)(void)) {
    resetCallback = callback;
  }

  PartType getPart(int index) {
    if(index >= 0 && index < PARTS) {
      return parts[index];
    }
  }

  unsigned long getLastReceiveFromMaster() {
    return lastReceive;
  }

  void reset() {
    currentTxChunk = 0;
    Serial.println("slave reset");
  }
};

template <typename PartType>
KosmoSlaveI2CService<PartType>* KosmoSlaveI2CService<PartType>::instance;

#endif

