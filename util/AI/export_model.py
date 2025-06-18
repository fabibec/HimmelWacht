from ultralytics import YOLO
import torch

# Load a model
model = YOLO('yolov8n_custom.pt')  # load an official model


# Export the model to ONNX format
model.export(format='onnx')