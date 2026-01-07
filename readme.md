# Didact

```sh
cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=Debug -D CMAKE_LINKER_TYPE=MOLD
```

Icons from: https://fonts.google.com/icons

Thoughts:
- Make the text read only during playback (until the stop button is pressed)
- Have a transcription class that handles all of the audio stuff
- Can we get sherpa-onnx to use our igpu
- Concept of UI pages?
- We have to implement out own text input widget if we want markdown syntax highlighting

Today:
- Allow the user to edit past lines
- Option to save the recording
  - Save the text file and the audio file to a default user folder
- Make the text input have a border and remove its background
- Make the buttons have icons instead of text
- Add a back buttonA
- Sketch some UI ideas for the note editor
- Make the text font bigger

A simple notes app

- Same ethos as obsidian where you're in charge of your files
- Minimailist ui (black background, clean white text in a large font)
- Big idea: Samsung Notes clone that's open source, cross platform and fast, with voice dicdation
