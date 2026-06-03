# Real-Time FFT Audio Analyzer

This project is a real-time audio analyzer for an MSPM0G3507 LaunchPad with an
attached audio BoosterPack.

Audio is captured from the BoosterPack microphone, sampled by the LaunchPad ADC,
and sent back out through the BoosterPack speaker using the LaunchPad DAC. The
current firmware provides live microphone monitoring with reduced output gain to
limit feedback.

Future work will process the sampled audio with an FFT and display the audio
spectrum on an LED output.

## Hardware

- TI MSPM0G3507 LaunchPad
- TI audio BoosterPack
- BoosterPack microphone input
- BoosterPack speaker output

## Current Features

- 40 kHz ADC sampling driven by a hardware timer
- ADC interrupt handling with ping-pong sample buffers
- Live mic-to-speaker audio passthrough
- Output gain scaling to reduce acoustic feedback

## Planned Features

- FFT processing of captured audio buffers
- Real-time audio spectrum display on LEDs

## Build

Compile, load, and run the project from Code Composer Studio.
