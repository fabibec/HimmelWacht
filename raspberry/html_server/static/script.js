// created with help of AI
// @author: Nicolas Koch

// This function fetches the IP and port configuration from the servers /config endpoint.
async function fetchConfig() {
    try {
        const response = await fetch('/config');
        if (!response.ok) {
            throw new Error(`Failed to fetch config: ${response.status}`);
        }
        return await response.json();
    } catch (error) {
        console.error("Could not fetch config:", error);
        // Fallback or error handling
        return {
            RASPBERRY_IP: 'localhost',
            WEBSOCKET_PORT: 8765,
            LAB_PC_IP: 'localhost',
            BOUNDING_BOX_PORT: 8001
        };
    }
}

// This function initializes the WebRTC connection, WebSocket connections, and canvas drawing.
async function initialize() {
    const config = await fetchConfig();

    const PORTS = {
        WEBRTC: 8889,
        BOUNDING_BOX: config.BOUNDING_BOX_PORT,
        SENSORS: config.WEBSOCKET_PORT,
    };

    // Construct full URLs
    const CONFIG = {
        WEBRTC_SERVER: `http://${config.RASPBERRY_IP}:${PORTS.WEBRTC}/stream/whep`,
        BBOX_WS_URL: `ws://${config.LAB_PC_IP}:${PORTS.BOUNDING_BOX}`,
        SENSOR_WS_URL: `ws://${config.RASPBERRY_IP}:${PORTS.SENSORS}`,
    };

    const video = document.getElementById('video');
    const canvas = document.getElementById('overlay');
    const ctx = canvas.getContext('2d');
    const loading = document.getElementById('loading');
    const gyroOutput = document.getElementById('gyro-output');

    // Status elements
    const videoStatus = document.getElementById('video-status');
    const bboxStatus = document.getElementById('bbox-status');
    const sensorStatus = document.getElementById('sensor-status');

    // Scale factors for coordinate conversion
    let scaleX = 1;
    let scaleY = 1;

    // Store current bounding boxes and ultrasonic data
    let currentBoxes = [];
    let currentUltrasonicData = null;

    function updateCanvasSize() {
        const rect = video.getBoundingClientRect();
        canvas.width = rect.width;
        canvas.height = rect.height;

        // Calculate scale factors for coordinate conversion
        scaleX = canvas.width / 1280;  // Original video width
        scaleY = canvas.height / 1080; // Original video height
    }

    video.onplaying = () => {
        loading.style.display = 'none';
        videoStatus.textContent = 'Video: Connected';
        videoStatus.className = 'status-item connection-active';
        updateCanvasSize();
        drawCrosshairLoop();
    };

    video.onloadedmetadata = () => {
        updateCanvasSize();
    };

    // Resize handler
    window.addEventListener('resize', updateCanvasSize);

    // === WebRTC via MediaMTX ===
    const peer = new RTCPeerConnection({
        iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    peer.oniceconnectionstatechange = () => {
        console.log('ICE State:', peer.iceConnectionState);
        if (peer.iceConnectionState === 'connected' || peer.iceConnectionState === 'completed') {
            videoStatus.textContent = 'Video: Connected';
            videoStatus.className = 'status-item connection-active';
        } else if (peer.iceConnectionState === 'disconnected' || peer.iceConnectionState === 'failed') {
            videoStatus.textContent = 'Video: Disconnected';
            videoStatus.className = 'status-item connection-inactive';
        }
    };

    peer.ontrack = (event) => {
        console.log("Track received:", event.track.kind);
        if (event.streams && event.streams[0]) {
            video.srcObject = event.streams[0];
        } else {
            const stream = new MediaStream([event.track]);
            video.srcObject = stream;
        }
        video.play().catch(err => {
            console.warn("Autoplay error:", err);
        });
    };

    async function connectWebRTC() {
        try {
            const offer = await peer.createOffer({ offerToReceiveVideo: true });
            await peer.setLocalDescription(offer);

            const res = await fetch(CONFIG.WEBRTC_SERVER, {
                method: 'POST',
                headers: { 'Content-Type': 'application/sdp' },
                body: offer.sdp
            });

            if (!res.ok) {
                throw new Error(`WHEP request failed: ${res.status}`);
            }

            const answerSdp = await res.text();
            await peer.setRemoteDescription({
                type: 'answer',
                sdp: answerSdp
            });

            console.log("WebRTC connection established.");
        } catch (err) {
            console.error("WebRTC connection failed:", err);
            videoStatus.textContent = 'Video: Failed';
            videoStatus.className = 'status-item connection-inactive';
        }
    }

    connectWebRTC();

    // === WebSocket for Bounding Boxes ===
    const bboxSocket = new WebSocket(CONFIG.BBOX_WS_URL);

    bboxSocket.onopen = () => {
        bboxStatus.textContent = 'Boxes: Connected';
        bboxStatus.className = 'status-item connection-active';
    };

    bboxSocket.onclose = () => {
        bboxStatus.textContent = 'Boxes: Disconnected';
        bboxStatus.className = 'status-item connection-inactive';
    };

    bboxSocket.onmessage = function (event) {
        try {
            const data = JSON.parse(event.data);
            if (data.boxes) {
                currentBoxes = data.boxes;
            }
        } catch (err) {
            console.error('Invalid JSON from bounding box server:', err);
        }
    };

    function drawCrosshair() {
        const centerX = canvas.width / 2;
        const centerY = canvas.height / 2;
        const size = 20;

        // Outer glow
        ctx.shadowColor = '#00ff00';
        ctx.shadowBlur = 10;
        ctx.strokeStyle = '#00ff00';
        ctx.lineWidth = 3;

        ctx.beginPath();
        ctx.moveTo(centerX - size, centerY);
        ctx.lineTo(centerX + size, centerY);
        ctx.moveTo(centerX, centerY - size);
        ctx.lineTo(centerX, centerY + size);
        ctx.stroke();

        // Inner crosshair
        ctx.shadowBlur = 0;
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 1;
        ctx.stroke();
    }

    function drawCrosshairLoop() {
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Always draw crosshair
        drawCrosshair();

        // Draw current bounding boxes if any (only show distance when boxes are present)
        if (currentBoxes && currentBoxes.length > 0) {
            // Draw bounding boxes with proper scaling
            ctx.shadowColor = '#00ff00';
            ctx.shadowBlur = 5;
            ctx.strokeStyle = '#00ff00';
            ctx.lineWidth = 2;
            ctx.font = '12px monospace';
            ctx.fillStyle = '#00ff00';

            currentBoxes.forEach(box => {
                // Scale coordinates to match display size
                const scaledX = box.x * scaleX;
                const scaledY = box.y * scaleY;
                const scaledWidth = box.width * scaleX;
                const scaledHeight = box.height * scaleY;

                // Draw box
                ctx.strokeRect(scaledX, scaledY, scaledWidth, scaledHeight);

                // Draw label with background
                if (box.label) {
                    const textWidth = ctx.measureText(box.label).width;
                    ctx.fillStyle = 'rgba(0, 255, 0, 0.8)';
                    ctx.fillRect(scaledX, scaledY - 20, textWidth + 8, 16);
                    ctx.fillStyle = '#000000';
                    ctx.fillText(box.label, scaledX + 4, scaledY - 8);
                    ctx.fillStyle = '#00ff00';
                }

                // Draw ultrasonic distance below the box ONLY if bounding box is present AND ultrasonic data is available
                if (currentUltrasonicData !== null && scaledHeight !== 0 && scaledWidth !== 0) {
                    const distanceText = `${currentUltrasonicData}`;
                    const distanceTextWidth = ctx.measureText(distanceText).width;
                    const distanceY = scaledY + scaledHeight + 20;

                    // Background for distance text
                    ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
                    ctx.fillRect(scaledX, distanceY - 16, distanceTextWidth + 8, 16);

                    // Distance text
                    ctx.fillStyle = 'rgba(0, 255, 0, 0.8)';
                    ctx.fillText(distanceText, scaledX + 4, distanceY - 4);
                    ctx.fillStyle = '#00ff00';
                }
            });
        }

        requestAnimationFrame(drawCrosshairLoop);
    }

    // === WebSocket for Sensor Data (Gyro + Ultrasonic) ===
    const sensorSocket = new WebSocket(CONFIG.SENSOR_WS_URL);

    sensorSocket.onopen = () => {
        sensorStatus.textContent = 'Sensors: Connected';
        sensorStatus.className = 'status-item connection-active';
    };

    sensorSocket.onclose = () => {
        sensorStatus.textContent = 'Sensors: Disconnected';
        sensorStatus.className = 'status-item connection-inactive';
    };

    sensorSocket.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);

            if (data.gyro !== undefined) {
                gyroOutput.textContent = `Geschützneigung: ${data.gyro}`;
            }

            // Store ultrasonic data but don't display it in a separate box
            if (data.ultrasonic !== undefined) {
                currentUltrasonicData = data.ultrasonic;
            }
        } catch (err) {
            console.error('Invalid JSON from sensor server:', err);
        }
    };

    sensorSocket.onerror = () => {
        gyroOutput.textContent = "Could not connect to sensor server.";
        sensorStatus.textContent = 'Sensors: Error';
        sensorStatus.className = 'status-item connection-inactive';
    };
}

document.addEventListener('DOMContentLoaded', initialize);
