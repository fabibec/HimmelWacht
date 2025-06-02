from curses import start_color
import re
import asyncio
import aiohttp
from aiortc import RTCPeerConnection, RTCConfiguration, RTCIceServer, RTCSessionDescription
import cv2
import time 


# http://github.com/bluenviron/mediamtx/discussions/3640
# Source
# Check if this is better? 

WHEP_URL = 'http://localhost:8889/stream/whep'

async def run_track(track):
    start_time = time.time()
    print("Connection established at %s" % time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
    while True:
        frame = await track.recv()
        if frame is None:
            break
        # convert to OpenCV format
        img = frame.to_ndarray(format='bgr24')
        # display the frame
        cv2.imshow('Video', img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            await track.stop()
            cv2.destroyAllWindows()
            print("Connection closed at %s" % time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
            print("time elapsed: %.2f seconds" % (time.time() - start_time))
            break
        await asyncio.sleep(0.01)
        

async def main():
    async with aiohttp.ClientSession() as session:
        # get ICE servers
        iceServers = []
        async with session.options(WHEP_URL) as res:
            for k, v in res.headers.items():
                if k == "Link":
                    m = re.match('^<(.+?)>; rel="ice-server"(; username="(.*?)"; credential="(.*?)"; credential-type="password")?', v)
                    iceServers.append(RTCIceServer(urls=m[1], username=m[2], credential=m[3], credentialType='password'))

        # setup peer connection
        pc = RTCPeerConnection(RTCConfiguration(iceServers=iceServers))
        pc.addTransceiver('video', direction='recvonly')

        # on connection state change callback
        @pc.on("connectionstatechange")
        async def on_connectionstatechange():
            print("Connection state is %s" % pc.connectionState)

        # on track callback
        @pc.on('track')
        async def on_track(track):
            print('on track')
            task = asyncio.ensure_future(run_track(track))

        # generate offer
        offer = await pc.createOffer()
        await pc.setLocalDescription(offer)

        # send offer, set answer
        async with session.post(WHEP_URL, headers={'Content-Type': 'application/sdp'}, data=offer.sdp) as res:
            answer = await res.text()
        await pc.setRemoteDescription(RTCSessionDescription(sdp=answer, type='answer'))

        await asyncio.sleep(10000)

asyncio.run(main())