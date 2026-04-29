"""
TinyGateway.py — CoreIOT → TinyBroker (Server)
================================================
Subscribe shared attributes từ CoreIOT (ESP32-B device)
→ Forward xuống TinyBroker local (port 1884) → ESP32-B nhận

Luồng data:
  CoreIOT (cloud) nhận telemetry từ ESP32-A
  → Rule Chain chuyển thành shared attributes cho device ESP32-B
  → Script này subscribe shared attributes
  → Forward xuống local broker
  → ESP32-B subscribe local broker nhận data cảm biến

Cách chạy: python TinyGateway.py
"""

import paho.mqtt.client as mqtt
import json
import time

# ─── CẤU HÌNH ────────────────────────────────────────────
# CoreIOT (cloud) — Device ESP32-B
COREIOT_HOST = "app.coreiot.io"
COREIOT_PORT = 1883
ACCESS_TOKEN = "s7Q19PPQOkdiby7TkULz"            # ← Token của device ESP32-B trên CoreIOT

# TinyBroker local (Server side)
LOCAL_BROKER = "127.0.0.1"
LOCAL_PORT = 1884                              # Port khác Sensor broker (1883)

# Topics
COREIOT_ATTR_TOPIC = "v1/devices/me/attributes"         # Nhận shared attr
LOCAL_PUBLISH_TOPIC = "sensor/data"                      # Publish xuống ESP32-B

# ─── CLIENT ĐẾN LOCAL BROKER ─────────────────────────────
local_client = mqtt.Client("GatewayToLocal")

def connect_local():
    try:
        local_client.connect(LOCAL_BROKER, LOCAL_PORT, 60)
        local_client.loop_start()
        print(f"📡 Connected to Local Broker ({LOCAL_BROKER}:{LOCAL_PORT})")
    except Exception as e:
        print(f"❌ Cannot connect to Local Broker: {e}")

# ─── CLIENT TỪ COREIOT ───────────────────────────────────
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"☁️  Connected to CoreIOT!")
        # Subscribe shared attributes (nhận remote_temp, remote_hum)
        client.subscribe(COREIOT_ATTR_TOPIC)
        print(f"👂 Subscribed: {COREIOT_ATTR_TOPIC}")
    else:
        print(f"❌ CoreIOT connection failed (rc={rc})")

def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8")
    print(f"☁️  From CoreIOT: {payload}")

    try:
        data = json.loads(payload)

        # Trích xuất remote_temp và remote_hum từ shared attributes
        sensor_data = {}
        if "remote_temp" in data:
            sensor_data["temperature"] = data["remote_temp"]
        if "remote_hum" in data:
            sensor_data["humidity"] = data["remote_hum"]

        if sensor_data:
            forward_payload = json.dumps(sensor_data)
            result = local_client.publish(LOCAL_PUBLISH_TOPIC, forward_payload)
            if result.rc == 0:
                print(f"📡 Forwarded to ESP32-B: {forward_payload} ✅")
            else:
                print(f"❌ Forward failed (rc={result.rc})")
    except json.JSONDecodeError as e:
        print(f"❌ JSON parse error: {e}")

# ─── MAIN ────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 50)
    print("  TinyGateway (Server) — CoreIOT → ESP32-B")
    print("=" * 50)

    # Kết nối local broker trước
    connect_local()

    # Kết nối CoreIOT
    cloud = mqtt.Client("ServerGatewayFromCloud")
    cloud.username_pw_set(ACCESS_TOKEN)
    cloud.on_connect = on_connect
    cloud.on_message = on_message

    try:
        cloud.connect(COREIOT_HOST, COREIOT_PORT, 60)
        print(f"☁️  Connecting to CoreIOT ({COREIOT_HOST}:{COREIOT_PORT})...")
        cloud.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Gateway stopped.")
    finally:
        local_client.loop_stop()
        local_client.disconnect()
        cloud.disconnect()
