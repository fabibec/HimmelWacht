# AI object detection

To run the AI-part of this project:

Prerequisites: Laboratory Computer needs to be connected to TIRoboter WIFI

1. Start MediaMTX-Server with the correct configuration on the RPi5 => /mediamtx/mediamtx.yml
2. Start MQTT-Broker with correct configuration => mosquttio.exe -c mosquitto.conf
3. Create execution-environment with CUDA-Support on the laboratory computer => setup.bat
4. Run receiver_inference.py

webRTC_inference/Inference_Scripts/receiver_inference.py needs to be ran on the laboratory computer.
