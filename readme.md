

https://stackoverflow.com/questions/6740545/understanding-fft-output
https://swharden.com/csdv/audio/fft/
https://dsp.stackexchange.com/questions/40766/calculating-values-of-frequency-bins-in-python
https://dlbeer.co.nz/articles/fftvis.html
https://www.nti-audio.com/en/support/know-how/fast-fourier-transform-fft
https://www.youtube.com/watch?v=h7apO7q16V0
https://download.ni.com/evaluation/pxi/Understanding%20FFTs%20and%20Windowing.pdf
https://etiand.re/posts/2025/01/how-to-decode-audio-streams-in-c-cpp-using-libav/
https://github.com/mackron/miniaudio
https://github.com/ggml-org/whisper.cpp
https://www.raylib.com/examples/audio/loader.html?name=audio_raw_stream

- whispercpp integration
- could extend as a cross platform voice notes app with real time ai transcriptions
  - run as deamon to have voice input
  - somehow get audio samples from microphone in real time
- ffmpeg for reading audio samples from video/real time
- save recordings + transcripts to disk
- simple ui to view voice recordings (using qt??? using lvgl???)
- audio playback
- live translate feature

4 threads:
- rendering
- reading audio frames
- doing fft
- whispercpp transcriptions
