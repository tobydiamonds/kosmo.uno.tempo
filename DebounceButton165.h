#ifndef DebounceButton165_h
#define DebounceButton165_h

class DebounceButton165 {
public:
  DebounceButton165(uint8_t bitIndex, bool activeLow = false, unsigned long debounceMs = 40)
    : bitIndex(bitIndex), activeLow(activeLow), debounceMs(debounceMs) {}

  // Call every loop with the freshly read shift register value
  void update(uint8_t regVal, unsigned long nowMs) {
    uint8_t mask = (uint8_t)(1U << bitIndex);
    bool raw = (regVal & mask) != 0;
    if (activeLow) raw = !raw; // convert to active HIGH if activeLow

    if (raw != lastRaw) {
      lastDebounceTime = nowMs;
      lastRaw = raw;
    }

    if ((nowMs - lastDebounceTime) >= debounceMs) {
      if (raw != stableState) {
        stableState = raw;
        if (stableState) {
          pressedEvent = true;   // rising edge
        } else {
          releasedEvent = true;  // falling edge
        }
      }
    }
  }

  // Check if button was pressed since last call
  bool wasPressed() {
    if (pressedEvent) {
      pressedEvent = false;
      return true;
    }
    return false;
  }

  // Check if button was released since last call
  bool wasReleased() {
    if (releasedEvent) {
      releasedEvent = false;
      return true;
    }
    return false;
  }

  // Current stable state (true if pressed)
  bool isDown() const { return stableState; }

private:
  uint8_t bitIndex;
  bool activeLow;
  unsigned long debounceMs;

  bool lastRaw = false;
  bool stableState = false;
  bool pressedEvent = false;
  bool releasedEvent = false;
  unsigned long lastDebounceTime = 0;
};


#endif