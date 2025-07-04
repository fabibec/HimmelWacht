# created with help of AI
# @author: Nicolas Koch

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
        self.main_tasks = []

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

    async def _sensor_reader_task(self, sensor_name, sensor_generator):
        """Reads from a sensor and broadcasts its data."""
        logger.info(f"Starting reader task for {sensor_name} sensor...")
        try:
            async for data_str in sensor_generator:
                try:
                    # The subprocess outputs a string, attempt to parse it as JSON
                    try:
                        data = json.loads(data_str)
                    except json.JSONDecodeError:
                        # If it's not JSON, use the raw string
                        data = data_str

                    message = {sensor_name: data}
                    await self.broadcast(json.dumps(message))
                    logger.debug(f"Broadcasted {sensor_name} data: {message}")
                except Exception as e:
                    logger.error(f"Error broadcasting {sensor_name} data: {e}")
            logger.warning(f"{sensor_name} sensor generator exhausted.")
        except asyncio.CancelledError:
            logger.info(f"Reader task for {sensor_name} cancelled.")
        except Exception as e:
            logger.error(f"Error in {sensor_name} reader task: {e}")
        finally:
            logger.info(f"Reader task for {sensor_name} stopped.")

    async def sensor_aggregator(self):
        """Launches and manages independent tasks for each sensor."""
        logger.info("Starting sensor aggregator...")

        try:
            gyro_gen = gyro_data()
            ultra_gen = ultrasonic_data()
            logger.info("Sensor generators initialized")
        except Exception as e:
            logger.error(f"Failed to initialize sensors: {e}")
            return

        # Create independent tasks for each sensor
        gyro_task = asyncio.create_task(self._sensor_reader_task("gyro", gyro_gen))
        ultra_task = asyncio.create_task(self._sensor_reader_task("ultrasonic", ultra_gen))

        try:
            # This will run until the sensor_aggregator task is cancelled on shutdown
            await asyncio.gather(gyro_task, ultra_task)
        except asyncio.CancelledError:
            logger.info("Sensor aggregator's main task cancelled.")
        finally:
            logger.info("Cancelling sensor reader tasks...")
            gyro_task.cancel()
            ultra_task.cancel()
            # Wait for tasks to finish their cancellation
            await asyncio.gather(gyro_task, ultra_task, return_exceptions=True)
            logger.info("Sensor aggregator stopped.")

    async def bbox_connection_handler(self):
        """Monitor bounding box connection"""
        BBOX_URI = "ws://172.16.3.105:8001"
        logger.info(f"Attempting connection to {BBOX_URI} (connection monitor only)...")

        while True:
            try:
                async with websockets.connect(BBOX_URI) as ws:
                    logger.info("Connected to bounding box source.")
                    while True:
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
        """Handle shutdown signals by cancelling main tasks."""
        logger.info("Received shutdown signal, cancelling main tasks...")
        for task in self.main_tasks:
            task.cancel()

    async def run(self):
        """Main run method"""
        loop = asyncio.get_running_loop()
        for sig in [signal.SIGINT, signal.SIGTERM]:
            loop.add_signal_handler(sig, self.signal_handler)

        http_runner = None
        ws_server = None
        try:
            http_runner = await self.start_http_server()
            ws_server = await self.start_websocket_server()

            sensor_task = asyncio.create_task(self.sensor_aggregator())
            bbox_task = asyncio.create_task(self.bbox_connection_handler())
            self.main_tasks = [sensor_task, bbox_task]

            logger.info("All services started. Press Ctrl+C to stop.")
            await asyncio.gather(*self.main_tasks)

        except asyncio.CancelledError:
            logger.info("Main run task was cancelled.")
        except Exception as e:
            logger.error(f"Unhandled error in main run loop: {e}", exc_info=True)
        finally:
            logger.info("Shutting down all services...")

            # The signal handler has already cancelled the tasks.
            # Now, we clean up the servers.
            if ws_server:
                ws_server.close()
                await ws_server.wait_closed()
                logger.info("WebSocket server stopped.")

            if http_runner:
                await http_runner.cleanup()
                logger.info("HTTP server stopped.")

            # Wait for the main tasks to complete their cancellation.
            if self.main_tasks:
                await asyncio.gather(*self.main_tasks, return_exceptions=True)

            logger.info("Shutdown complete.")

async def main():
    server = SensorServer()
    await server.run()


if __name__ == "__main__":
    asyncio.run(main())
