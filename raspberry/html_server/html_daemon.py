import asyncio
import json
import signal
import logging
from sensors_startup.gyro_startup import gyro_data
from sensors_startup.ultrasonic_startup import ultrasonic_data  
from aiohttp import web
import websockets

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class SensorServer:
    def __init__(self):
        self.clients = set()
        self.shutdown_event = asyncio.Event()
        
    async def broadcast(self, message):
        """Broadcast message to all connected WebSocket clients"""
        if not self.clients:
            logger.debug("No clients to broadcast to")
            return
        
        logger.debug(f"Broadcasting to {len(self.clients)} clients: {message}")
        disconnected_clients = set()
        
        for ws in list(self.clients):
            try:
                await ws.send(message)
            except websockets.exceptions.ConnectionClosed:
                logger.info("Client disconnected during broadcast")
                disconnected_clients.add(ws)
            except Exception as e:
                logger.error(f"Broadcast error: {e}")
                disconnected_clients.add(ws)
        
        # Remove disconnected clients
        self.clients -= disconnected_clients

    async def sensor_aggregator(self):
        """Aggregate sensor data and broadcast to clients"""
        logger.info("Starting sensor aggregator...")

        try:
            gyro_gen = gyro_data()
            ultra_gen = ultrasonic_data()
            logger.info("Sensor generators initialized")
        except Exception as e:
            logger.error(f"Failed to initialize sensors: {e}")
            return

        while not self.shutdown_event.is_set():
            try:
                # Get data from both sensors concurrently
                results = await asyncio.gather(
                    anext(gyro_gen),
                    anext(ultra_gen),
                    return_exceptions=True
                )
                
                message = {}
                
                # Process gyro result
                if not isinstance(results[0], Exception):
                    message["gyro"] = results[0]
                    logger.debug(f"Gyro data: {results[0]}")
                else:
                    logger.error(f"Gyro sensor error: {results[0]}")
                
                # Process ultrasonic result
                if not isinstance(results[1], Exception):
                    message["ultrasonic"] = results[1]
                    logger.debug(f"Ultrasonic data: {results[1]}")
                else:
                    logger.error(f"Ultrasonic sensor error: {results[1]}")

                if message:
                    await self.broadcast(json.dumps(message))
                else:
                    # Small delay if no data was received
                    await asyncio.sleep(0.1)

            except asyncio.CancelledError:
                logger.info("Sensor aggregator cancelled")
                break
            except StopAsyncIteration:
                logger.warning("One or both sensor generators exhausted")
                await asyncio.sleep(1)
            except Exception as e:
                logger.error(f"Error in sensor aggregator: {e}")
                await asyncio.sleep(1)

        logger.info("Sensor aggregator stopped")

    async def bbox_connection_handler(self):
        """Monitor bounding box connection"""
        BBOX_URI = "ws://172.16.3.105:8001"
        logger.info(f"Attempting connection to {BBOX_URI} (connection monitor only)...")

        while not self.shutdown_event.is_set():
            try:
                async with websockets.connect(BBOX_URI) as ws:
                    logger.info("Connected to bounding box source.")
                    while not self.shutdown_event.is_set():
                        try:
                            msg = await asyncio.wait_for(ws.recv(), timeout=5)
                            logger.debug(f"Received bbox message (length: {len(msg)})")
                            continue
                        except asyncio.TimeoutError:
                            pass  # keep alive
            except asyncio.CancelledError:
                logger.info("Bounding box connection handler cancelled.")
                break
            except Exception as e:
                logger.warning(f"Connection to bounding box source failed: {e}")
                if not self.shutdown_event.is_set():
                    await asyncio.sleep(3)

        logger.info("Bounding box connection handler stopped.")

    async def ws_handler(self, websocket, path):
        """Handle WebSocket connections"""
        self.clients.add(websocket)
        client_address = websocket.remote_address
        logger.info(f"Client connected: {client_address}, total clients: {len(self.clients)}")
        
        try:
            # Send a welcome message to test the connection
            await websocket.send(json.dumps({"status": "connected", "message": "Sensor data stream ready"}))
            
            async for message in websocket:
                # Handle any incoming messages from clients if needed
                logger.debug(f"Received message from client: {message}")
                
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Client disconnected: {client_address}")
        except Exception as e:
            logger.error(f"WebSocket handler error: {e}")
        finally:
            self.clients.discard(websocket)
            logger.info(f"Client removed: {client_address}, remaining clients: {len(self.clients)}")

    async def start_http_server(self):
        """Start HTTP server for static files"""
        app = web.Application()
        #app.router.add_static('/', path='./static', show_index=True)
        app.router.add_get('/favicon.ico', lambda request:web.FileResponse('./static/favicon.ico'))
        app.router.add_get('/', lambda request: web.FileResponse('./static/index.html'))
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, '0.0.0.0', 8000)
        await site.start()
        logger.info("HTTP server at http://172.16.9.13:8000/index.html")
        return runner

    async def start_websocket_server(self):
        """Start WebSocket server"""
        ws_server = await websockets.serve(self.ws_handler, "0.0.0.0", 8765)
        logger.info("WebSocket server at ws://172.16.9.13:8765")
        return ws_server

    def signal_handler(self):
        """Handle shutdown signals"""
        logger.info("Received shutdown signal")
        self.shutdown_event.set()

    async def run(self):
        """Main run method"""
        loop = asyncio.get_running_loop()
        for sig in [signal.SIGINT, signal.SIGTERM]:
            loop.add_signal_handler(sig, self.signal_handler)

        try:
            http_runner = await self.start_http_server()
            ws_server = await self.start_websocket_server()

            sensor_task = asyncio.create_task(self.sensor_aggregator())
            bbox_task = asyncio.create_task(self.bbox_connection_handler())

            logger.info("All services started. Press Ctrl+C to stop.")
            await self.shutdown_event.wait()
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

async def main():
    server = SensorServer()
    await server.run()

if __name__ == "__main__":
    asyncio.run(main())