# Didact

A simple notes app

```sh
cmake -S . -B build -G Ninja -D CMAKE_BUILD_TYPE=Debug
```

Icons from: https://fonts.google.com/icons

Using clay as the layout engine

TODO:

[ ] Define a text editing widget area. It should have a faint border

[ ] Improve button rendering (add changing background on hover/click)

[ ] Implement and test the rope data structure

[ ] Start working on a basic text editor --> get os independent input from SDL3

[ ] Have a text editing cursor that can move with the arrow keys (or the android space slider)

[ ] Figure out how to exit instantly the app while the model is loading

[ ] Add 1 play/pause button next to the waveform visualization

[ ] Copy the transcript to the clipboard once the copy icon is clicked

[ ] Open the OS file picker when the save button is clicked

[ ] Define a good way to handle events (callback vs immediate mode pros and cons for our specific use case?)

[ ] Consider how a simple, ergonomic ui framework can be designed

[ ] Have a way to align elements (float left, float right, etc), instead of having to manually set xy position

[ ] Have a way to align elements relative to other elements