#pragma once

#include "DaisyDSP.h"


class Voice {
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate) {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);
        filt_.Init(samplerate);
        filt_.SetFreq(6000.f);
        filt_.SetRes(0.6f);
        //filt_.SetDrive(0.8f);
        touch_mode_ = 0;
        osc_shape_ = 0;
        cutoff_range_ = 8000.0;
    }

    float Process() {
        if (active_) {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if (!env_.IsRunning())
                active_ = false;
            sig = osc_.Process();

            if (filter_envelope_ == true) {
                float cutoff_frequency = amp * amp * cutoff_range_ + 600.0; // frequency from 600 to 8600 exponential //TODO Control the 8000 cutoff form the filter knob here
                SetCutoff(cutoff_frequency);
            }

            return filt_.Process(sig) * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity) {
        note_ = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
        active_ = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void OnNoteAftertouch(float aftertouch) {  //TODO Need a way to reset default after switching aftertouch mode
        if (touch_mode_ == 0) {  // Volume
            osc_.SetAmp(aftertouch);
        } else if (touch_mode_ == 1) { // Cutoff Freq
            float cutoff_frequency = aftertouch * aftertouch * cutoff_range_ + 600.0; // frequency from 600 to 10600 exponential
            SetCutoff(cutoff_frequency);
        } else if (touch_mode_ == 2) { // PWM if saw wave, else, cutoff freq
            if (osc_shape_ == 2) { // if square wave
                //osc_.SetPw(aftertouch); // NOTE: The Oscillator class files in DaisyDuino are not up to date. Manually added and modified library with updated DaisySP Oscillator class to work (also had to update dsp.h)
            } else {
                float cutoff_frequency = aftertouch * aftertouch * cutoff_range_ + 600.0; // frequency from 600 to 10600 exponential
                SetCutoff(cutoff_frequency);
            }
        }

    }

    void SetVCOShape(int shape) {
        osc_shape_ = shape;
        if (shape == 0) {
            osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        } else if (shape == 1) {
            osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
        } else { 
            osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);
        }
    }

    void SetTouch(int val) { touch_mode_ = val; }

    void SetAttack(float val) { env_.SetTime(ADSR_SEG_ATTACK, val); }

    void SetDecay(float val) { env_.SetTime(ADSR_SEG_DECAY, val); }

    void SetSustain(float val) { env_.SetSustainLevel(val); }

    void SetRelease(float val) { env_.SetTime(ADSR_SEG_RELEASE, val); }

    void SetCutoff(float val) { filt_.SetFreq(val); }

    void SetCutoffRange(float val) { cutoff_range_ = val; } 

    void FilterEnv(bool val) {filter_envelope_ = val; }

    void SetSquarePWM(float val) {} //osc_.SetPw(val); }

    void SetPitchMod(float val) { osc_.SetFreq(mtof(note_ + 12.0 * val)); } // Pitch mod +- one octave, expects value of -1 to 1

    /// TODO Add PWM control for square wave

    inline bool IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc_;   // TODO Use the VariableShapeOscillator class instead with a knob control
    //Svf filt_;
    MoogLadder filt_;
    Adsr env_;
    float note_, velocity_;
    bool active_;
    bool env_gate_;
    int touch_mode_;
    int osc_shape_;
    bool filter_envelope_;
    float cutoff_range_;
};


template <size_t max_voices> class VoiceManager {
  public:
    VoiceManager() {}
    ~VoiceManager() {}

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
        Voice *v = FindFreeVoice();
        if (v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity) {
        for (size_t i = 0; i < max_voices; i++) {
            Voice *v = &voices[i];
            if (v->IsActive() && v->GetNote() == notenumber) {
                v->OnNoteOff();
            }
        }
    }

    void OnNoteAftertouch(float note, float aftertouch) {

        for (size_t i = 0; i < max_voices; i++) {
            Voice *v = &voices[i];
            if (v->IsActive() && v->GetNote() == note) {
                v->OnNoteAftertouch(aftertouch);
            }
        }  
    }

    void FreeAllVoices() {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].OnNoteOff();
        }
    }

    void SetVCOShape(int shape) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetVCOShape(shape);
        }
    }

    void SetTouch(int touch) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetTouch(touch);
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


    void SetCutoff(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCutoff(all_val);
        }
    }

    void SetCutoffRange(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetCutoffRange(all_val);
        }
    }

    void FilterEnv(bool all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].FilterEnv(all_val);
        }
    }

    void SetSquarePWM(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetSquarePWM(all_val);
        }
    }

    void SetPitchMod(float all_val) {
        for (size_t i = 0; i < max_voices; i++) {
            voices[i].SetPitchMod(all_val);
        }
    }

    void ReleaseAllNotes() {
        for (size_t i = 0; i < max_voices; i++) {
            Voice *v = &voices[i];
            if (v->IsActive()) {
                v->OnNoteOff();
            }
        }
    }


  private:
    Voice voices[max_voices];
    Voice *FindFreeVoice() {
        Voice *v = NULL;
        for (size_t i = 0; i < max_voices; i++) {
            if (!voices[i].IsActive()) {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};



class Vox {
public:
Vox() {}

~Vox() {}

void Init(float sample_rate) {
  _osc.Init(sample_rate);

}

void NoteOn(float note, float amp) {
  _osc.OnNoteOn(note, amp);

}

void NoteOff(float note, float amp) {
  _osc.OnNoteOff(note, amp);

}

void Aftertouch(float note, float aftertouch) {
  _osc.OnNoteAftertouch(note, aftertouch);

}


void SetVCOShape(int shape) { _osc.SetVCOShape(shape); }

void SetTouch(int touch) { _osc.SetTouch(touch); }

void SetAttack(float val) { _osc.SetAttack(val); }

void SetDecay(float val) { _osc.SetDecay(val); }

void SetSustain(float val) { _osc.SetSustain(val); }

void SetRelease(float val) { _osc.SetRelease(val); }

void SetCutoff(float val) { _osc.SetCutoff(val); }

void SetCutoffRange(float val) { _osc.SetCutoffRange(val); }

void SetSquarePWM(float val) { _osc.SetSquarePWM(val); }

void FilterEnv(bool val) { _osc.FilterEnv(val); }

void SetPitchMod(float val) { _osc.SetPitchMod(val); }


void ReleaseAllNotes() { _osc.ReleaseAllNotes(); }


float Process() { 
    return _osc.Process();
}

private:

  VoiceManager<8> _osc;  // Play around with the number of polyphony, using 8 introducing cause processing issues when pressing keys too fast
};
