import asyncio
import websockets
import json

async def handler(websocket):
    print("Client connected.")

    try:
        i = 1
        while True:
  
            message = { 
                "boxes": [
                    {
                        "x": 50+i,
                        "y": 100+i,
                        "width": 30+i,
                        "height": 40+i
                    }
                ]
            }
            await websocket.send(json.dumps(message))
            i = i + 1
            await asyncio.sleep(0.1)
    except websockets.ConnectionClosed:
        print("Client disconnected.")

async def main():
    server = await websockets.serve(handler, "0.0.0.0", 8001)
    print("WebSocket server started on ws://0.0.0.0:8001")
    await server.wait_closed()

asyncio.run(main())
