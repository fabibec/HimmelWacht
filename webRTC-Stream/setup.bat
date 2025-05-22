@echo off
echo Installing required Python packages...

pip install ultralytics
pip install aiortc
pip install paho-mqtt

echo Uninstalling conflicting packages...
pip uninstall -y opencv-python
pip uninstall -y numpy
pip uninstall -y torch
pip uninstall -y torchvision

echo Installing PyTorch with CUDA 12.8 support...
pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu128

echo All tasks completed.
pause
