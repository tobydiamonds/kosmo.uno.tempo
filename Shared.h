#ifndef Shared_h
#define Shared_h

#define PPQN 24.0
#define PPQN_PR_BEAT 6
#define STEPS_PR_BAR 16


#define INPUT_SCAN_INTERVAL 10
#define OUTPUT_PULSE 100
#define LED_SHORT_PULSE 50
#define LED_VERY_SHORT_PULSE 25






uint8_t MapToByte(uint16_t value, uint8_t lower, uint8_t upper) {
  uint8_t result = map(value, 0, 1023, lower, upper);
  if(value >= 1000)
    result = upper;
  return result;
}

void printByte(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  if(bitOrder == LSBFIRST) {
    for(int j=7; j>=0; j--) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }    
  } else {
    for(int j=0; j<8; j++) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }        
  }
  Serial.print(" ");
}

void printByteln(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  printByte(b, bitOrder);
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

#endif