# Didact

A simple notes app

```sh
cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=Debug
```

Icons from: https://fonts.google.com/icons

Goals for today:
[ ] Visualize waveforms instead of FFT output. That way we don't need to remove noise, which means
  that we can update the waveform in real time from the audio callback.

[ ] Simplify the code. Can we merge the spectrum analyzer and the transcriber into one class?
  We pull samples from the audio stream, output to the file (if capture), in the callback.
  Then in the callback function we'll provide, we run RMS and queue the samples.
  In a separate thread we read the samples and then run rnnoise then the speech to text
  (Actually combine the RNNoise usage and the sherpa[ ]onnx usage in the same class).

[ ] Visualize the waveform in real time. Bsaically, each data callback we get we should
  calculate RMS on the provided samples. Add the output to a list, then when rendering we
  just render the bars sliding to the left.

[ ] Render text by blitting an entire unicode charset to a massive texture. Then just crop
  part of the text to render a character.

[ ] Create a UI module. Move the code for the texture atlas and the icon class and the button class there

[ ] Render the basic UI, place an audio waveform at the bottom, with a play/pause button on the side.
  Then the transcribed text near the top of the window, with a copy button and save button to the top right corner.

[ ] Shave the compile times down to a few miliseconds

[ ] Consider what data structure to use for the text editing component
