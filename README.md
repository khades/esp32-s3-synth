# ESP32-S3 synth using ESP-IDF

I was trying to make synth with Arduino with ESP32-S3 and after messing with it for some time i came to conclusion.

1) Arduino USBHost feature suck because [Espressif "forgot" to implement that feature in Arduino](https://github.com/espressif/arduino-esp32/issues/10978). There's [external library and it uses base ESP-IDF library for USBMidi host](https://github.com/enudenki/esp32-usb-host-midi-library.git). 
2) When i started to wire midi to some basic oscillator i came to realisation i need to learn FreeRTOS to make sound not crackling and MIDI being as low-latency as possible.
3) ESP32-S3 is multicore board.

So i came to conclusion - I need to use bare FreeRTOS, or in that case - ESP-IDF which is variant of FreeRTOS.

Background - I am JS developer for shit ton of years and have experience in Go. I didn't knew either C or C++ at the start of that project.

## What do i want from synth

- I want Clonewheel organ with leslie, distortion and spring reverb with MIDI configuration. I want parameters to be simple and for now i feel that tuning of stuff should be done in header files, and not through midi. I want ONE good sound. There's enough clonewheel organs out there with sources, even for ESP32.
- Rhodes synth with distortion, tremolo and spring reverb. There's MDA ePiano port, and there's numerous modelling tutorials.
- Clavinet - i have no idea yet how will i model this, i havent found modelling tutorial
- Simple synth alike OBXD or tal noiseMaker

## Steps

### Finding good board for synth - ✔ 

ESP32-S3 has USB-OTG host on board so i can potentially use that for USBMIDI

### Wiring sound in - ✔

I am using I2S bus with PCM5102A. It was making sound with Arduino and now it makes sound with bare ESP-IDF.

### Make USBMIDI work - ✔

Somehow i managed to port https://github.com/enudenki/esp32-usb-host-midi-library.git from C++ to C and wire it in.

### Learning C - ✔

I've read C Primer Plus and i have idea what to C code does now.

### Wiring MIDI to Sound - ✖

I did that with Arduino, mapped midi keyboard event to frequency, copypasted simple oscillator and made primitive square-wave monophonic synth with simpliest volume envelope ever. BUT the sound was interrupted by midi reading procedure, so either i had really long reaction time to key press, or i had sound crackling

### Reading how to use DMA buffers for sound, whats buffers etc - ✖

It seems that's the main idea how that would work, i'll sythesize some sound based on current midi state and time state, write some data ahead, then process midi after that in 1000HZ tick time. 

My vague naive idea that there would be some byte buffer or even two of them that i will fill and somehow pass to DMA I2s async write method.

If that's true - i need to make it abstract enough so i can use any DSP library, so i wont write my own oscillators, mixers, revebs from scratch. 

### Writing document with idea about simple HAL or even struct\data pipeline for realtime synth  - ✖

That should be documented, it is kinda universal for all synth engines. Maybe even someone has tutorial about that.

### Wiring in polyphonic synth with simple volume\filter envelopes that is controllable by midi - ✖

I want to use external DSP libraries for oscillators, mixers and filters first so i dont need to learn DSP at first while doing overall synth layout. I need simple synth working first.

### Learning how DSP works - ✖

When i will do simple good synth, i will move to the more processing\modeling stuff i wanted.

### Serial MIDI - ✖

This is not hard step to make, but i will push it way after synth itself.

### Screen and MIDI learn - ✖

After i do at least one good synth engine, i'll start doing MIDI learn setup using some simple button interface and screen.

### Control Surface  - ✖

I have Novation Launchkey mk3 and i will like to use its screen instead of I2c screen i'd have on. Also Launchkey is prety slick when coming to emulating switches with pads.