#ifndef Common_h
#define Common_h

#define I2C_MAX 32
//#define I2C_CHUNK_MAX 25
#define DRUM_CHANNELS 5
#define PARTS 8
#define PPQN 24.0
#define PPQN_PR_BEAT 6
#define STEPS_PR_BAR 16


#define INPUT_SCAN_INTERVAL 10
#define OUTPUT_PULSE 100
#define LED_SHORT_PULSE 50
#define LED_VERY_SHORT_PULSE 25

const int I2C_CHUNK_MAX = 30;

enum Instruction {
  SetPartIndex = 0x10, 
  SetParts = 0x20,     
  Start = 0x30,        
  Stop = 0x40,         
  InitPart = 0x50,     
  InitParts = 0x60,
  SetAutomation = 0x70,
  Reset = 0xF0
};

void printByte(uint8_t data) {
  char s[9];
  for (int i = 0; i < 8; i++) {
    s[i] = (data & (1 << (7 - i))) ? '1' : '0';
  }
  s[8] = '\0'; // Null terminator for the string  
  Serial.print(s);
}

void printByteln(uint8_t data) {
  printByte(data);
  Serial.println();
}


void printInt(uint16_t value) {
  for(int i=0; i<16; i++) {
    if((value >> i) & 1) Serial.print("1");
    else Serial.print("0");
  }
  Serial.print(" ");
}

void printIntln(uint16_t value) {
  printInt(value);
  Serial.println();
}

void printBuffer(const uint8_t* buffer, size_t size) {
  Serial.print("Buffer contents: ");
  for (size_t i = 0; i < size; i++) {
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool isString(const uint8_t* buffer, size_t size) {
  for(size_t i=0; i<size; i++) {
    if(buffer[i] < 32 || buffer[i] > 126)
      return false;
  }
  return true;
}



#endif