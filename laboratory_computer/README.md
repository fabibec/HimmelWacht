# Laboratory Computer

This part of the repo contains all the code that needs to be executed on the laboratory computer.

## AI object detection

To run the AI-part of this project:

Prerequisites: Laboratory Computer needs to be connected to TIRoboter WIFI

1. Start MQTT-Broker with correct configuration => mosquttio.exe -c mosquitto.conf
2. Create execution-environment with CUDA-Support => setup.bat
3. Run webRTC_inference/Inference_Scripts/receiver_inference.py

