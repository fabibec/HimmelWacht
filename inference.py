import os
os.add_dll_directory("C:/Program Files/gstreamer/1.0/msvc_x86_64/bin")
import cv2
import time
from ultralytics import YOLO
import torch
import torch.version

print("Torch version:", torch.__version__)
print("Torch CUDA version:", torch.version.cuda)

# Check if CUDA is available and set device
device = 'cuda' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {device}")

# Load the custom YOLOv8 model
model = YOLO('yolov8n_custom.pt')
model.to(device)
model.eval()  # Set the model to evaluation mode 


# RTSP stream URL - replace with your actual RTSP stream URL
rtsp_url = "rtsp://172.16.9.13:8554/libcamera"

# GStreamer pipeline for RTSP
gst_str = (
    f'rtspsrc location={rtsp_url} latency=50 protocols=tcp ! '
    'rtph264depay ! h264parse ! avdec_h264 ! '
    'videoconvert ! appsink '
)

pipeline = (
    "udpsrc port=50000 ! application/x-rtp,encoding-name=H264,payload=128 ! "
    "rtph264depay ! h264parse ! avdec_h264 ! "
    "videoconvert ! video/x-raw,format=BGR ! "
    "appsink drop=true sync=false"
)


# Create video capture object using GStreamer pipeline
cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

# Check if camera opened successfully
if not cap.isOpened():
    print("Error: Could not open RTSP stream.")
    exit()

print("Stream opened successfully. Press 'q' to exit.")

# Process frames
while True:
    # Read frame
    ret, frame = cap.read()
    
    if not ret:
        print("Failed to receive frame. Exiting...")
        break
    
    # Run inference on the frame
   # start_time = time.time()
    
    resized = cv2.resize(frame, (640, 640))

    results = model(frame, device=device, conf=0.25, iou=0.45, agnostic_nms=True, max_det=1000)
    #inference_time = time.time() - start_time
    


    # Draw results on frame
    annotated_frame = results[0].plot()
    
    # Display FPS
    #fps = 1.0 / inference_time if inference_time > 0 else 1.0
    cv2.putText(annotated_frame, f"FPS: NaN", (10, 30), 
                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    
    # Display the annotated frame
    cv2.imshow("YOLOv8 Inference", annotated_frame)
    
    # Press 'q' to exit
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Release resources
cap.release()
cv2.destroyAllWindows()