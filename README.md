# TouchedOut PolySynth

![app](https://github.com/GuitarML/TouchedOutSynth/blob/main/images/touchedout.png)


[Video Demos on YouTube](https://youtu.be/tRJoIYXkm-U?si=PkRd3XZ30BAMkGUb)




## Hardware



## Software



### Build the Software
Head to the [Electro-Smith Wiki](https://github.com/electro-smith/DaisyWiki) to learn how to set up the Daisy environment on your computer.

```
# Clone the repository
$ git clone https://github.com/GuitarML/Funbox.git
$ cd Funbox

# initialize and set up submodules
$ git submodule update --init --recursive

# Build the daisy libraries (after installing the Daisy Toolchain):
# Replace the daisy_petal files in libDaisy/src with the files in the "mod" directory to properly map controls on Funbox.
$ make -C libDaisy
$ make -C DaisySP

# Build the desired pedal firmware (Mars pedal shown below as example) (after installing the Daisy Toolchain)
$ cd software/Mars
$ make
```

Then upload the firmware (.bin) to your Funbox pedal with the following commands (or use the [Electrosmith Web Programmer](https://electro-smith.github.io/Programmer/))
```
# This is the procedure for uploading to Flash memory on the Daisy Seed, if using SRAM memory use Bootloader method (Mars and Neptune use SRAM).
cd your_pedal
# using USB (after entering bootloader mode)
make program-dfu
# using JTAG/SWD adaptor (like STLink)
make program
```



