import asyncio
import os
os.add_dll_directory("C:/Program Files/gstreamer/1.0/msvc_x86_64/bin")
import cv2
import numpy as np
from aiortc import RTCPeerConnection, RTCSessionDescription, MediaStreamTrack
from aiortc.contrib.signaling import TcpSocketSignaling
from av import VideoFrame, codec
from datetime import datetime, timedelta
from ultralytics import YOLO
import torch
import torch.version
import paho.mqtt.client as mqtt
import subprocess



# MQTT-Client for publishing to MQTT-Broker
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.connect("127.0.0.1", 1883, 60)

def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected to MQTT broker with result code " + str(rc))
    client.subscribe("sensor/GYRO")
    client.subscribe("sensor/US")

def on_message(client, userdata, msg):
    global GYRO_DATA, US_DATA

    if msg.topic == "sensor/topic1":
        sensor_data_1 = msg.payload.decode()
        print(f"sensor_data_1 updated: {sensor_data_1}")
    elif msg.topic == "sensor/topic2":
        sensor_data_2 = msg.payload.decode()
        print(f"sensor_data_2 updated: {sensor_data_2}")


# Check if CUDA is available and set device to CUDA (GPU)
device = 'cuda' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {device}")

# Load the custom YOLOv8 model
model = YOLO('yolov8n_custom.pt')
model.to(device)
model.eval()  # Set the model to evaluation mode 

MEDIAMTX_URL = "rtsp://172.16.9.13:8554/camera"

# FFmpeg command to push raw BGR video frames to RTSP
FFMPEG_CMD = [
    'ffmpeg',
    '-f', 'rawvideo',
    '-vcodec', 'rawvideo',
    '-pix_fmt', 'bgr24',
    '-s', '640x480',
    '-r', '15',
    '-i', '-',
    '-c:v', 'libx264',
    '-preset', 'ultrafast',
    '-tune', 'zerolatency',
    '-g', '15',  # Keyframe interval = 1 second
    '-keyint_min', '30',
    '-sc_threshold', '0',
    '-b:v', '800k',
    '-maxrate', '800k',
    '-bufsize', '1600k',
    '-f', 'rtsp',
    '-rtsp_transport', 'udp',
    MEDIAMTX_URL
]

ffmpeg = subprocess.Popen(FFMPEG_CMD, stdin=subprocess.PIPE)


class VideoReceiver:
    def __init__(self):
        self.track = None

    async def handle_track(self, track):
        print("Inside handle track")
        self.track = track
        frame_count = 0
        while True:
            try:
                frame = await asyncio.wait_for(track.recv(), timeout=2.0)
                

                # ---------------------- Debugging Comment ------------------------------------------
                #frame_count += 1
                #print(f"Received frame {frame_count}")
                
                if isinstance(frame, VideoFrame):
                    #print(f"Frame type: VideoFrame, pts: {frame.pts}, time_base: {frame.time_base}") -- Debugging Comment
                    frame = frame.to_ndarray(format="rgb24")
                    frame = np.flipud(frame)  # Convert RGB to BGR
                    frame = np.fliplr(frame)  # Flip the frame horizontally

                elif isinstance(frame, np.ndarray):
                    print(f"Frame type: numpy array")
                else:
                    print(f"Unexpected frame type: {type(frame)}")
                    continue
              
                 # Add timestamp to the frame
                current_time = datetime.now()
                new_time = current_time - timedelta( seconds=55)
                timestamp = new_time.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                
                #Run regular Inference on captured frame
                results = model(frame, device=device, conf=0.85, iou=0.45, agnostic_nms=True, max_det=1000)


                # Run Inference with explicit tracking algorithm
                # Result => Adds lot of latency due to CPU-heavy tracking algorithm 
                # Therefore model.track is not usable for real-time 
                # Falling back to regular inference frame-by-frame
                #results = model.track(frame,stream=False, persist=True, conf=0.9, verbose=True, device=device, iou=0.45) 
                #for res in results:
                #    
                #    annotated_frame = res.plot()
                #    cv2.imshow("Tracker", annotated_frame)
                #    if cv2.waitKey(1) & 0xFF == ord('q'):
                #        break

                
                # Draw results on frame
                annotated_frame = results[0].plot()
                img = cv2.circle(annotated_frame,(310,397), 5, (0,0,255), 3)


                # Extract coordinates of the detected objects
                boxes = results[0].boxes

                # Print coordinates only if objects are detected
                if boxes is not None and len(boxes) > 0:
                    for i, box in enumerate(boxes):
                        coords = box.xyxy[0].cpu().numpy()
                        x1, y1, x2, y2 = coords
                        x1 = int(x1)
                        y1 = int(y1)
                        x2 = int(x2)
                        y2 = int(y2)

                        center_x = (x1 + x2) / 2
                        center_y = (y1 + y2) / 2
                        center = [int(center_x), int(center_y)]
                        


                        message = f"{center}"
                        client.publish("center", message)
                        print(f"Box {i}: x1={int(x1)}, y1={int(y1)}, x2={int(x2)}, y2={int(y2)}")


                cv2.imshow("Frame", img)
                ffmpeg.stdin.write(img.tobytes())
    
                # Exit on 'q' key press
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            except asyncio.TimeoutError:
                print("Timeout waiting for frame, continuing...")
            except Exception as e:
                #print(f"Error in handle_track: {str(e)}")
                if "Connection" in str(e):
                    break
        print("Exiting handle_track")
        ffmpeg.stdin.close()
        ffmpeg.wait()

async def run(pc, signaling):
    await signaling.connect()

    @pc.on("track")
    def on_track(track):
        if isinstance(track, MediaStreamTrack):
            print(f"Receiving {track.kind} track")
            asyncio.ensure_future(video_receiver.handle_track(track))

    @pc.on("datachannel")
    def on_datachannel(channel):
        print(f"Data channel established: {channel.label}")

        @channel.on("message")
        def on_message(message):
            if message == "keepalive":
                print("Received keepalive ping")
                # Optionally send a pong response
                channel.send("keepalive-ack")

    @pc.on("connectionstatechange")
    async def on_connectionstatechange():
        print(f"Connection state is {pc.connectionState}")
        if pc.connectionState == "connected":
            print("WebRTC connection established successfully")

        if pc.connectionState == "closed":
            print("Connection closed")
            print(pc.getStats())

    print("Waiting for offer from sender...")
    offer = await signaling.receive()
    print("Offer received")
    await pc.setRemoteDescription(offer)
    print("Remote description set")

    answer = await pc.createAnswer()
    print("Answer created")
    await pc.setLocalDescription(answer)
    print("Local description set")

    await signaling.send(pc.localDescription)
    print("Answer sent to sender")

    print("Waiting for connection to be established...")
    while pc.connectionState != "connected":
        await asyncio.sleep(0.1)

    print("Connection established, waiting for frames...")
    connection_alive = asyncio.Event()
    try:
        await connection_alive.wait()  # Wait until connection is explicitly closed
    except asyncio.CancelledError:
        print("Connection wait was cancelled")

    print("Closing connection")

async def main():
    signaling = TcpSocketSignaling("172.16.9.13", 9999) # IP and Port from the Sender you want to connect to
    pc = RTCPeerConnection()
    
    global video_receiver
    video_receiver = VideoReceiver()

    try:
        await run(pc, signaling)
    except Exception as e:
        print(f"Error in main: {str(e)}")
    finally:
        print("Closing peer connection")
        await pc.close()

if __name__ == "__main__":
    asyncio.run(main())
