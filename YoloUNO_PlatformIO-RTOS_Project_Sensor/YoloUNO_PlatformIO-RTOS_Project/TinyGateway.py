"""
TinyGateway.py — Cầu nối TinyBroker ↔ CoreIOT
================================================
- Subscribe TinyBroker (localhost:1883) nhận data từ ESP32
- Forward lên CoreIOT (app.coreiot.io:1883) dưới dạng telemetry

Cách chạy: python TinyGateway.py
"""

import paho.mqtt.client as mqtt
import json
import time

# ─── CẤU HÌNH ────────────────────────────────────────────
# TinyBroker (local)
LOCAL_BROKER = "127.0.0.1"
LOCAL_PORT = 1883
LOCAL_TOPIC = "v1/devices/me/telemetry"   # Topic ESP32-A publish

# CoreIOT (cloud)
COREIOT_HOST = "app.coreiot.io"
COREIOT_PORT = 1883
ACCESS_TOKEN = "x6Ng41BBGZro4aULQbOu"      
COREIOT_TOPIC = "v1/devices/me/telemetry"

# ─── CLIENT LÊN COREIOT ──────────────────────────────────
cloud = mqtt.Client("GatewayToCloud")
cloud.username_pw_set(ACCESS_TOKEN)        # CoreIOT dùng token làm username

def connect_cloud():
    try:
        cloud.connect(COREIOT_HOST, COREIOT_PORT, 60)
        cloud.loop_start()
        print(f"☁️  Connected to CoreIOT ({COREIOT_HOST}:{COREIOT_PORT})")
    except Exception as e:
        print(f"❌ Cannot connect to CoreIOT: {e}")

# ─── CLIENT TỪ TINYBROKER ────────────────────────────────
def on_connect(client, userdata, flags, rc):
    print(f"📡 Connected to TinyBroker (rc={rc})")
    client.subscribe(LOCAL_TOPIC)
    print(f"👂 Listening on topic: {LOCAL_TOPIC}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8")
    print(f"📥 Received: {payload}")

    # Forward lên CoreIOT
    result = cloud.publish(COREIOT_TOPIC, payload)
    if result.rc == 0:
        print(f"☁️  Forwarded to CoreIOT ✅")
    else:
        print(f"❌ Forward failed (rc={result.rc})")

# ─── MAIN ────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 50)
    print("  TinyGateway — TinyBroker → CoreIOT")
    print("=" * 50)

    # Kết nối CoreIOT trước
    connect_cloud()

    # Kết nối TinyBroker
    local = mqtt.Client("GatewayFromLocal")
    local.on_connect = on_connect
    local.on_message = on_message

    try:
        local.connect(LOCAL_BROKER, LOCAL_PORT)
        print(f"📡 Connecting to TinyBroker ({LOCAL_BROKER}:{LOCAL_PORT})...")
        local.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Gateway stopped.")
    finally:
        cloud.loop_stop()
        cloud.disconnect()
        local.disconnect()
