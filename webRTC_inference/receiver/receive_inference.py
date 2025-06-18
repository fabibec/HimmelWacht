import re
import asyncio
import aiohttp
import os
os.add_dll_directory("C:/Program Files/gstreamer/1.0/msvc_x86_64/bin")
import cv2
import time
import numpy as np
from datetime import datetime, timedelta
from av import VideoFrame, codec
from ultralytics import YOLO
import torch
import paho.mqtt.client as mqtt
import websockets
import json
from loguru import logger
from aiortc import RTCPeerConnection, RTCConfiguration, RTCIceServer, RTCSessionDescription
import logging

# ************************************** DOCUMENTATION **************************************
# From x:0 y:48 (neutral) to upper / lower boundary it approx. takes y=16
# From x:0 y:48 (neutral) to left boundary takes x=18
# From x:0 y:48 (neutral) to right boundary takes x=-15


# ************************************** CONSTANTS & GLOBALS **************************************
WHEP_URL = 'http://172.16.9.13:8889/stream/whep'
x_angle, y_angle = 0, 48
bb_x, bb_y, bb_w, bb_h = 0, 0, 0, 0
clients = set()

# ************************************** AI MODEL SETUP **************************************
logging.getLogger('ultralytics').setLevel(logging.ERROR)
device = 'cuda' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {device}")
model = YOLO('yolov8n_custom.pt').to(device)
model.eval()

# ************************************** MQTT SETUP **************************************
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.connect("127.0.0.1", 1883, 60)


# ************************************** HELPER FUNCTIONS **************************************
def pixel_to_motor_angles(bbox_center_x, bbox_center_y, 
                         image_width=1280, image_height=1080,
                         center_x=640, center_y=540,
                         neutral_pos=(0, 48),
                         x_range=(-18, 15), y_range=(-16, 16)):
    """
    Map bounding box center coordinates to motor angles.
    
    Args:
        bbox_center_x, bbox_center_y: Center of bounding box in pixels
        image_width, image_height: Camera resolution
        center_x, center_y: Image center coordinates
        neutral_pos: Motor neutral position (x_motor, y_motor)
        x_range: Motor x-axis range (right_boundary, left_boundary)
        y_range: Motor y-axis range (lower_boundary, upper_boundary)
    
    Returns:
        tuple: (motor_x_angle, motor_y_angle)
    """
    
    # Calculate pixel offset from center
    pixel_offset_x = bbox_center_x - center_x
    pixel_offset_y = bbox_center_y - center_y
    
    # Calculate the pixel range from center to boundaries
    pixel_range_x = center_x  # 640 pixels from center to edge
    pixel_range_y = center_y  # 540 pixels from center to edge
    
    # Map pixel offsets to motor angle offsets
    # For X: negative pixel offset (left) -> positive motor angle (left boundary +15)
    # For X: positive pixel offset (right) -> negative motor angle (right boundary -18)
    motor_offset_x = -pixel_offset_x * (x_range[1] - x_range[0]) / (2 * pixel_range_x)
    
    # For Y: negative pixel offset (up) -> positive motor angle (upper boundary +16)  
    # For Y: positive pixel offset (down) -> negative motor angle (lower boundary -16)
    motor_offset_y = -pixel_offset_y * (y_range[1] - y_range[0]) / (2 * pixel_range_y)
    
    # Add offsets to neutral position
    motor_x = neutral_pos[0] + motor_offset_x
    motor_y = neutral_pos[1] + motor_offset_y
    
    # Clamp to motor limits
    motor_x = np.clip(motor_x, x_range[0], x_range[1])
    motor_y = np.clip(motor_y, y_range[0], y_range[1])
    
    return int(motor_x), int(motor_y)



# ************************************** FUNCTIONS **************************************

async def run_track(track):
    global bb_x, bb_y, bb_w, bb_h
    global x_angle, y_angle
    print("Track started")

    while True:
        frame = await track.recv()
        if frame is None:
            break

        # Convert to OpenCV BGR
        frame = cv2.cvtColor(frame.to_ndarray(format="rgb24"), cv2.COLOR_RGB2BGR)
        results = model(frame, device=device, conf=0.9, iou=0.5, agnostic_nms=True)
        annotated = results[0].plot()
        img = cv2.circle(annotated,(540,640), 5, (0,0,255), 3)
        img = cv2.circle(img,(640,540), 5, (0,255,0), 3)
        boxes = results[0].boxes

        if boxes is not None and len(boxes) > 0:
            for i, box in enumerate(boxes):
                coords = box.xyxy[0].cpu().numpy()
                x1, y1, x2, y2 = map(int, coords)
                center = [(x1 + x2) // 2, (y1 + y2) // 2]

                coords = box.xyxy[0].cpu().numpy()
                x1, y1, x2, y2 = coords
                x1 = int(x1)
                y1 = int(y1)
                x2 = int(x2)
                y2 = int(y2)

                bb_x = x1
                bb_y = y1
                bb_w = x2 - x1
                bb_h = y2 - y1

                center_x = (x1 + x2) / 2
                center_y = (y1 + y2) / 2
                center = [int(center_x), int(center_y)]
                   
                motor_angle_x, motor_angle_y = pixel_to_motor_angles(center_x, center_y)


                motor_angle_x = motor_angle_x * -1 # Float as return

                print(type(motor_angle_x))
                print(type(motor_angle_y))

                x_angle = x_angle + motor_angle_x if (x_angle < 90 and x_angle > -90) else 0
                y_angle = y_angle + motor_angle_y if (y_angle < 70 and y_angle > 0) else y_angle

                logger.critical(f"x_angle is {int(x_angle)} & y_angle is {int(y_angle)}")
                x_angle = x_angle * -1

                payload = {
                    "platform_x_angle": int(x_angle),
                    "platform_y_angle": int(y_angle),
                    "fire_command": False
                }

                client.publish("vehicle/turret/cmd", json.dumps(payload))
                logger.debug(f"{x_angle}, {y_angle}")

        else:
            bb_x = bb_y = bb_w = bb_h = 0



        cv2.imshow("Annotated", img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            await track.stop()
            break

        await asyncio.sleep(0.01)


async def websocket_handler(websocket):
    logger.info("WebSocket client connected")
    clients.add(websocket)
    try:
        while True:
            message = {
                "boxes": [{
                    "x": bb_x,
                    "y": bb_y,
                    "width": bb_w,
                    "height": bb_h
                }]
            }
            await websocket.send(json.dumps(message))
            await asyncio.sleep(0.1)
    except websockets.ConnectionClosed:
        logger.info("WebSocket client disconnected")
    finally:
        clients.remove(websocket)


async def start_websocket_server():
    async with websockets.serve(websocket_handler, "0.0.0.0", 8001):
        logger.info("WebSocket server running at ws://0.0.0.0:8001")
        await asyncio.Future()  # run forever


async def start_webrtc_stream():
    async with aiohttp.ClientSession() as session:
        # Get ICE servers
        iceServers = []
        async with session.options(WHEP_URL) as res:
            for k, v in res.headers.items():
                if k.lower() == "link":
                    m = re.match(r'^<(.+?)>; rel="ice-server"(; username="(.*?)"; credential="(.*?)"; credential-type="password")?', v)
                    if m:
                        iceServers.append(RTCIceServer(
                            urls=m[1],
                            username=m[3],
                            credential=m[4],
                            credentialType='password'
                        ))

        # Setup WebRTC
        pc = RTCPeerConnection(RTCConfiguration(iceServers=iceServers))
        pc.addTransceiver('video', direction='recvonly')

        @pc.on("connectionstatechange")
        async def on_connectionstatechange():
            logger.info(f"Connection state: {pc.connectionState}")

        @pc.on("track")
        async def on_track(track):
            logger.info("Track received")
            await run_track(track)

        # Offer/answer
        offer = await pc.createOffer()
        await pc.setLocalDescription(offer)

        async with session.post(WHEP_URL, headers={'Content-Type': 'application/sdp'}, data=offer.sdp) as res:
            answer = await res.text()
        await pc.setRemoteDescription(RTCSessionDescription(sdp=answer, type='answer'))

        await asyncio.Future()  # run forever


# ************************************** ENTRY POINT **************************************
async def main():
    await asyncio.gather(
        start_websocket_server(),
        start_webrtc_stream()
    )

if __name__ == "__main__":
    asyncio.run(main())