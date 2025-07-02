import torch
import torchvision
import os
os.add_dll_directory("C:/Program Files/gstreamer/1.0/msvc_x86_64/bin") # Hardcoded path for GStreamer DLLs on Lab PC
import cv2
from ultralytics import YOLO

print("PyTorch version:", torch.__version__)
print("TorchVision version:", torchvision.__version__)
print("CUDA available:", torch.cuda.is_available())
print("CUDA version:", torch.version.cuda)

print("####################### \n")

print(cv2.getBuildInformation())


