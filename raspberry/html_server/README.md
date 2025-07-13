# HTML Server

This Python script starts an HTTP server that:

- Serves a static webpage which pulls a **WebRTC stream** from a MediaMTX server running on a Raspberry Pi.
- Connects to a **WebSocket** to receive bounding-box coordinates sent from the inference script running on the laboratory PC.
- Supports **real-time video annotation** in the browser using the received data.

The script creates an additional **WebSocket** which streams the real-time date of the Gyroscope and Ultrasonic sensor.


## Configuration

All network parameters (e.g., IP address of the Lab PC, port numbers) can be configured via a `.env` file.

Example `.env`:

```env
LAB_PC_IP=172.16.3.105
HTTP_PORT=8000
WEBSOCKET_PORT=8765
```

## Usage

The server is started automatically by a `systemd` service.

To apply changes (e.g., updated source code or `.env` configuration), restart the service:

```bash
sudo systemctl restart webserver.service
```

Once running, you can access the web interface at:

```
http://<your-system-ip>:<HTTP_PORT>
```

The server will log its current IP address on startup.

## Credits

The favicon used is free to use.
Source: [Missiles icon by Freepik - Flaticon](https://www.flaticon.com/free-icon/missiles_3857446?term=military&page=1&position=13&origin=tag&related_id=3857446)
