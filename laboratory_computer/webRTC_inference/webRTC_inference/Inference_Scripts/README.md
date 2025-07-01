# Inference Scripts

Contains the inference scripts for AI inference.

## receiver_p2p_DEPRECATED.py
Deprecated implementation that contains the receiver script for receiving a low-latency webrtc-stream from the connection peer. 

Deprecated because of the requirement to grab the videostream from a single point (and make it watchable from any device). The AI prediction is being ran on the received 
stream and printed to the screen.

## receiver_inference.py
Latest implementation Script that grabs a low-latency webrtc-stream from a running mediamtx server which is ran on the Raspberry Pi 5. 
Utilizes the WHEP API from Mediamtx for receiving the streaming data. No need for Keep-Alive-Datachannel (in contrast to old receiver script)
Combines grabbing the video, running AI-Inference, publishing the bounding-box coordinates via Websocket to a running webserver on the Raspberry Pi. 
Also utilizes the coordinates of the bounding-box for computation of rotation angle that is needed to realize real-time object tracking. Rotation Angles are being published via MQTT.