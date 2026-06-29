# TouchedOut PolySynth

The TouchedOut Polysynth is a polyphonic capacitive touch synth on a PCB, using the Daisy Seed. 
Two Adafruit MPR121 capacitive touch modules are used for keys and buttons. The firmware 
was designed and built using Arduino IDE with DaisyDuino. See Electrosmith's tutorials for
setting up the DaisyDuino environment with Arduino IDE. This repo also includes the
KiCad schematic and pcb files for ordering your own from a PCB manufacturer (JLCPcb gerbers provided).

![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/touchedout.png)


[Video Demos on YouTube](https://youtu.be/tRJoIYXkm-U?si=PkRd3XZ30BAMkGUb)


Follow this tutorial to set up DaisyDuino in Arduino IDE:
[How to Add Daisy Support to Arduino IDE](https://youtu.be/UyQWK8JFTps?si=kI6pP10nuPIkyu_V)

IMPORTANT!!! Updating Adafruit_MPR121 package to version 1.2.0 breaks functionality due to I2C incompatibilites. Use Adafruit_MPR121 version 1.1.3

Use these Tools settings in your Arduino IDE (pay attention to -O3 optimization and USB support: generic serial supercede USART)

![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/ArduinoIDE_tool_settings.jpg)
