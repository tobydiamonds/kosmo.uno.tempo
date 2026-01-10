#ifndef AnalogInput_h
#define AnalogInput_h

#define ANALOG_INPUT_SCAN_INTERVAL 25
#define ANALOG_INPUT_SENSITIVITY 5
#define ANALOG_INPUT_CATCHUP_ZONE 10

class AnalogInput {
private:
  uint8_t _pin;
  int _value;
  int _oldValue;
  unsigned long _lastScan;
  bool _catching;

  void Read(unsigned long now) {
    _lastScan = now;
    int newValue = analogRead(_pin);

    // if(_catching && abs(newValue - _oldValue) < ANALOG_INPUT_CATCHUP_ZONE) {
    //   _catching = false;
    // }

    // if(!_catching && abs(newValue - _oldValue) > ANALOG_INPUT_SENSITIVITY) {
    //   _oldValue = _value;
    //   _value = newValue;
    // }

    if (_catching) {
      // Exit catch-up once we're near the reference (_oldValue)
      if (abs(newValue - _oldValue) <= ANALOG_INPUT_CATCHUP_ZONE) {
        _catching = false;
      }
      _value = newValue; // still track where the knob actually is
      return;
    }

    // Normal mode: accept only meaningful movement relative to current value
    if (abs(newValue - _value) > ANALOG_INPUT_SENSITIVITY) {
      _value = newValue;
    }    
  }
  
public:
  // AnalogInput(uint8_t pin) {
  //   _pin = pin;
  //   //pinMode(_pin, INPUT);
  //   _value = 0;
  //   _oldValue = 0;
  //   _lastScan = 0;
  //   _catching = true;
  // }
  explicit AnalogInput(uint8_t pin)  : _pin(pin), _value(0), _oldValue(0), _lastScan(0), _catching(true) {}

  void Begin(int init = -1) {
    if(init == -1)
      init = analogRead(_pin);
    _value = _oldValue = init;
    _lastScan = millis();
    _catching = false; // start in normal mode
  }  

  // bool Changed(unsigned long now, uint16_t& value) {
  //   value = _value;
  //   if(now > (_lastScan + ANALOG_INPUT_SCAN_INTERVAL)) {
  //     Read(now);
  //     return !_catching && abs(_value - _oldValue) > ANALOG_INPUT_SENSITIVITY;
  //   } else {
  //     return false;
  //   }
  // }

  void SetReferenceValue(int value) {
    _oldValue = value;
  }

  void StartCatchUp() {
    _catching = true;
  }

  bool Changed(unsigned long now, uint16_t& valueOut) {
    if ((now - _lastScan) >= ANALOG_INPUT_SCAN_INTERVAL) {  // rollover-safe
      Read(now);
      bool changed = (!_catching && (abs(_value - _oldValue) > ANALOG_INPUT_SENSITIVITY));
      if (changed) {
        _oldValue = _value;           // consume the change (report once)
      }
      valueOut = (uint16_t)_value;    // return the latest reading
      return changed;
    }
    // Optionally keep the out param fresh even if we didn't sample this call
    valueOut = (uint16_t)_value;
    return false;
  }  
};


#endif