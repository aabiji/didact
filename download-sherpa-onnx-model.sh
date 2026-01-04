#!/bin/sh

cd assets
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06.tar.bz2
tar xvf sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06.tar.bz2
rm sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06.tar.bz2
cd ..
