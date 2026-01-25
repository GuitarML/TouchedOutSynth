#pragma once

#include "DaisyDuino.h"
#include "Adafruit_MPR121.h"  // IMPORTANT!!! Updating to version 1.2.0 broke it, I2C incompatibilites? Built, uploaded, but touches were unresponsive. Rolled back to 1.1.3 to fix.
#include <array>


namespace simpletouch {

///////////////////////////////////////////////////////////////
//////////////////////// TOUCH SENSOR /////////////////////////

class Touch {
  public:
    Touch():
      _state { 0 },
      _on_touch { nullptr },
      _on_release { nullptr }
      {}

    void Init() {
      
      if (!_cap.begin(0x5A)) {
        Serial.println("MPR121 0x5A not found, check wiring?");
        while (1) {
          Serial.println("PLEASE CONNECT 0x5A MPR121 TO CONTINUE TESTING");
          delay(200);
        }
      }
      // Make sure to double check address used here
      if (!_cap2.begin(0x5B)) { // This address is when back pad is soldered (ADDR connected to 3v3) 
        Serial.println("MPR121 0x5B not found, check wiring?");
        while (1) {
          Serial.println("PLEASE CONNECT 0x5B MPR121 TO CONTINUE TESTING");
          delay(200);
        }
      }
    }

    // Register note on callback
    void SetOnTouch(void(*on_touch)(uint16_t pad)) {
      _on_touch = on_touch;
    }

    void SetOnRelease(void(*on_release)(uint16_t pad)) {
      _on_release = on_release;
    }

    void SetOnAftertouch(void(*on_aftertouch)(uint16_t pad, float volume_aftertouch)) {
      _on_aftertouch = on_aftertouch;
    }

    bool IsTouched(uint16_t pad) {
      return _state & (1 << pad);
    }

    bool HasTouch() {
      return _state > 0;
    }

    void Process() {
        uint16_t pad;
        bool is_touched;
        bool was_touched;

        // Process first MPR121 

        auto state = _cap.touched();
        for (uint16_t i = 0; i < 12; i++) { // modified to leave out first touchpad

          pad = 1 << i;
          is_touched = state & pad;
          was_touched = _state & pad;
          if (_on_touch != nullptr && is_touched && !was_touched) {
            _on_touch(i);
            Serial.print("Pad Touch Detected: ");
            Serial.println(i);

          }
          else if (_on_release != nullptr && was_touched && !is_touched) {
            _on_release(i);
            Serial.print("Pad Release Detected: ");
            Serial.println(i);
            
          }

          // Get the filtered data for the channel
          uint16_t filtered = _cap.filteredData(i);

          // Get the baseline data for the channel
          uint16_t baseline = _cap.baselineData(i);

          // The 'pressure' is the difference between baseline and filtered data
          // This value will increase as a touch becomes more substantial
          int16_t pressure = baseline - filtered;

          // You can set a threshold to ignore minor fluctuations
          if (pressure > 0) { // Adjust threshold as needed // Reduced threshold from 10 to 5 to try to prevent no sound on touch TODO verify if this helps
            Serial.print("Electrode ");
            Serial.print(i);
            Serial.print(": Pressure = ");
            Serial.println(pressure);

            float aftertouch = pressure / 150.0;  // TODO test sensitivity, capacitence measurements
            _on_aftertouch(i, aftertouch);

          }

        }
        _state = state;



        // Process second MPR121 

        auto state2 = _cap2.touched();
        for (uint16_t i = 0; i < 12; i++) { // modified to leave out first touchpad

          pad = 1 << i;
          is_touched = state2 & pad;
          was_touched = _state2 & pad;
          if (_on_touch != nullptr && is_touched && !was_touched) {
            _on_touch(i + 12);  // Adding 12 because we need to count first MPR121 pads
            Serial.print("Pad Touch Detected (2nd MPR121): ");
            Serial.println(i);

          }
          else if (_on_release != nullptr && was_touched && !is_touched) {
            _on_release(i + 12);  // Adding 12 because we need to count first MPR121 pads
            Serial.print("Pad Release Detected (2nd MPR121): ");
            Serial.println(i);
            
          }

          // Get the filtered data for the channel
          uint16_t filtered = _cap2.filteredData(i);

          // Get the baseline data for the channel
          uint16_t baseline = _cap2.baselineData(i);

          // The 'pressure' is the difference between baseline and filtered data
          // This value will increase as a touch becomes more substantial
          int16_t pressure = baseline - filtered;

          
          // You can set a threshold to ignore minor fluctuations
          if (pressure > 0) { // Adjust threshold as needed // Reduced threshold from 10 to 5 to try to prevent no sound on touch TODO verify if this helps
            Serial.print("Electrode (2nd MPR121) ");
            Serial.print(i);
            Serial.print(": Pressure = ");
            Serial.println(pressure);

            float aftertouch = pressure / 150.0;
            _on_aftertouch(i + 12, aftertouch);  // Adding 12 because we need to count first MPR121 pads

          }

        }
        _state2 = state2;

    }

  private:

    void(*_on_touch)(uint16_t pad);
    void(*_on_release)(uint16_t pad);
    void(*_on_aftertouch)(uint16_t pad, float volume_aftertouch);

    Adafruit_MPR121 _cap;
    Adafruit_MPR121 _cap2;
    uint16_t _state;
    uint16_t _state2;
};




};
