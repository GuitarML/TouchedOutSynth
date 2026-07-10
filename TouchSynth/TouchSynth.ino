#include "touch.h"
//#include "vox.h"
#include "varSpeedLooper.h"

#include "DaisyDuino.h"
#include "DaisyDSP.h"
//#include "daisysp.h"
using namespace daisysp;

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
//static Vox vox;

////////////////////////////////////////////////////////////
//////////////////////// EFFECTS ///////////////////////////

int currentOctave = 0; // range of -3 to 3, 0 is middle c octave
bool first_start = true;
int c = 0;
float vVolume = 0.5;

// Reverb
ReverbSc        verb;
float ambient_mix = 0.0;

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

// Filter
Svf filterLPHP;       // Low Pass and high pass
float vfilter;

// Sampler /////////////////// 
#define MAX_SAMPLE static_cast<int>(48000.0 * 10.0) // 10 second sample
#define MAX_SAMPLE_SIZET static_cast<size_t>(MAX_SAMPLE) 
float DSY_SDRAM_BSS audioSample[3][MAX_SAMPLE_SIZET];  // three sample banks selected with right toggle

bool recording = false;

int current_sample_size[3]{};
int sample_mode = 0;
int current_sample_bank = 0;

int recording_sample_index = 0;
bool trigger;

float middleC = 261.6256;

int trim1_index = 0;
int trim2_index = 0;

float ptrim1, ptrim2;

bool samplerModeOn = false;



///////////////////////////////////////////////////////// Delay

float global_delay_level = 0.0;
// Delay
#define MAX_DELAY static_cast<size_t>(24000) // 2 second max delay
//DelayLine<float, MAX_DELAY> delayLine;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;

struct delayStruct
{
    DelayLine<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    float                        volume = 1.0;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        if (active) {
            del->Write((feedback * read) + in);
        } else {
            del->Write((feedback * read)); // if not active, don't write any new sound to buffer
        }

        return read * volume;
    }
};


delayStruct  delay1;


/////////////////////////////////////////////////////////////  SAMPLER VOICE

class SamplerVoice {
  public:
    SamplerVoice() {}
    ~SamplerVoice() {}
    void Init(float samplerate) {
        active_ = false;

        env_.Init(samplerate);
        env_.SetSustainLevel(1.0f);
        env_.SetAttackTime(0.05);
        env_.SetDecayTime(0.05);
        env_.SetReleaseTime(0.2); 

        playhead = static_cast<float>(trim1_index);  // set initial playhead to trim1 location
        play_speed_ = 1.0;
        is_first_sample = true;
        pitch_mod_ = 0.0;

    }

    // Process a single sample
    float getNextSample() {
        if (current_sample_size[current_sample_bank] == 0) return 0.0f;

        if (is_first_sample) {
            is_first_sample = false;
            playhead = static_cast<float>(trim1_index);
        }

        bool isReverse = false;
        if (trim1_index > trim2_index) {  // User could trigger reverse mid sample, so put here and not in initialization (TODO see if this works as expected)
            isReverse = true;
        }

        // If we get to the end of the sample, reset
        if (!isReverse && playhead >= static_cast<float>(trim2_index)) {
            playhead = static_cast<float>(trim1_index);
            if (sample_mode == 0 ) {
                active_ = false; // for now, end sample playback at end of sample in sample_mode= 0 (active remains true and repeats sample in other modes)
            } 
        }

        if (isReverse && playhead <= static_cast<float>(trim2_index)) {
            playhead = static_cast<float>(trim1_index);
            if (sample_mode == 0 ) {
                active_ = false; // for now, end sample playback at end of sample in sample_mode= 0 (active remains true and repeats sample in other modes)
            }
        }

        // Separate integer and fractional parts
        size_t index = static_cast<size_t>(playhead);
        float fraction = playhead - index;

        if (isReverse && index < 1) {
            return 0.0;
        }

        // Get current and next samples (handle boundary protection)
        float sample1 = audioSample[current_sample_bank][index];

        float sample2;
        if (!isReverse) {
            sample2 = (index + 1 < static_cast<float>(trim2_index)) ? audioSample[current_sample_bank][index + 1] : 0.0f;
        } else {
            sample2 = (index - 1 > static_cast<float>(trim2_index)) ? audioSample[current_sample_bank][index - 1] : 0.0f;
        }

        // Linear Interpolation
        float output = sample1 + static_cast<float>(fraction) * (sample2 - sample1);

        // Advance the playhead
        if (!isReverse) {
            playhead += play_speed_ * pitch_mod_; // TODO (To try) pitch_mod could be a convenient place to apply tape (or other) modulation
        } else {
            playhead -= play_speed_ * pitch_mod_;
        }

        // Short fade at end of sample to prevent clicks
        //   TODO: Maybe use this section for crossfading repeating samples (sample_mode = 1)
        float auto_fade = 1.0;
        if (!isReverse) {   // If forward 
            if (trim2_index - index < 240) { // 240 samples is 5 milliseconds at base pitch
                auto_fade = (trim2_index - index) / 240;
            }
        } else {            // If reverse
            if (index - trim1_index < 240) { // 240 samples is 5 milliseconds at base pitch
                auto_fade = (index - trim1_index) / 240;
            }
        }

        return output * auto_fade;
    }

    float Process() {
        if (active_) {
            float amp;
            amp = env_.Process(env_gate_);
            if (!env_.IsRunning()) {
                active_ = false;
                playhead = 0.0;
            } 

            return getNextSample() * amp;
            //return getNextSample() * (velocity_ / 127.f) * amp ;
            
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity) {
        note_ = note;
        velocity_ = velocity;

        active_ = true;
        env_gate_ = true;
        play_speed_ = mtof(note_) / middleC;
        is_first_sample = true;

    }

    void OnNoteOff() { env_gate_ = false; }

    void SetAttack(float val) { env_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetDecay(float val) { env_.SetTime(ADSR_SEG_DECAY, val); }

    void SetSustain(float val) { env_.SetSustainLevel(val); }

    void SetRelease(float val) { env_.SetTime(ADSR_SEG_RELEASE, val); }

    void SetPitchMod(float val) { pitch_mod_ = val; }

    inline bool IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:

    Adsr env_;
    float note_, velocity_;
    bool active_;
    bool env_gate_;
    float play_speed_;
    float playhead;
    bool is_first_sample;
    float pitch_mod_;
    
};

template <size_t max_voices> class SamplerVoiceManager {
  public:
    SamplerVoiceManager() {}
    ~SamplerVoiceManager() {}

    void Init(float samplerate) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].Init(samplerate);
        }
    }

    float Process() {
        float sum;
        sum = 0.f;
        for (size_t i = 0; i < max_voices; i++) {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity) {
        SamplerVoice *v = FindFreeVoice();
        if (v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity) {
        for (size_t i = 0; i < max_voices; i++) {
            SamplerVoice *v = &voices[i];
            if (v->IsActive() && v->GetNote() == notenumber) {
                v->OnNoteOff();
            }
        }
    }

    void SetAttack(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].SetAttack(all_val);
        }
    }

    void SetDecay(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].SetDecay(all_val);
        }
    }

    void SetSustain(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].SetSustain(all_val);
        }
    }

    void SetRelease(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].SetRelease(all_val);
        }
    }

    void SetPitchMod(float all_val) {
        for(size_t i = 0; i < max_voices; i++) {
            voices[i].SetPitchMod(all_val);
        }
    }


    void FreeAllVoices() {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].OnNoteOff();
        }
    }



  private:
    SamplerVoice voices[max_voices];
    SamplerVoice *FindFreeVoice() {
        SamplerVoice *v = NULL;
        for (size_t i = 0; i < max_voices; i++) {
            if (!voices[i].IsActive()) {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};


// Sampler
//static SamplerVoiceManager<12> sampler_voice_handler;
SamplerVoiceManager<12> sampler_voice_handler;


////////////////////////////////////////////////////////////
///////////////////// CALLBACKS ////////////////////////////

bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.003;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}

void looperPadTouched() {

   if (samplerModeOn) {
        recording = true;
        ledBrightness = 1.0;

   } else {
       looperStartTime = millis();

       // Looper footswitch pressed (start/stop recording)
       looper.TrigRecord();
       if (!looper.Recording()) {  // Turn on LED if not recording and in playback
         ledBrightness = 1.0;
       }

       looper_cleared = false;
   }

}

// Pad Released stop sample recording if in sampler mode
void looperPadReleased() {
   if (samplerModeOn) {
       recording = false;
       recording_sample_index = 0;
       ledBrightness = 0.0;
   }
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
    //vox.ReleaseAllNotes(); // Work around to prevent notes from hanging, would be better to transfer note pitch though

  } else if (pad == 2) { // If octave up
    currentOctave += 1;
    if (currentOctave > 3) {
      currentOctave = 3;
    }
    //vox.ReleaseAllNotes(); // Work around to prevent notes from hanging, would be better to transfer note pitch though
    // TODO Need to add a function like this to sampler?

  // Notes
  } else {
  
    auto note_num = pad; // pad for notes is 3 to 23 (3 is E3, Middle C is 11, C5 is 23)

    // convert pad to a midi note, then convert midi note to frequency
    float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

    // Apply current octave
    midi_note = midi_note + currentOctave * 12;
    
    sampler_voice_handler.OnNoteOn(midi_note, 127.0);

  }

}
void OnPadRelease(uint16_t pad) {

  // Handle non-note touch (looper, up/down octave)
  if (pad == 0) {
    looperPadReleased();

  } else if (pad == 1 || pad == 2) { // If octave up or down 

  // Notes
  } else {

    auto note_num = pad; // pad for notes is 3 to 23 (3 is E3, Middle C is 11, C5 is 23)

    // convert pad to a midi note, then convert midi note to frequency
    float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

    // Apply current octave
    midi_note = midi_note + currentOctave * 12;

    sampler_voice_handler.OnNoteOff(midi_note, 0.0);

  }

}

void OnPadAftertouch(uint16_t pad, float aftertouch) {

    auto note_num = pad; // pad is 0 to 23

    if (!samplerModeOn) {
        if (pad == 0) {  // If the looper pad is being held
            unsigned long interval = millis() - looperStartTime; // Determine length of touch on looper pad

            // If held long than 1 second, clear loop ( but only once )
            if (interval > 1000 && !looper_cleared) {
                looper.Clear();
                looper_cleared = true;
                ledBrightness = 0.0;
            }
        }
    }

    // Keys Aftertouch
    /*
    if (pad > 2 && disableAfterTouch == false) {

        // convert pad to a midi note, then convert midi note to frequency
        float midi_note = pad + 49; // start at E3 (accounting for the fact pad key number starts at 3)

        // Apply current octave
        midi_note = midi_note + currentOctave * 12;

        vox.Aftertouch(midi_note, aftertouch); // Setting amp to 0 to turn note off (0.0 freq is just placeholder, not used)
    }
    */
}


///////////////////////////////////////////////////////////////
///////////////////// AUDIO CALLBACK //////////////////////////

float bus[2];
void AudioCallback(float **in, float **out, size_t size) {


  float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs 

  /*
  if (c < 50) {
    c+=1;
  } else {
    first_start = false;
  }
  */

  for (size_t i = 0; i < size; i++) {


    float input = in[0][i];

    // Record input audio while footswitch is held, or until max buffer is reached
    if (!samplerModeOn) {
       recording = false;
       recording_sample_index = 0;
    }
    if (recording && samplerModeOn) {
        audioSample[current_sample_bank][recording_sample_index] = input;
        current_sample_size[current_sample_bank] = recording_sample_index;
        recording_sample_index++;
                
        if (recording_sample_index >= MAX_SAMPLE) {
            recording = false;
            recording_sample_index = 0;
            current_sample_size[current_sample_bank] = MAX_SAMPLE;

        }

    }

    // Handle Looper LED
    if (looper.Recording()) {  // Only update the led oscillations if currently recording
        ledBrightness = led_osc.Process() * 0.8 + 0.6;
    }

    float sampler_output = sampler_voice_handler.Process() * vVolume * 5.0; //5 is just volume scaling

    // Lowpass Highpass filter //
    float filtered_sum = 0.0;
    filterLPHP.Process(sampler_output);


    if (vfilter <= 0.5) {  // is lowpass
        bus[0] = bus[1] = filterLPHP.Low();
    } else {  // is highpass
        bus[0] = bus[1] = filterLPHP.High();
    }

    // Process Delay (Note: delay and reverb currently processed before looper, to bake effects into loop
    float delay_input = bus[0];
    float delay_out = delay1.Process(delay_input);  

    sendl = sendr = bus[0];

    verb.Process(sendl, sendr, &wetl, &wetr); // Note:Reverb added after looper



   
    // Smooth out Looper transitions
    fonepole(currentSpeedA, speed_inputA, .00006f); 

    if (currentSpeedA < 0.0) {
      looper.SetReverse(true);
    } else {
      looper.SetReverse(false);
    }
    float speed_inputAabs = abs(currentSpeedA);
    looper.SetIncrementSize(speed_inputAabs);

    float loop_out = looper.Process(bus[0] + delay_out + wetl); // TODO: Mono looper for now, is there a need for stereo?



    out[0][i] = (bus[0] * (1.0 - ambient_mix)  + loop_out + ((wetl / 30.0) + delay_out) * ambient_mix);
    out[1][i] = (bus[0] * (1.0 - ambient_mix)  + loop_out + ((wetr / 30.0) + delay_out) * ambient_mix);


  }
}

///////////////////////////////////////////////////////////////
////////////////////////// SETUP //////////////////////////////

void setup() {  

  DAISY.init(DAISY_SEED, AUDIO_SR_48K);

  DAISY.SetAudioBlockSize(48);


  float sample_rate = DAISY.AudioSampleRate();
  float buffer_size = DAISY.AudioBlockSize();
  int loopTimer = 0;

  ptrim1 = 0.1;
  ptrim2 = 0.9;

  sampler_voice_handler.Init(sample_rate);

  for(int i = 0; i < MAX_SAMPLE; i++) {
      audioSample[0][i] = 0.;
      audioSample[1][i] = 0.;
      audioSample[2][i] = 0.;
  }

  delayLine.Init();
  delay1.del = &delayLine;
  delay1.delayTarget = 12000; // in samples
  delay1.feedback = 0.0;
  delay1.active = true;     // Default to no delay

  filterLPHP.Init(sample_rate);
  filterLPHP.SetFreq(20000.0);

  touch.Init();
  touch.SetOnTouch(OnPadTouch);
  touch.SetOnRelease(OnPadRelease);
  touch.SetOnAftertouch(OnPadAftertouch);

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


  // Toggle Switches //
  pinMode(SWITCH_1_UP, INPUT_PULLUP);
  pinMode(SWITCH_1_DOWN, INPUT_PULLUP);
  pinMode(SWITCH_2_UP, INPUT_PULLUP);
  pinMode(SWITCH_3_UP, INPUT_PULLUP);

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

  ambient_mix = vReverbTime;
  if (vReverbTime < 0.05) {
      delay1.volume = 0.0;
      delay1.feedback = 0.0;
  } else if (vReverbTime < 0.55) {
      delay1.volume = (vReverbTime - 0.05) * 1.6;
      delay1.feedback = (vReverbTime - 0.05) * 1.2;
  } else {
      delay1.volume = 0.8;
      delay1.feedback = 0.6;
  }



  // Joystick
  float vPitchMod = static_cast<float>(analogRead(JOYSTICK_Y)) * kFrac; 

  //vPitchMod = vPitchMod * 2.0; // map value from 0.5 to +1
  if (vPitchMod <= 0.45) {
      sampler_voice_handler.SetPitchMod(vPitchMod * 1.1111 + 0.5); // map 0.5 to 1
  } else if (vPitchMod >= 0.55) {
      sampler_voice_handler.SetPitchMod((vPitchMod - 0.55) * 2.2222   + 1.0); // map 1 to 2
  } else {  // If knob is centered (within 10% range) lock at no pitch mod
      sampler_voice_handler.SetPitchMod(1.0);
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

    current_sample_bank = 0;
  
  } else if (switch1_down == LOW) {
    current_sample_bank = 2;

  } else {
    current_sample_bank = 1;

  }

  // Sampler Mode On/Off
  auto switch2_up = static_cast<uint32_t>(digitalRead(SWITCH_2_UP));

  if (switch2_up == LOW) {
    samplerModeOn = true;

  } else {
    samplerModeOn = false;
  }

  // ADSR ///
  float vAttack = static_cast<float>(analogRead(SLIDER_1)) * kFrac;
  //float temp_attack = 0.4;
  sampler_voice_handler.SetAttack(vAttack); 

  float vDecay = static_cast<float>(analogRead(SLIDER_2)) * kFrac;
  //float temp_decay = 0.04;
  sampler_voice_handler.SetDecay(vDecay); 


  float vSustain = static_cast<float>(analogRead(SLIDER_3)) * kFrac;
  sampler_voice_handler.SetSustain(vSustain + 0.001); 


  float vRelease = static_cast<float>(analogRead(SLIDER_4)) * kFrac;
  //float temp_release = 0.4;
  sampler_voice_handler.SetRelease(vRelease); 


  // Sample trim controls
  float vtrim2 = static_cast<float>(analogRead(KNOB_2)) * kFrac;
  float vtrim1 = static_cast<float>(analogRead(KNOB_1)) * kFrac;

  float current_sample_size_float = static_cast<float>(current_sample_size[current_sample_bank]);


  if (knobMoved(ptrim1, vtrim1) || first_start) {
      ptrim1 = vtrim1;

  }
  trim1_index = static_cast<int>(ptrim1 * current_sample_size_float);

  if (knobMoved(ptrim2, vtrim2) || first_start) {
      ptrim2 = vtrim2;
  }
  trim2_index = static_cast<int>(ptrim2 * current_sample_size_float);
  first_start = false;

  auto switch3_up = static_cast<uint32_t>(digitalRead(SWITCH_3_UP));
  if (switch3_up == LOW) {
    sample_mode = 1;
  } else {
    sample_mode = 0;
  }

  vfilter = static_cast<float>(analogRead(KNOB_4)) * kFrac;

  float cutoff_filter;
  if (vfilter <= 0.5) {

      cutoff_filter = vfilter * vfilter * 2 * 18000.0 + 150.0; // exponential range 150 to 18150 as knob goes from 0 to 0.5
      filterLPHP.SetFreq(cutoff_filter);

  } else {
      cutoff_filter = ((vfilter - 0.5) * 2.0) * ((vfilter - 0.5) * 2.0) * 3000.0;
      filterLPHP.SetFreq(cutoff_filter); // exp range 0 to 3000 as knob goes from 0.5 to 1.0

  }
  
  vVolume = static_cast<float>(analogRead(KNOB_5)) * kFrac;

  // Looper LED Brightness
  int ledBrightness_int = static_cast<int>(ledBrightness * 125.0 + 130.0);
  analogWrite(LOOPER_LED, ledBrightness_int); // ledBrightness_int needs to be 0 to 255

  //delay(4); // removed delay
}