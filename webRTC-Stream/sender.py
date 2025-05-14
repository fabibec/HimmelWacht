import asyncio
import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription, VideoStreamTrack
from aiortc.contrib.signaling import TcpSocketSignaling
from av import VideoFrame
import fractions
from datetime import datetime
import logging


logging.basicConfig(level=logging.INFO)
logging.getLogger("aiortc").setLevel(logging.INFO)
logging.getLogger("cv2").setLevel(logging.INFO)




class CustomVideoStreamTrack(VideoStreamTrack):
    def __init__(self, pipeline):
        super().__init__()
        self.pipeline = pipeline
        self.cap = cv2.VideoCapture(self.pipeline, cv2.CAP_GSTREAMER)
        self.frame_count = 0
        self.first_frame_sent = False
        self.first_frame_timestamp = None
        self.last_frame_timestamp = None

    async def recv(self):
        self.frame_count += 1
        ret, frame = self.cap.read()
        if not ret:
            print("Failed to read frame from camera")
            await asyncio.sleep(0.1)  # Avoid CPU spin
            return await self.recv()

        # Convert frame only once and create VideoFrame properly
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        video_frame = VideoFrame.from_ndarray(frame_rgb, format="rgb24")
        
        # Use proper timing values
        pts = int(self.frame_count * 90000 / 15)  # 90kHz clock (standard for RTP)
        video_frame.pts = pts
        video_frame.time_base = fractions.Fraction(1, 90000)
        
        # Track the first frame
        if not self.first_frame_sent:
            self.first_frame_sent = True
            self.first_frame_timestamp = datetime.now().isoformat()
            print(f"FIRST FRAME SENT at {self.first_frame_timestamp} with pts={pts}")
        
        print(f"Frame {self.frame_count} sent with pts={pts}")
        self.last_frame_timestamp = datetime.now().isoformat()

        return video_frame

async def setup_webrtc_and_run(ip_address, port, camera_id):
    signaling = TcpSocketSignaling(ip_address, port)
    pc = RTCPeerConnection()
    video_sender = CustomVideoStreamTrack(camera_id)
    pc.addTrack(video_sender)
    
    # Create data channel for keepalive
    dc = pc.createDataChannel("keepalive")
    
    # Track for cleanup
    keepalive_task = None

    try:
        await signaling.connect()

        @pc.on("datachannel")
        def on_datachannel(channel):
            print(f"Data channel established: {channel.label}")

        @pc.on("connectionstatechange")
        async def on_connectionstatechange():
            print(f"Connection state is {pc.connectionState}")
            if pc.connectionState == "connected":
                print("WebRTC connection established successfully")
                # Start keepalive when connected
                nonlocal keepalive_task
                keepalive_task = asyncio.create_task(send_keepalive(dc))
            if pc.connectionState == "closed":
                print(f"First frame timestamp: {video_sender.first_frame_timestamp} \n  last frame timestamp: {video_sender.last_frame_timestamp}")
                # Clean up keepalive task
                if keepalive_task:
                    keepalive_task.cancel()

        # Add ICE configuration with longer timeout
        offer = await pc.createOffer()
        await pc.setLocalDescription(offer)
        await signaling.send(pc.localDescription)

        while True:
            obj = await signaling.receive()
            if isinstance(obj, RTCSessionDescription):
                await pc.setRemoteDescription(obj)
                print("Remote description set")
            elif obj is None:
                print("Signaling ended")
                break
        print("Closing connection")
    finally:
        if keepalive_task:
            keepalive_task.cancel()
        await pc.close()

async def send_keepalive(datachannel):
    """Send periodic keepalive messages to maintain the connection."""
    try:
        while True:
            # Send keepalive every 30 seconds
            datachannel.send("keepalive")
            print("Sent keepalive ping")
            await asyncio.sleep(30)
    except Exception as e:
        print(f"Keepalive error: {e}")

async def main():
    ip_address = "10.42.0.1" # Ip Address of Remote Server/Machine as (the sender)
    port = 9999
    pipeline = "libcamerasrc ! queue leaky=downstream ! video/x-raw,width=640,height=480,framerate=15/1,format=NV12 ! videoconvert ! queue leaky=downstream ! video/x-raw,format=BGR ! queue max-size-buffers=4 ! appsink max-buffers=2 drop=true"    
    await setup_webrtc_and_run(ip_address, port, pipeline)

if __name__ == "__main__":
    asyncio.run(main())
