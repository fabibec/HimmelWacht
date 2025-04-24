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

For sending the stream via network (or to localhost) from the camera to a receiver you can use this command 

```bash
libcamera-vid -t 0 --width 1280 --height 720 --framerate 30 --codec h264 \
--inline --libav-format h264 --output - | gst-launch-1.0 fdsrc ! h264parse ! \
rtph264pay config-interval=1 pt=96 ! udpsink host=<ZIEL-IP> port=5000
```

Due to size limitations the images and labels for training, validation and testing is not yet in this repository. 

## Update: 24.04.2025

With the `--nopreview` flag you can suppress the rendering to the screen 

```bash
libcamera-vid --nopreview -t 0 --width 1280 --height 720 --framerate 30 --codec h264 \
--inline --libav-format h264 --output - | gst-launch-1.0 fdsrc ! h264parse ! \
rtph264pay config-interval=1 pt=96 ! udpsink host=<ZIEL-IP> port=5000
```

```python
import cv2
import subprocess
from ultralytics import YOLO
import torch
from loguru import logger

# GStreamer-Pipeline for receiving media stream
pipeline = (
    "udpsrc port=5000 ! application/x-rtp,encoding-name=H264,payload=96 ! "
    "rtph264depay ! h264parse ! avdec_h264 ! "
    "videoconvert ! video/x-raw,format=BGR ! "
    "appsink drop=true sync=false"
)

model = YOLO("yolov8n_custom.pt")

# Check if CUDA is available and move model to the right device
logger.info(f"CUDA Available: {torch.cuda.is_available()}")
device = "cuda" if torch.cuda.is_available() else "cpu"
model.to(device)

# Check OpenCV build info
print(cv2.getBuildInformation())

# Open the video stream
cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

if not cap.isOpened():
    logger.error("Failed to open video stream with GStreamer pipeline.")
    exit()

logger.info("Video stream opened successfully.")

while True:
    ret, frame = cap.read()
    if not ret:
        logger.warning("Failed to grab frame")
        continue

    # Run inference
    results = model(frame)

    # Visualize the results on the frame
    annotated_frame = results[0].plot()  # Assumes a single image

    # Display the frame
    cv2.imshow("YOLOv8 Inference", annotated_frame)

    # Break loop with 'q'
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# Release resources
cap.release()
cv2.destroyAllWindows()
```

When using venv it is required to add the cv2*.so to the `/home/HW/test/venv/lib/python3.11/site-packages` path 

