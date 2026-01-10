#include "Shared.h"
#include "DebounceButton165.h"
#include <SoftwareSerial.h>
#include "AnalogInput.h"
#include "kosmo-comm-slave.h"

#define LED_DATA_PIN 7 // 595/14
#define LED_LATCH_PIN 6 // 595/12
#define LED_CLOCK_PIN 5 // 595/11 

#define SWITCH_LOAD_PIN 8 // 165/1
#define SWITCH_CLOCK_PIN 9 // 165/2
#define SWITCH_DATA_PIN 10 // 165/9
#define SWITCH_FREEZE_PIN 11 // 165/15

#define CLOCK_OUT_PIN 12
#define RESET_OUT_PIN 13
#define MIDI_TX_PIN 4

#define PLAY_BTN_BIT 0
#define STOP_BTN_BIT 6
#define SYNC_BTN_BIT 2
#define TAP_BTN_BIT 1
#define MORPH_BTN_BIT 7

#define PULSE_WIDTH_MICROS 2000

const int INIT_BPM = 120;
const int BPM_MIN = 40;
const int BPM_MAX = 200;
const int POT_BPM_DEADBAND = 5;

// tap settings
const unsigned long TAP_DEBOUNCE_MS = 40;  // basic debounce for the tap button
const unsigned long TAP_MAX_GAP_MS = 2000; // maximum allowed gap between taps (if larger, tapping restarts)
const int REQUIRED_TAPS = 4;

unsigned long now = 0;
unsigned long lastScanInterval = 0;
unsigned long lastPotReadMillis = 0;
unsigned long lastClockOutLed = 0;
unsigned long lastResetOut = 0;

// runtime state
volatile int currentBpm = INIT_BPM;
volatile int morphTargetBpm = 90;
volatile bool tickFlag = false;
volatile bool midiClockPending = false;
volatile bool morphChangePending = false;
uint8_t morphBars = 4;
bool morphEnabled = false;
bool morphInProgress = false;
float morphChangePrBeat = 0.0;
float morphRemaining = 0.0;
bool clockOutLed = false;
bool resetOut = false;
bool resetOutLed = true;
enum State {STOPPED, PLAYING, PAUSED};
State state = STOPPED;

bool morphEnabledSetFromMaster = false;
bool prevMorphEnabledState = false;


//bool clockOutActive = false;
uint8_t ppqnPulses = 0;
uint8_t currentStep = 0;

// For tap detection:
unsigned long lastTapMillis = 0;
unsigned long tapTimes[REQUIRED_TAPS]; // store timestamps of taps (millis)
int tapCount = 0;
bool lastTapRawState = HIGH; // last raw read from shift register (assuming pullup)
unsigned long lastTapDebounceTime = 0;

DebounceButton165 tapButton(TAP_BTN_BIT);
DebounceButton165 startButton(PLAY_BTN_BIT);
DebounceButton165 stopButton(STOP_BTN_BIT);
DebounceButton165 syncButton(SYNC_BTN_BIT);
DebounceButton165 morphButton(7);

SoftwareSerial midiSerial(3, MIDI_TX_PIN); // RX, TX (RX not used)

AnalogInput tempoPot(A1);
AnalogInput morphPot(A0);


void setupTimer1(unsigned long microsPerTick) {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  // Timer tick = 0.5 us (prescaler 8)
  unsigned long ocr = (microsPerTick * 2) - 1;
  if (ocr > 65535) ocr = 65535;
  OCR1A = (uint16_t)ocr;

  // pulse width (in timer ticks)
  unsigned long ocrb = (PULSE_WIDTH_MICROS * 2);
  if (ocrb > 65535) ocrb = 65535;
  OCR1B = (uint16_t)ocrb;

  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS11);    // prescaler 8

  TIMSK1 |= (1 << OCIE1A);  // enable compare A interrupt
  TIMSK1 |= (1 << OCIE1B);  // enable compare B interrupt
  sei();
}

ISR(TIMER1_COMPA_vect) {
  // start the clock pulse signal

  if(state == PLAYING) {
    digitalWrite(CLOCK_OUT_PIN, HIGH);
    tickFlag = true;
    midiClockPending = true;

    ppqnPulses = (ppqnPulses < PPQN-1) ? ppqnPulses + 1 : 0;

    if(morphInProgress && ppqnPulses == 0) 
      morphChangePending = true;

  }
}

ISR(TIMER1_COMPB_vect) {
  // end the clock pulse signal
  if(state == PLAYING)
    digitalWrite(CLOCK_OUT_PIN, LOW);
}

void setBpm(uint16_t bpm) {
  if (bpm < 40) bpm = 40;
  if (bpm > 240) bpm = 240;
  currentBpm = bpm;
  registers.bpm = bpm;

  // microseconds per MIDI clock tick (24 ppqn)
  unsigned long usPerQuarter = 60000000UL / bpm;
  unsigned long usPerTick = usPerQuarter / PPQN;

  setupTimer1(usPerTick);
}

void setup() {
  Serial.begin(115200);
  midiSerial.begin(31250);

  // switches setup
  pinMode(SWITCH_LOAD_PIN, OUTPUT);
  pinMode(SWITCH_CLOCK_PIN, OUTPUT);
  pinMode(SWITCH_FREEZE_PIN, OUTPUT);
  pinMode(SWITCH_DATA_PIN, INPUT);

  // leds setup
  pinMode(LED_DATA_PIN, OUTPUT);
  pinMode(LED_LATCH_PIN, OUTPUT);
  pinMode(LED_CLOCK_PIN, OUTPUT);

  // clock+reset outputs
  pinMode(CLOCK_OUT_PIN, OUTPUT);
  digitalWrite(CLOCK_OUT_PIN, LOW);
  pinMode(RESET_OUT_PIN, OUTPUT);
  digitalWrite(RESET_OUT_PIN, LOW);

  // i2c comm
  setupSlave(INIT_BPM, morphTargetBpm, morphBars, morphEnabled);

  tempoPot.Begin(720);
  morphPot.Begin(4);

  Serial.println("Clock ready");
}

void midiSend(uint8_t data) {
  midiSerial.write(data);
}

// Common midi messages
void midiClockTick() { midiSend(0xF8); }
void midiStart()     { midiSend(0xFA); }
void midiContinue()  { midiSend(0xFB); }
void midiStop()      { midiSend(0xFC); }

uint8_t read165byte() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(SWITCH_CLOCK_PIN, LOW);      // prepare falling edge
    if (digitalRead(SWITCH_DATA_PIN)) {
      value |= (1 << i);               // store bit in LSB first
    }
    digitalWrite(SWITCH_CLOCK_PIN, HIGH);     // shift register updates here
  }
  return value;
}

void start() {
  state = PLAYING;
  midiStart();
}

void stop() {
  state = STOPPED;
  digitalWrite(RESET_OUT_PIN, HIGH);
  midiStop();
  resetOut = true;
  resetOutLed = true;
  lastResetOut = now;
  ppqnPulses = 0;
  morphChangePrBeat = 0;
  morphRemaining = 0;
  morphInProgress = false;        
  currentStep = 0;  
}

void pause() {
  state = PAUSED;        
  midiStop();
}

void resume() {
  state = PLAYING;             
  midiContinue();
}

void scanInputs() {
    // load shift register
    digitalWrite(SWITCH_LOAD_PIN, LOW);
    //delayMicroseconds(5);
    digitalWrite(SWITCH_LOAD_PIN, HIGH);
    //delayMicroseconds(5);

    digitalWrite(SWITCH_CLOCK_PIN, HIGH);
    digitalWrite(SWITCH_FREEZE_PIN, LOW);

    uint8_t incoming = read165byte();
    //printByteln(incoming);

    bool morphEnabledState = !(incoming & (1 << 3));
    if(morphEnabledState != prevMorphEnabledState) {
      morphEnabledSetFromMaster = false;
      prevMorphEnabledState = morphEnabledState;
    }

    if(!morphEnabledSetFromMaster) { // only update if not set from master, or if it was changed after set from master
      morphEnabled = morphEnabledState;
      registers.morphEnabled = morphEnabledState;
    }

    tapButton.update(incoming, now);

    if(programming) {
      if(tapButton.wasPressed()) {
        Serial.println("we are in programming mode and the tap button has been pressed");
        readyToSendRegisters = true;
      }
      
    } else {
      startButton.update(incoming, now);
      stopButton.update(incoming, now);
      syncButton.update(incoming, now);
      morphButton.update(incoming, now);

      if(tapButton.wasPressed()) {
        registerTap(now);
      }
    
      if(startButton.wasPressed()) {
        if(state == STOPPED) {
          start();
        } else if(state == PLAYING) {
          pause();
        } else if(state == PAUSED) {
          resume();
        }
      } 
      if(stopButton.wasPressed()) {
          stop();
      }
      if(syncButton.wasPressed() && state == PLAYING) {
          state = PLAYING;
          digitalWrite(RESET_OUT_PIN, HIGH);
          midiStart();
          resetOut = true;
          resetOutLed = true;
          lastResetOut = now;     
          ppqnPulses = 0;
      }
      if(morphButton.wasPressed() && morphEnabled) {
        setupMorphing(!morphInProgress);
      }
    } // programming
    


    digitalWrite(SWITCH_FREEZE_PIN, HIGH);    
}

void setupMorphing(bool enabled) {
    // if we have 5 bars and wants to go from 125 to 100 bpm we want to decrease 25 bpm over 20 beats so that is 25/20 bpm decrease pr beat

  if(enabled) {
    float morphDelta = abs(currentBpm - morphTargetBpm);
    int morphBeats = morphBars * 4;
    morphChangePrBeat = (currentBpm > morphTargetBpm) ? (morphDelta / morphBeats) * -1 : morphDelta / morphBeats;
    morphRemaining = morphDelta;
    morphInProgress = true;
  } else {
    morphChangePrBeat = 0;
    morphRemaining = 0;
    morphInProgress = false;
  }  
}

// 3-digit display
const byte digitEnable_3digit[3] = {
  ~(B00100000),
  ~(B00010000),
  ~(B00000001)
};

// 2-digit display
const byte digitEnable_2digit[2] = {
  ~(B01000000), // QG -> digit1 (MSD)
  ~(B10000000)  // QH -> digit2
};

const byte digitToSegment[14] = {
  // dpGFEDCBA
  B00111111, // 0
  B00000110, // 1
  B01011011, // 2
  B01001111, // 3
  B01100110, // 4
  B01101101, // 5
  B01111101, // 6
  B00000111, // 7
  B01111111, // 8
  B01101111, // 9
  B00000000,  // reset display
  B01110011, // P
  B01010000, // r
  B01111101  // G
};

int getDigit(long number, int position) {
  // position = 0 => least significant digit
  for (int i = 0; i < position; i++) {
    number /= 10;
  }
  return number % 10;
}

int getProgrammingDigit(int position) {
  switch(position) {
    case 0: return 13; break;
    case 1: return 12; break;
    case 2: return 11; break;
    default: return 10;
  }
}

void sendToShiftRegisters(byte sr1, byte sr2, byte sr3) {
  digitalWrite(LED_LATCH_PIN, LOW);
  shiftOut(LED_DATA_PIN, LED_CLOCK_PIN, MSBFIRST, sr3);
  shiftOut(LED_DATA_PIN, LED_CLOCK_PIN, MSBFIRST, sr2);
  shiftOut(LED_DATA_PIN, LED_CLOCK_PIN, MSBFIRST, sr1);
  digitalWrite(LED_LATCH_PIN, HIGH);
}

void displayValues(int val3, int val2) {
  // multiplex all 5 digits
  for (int d = 0; d < 5; d++) {
    byte sr1 = 0;
    byte sr2 = 0;
    byte sr3 = 0;

    if (d < 3) {
      // 3-digit display
      int digitVal = (!programming) ? getDigit(val3, d) : getProgrammingDigit(d);
      sr3 = digitToSegment[digitVal];
      sr1 = digitEnable_3digit[d];
    } else {
      // 2-digit display
      int digitVal = (morphEnabled) ? getDigit(val2, d - 3) : 10; // when we are not in morph, show no digits (10=no leds on)
      sr2 = digitToSegment[digitVal];
      sr1 = digitEnable_2digit[d - 3];
    }

    if(!morphEnabled) // 0x02
      sr1 &= 0xFD;
    if(!clockOutLed) // 0x04
      sr1 &= 0xFB;
    if(!resetOutLed) // 0x08 
      sr1 &= 0xF7;

    sendToShiftRegisters(sr1, sr2, sr3);
    delayMicroseconds(2000); // adjust refresh speed
  }
}

void updateUI() {
  if(programming) {
    displayValues(0, morphBars);
  } else {
    int bpmToDisplay = currentBpm;
    if(morphEnabled && !morphInProgress)
      bpmToDisplay = morphTargetBpm;
    displayValues(bpmToDisplay, morphBars);
  }
}


void loop() {
  now = millis();

  if(startTheClock) {
    startTheClock = false;
    Serial.println("Starting the clock");
    start();
  }

  if(stopTheClock) {
    stopTheClock = false;
    Serial.println("Stopping the clock");
    stop();
  }

  if(now > (lastScanInterval + INPUT_SCAN_INTERVAL)) {
    lastScanInterval = now;
    scanInputs();

    if(!morphInProgress) {
      // Read pots periodically
      uint16_t morphBarsRaw;
      if(morphPot.Changed(now, morphBarsRaw)) {
        morphBars = map(morphBarsRaw, 1024, 0, 16, 0);
        registers.morphBars = morphBars;
      }

      uint16_t bpmRaw;
      if(tempoPot.Changed(now, bpmRaw)) {
        if(morphEnabled) {
          morphTargetBpm = mapPotToBpm(bpmRaw);
          registers.morphTargetBpm = morphTargetBpm;
        } else {
          setBpm(mapPotToBpm(bpmRaw));
        }
      }      
    }
  }

  if (state == PLAYING && tickFlag) {
    tickFlag = false;
    if(ppqnPulses % 6 == 0) {
      currentStep = (currentStep < STEPS_PR_BAR-1) ? currentStep + 1 : 0;
    }
    //clockOutActive = true;
    clockOutLed = (ppqnPulses == 0); // turn on led every beat
    if(clockOutLed)
      lastClockOutLed = now;

    if(morphChangePending) {
      morphChangePending = false;
      int newBpm = currentBpm + morphChangePrBeat;
      
      if(newBpm > BPM_MAX) {
        setBpm(BPM_MAX);
        morphInProgress = false;
      }
      else if(newBpm < BPM_MIN) {
        setBpm(BPM_MIN);
        morphInProgress = false;
      }
      else if(newBpm != morphTargetBpm) {
        setBpm(newBpm);
        morphRemaining -= abs(morphChangePrBeat);
        morphInProgress = morphRemaining > 0;
      } else {
        setBpm(morphTargetBpm);
        morphInProgress = false;
      }

    }    
  }    

  if (clockOutLed && (now > (lastClockOutLed + LED_SHORT_PULSE))) {
    clockOutLed = false;
  }

  if (resetOut && (now > lastResetOut + OUTPUT_PULSE)) {
    digitalWrite(RESET_OUT_PIN, LOW);
    resetOut = false;
  }

  if (resetOutLed && (now > lastResetOut + LED_SHORT_PULSE)) {
    resetOutLed = false;
  }

  if (tapCount > 0 && (now - tapTimes[tapCount - 1] > TAP_MAX_GAP_MS)) {
    tapCount = 0; // restart tapping sequence
  }

  if (midiClockPending) {
    midiClockTick();
    midiClockPending = false;
  }

  if(newPartData && (state != PLAYING || currentStep == STEPS_PR_BAR - 1)) {
    newPartData = false;

    setBpm(nextRegisters.bpm);
    morphTargetBpm = nextRegisters.morphTargetBpm;
    morphBars = nextRegisters.morphBars;
    morphEnabled = nextRegisters.morphEnabled;
    morphEnabledSetFromMaster = true;
    setupMorphing(state == PLAYING && morphEnabled);   

  }



  updateUI();

  
}

int mapPotToBpm(int analogVal) {
  // analogVal is 0..1023 -> BPM_MIN..BPM_MAX
  float fraction = constrain((float)analogVal / 1023.0, 0.0, 1.0);
  return BPM_MIN + (BPM_MAX - BPM_MIN) * fraction;
}


void registerTap(unsigned long nowMillis) {
  // Add tap timestamp
  if (tapCount < REQUIRED_TAPS) {
    tapTimes[tapCount++] = nowMillis;
  } else {
    // shift left and append new
    for (int i = 0; i < REQUIRED_TAPS - 1; ++i) tapTimes[i] = tapTimes[i + 1];
    tapTimes[REQUIRED_TAPS - 1] = nowMillis;
  }


  if (tapCount >= REQUIRED_TAPS) {
    // compute intervals between consecutive taps
    unsigned long intervalsSum = 0;
    int intervals = 0;
    for (int i = 1; i < REQUIRED_TAPS; ++i) {
      unsigned long dt = tapTimes[i] - tapTimes[i - 1];
      // ignore unrealistic dt
      if (dt == 0 || dt > TAP_MAX_GAP_MS) {
        // invalid sequence, reset and return
        tapCount = 0;
        return;
      }
      intervalsSum += dt;
      intervals++;
    }
    if (intervals > 0) {
      float avgMs = (float)intervalsSum / (float)intervals;
      float bpm = 60000.0f / avgMs; // because avgMs is ms per beat (tap is quarter note)
      // clamp
      if (bpm < BPM_MIN) bpm = BPM_MIN;
      if (bpm > BPM_MAX) bpm = BPM_MAX;
      //currentBpm = bpm;
      setBpm(bpm);
      // keep next tick phase coherent
      // reset taps so we require 4 new ones to change again
      tapCount = 0;
    }
  }
}