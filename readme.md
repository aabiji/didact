# Didact

A simple notes app

```sh
cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=Debug
```

Icons from: https://fonts.google.com/icons

Thoughts:
- Make the text read only during playback (until the stop button is pressed)
- Concept of UI pages?
- We have to implement out own text input widget if we want markdown syntax highlighting
- Button to copy the transcription or save the transcription
- Start implementing a custon text editing widget
- Some improvements to the transcriber module:
  - In the ring buffers, first copy both portions to a linear array (using memcpy), then
    process on the array. Is is less ergonomic than just twiddling the index,
    but it improves cache coehrence.
  - SampleQueue should use a ring buffer, not a queue, make the ring buffer [lock free](https://github.com/QuantumLeaps/lock-free-ring-buffer/tree/main)
  - Move the fft and rnnoise out of the audio callback, push the samples to a
    queue and operate on samples from the queue (maybe the central data queue idea wasn't bad at all)
  - Handle possible failues better during initialization (and handle cleanup properly in case of failure)
  - Deal with race conditions (add mutex to the text and fft bars getters and setters)
  - Replace the magic numbers with constnats, use ma_result_description in errors
  - Pass a string view to the speech callback
  - Use std::jthread instead of std::thread
- Render the text from a precomputed texture atlas
- Improve the building/linking speeds

- Same ethos as obsidian where you're in charge of your files
- Minimailist ui (black background, clean white text in a large font)
- Big idea: Samsung Notes clone that's open source, cross platform and fast, with voice dicdation
