#ifndef KosmoCommSlave_h
#define KosmoCommSlave_h

#include <Wire.h>

#define SLAVE_ADDR 8

bool programming = false;
bool readyToSendRegisters = false;
bool newPartData = false;
bool stopTheClock = false;
bool startTheClock = false;

struct TempoRegisters {
  uint8_t bpm;
  uint8_t morphTargetBpm;
  uint8_t morphBars;
  bool morphEnabled;
};

TempoRegisters registers;
TempoRegisters nextRegisters;

void onRequest() {
  Wire.write((byte*)&registers, sizeof(registers)); // Send the registers
}

void printTempoRegisters(const TempoRegisters& regs) {
  Serial.print("BPM: ");
  Serial.println(regs.bpm);
  Serial.print("Morph Target BPM: ");
  Serial.println(regs.morphTargetBpm);
  Serial.print("Morph Bars: ");
  Serial.println(regs.morphBars);
  Serial.print("Morph Enabled: ");
  Serial.println(regs.morphEnabled ? "Yes" : "No");
}

void onReceive(int size) {
  char cmd[6] = {0};  // Initialize with zeros
  int index = 0;
  bool done = false;

  while (Wire.available()) {
    char byte = Wire.read(); // read even were done to get all remaning bytes off the i2c bus
    if(!done) {
      if (index < 5) {
        cmd[index++] = byte;
      }

      // Check for "set" command
      if (index == 3 && strncmp(cmd, "set", 3) == 0) {
        if (size >= 3 + sizeof(TempoRegisters)) {
          Wire.readBytes((char*)&nextRegisters, sizeof(TempoRegisters));
          newPartData = true;
          //printTempoRegisters(nextRegisters);  // Print the received data
        }
        done = true;
      } 
      // Check for "stop" command
      else if (index == 4 && strncmp(cmd, "stop", 4) == 0) {
        //Serial.println("Received stop command");
        stopTheClock = true;
        startTheClock = false;
        done = true;
      }
      // Check for "start" command
      else if (index == 5 && strncmp(cmd, "start", 5) == 0) {
        //Serial.println("Received start command");
        startTheClock = true;
        stopTheClock = false;
        done = true;
      }
    }
  }
}


void setupSlave(uint8_t bpm, uint8_t morphTargetBpm, uint8_t morphBars, bool morphEnabled) {
  registers.bpm = bpm;
  registers.morphTargetBpm = morphTargetBpm;
  registers.morphBars = morphBars;
  registers.morphEnabled = morphEnabled;

  Wire.begin(SLAVE_ADDR);
  Wire.setClock(400000);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Serial.println("kosmo clock slave ready");
}



#endif