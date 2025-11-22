#include "touch.h"
#include "vox.h"
#include "varSpeedLooper.h"

#include "DaisyDuino.h"

////////////////////////////////////////////////////////////
/////////////////////// CONSTANTS //////////////////////////

static constexpr float kFrac = 1.f / 1023.f;
bool disableAfterTouch = true;

////////////////////////////////////////////////////////////
///////////////////// KNOBS & SWITCHES /////////////////////

#define JOYSTICK_X A9  // Looper Time
#define JOYSTICK_Y A10  // Filter Time (Overdrive?)

#define SLIDER_1 A4 // Attack
#define SLIDER_2 A6 // Decay
#define SLIDER_3 A7 // Sustain
#define SLIDER_4 A5 // Release

#define KNOB_1 A0 // LFO Rate
#define KNOB_2 A1 // LFO RATE
#define KNOB_3 A2 // Reverb Time / Amount
#define KNOB_4 A3 // Reverb Mix OR Overall Lowpass/Highpass Filter
#define KNOB_5 A11 // Reverb Mix OR Overall Lowpass/Highpass Filter

#define SWITCH_1_DOWN    D9  // VCO Shape
#define SWITCH_1_UP  D8  // VCO Shape

#define SWITCH_2_UP    D7  // fILTER ENV

#define SWITCH_3_UP   D5  // LFO Effect


#define LOOPER_LED  D23  // LED for Looper 


////////////////////////////////////////////////////////////
//////////////////////// MODULES ///////////////////////////

static simpletouch::Touch touch;
static Vox vox;

////////////////////////////////////////////////////////////
//////////////////////// EFFECTS ///////////////////////////

int currentOctave = 0; // range of -3 to 3, 0 is middle c octave
Oscillator Lfo;
float Lfo_out;
int LFO_effect;
float vVolume = 0.5;

// Reverb
ReverbSc        verb;

// Looper
unsigned long looperStartTime;
bool looper_cleared;
float speed_inputA;
float currentSpeedA;

// Looper Parameters
#define MAX_SIZE (48000 * 60 * 1) // 1 minute of floats at 48 khz
float DSY_SDRAM_BSS buf[MAX_SIZE];
varSpeedLooper  looper;
Oscillator      led_osc; // For pulsing the led when recording
float           ledBrightness;



////////////////////////////////////////////////////////////
///////////////////// CALLBACKS ////////////////////////////

void looperPadTouched() {
   looperStartTime = millis();

   // Looper footswitch pressed (start/stop recording)
   looper.TrigRecord();
   if (!looper.Recording()) {  // Turn on LED if not recording and in playback
     ledBrightness = 1.0;
   }

   looper_cleared = false;

}

// Pad Released callback currently not used
void looperPadReleased() {

}

void OnPadTouch(uint16_t pad) {

  // Handle non-note touch (looper, up/down octave)
  if (pad == 0) {
    looperPadTouched();

  } else if (pad == 1) { // If octave down
    currentOctave -= 1;
    if (currentOctave < -3) {
      currentOctave = -3;
    }
    vox.ReleaseAllNotes(); // Work around to prevent notes from hanging, would be better to transfer note pitch though

  } else if (pad == 2) { // If octave up
    currentOctave += 1;
    if (currentOctave > 3) {
      currentOctave = 3;
    }
    vox.ReleaseAllNotes(); // Work around to prevent notes from hanging, would be better to transfer note pitch though

  // Notes
  } else {
  
    auto note_num = pad; // pad for notes is 3 to 23 (3 is E3, Middle C is 11, C5 is 23)

    // convert pad to a midi note, then convert midi note to frequency
    float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

    // Apply current octave
    midi_note = midi_note + currentOctave * 12;
    

    vox.NoteOn(midi_note, 5.0f); // Setting amp to 0.5 to turn note on // TODO Fix amp level, gets divided by 127
  }

}
void OnPadRelease(uint16_t pad) {

  // Handle non-note touch (looper, up/down octave)
  if (pad == 0) {
    //looperPadReleased();

  } else if (pad == 1 || pad == 2) { // If octave up or down 

  // Notes
  } else {

    auto note_num = pad; // pad for notes is 3 to 23 (3 is E3, Middle C is 11, C5 is 23)

    // convert pad to a midi note, then convert midi note to frequency
    float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

    // Apply current octave
    midi_note = midi_note + currentOctave * 12;

    vox.NoteOff(midi_note, 0.0f); // Setting amp to 0 to turn note off (0.0 freq is just placeholder, not used)
  }

}

void OnPadAftertouch(uint16_t pad, float aftertouch) {

    auto note_num = pad; // pad is 0 to 23

    if (pad == 0) {  // If the looper pad is being held
        unsigned long interval = millis() - looperStartTime; // Determine length of touch on looper pad

        // If held long than 1 second, clear loop ( but only once )
        if (interval > 1000 && !looper_cleared) {
            looper.Clear();
            looper_cleared = true;
            ledBrightness = 0.0;
        }
    }

    // Keys Aftertouch
    if (pad > 2 && disableAfterTouch == false) {

        // convert pad to a midi note, then convert midi note to frequency
        float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

        // Apply current octave
        midi_note = midi_note + currentOctave * 12;

        vox.Aftertouch(midi_note, aftertouch); // Setting amp to 0 to turn note off (0.0 freq is just placeholder, not used)
    }
}


///////////////////////////////////////////////////////////////
///////////////////// AUDIO CALLBACK //////////////////////////

float bus[2];
void AudioCallback(float **in, float **out, size_t size) {


  float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs 
  if (LFO_effect == 1) {  // Cutoff Freq
      vox.SetCutoff(Lfo_out * Lfo_out * 10000.0 + 600.0); // frequency from 600 to 8600 exponential);  //TODO Control the 8000 cutoff form the filter knob here
  } else if (LFO_effect == 2) {  // PWM TODO test
      vox.SetSquarePWM(Lfo_out);
  }

  for (size_t i = 0; i < size; i++) {

    // Update LFO
    Lfo_out = Lfo.Process();


    // Handle Looper LED
    if (looper.Recording()) {  // Only update the led oscillations if currently recording
        ledBrightness = led_osc.Process() * 0.8 + 0.6;
    }


    if (LFO_effect == 0) { // Volume LFO
        bus[0] = bus[1] = vox.Process() * 2.0 * (1.0 - Lfo_out);  
    } else {
        bus[0] = bus[1] = vox.Process() * 2.0;  // *2 for volume adjustment
    }

    // Smooth out Looper transitions
    fonepole(currentSpeedA, speed_inputA, .00006f); 

    if (currentSpeedA < 0.0) {
      looper.SetReverse(true);
    } else {
      looper.SetReverse(false);
    }
    float speed_inputAabs = abs(currentSpeedA);
    looper.SetIncrementSize(speed_inputAabs);

    float loop_out = looper.Process(bus[0]); // TODO: Mono looper for now, is there a need for stereo?

    sendl = sendr = bus[0] + loop_out;

    verb.Process(sendl, sendr, &wetl, &wetr); // Note:Reverb added after looper

    out[0][i] = (bus[0] + loop_out + wetl) * 2.0 * vVolume;
    out[1][i] = (bus[0] + loop_out + wetr) * 2.0 * vVolume;
  }
}

///////////////////////////////////////////////////////////////
////////////////////////// SETUP //////////////////////////////

void setup() {  
  
  //System::Config syscfg; // Not sure if this does anything here
  //syscfg.Boost();

  DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  //DAISY.init(DAISY_SEED, AUDIO_SR_16K); // Use 16kHz samplerate to eliminate processing issues, will introduce aliasing noise though

  DAISY.SetAudioBlockSize(48);


  float sample_rate = DAISY.AudioSampleRate();
  float buffer_size = DAISY.AudioBlockSize();
  int loopTimer = 0;
  
  touch.Init();
  touch.SetOnTouch(OnPadTouch);
  touch.SetOnRelease(OnPadRelease);
  touch.SetOnAftertouch(OnPadAftertouch);
  
  vox.Init(sample_rate);

  verb.SetFeedback(0.0);
  verb.SetLpFreq(10000.0);

  ////// Looper //////////////////
  looper.Init(buf, MAX_SIZE);
  looper.SetMode(varSpeedLooper::Mode::NORMAL);
  speed_inputA = currentSpeedA = 1.0;
  looper_cleared = true;

  led_osc.Init(sample_rate);
  led_osc.SetFreq(0.5);
  led_osc.SetWaveform(1);
  ledBrightness = 0.0;

  // LFO ////////////////////////
  Lfo.Init(sample_rate);
  Lfo.SetFreq(0.5);
  Lfo.SetAmp(1.0);
  Lfo.SetWaveform(0); // Sine wave
  Lfo_out = 1.0;
  LFO_effect = 1;
  ///////////////////////////////

  // Toggle Switches //
  pinMode(SWITCH_1_UP, INPUT_PULLUP);
  pinMode(SWITCH_1_DOWN, INPUT_PULLUP);
  pinMode(SWITCH_2_UP, INPUT_PULLUP);
  //pinMode(SWITCH_2_DOWN, INPUT_PULLUP);
  pinMode(SWITCH_3_UP, INPUT_PULLUP);
  //pinMode(SWITCH_3_DOWN, INPUT_PULLUP);
  //pinMode(SWITCH_4_UP, INPUT_PULLUP);
  //pinMode(SWITCH_4_DOWN, INPUT_PULLUP);

  pinMode(LOOPER_LED, OUTPUT);

  verb.Init(sample_rate);
  DAISY.begin(AudioCallback);
}

///////////////////////////////////////////////////////////////
////////////////////////// LOOP ///////////////////////////////

void loop() {

  touch.Process();

  // Process Controls

  // REVERB CONTROLS
  
  float vReverbTime = static_cast<float>(analogRead(KNOB_3)) * kFrac;
  verb.SetFeedback(vReverbTime);

  // Joystick
  float vPitchMod = static_cast<float>(analogRead(JOYSTICK_Y)) * kFrac;  // TODO Add smoothing to pitchmod and move to audio callback

  vPitchMod = vPitchMod * 2.0 - 1.0; // map value from -1 to +1
  if (vPitchMod <= -0.1) {
      vox.SetPitchMod(vPitchMod * 1.111 + 0.1); // TODO Verify math is correct here
  } else if (vPitchMod >= 0.1) {
      vox.SetPitchMod(vPitchMod * 1.111 - 0.1);
  } else {  // If knob is centered (within 10% range) lock at 0 pitch mod
      vox.SetPitchMod(0.0);
  }


  // Looper CONTROLS
  float vLoopSpeed = static_cast<float>(analogRead(JOYSTICK_X)) * kFrac;
  if (vLoopSpeed <= 0.45 && !looper.Recording()) {
    speed_inputA = vLoopSpeed * 6.6667 - 2.0; // maps 0 to 0.45 control to -2x to 1x speed

  } else if (vLoopSpeed >= 0.55 && !looper.Recording()) {
    speed_inputA = 1.0 + 2.222 * (vLoopSpeed - 0.55) ;  // maps 0.55 to 1.0 control to 1x to 2x speed

  } else {  // If knob is centered (within 10% range) lock at 1x speed or looper is recording
    speed_inputA = 1.0;
  }


  // Voice Controls ///////////////////////////////////////////////

  // VCO Shape
  auto switch1_up = static_cast<uint32_t>(digitalRead(SWITCH_1_UP));
  auto switch1_down = static_cast<uint32_t>(digitalRead(SWITCH_1_DOWN));
  if (switch1_up == LOW) {
    vox.SetVCOShape(0);
  } else if (switch1_down == LOW) {
    vox.SetVCOShape(2);
  } else {
    vox.SetVCOShape(1);
  }

  // AfterTouch Effect
  auto switch2_up = static_cast<uint32_t>(digitalRead(SWITCH_2_UP));
  //auto switch2_down = static_cast<uint32_t>(digitalRead(SWITCH_2_DOWN));
  bool env_filter_on = false;
  if (switch2_up == LOW) {
    //vox.SetTouch(0);
    vox.FilterEnv(false);

  } else {
    //vox.SetTouch(1);
    vox.FilterEnv(true);
    env_filter_on = true;
  }

  // ADSR ///
  float vAttack = static_cast<float>(analogRead(SLIDER_1)) * kFrac;
  vox.SetAttack(vAttack * vAttack + 0.001);  // Adding .001 so it can't be 0, makes clicky sound when 0

  float vDecay = static_cast<float>(analogRead(SLIDER_2)) * kFrac;
  vox.SetDecay(vDecay * vDecay + 0.001);

  float vSustain = static_cast<float>(analogRead(SLIDER_3)) * kFrac;
  vox.SetSustain(vSustain + 0.001);

  float vRelease = static_cast<float>(analogRead(SLIDER_4)) * kFrac;
  float release_fraction = 0.25;
  vox.SetRelease(vRelease * vRelease *0.25 + 0.001);

  // LFO //

  float vLfoRate = static_cast<float>(analogRead(KNOB_2)) * kFrac;
  float calculated_LFO_rate = vLfoRate * vLfoRate * 10.0 + 0.1; // frequency of LFO, exponential from 0.1 to  10
  Lfo.SetFreq(calculated_LFO_rate);
  float vLfoDepth = static_cast<float>(analogRead(KNOB_1)) * kFrac;
  Lfo.SetAmp(vLfoDepth); 

  auto switch3_up = static_cast<uint32_t>(digitalRead(SWITCH_3_UP));
  if (switch3_up == LOW) {
    LFO_effect = 1;

  } else {
    LFO_effect = 0;
  }


  float vFilter = static_cast<float>(analogRead(KNOB_4)) * kFrac;
  if (LFO_effect == 1 || env_filter_on) { // if Filter LFO selected or Filter envelope selected, use filter knob as the max range of the filter
      vox.SetCutoffRange(vFilter * vFilter * 10000.0 + 600.0); // frequency from 600 to 8600 exponential); 
  } else {
      vox.SetCutoffRange(12000.0); // Reset to default range
      vox.SetCutoff(vFilter * vFilter * 10000.0 + 600.0); // frequency from 600 to 8600 exponential);  // TODO See how this interacts/overrides with other filter controls like Touch and LFO
  }

  vVolume = static_cast<float>(analogRead(KNOB_5)) * kFrac;
  vVolume = vVolume * vVolume; // Exponential control


  // Looper LED Brightness
  int ledBrightness_int = static_cast<int>(ledBrightness * 125.0 + 130.0);
  analogWrite(LOOPER_LED, ledBrightness_int); // ledBrightness_int needs to be 0 to 255

  //delay(4); // removed delay
}