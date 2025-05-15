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


# Check if CUDA is available and set device to CUDA (GPU)
device = 'cuda' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {device}")

# Load the custom YOLOv8 model
model = YOLO('yolov8n_custom.pt')
model.to(device)
model.eval()  # Set the model to evaluation mode 




class VideoReceiver:
    def __init__(self):
        self.track = None

    async def handle_track(self, track):
        print("Inside handle track")
        self.track = track
        frame_count = 0
        while True:
            try:
                print("Waiting for frame...")
                frame = await asyncio.wait_for(track.recv(), timeout=2.0)
                
                frame_count += 1
                print(f"Received frame {frame_count}")
                
                if isinstance(frame, VideoFrame):
                    print(f"Frame type: VideoFrame, pts: {frame.pts}, time_base: {frame.time_base}")
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
                
                # Run Inference on captured frame
                results = model(frame, device=device, conf=0.25, iou=0.45, agnostic_nms=True, max_det=1000)

                # Draw results on frame
                annotated_frame = results[0].plot()

                # Extract coordinates of the detected objects
                boxes = results[0].boxes

                # Print coordinates only if objects are detected
                if boxes is not None and len(boxes) > 0:
                    for i, box in enumerate(boxes):
                        coords = box.xyxy[0].cpu().numpy()
                        x1, y1, x2, y2 = coords
                        print(f"Box {i}: x1={int(x1)}, y1={int(y1)}, x2={int(x2)}, y2={int(y2)}")


                #cv2.putText(annotated_frame, f"FPS: NaN", (10, 30), 
                #            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)


                #cv2.putText(frame, timestamp, (10, frame.shape[0] - 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)
                cv2.imshow("Frame", annotated_frame)
    
                # Exit on 'q' key press
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            except asyncio.TimeoutError:
                print("Timeout waiting for frame, continuing...")
            except Exception as e:
                print(f"Error in handle_track: {str(e)}")
                if "Connection" in str(e):
                    break
        print("Exiting handle_track")

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
