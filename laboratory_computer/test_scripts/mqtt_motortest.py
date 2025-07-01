import paho.mqtt.client as mqtt
import argparse
import json

def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ("yes", "true", "t", "1"):
        return True
    elif v.lower() in ("no", "false", "f", "0"):
        return False
    else:
        raise argparse.ArgumentTypeError("Boolean value expected.")

parser = argparse.ArgumentParser(description="Send turret commands via MQTT.")
parser.add_argument("--x_angle", type=int, required=True, help="Platform X angle")
parser.add_argument("--y_angle", type=int, required=True, help="Platform Y angle")
parser.add_argument("--fire", type=str2bool, required=True, help="Fire command (True/False)")
args = parser.parse_args()


# MQTT client setup
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.connect("127.0.0.1", 1883, 60)


# Payload from parsed arguments
payload = {
    "platform_x_angle": args.x_angle,
    "platform_y_angle": args.y_angle,
    "fire_command": args.fire
}

# Function to publish message
def publish():
    client.publish("vehicle/turret/cmd", json.dumps(payload))


publish()
