# Sender

Contains the deprecated sending python script for a peer-to-peer connection between the Raspberry Pi 5 and the laboratory computer.

## sender_p2p_DEPRECATED.py

Utilizes an opencv installation with GStreamer-support to pipe libcamera-stream into the application. The stream is sent via webrtc (aiortc) to the receiving end.
Deprecated due to the requirement of a solution that provides the videostream to multiple peers.
