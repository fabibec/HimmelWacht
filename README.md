# AI Object Detection - Documentation

<par> INSERT TOC HERE </par>


## Tooling 

For labeling and image annotation we used [Label Studio](https://labelstud.io/) which provides a free & open-source Solution for data annotation in various types. For the first training runs (Run 1 to Run 4) we used rectangle anotation to ensure the least amount of processing is needed when inferencing the models.

## General Information

In the external directory there is the self-compiled opencv-python version with gstreamer enabled for peer-to-peer camera streaming (Can be used for external inference if Raspi is too slow). When working with virtual environments just paste it into existing site-packages. Uninstall any other opencv-python instances which might be active with:

`pip uninstall opencv-python`

This code uses the yolov8n_custom.pt. Not using ONNX-Backend. GStreamer pipeline takes in the media stream received on port 5000, can be improved by tweaking for ultra low latency. 

Since raspi camera module 3 is not yet (afaik) native usable in opencv we one possibility is to take the libcamera stream direct via pipeline **or** stream the libcamera to localhost:5000 and grab the stream the way mentioned below.

```python
import cv2
import subprocess
from ultralytics import YOLO
import torch
from loguru import logger

# GStreamer-Pipeline for receiving media stream
pipeline = (
    "gst-launch-1.0 udpsrc port=5000 ! application/x-rtp,encoding-name=H264,payload=96 ! "
    "rtph264depay ! h264parse ! avdec_h264 ! "
    "videoconvert ! video/x-raw,format=BGR ! "
    "appsink drop=true sync=false"
)

model = YOLO("yolov8n_custom.pt")
# Check if cuda is available
logger.info(torch.cuda.is_available())

device = "cuda" if torch.cuda.is_available() else "cpu"
model.to(device)  # Move to Cuda for GPU inference if available


model.to(device) 

print(cv2.getBuildInformation())  # Check if GStreamer integration is installed in cv2

cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

```

Due to size limitations the images and labels for training, validation and testing is not yet in this repository. 

