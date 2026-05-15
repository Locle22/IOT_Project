"""
FakeSensor.py — Lấy nhiệt độ & độ ẩm THỰC TẾ tại Gò Vấp, TP.HCM
===================================================================
Dùng Open-Meteo API (miễn phí, không cần API key)
Publish lên TinyBroker mỗi 5 giây.

Cách chạy: python FakeSensor.py
"""

import paho.mqtt.client as mqtt
import json
import time
import requests

BROKER = "127.0.0.1"
PORT = 1883
TOPIC = "v1/devices/me/telemetry"

# ─── Tọa độ Gò Vấp, TP.HCM ──────────────────────────────────────
LATITUDE = 10.8326
LONGITUDE = 106.6581
WEATHER_URL = (
    f"https://api.open-meteo.com/v1/forecast"
    f"?latitude={LATITUDE}&longitude={LONGITUDE}"
    f"&current=temperature_2m,relative_humidity_2m"
    f"&timezone=Asia/Ho_Chi_Minh"
)

# Cache thời tiết (API free giới hạn request, nên cache 60s)
cached_temp = None
cached_hum = None
last_fetch = 0
CACHE_DURATION = 60  # giây

def fetch_weather():
    """Gọi Open-Meteo API lấy nhiệt độ & độ ẩm hiện tại"""
    global cached_temp, cached_hum, last_fetch
    
    now = time.time()
    if cached_temp is not None and (now - last_fetch) < CACHE_DURATION:
        return cached_temp, cached_hum
    
    try:
        resp = requests.get(WEATHER_URL, timeout=10)
        resp.raise_for_status()
        data = resp.json()
        
        current = data.get("current", {})
        cached_temp = current.get("temperature_2m", 30.0)
        cached_hum = current.get("relative_humidity_2m", 70.0)
        last_fetch = now
        
        print(f"🌐 Weather API: {cached_temp}°C, {cached_hum}%RH "
              f"(Gò Vấp, TP.HCM)")
        
    except Exception as e:
        print(f"⚠️  API error: {e} — dùng giá trị cache")
        if cached_temp is None:
            cached_temp = 30.0
            cached_hum = 70.0
    
    return cached_temp, cached_hum

def on_connect(client, userdata, flags, rc):
    print(f"📡 Connected to TinyBroker (rc={rc})")

client = mqtt.Client("FakeSensor-ESP32A")
client.on_connect = on_connect
client.connect(BROKER, PORT)
client.loop_start()

print("=" * 55)
print("  FakeSensor — Dữ liệu thực tế Gò Vấp, TP.HCM")
print("  Source: Open-Meteo API (free, no API key)")
print("=" * 55)

try:
    while True:
        temp, hum = fetch_weather()
        
        # Thêm nhiễu nhẹ giống cảm biến thật (±0.2°C, ±0.5%RH)
        import random
        temp_noisy = round(temp + random.uniform(-0.2, 0.2), 1)
        hum_noisy  = round(hum  + random.uniform(-0.5, 0.5), 1)
        
        payload = json.dumps({
            "temperature": temp_noisy,
            "humidity": hum_noisy
        })
        
        client.publish(TOPIC, payload)
        print(f"🌡️  Published: {payload}")
        time.sleep(5)

except KeyboardInterrupt:
    print("\n🛑 Stopped.")
finally:
    client.loop_stop()
    client.disconnect()
