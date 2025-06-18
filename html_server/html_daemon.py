import asyncio
import json
from sensors.gyro_startup import gyro_data
from aiohttp import web
import websockets

# Track WebSocket clients
clients = set()

# === WebSocket sensor stream ===
async def sensor_aggregator():
    async for gyro in gyro_data():
        message = json.dumps({"gyro": gyro})
        await broadcast(message)

async def broadcast(message):
    for ws in list(clients):
        try:
            await ws.send(message)
        except:
            clients.remove(ws)

async def ws_handler(websocket, _):
    clients.add(websocket)
    try:
        await websocket.wait_closed()
    finally:
        clients.remove(websocket)

# === HTTP server for static files ===
async def start_http_server():
    app = web.Application()
    app.router.add_static('/', path='./static', show_index=True)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', 8000)
    await site.start()
    print("HTTP server running at http://172.16.9.13:8000/index.html")

# === Main ===
async def main():
    # Start HTTP server for index.html
    await start_http_server()

    # Start WebSocket server
    ws_server = await websockets.serve(ws_handler, "0.0.0.0", 8765)
    print("WebSocket server on ws://172.16.9.13:8765")

    # Start streaming sensor data
    await sensor_aggregator()

asyncio.run(main())
