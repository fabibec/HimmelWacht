# Raspberry Pi Code

his directory contains all code that needs to be executed on the Raspberry Pi.

## Directory Structure

```
.
├── html_server/         # Python-based web server
├── mediamtx/            # Configuration for the MediaMTX media server.
├── sensors/             # Source code for the Gyroscope and Ultrasonic sensors.
├── systemd_services/    # systemd service files.
└── webRTC_sender/       # Deprecated peer-to-peer WebRTC sender script.
```

## System Setup and Configuration

### 1. Raspberry Pi Credentials

- **Username**: `hw`
- **Password**: `dt`

Access the Raspberry Pi via SSH:
```bash
ssh hw@<IP_ADDRESS>
```
