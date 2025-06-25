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
import math


# ************************************** SOURCES  **************************************
# AioRTC <-> MediaMTX: https://github.com/bluenviron/mediamtx/discussions/3640
# Ultralytics Inference: https://docs.ultralytics.com/de/tasks/detect/#predict
# WebSocket API: https://websockets.readthedocs.io/en/stable/ + ChatGPT / Anthropics Claude AI
# MQTT API: https://pypi.org/project/paho-mqtt/#description + ChatGPT / Anthropics Claude AI

# ************************************** DOCUMENTATION **************************************
# From x:0 y:48 (neutral) to upper / lower boundary it approx. takes y=16
# From x:0 y:48 (neutral) to left boundary takes x=18
# From x:0 y:48 (neutral) to right boundary takes x=-15

# shape: (1280,1080,3) => 1280 width 1080 height


class ServoTracker:
    def __init__(self):
        # Current absolute servo positions
        self.current_x_angle = 0    # degrees
        self.current_y_angle = 48   # degrees (your zero position)
        
        # Camera FOV constants
        self.HORIZONTAL_FOV = 60.3
        self.VERTICAL_FOV = 50.9
        self.IMAGE_WIDTH = 1280
        self.IMAGE_HEIGHT = 1080
    
    def calculate_camera_relative_angles(self, x_pixel, y_pixel):
        """Calculate angles relative to current camera center"""
        degrees_per_pixel_h = self.HORIZONTAL_FOV / self.IMAGE_WIDTH
        degrees_per_pixel_v = self.VERTICAL_FOV / self.IMAGE_HEIGHT
        
        # Angles relative to camera center
        horizontal_angle = (x_pixel - self.IMAGE_WIDTH/2) * degrees_per_pixel_h
        vertical_angle = (self.IMAGE_HEIGHT/2 - y_pixel) * degrees_per_pixel_v
        
        return horizontal_angle, vertical_angle
    
    def update_servo_position(self, x_pixel, y_pixel):
        """Update absolute servo positions based on camera detection"""
        
        # Get camera-relative angles
        rel_horizontal, rel_vertical = self.calculate_camera_relative_angles(x_pixel, y_pixel)
        
        # Convert to absolute servo positions
        new_x_angle = self.current_x_angle + rel_horizontal
        new_y_angle = self.current_y_angle + rel_vertical
        
        # Apply servo limits
        new_x_angle = max(-90, min(90, new_x_angle))
        new_y_angle = max(0, min(80, new_y_angle))
        

        # Update current positions
        self.current_x_angle = new_x_angle
        self.current_y_angle = new_y_angle
        
        return new_x_angle, new_y_angle
    
    def get_current_position(self):
        """Get current absolute servo positions"""
        return self.current_x_angle, self.current_y_angle
    
    def reset_to_zero(self):
        """Reset to zero position"""
        self.current_x_angle = 0
        self.current_y_angle = 48

servo_tracker = ServoTracker()



# ************************************** CONSTANTS & GLOBALS **************************************
WHEP_URL = 'http://172.16.9.13:8889/stream/whep'
x_angle, y_angle = 0, 48
abs_h, abs_v = 0, 0
bb_x, bb_y, bb_w, bb_h = 0, 0, 0, 0
clients = set()
counter = 0
bb_counter = 0

old_center = [0,0]

# ************************************** AI MODEL SETUP **************************************
logging.getLogger('ultralytics').setLevel(logging.ERROR)
device = 'cuda' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {device}")
model = YOLO('best.pt').to(device)
model.eval()

# ************************************** MQTT SETUP **************************************
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.connect("127.0.0.1", 1883, 60)

# ************************************** MISC **************************************


diagonal_fov = 75  # degrees (Camera Module 3)
image_width = 1280
image_height = 1080

# Calculate aspect ratio
aspect_ratio = image_width / image_height  # = 1.185

# Convert diagonal FOV to radians
diagonal_fov_rad = math.radians(diagonal_fov)

# Calculate horizontal and vertical FOV
# Using the relationship for rectangular sensors
horizontal_fov_rad = 2 * math.atan(
    aspect_ratio * math.tan(diagonal_fov_rad / 2) / 
    math.sqrt(1 + aspect_ratio**2)
)

vertical_fov_rad = 2 * math.atan(
    math.tan(diagonal_fov_rad / 2) / 
    math.sqrt(1 + aspect_ratio**2)
)

# Convert back to degrees
horizontal_fov = math.degrees(horizontal_fov_rad)
vertical_fov = math.degrees(vertical_fov_rad)

print(f"Horizontal FOV: {horizontal_fov:.1f}°")
print(f"Vertical FOV: {vertical_fov:.1f}°")


# Calculated FOV values for your 1280x1080 resolution
HORIZONTAL_FOV = 60.8  # degrees
VERTICAL_FOV = 52.7    # degrees
IMAGE_WIDTH = 1280
IMAGE_HEIGHT = 1080

def calculate_servo_angles_from_pixels(x_pixel, y_pixel):
    """
    Calculate servo angles directly from pixel coordinates
    
    x_pixel: horizontal pixel position of target
    y_pixel: vertical pixel position of target
    """
    
    # Calculate degrees per pixel
    degrees_per_pixel_h = HORIZONTAL_FOV / IMAGE_WIDTH
    degrees_per_pixel_v = VERTICAL_FOV / IMAGE_HEIGHT
    
    # Calculate angles from camera center
    # Horizontal: negative for right side, positive for left side
    horizontal_angle = (x_pixel - IMAGE_WIDTH/2) * degrees_per_pixel_h
    
    # Vertical: positive for top, negative for bottom
    vertical_angle = (IMAGE_HEIGHT/2 - y_pixel) * degrees_per_pixel_v
    
    # Map to your servo coordinate system
    # Horizontal: -90° (right) to +90° (left), 0° is center
    servo_horizontal = max(-90, min(90, horizontal_angle))
    
    # Vertical: 0° (bottom) to 80° (top), 48° is center (0°)
    servo_vertical = max(0, min(80, vertical_angle + 48))

    print("HUHU", servo_horizontal, servo_vertical)
    
    return servo_horizontal, servo_vertical

# Example usage:
# If target is at pixel (800, 400)
h_angle, v_angle = calculate_servo_angles_from_pixels(800, 400)
print(f"Horizontal servo angle: {h_angle:.1f}°")
print(f"Vertical servo angle: {v_angle:.1f}°")


# ************************************** FUNCTIONS **************************************

async def run_track(track):
    global bb_x, bb_y, bb_w, bb_h
    global x_angle, y_angle
    global counter, bb_counter
    global old_center
    print("Track started")
    initial = True  # Start with True, not False
    
    while True:
        frame = await track.recv()
   
        if frame is None:
            break
            
        # Convert to OpenCV BGR
        frame = cv2.cvtColor(frame.to_ndarray(format="rgb24"), cv2.COLOR_RGB2BGR)
        results = model(frame, device=device, conf=0.9, iou=0.5, agnostic_nms=True, max_det = 1)
        annotated = results[0].plot()
        img = cv2.circle(annotated,(640,450), 5, (0,255,0), 3)
        boxes = results[0].boxes
        
        if boxes is not None and len(boxes) > 0:
            # Reset bb_counter since we found a target
            bb_counter = 0
            
            for i, box in enumerate(boxes):
                coords = box.xyxy[0].cpu().numpy()
                x1, y1, x2, y2 = map(int, coords)
                
                bb_x = x1
                bb_y = y1
                bb_w = x2 - x1
                bb_h = y2 - y1
                center_x = (x1 + x2) / 2
                center_y = (y1 + y2) / 2
                
                # Always reset counter when limit is reached
                if counter % 7 == 0:
                    counter = 0
                   
                    # Check conditions: initial run OR movement within bounds
                    movement_within_bounds = (initial or 
                                            (abs(center_x - old_center[0]) < 250 or 
                                             abs(center_y - old_center[1]) < 250))
                    
                    if movement_within_bounds:
                        # Store previous values and clear initial flag
                        old_center = [center_x, center_y]
                        initial = False
                       
                        rel_h, rel_v = servo_tracker.calculate_camera_relative_angles(center_x, center_y)
                        logger.debug(f"rel_h: {rel_h}, rel_v: {rel_v}")
                       
                        if abs(rel_h) >= 7 or abs(rel_v) >= 7:
                            logger.debug(f"rel_h: {rel_h}, rel_v: {rel_v}")
                            abs_h, abs_v = servo_tracker.update_servo_position(center_x, center_y)
                            logger.debug(f"abs_h: {abs_h}, abs_v: {abs_v}")
                            payload = {
                                "platform_x_angle": int(abs_h * -1),
                                "platform_y_angle": int(abs_v),
                                "fire_command": False
                            }
                            client.publish("vehicle/turret/cmd", json.dumps(payload))
                    else:
                        logger.debug(f"Skipping frame - large movement detected: dx={abs(center_x - old_center[0])}, dy={abs(center_y - old_center[1])}")
        else:
            # No target detected
            bb_x = bb_y = bb_w = bb_h = 0
            bb_counter = bb_counter + 1
           
        cv2.imshow("Annotated", img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            await track.stop()
            break
            
        await asyncio.sleep(0.01)
        
        # Increment command counter
        counter = counter + 1
       
        # Reset platform if no target detected for 5 seconds (150 frames at ~30fps)
        if bb_counter % 150 == 0 and bb_counter > 0:
            logger.info("No target detected for 5 seconds, resetting platform position")
            bb_counter = 0
            
            # Reset platform to home position
            abs_h = 0
            abs_v = 48
            payload = {
                "platform_x_angle": int(abs_h),
                "platform_y_angle": int(abs_v),
                "fire_command": False
            }
            client.publish("vehicle/turret/cmd", json.dumps(payload))
            
            # CRITICAL FIX: Update servo_tracker's internal position to match the reset
            servo_tracker.reset_to_zero()
            
            # Reset tracking state
            initial = True
            old_center = [640, 450]  # Reset to center of frame or known safe position
            
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