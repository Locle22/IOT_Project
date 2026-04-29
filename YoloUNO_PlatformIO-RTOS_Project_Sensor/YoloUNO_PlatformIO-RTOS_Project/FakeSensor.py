"""
FakeSensor.py — Giả lập ESP32-A gửi data cảm biến vào TinyBroker
==================================================================
Publish nhiệt độ + độ ẩm giả lên TinyBroker mỗi 5 giây.
Dùng khi không có ESP32-A thật.

Cách chạy: python FakeSensor.py
"""

import paho.mqtt.client as mqtt
import json
import time
import random

BROKER = "127.0.0.1"
PORT = 1883
TOPIC = "v1/devices/me/telemetry"

# Giá trị ban đầu
temperature = 25.0
humidity = 60.0

def on_connect(client, userdata, flags, rc):
    print(f"📡 Connected to TinyBroker (rc={rc})")

def on_publish(client, userdata, mid):
    pass  # Không cần log mỗi lần

client = mqtt.Client("FakeSensor-ESP32A")
client.on_connect = on_connect
client.connect(BROKER, PORT)
client.loop_start()

print("=" * 50)
print("  FakeSensor — Giả lập ESP32-A")
print("=" * 50)

try:
    while True:
        # Dao động ngẫu nhiên giống cảm biến thật
        temperature += random.uniform(-0.3, 0.5)
        humidity += random.uniform(-0.5, 0.5)
        temperature = max(20, min(40, temperature))  # Giới hạn 20-40°C
        humidity = max(40, min(90, humidity))          # Giới hạn 40-90%

        payload = json.dumps({
            "temperature": round(temperature, 1),
            "humidity": round(humidity, 1)
        })

        client.publish(TOPIC, payload)
        print(f"🌡️  Published: {payload}")
        time.sleep(5)

except KeyboardInterrupt:
    print("\n🛑 Stopped.")
finally:
    client.loop_stop()
    client.disconnect()
