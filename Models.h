#ifndef Models_h
#define Models_h

#include "Common.h"

#pragma pack(push, 1) // Align to 1-byte boundaries
struct ClockPart {
  uint8_t bpm = 120;
  uint8_t morphTargetBpm = 100;
  uint8_t morphBars = 4;
  bool morphEnabled = false;
};

struct DrumSequencerChannel {
  uint16_t page[4] = {0};
  int16_t divider = 6;
  int16_t lastStep = 0;
  uint8_t enabled = 0;  
};

struct DrumSequencerPart {
  DrumSequencerChannel channel[DRUM_CHANNELS];
  uint8_t chainModeEnabled = 0;
};

struct SamplerPart {
  uint8_t bank = 0;
  uint16_t mix[DRUM_CHANNELS] = {0};
};

struct Automation {
  uint8_t slaveAddress;  
  uint8_t target;
  uint16_t value;
};
#pragma pack(pop)


struct Part {
  uint8_t pages = 0;
  uint8_t repeats = 0;
  int8_t chainTo = -1;
  ClockPart clockData;
  DrumSequencerPart drumSequencerData;
  SamplerPart samplerData;
};

struct Song {
  Part parts[PARTS];

  Song() {
    for(int i = 0; i < PARTS; i++) {
      parts[i] = Part();
    }
  }
};



void printSamplerRegisters(SamplerPart reg) {
  char s[100];
  sprintf(s, "sampler => bank: %d", reg.bank);
  Serial.println(s);
  for(int i=0; i<5; i++) {
    sprintf(s, "ch%d => mix: %d", i, reg.mix[i]);
    Serial.println(s);
  }  
}

void printTempoRegisters(ClockPart reg) {
  char s[100];
  sprintf(s, "tempo => bpm: %d | target bpm: %d | morph bars: %d | morph enabled: ", reg.bpm, reg.morphTargetBpm, reg.morphBars);
  Serial.print(s);
  Serial.println(reg.morphEnabled);
}

void printDrumSequencerChannel(DrumSequencerChannel channel, int index) {
  char s[100];
  sprintf(s, "ch%d => laststep: %d | divider: %d | output enabled: ", index, channel.lastStep, channel.divider);
  Serial.print(s);
  Serial.println(channel.enabled);
  Serial.print("steps: ");  
  for(int i=0; i<4; i++) {
    printInt(channel.page[i]);
  }
  Serial.println();
}

void printDrumSequencer(DrumSequencerPart drums) {
  for(int i=0; i<5; i++) {
    printDrumSequencerChannel(drums.channel[i], i);
  }
  Serial.print("chain mode enabled: ");
  Serial.println(drums.chainModeEnabled);
}

void printSongPart(Part part, int index) {
  char s[100];
  sprintf(s, "part %d => pages: %d | repeats: %d | chainTo: %d", index, part.pages, part.repeats, part.chainTo);
  Serial.println(s);
  printTempoRegisters(part.clockData);
  printDrumSequencer(part.drumSequencerData);
  printSamplerRegisters(part.samplerData);
}

void printSong(Song song) {
  Serial.println("SONG:");

  for(int i=0; i<PARTS; i++) {
    printSongPart(song.parts[i], i);
  }
}

#endif