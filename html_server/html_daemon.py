import asyncio
import json
import signal
import logging
from sensors.gyro_startup import gyro_data
from aiohttp import web
import websockets

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Track WebSocket clients
clients = set()

# Global shutdown flag
shutdown_event = asyncio.Event()

# === Sensor Aggregator ===
async def sensor_aggregator():
    logger.info("Starting sensor aggregator...")
    while not shutdown_event.is_set():
        try:
            async for gyro in gyro_data():
                if shutdown_event.is_set():
                    break
                message = json.dumps({"gyro": gyro})
                await broadcast(message)
        except asyncio.CancelledError:
            logger.info("Sensor aggregator cancelled")
            break
        except Exception as e:
            logger.error(f"Error in sensor aggregator: {e}")
            await asyncio.sleep(1)
    logger.info("Sensor aggregator stopped")

# === Bounding Box Connection Monitor Only ===
BBOX_URI = "ws://172.16.3.105:8001"

async def bbox_connection_handler():
    logger.info(f"Attempting connection to {BBOX_URI} (connection monitor only)...")

    while not shutdown_event.is_set():
        try:
            async with websockets.connect(BBOX_URI) as ws:
                logger.info("Connected to bounding box source.")
                while not shutdown_event.is_set():
                    try:
                        msg = await asyncio.wait_for(ws.recv(), timeout=5)
                        # Optional: just log or discard message
                        continue
                    except asyncio.TimeoutError:
                        pass  # keep alive
        except asyncio.CancelledError:
            logger.info("Bounding box connection handler cancelled.")
            break
        except Exception as e:
            logger.warning(f"Connection to bounding box source failed: {e}")
            await asyncio.sleep(3)

    logger.info("Bounding box connection handler stopped.")


# === Broadcast to WebSocket Clients ===
async def broadcast(message):
    if not clients:
        return
    for ws in list(clients):
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            clients.discard(ws)
        except Exception as e:
            logger.error(f"Broadcast error: {e}")
            clients.discard(ws)

# === WebSocket Server Handler ===
async def ws_handler(websocket, path):
    clients.add(websocket)
    client_address = websocket.remote_address
    logger.info(f"Client connected: {client_address}")
    try:
        async for _ in websocket:
            pass
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client disconnected: {client_address}")
    except Exception as e:
        logger.error(f"WebSocket handler error: {e}")
    finally:
        clients.discard(websocket)

# === HTTP Server for Static Files ===
async def start_http_server():
    app = web.Application()
    app.router.add_static('/', path='./static', show_index=True)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', 8000)
    await site.start()
    logger.info("HTTP server at http://172.16.9.13:8000/index.html")
    return runner

async def start_websocket_server():
    ws_server = await websockets.serve(ws_handler, "0.0.0.0", 8765)
    logger.info("WebSocket server at ws://172.16.9.13:8765")
    return ws_server

# === Shutdown Handling ===
def signal_handler():
    logger.info("Received shutdown signal")
    shutdown_event.set()

# === Main Entry ===
async def main():
    loop = asyncio.get_running_loop()
    for sig in [signal.SIGINT, signal.SIGTERM]:
        loop.add_signal_handler(sig, signal_handler)

    try:
        http_runner = await start_http_server()
        ws_server = await start_websocket_server()

        sensor_task = asyncio.create_task(sensor_aggregator())
        bbox_task = asyncio.create_task(bbox_connection_handler())


        logger.info("All services started. Press Ctrl+C to stop.")
        await shutdown_event.wait()
        logger.info("Shutting down...")

        sensor_task.cancel()
        bbox_task.cancel()

        ws_server.close()
        await ws_server.wait_closed()
        await http_runner.cleanup()

        try:
            await sensor_task
            await bbox_task
        except asyncio.CancelledError:
            pass

        logger.info("Shutdown complete.")
    except Exception as e:
        logger.error(f"Main error: {e}")
        raise

if __name__ == "__main__":
    asyncio.run(main())
