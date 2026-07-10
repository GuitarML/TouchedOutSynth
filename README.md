# TouchedOut Sampler

The TouchedOut Polysynth is a polyphonic capacitive touch synth on a PCB, using the Daisy Seed. 
Two Adafruit MPR121 capacitive touch modules are used for keys and buttons. The firmware 
was designed and built using Arduino IDE with DaisyDuino. See Electrosmith's tutorials for
setting up the DaisyDuino environment with Arduino IDE. This repo also includes the
KiCad schematic and pcb files for ordering your own from a PCB manufacturer (JLCPcb gerbers provided).

![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/touchedout.png)


[Video Demos on YouTube](https://youtu.be/tRJoIYXkm-U?si=PkRd3XZ30BAMkGUb)

## Microphone Circuit
![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/microphone_circuit.jpg)

The added microphone circuit for the TouchedOutSampler firmware is shown above. The mic used is from [Adafruit](https://www.adafruit.com/product/1063?srsltid=AfmBOooQ07_40sVdienoSQDztJte4-cMyLQY7wpDJVhW7ct12bVJHef1), MAX4466.
The 100uF capacitor on the audio line couples the audio to AC, as the output from the MAX4466 is DC coupled.
The RC filter using the 100 ohm resistior and 100uF capacitor was required for noise reduction. There 
is alot of noise coming from the Daisy Seed 3v3A power, but this RC filter does a good job of filtering it out. 

Follow this tutorial to set up DaisyDuino in Arduino IDE:
[How to Add Daisy Support to Arduino IDE](https://youtu.be/UyQWK8JFTps?si=kI6pP10nuPIkyu_V)

IMPORTANT!!! Updating Adafruit_MPR121 package to version 1.2.0 breaks functionality due to I2C incompatibilites. Use Adafruit_MPR121 version 1.1.3

Use these Tools settings in your Arduino IDE (pay attention to -O3 optimization and USB support: generic serial supercede USART)

![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/ArduinoIDE_tool_settings.jpg)
