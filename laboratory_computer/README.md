# Laboratory Computer

This directory contains all code that needs to be executed on the laboratory computer.

## AI Object Detection

### Prerequisites

- The laboratory computer must be connected to the **TIRoboter** Wi-Fi.
- Ensure that [Mosquitto MQTT](https://mosquitto.org/download/) is installed.
- NVIDIA GPU with CUDA support is required.

### Steps to Run Inference

1. **Start the MQTT Broker** <br>
   Run the following command with the correct configuration file:
   ```cmd
   mosquitto.exe -c .\mosquitto\mosquitto.conf
   ```

2. **Set up the CUDA-enabled Python environment** <br>
    Execute the setup script:
    ```cmd
    .\setup\setup.bat
    ```

3. **Run the inference script** <br>
    Launch the AI inference receiver:
    ```cmd
    python webRTC_inference/Inference_Scripts/receiver_inference.py
    ```
