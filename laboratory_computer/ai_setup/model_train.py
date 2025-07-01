from ultralytics import YOLO
import torch

# Load a model
model = YOLO('yolov8n.pt')  # load an official model
# Train the model
data_path = "/home/jendr/Projects/DT/yolov5/data.yml"  # path to your dataset
epochs = 100 # number of epochs
img_size = 640 # image size
device = 'cuda' if torch.cuda.is_available() else 'cpu'

# Train the model
model.train(data=data_path, epochs=epochs, imgsz=img_size, device=device)
# Save the model
model.save('yolov8n_custom.pt')  # save the trained model
# Export the model
model.export(format='onnx')  # export the model to ONNX format
