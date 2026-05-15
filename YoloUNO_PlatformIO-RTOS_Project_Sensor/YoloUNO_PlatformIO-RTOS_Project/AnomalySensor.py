"""
AnomalySensor.py — Mô phỏng dữ liệu bất thường về nhiệt độ & độ ẩm
=====================================================================
Gửi dữ liệu lên TinyBroker (localhost:1883) qua 4 giai đoạn:

  ┌─────────────────────────────────────────────────────────────────┐
  │ Phase 1: 🔥 Nhiệt độ tăng nhanh đột ngột (Rapid Spike)       │
  │ Phase 2: 🌡️  Nhiệt độ cao kéo dài (Sustained High Temp)      │
  │ Phase 3: 🏜️  Độ ẩm thấp – Nguy cơ cháy (Fire Risk)          │
  │ Phase 4: 🌊 Dấu hiệu bão lũ (Storm/Flood Indicators)        │
  └─────────────────────────────────────────────────────────────────┘

Cách chạy: python AnomalySensor.py
"""

import paho.mqtt.client as mqtt
import json
import time
import random
import math
import sys

# ─── CẤU HÌNH MQTT ───────────────────────────────────────────────
BROKER = "127.0.0.1"
PORT = 1883
TOPIC = "v1/devices/me/telemetry"

# ─── CẤU HÌNH THỜI GIAN ─────────────────────────────────────────
PUBLISH_INTERVAL = 3       # Giây giữa mỗi lần gửi
TRANSITION_STEPS = 5       # Số bước chuyển tiếp giữa các phase

# ─── MÀU SẮC TERMINAL ────────────────────────────────────────────
class Color:
    HEADER  = '\033[95m'
    BLUE    = '\033[94m'
    CYAN    = '\033[96m'
    GREEN   = '\033[92m'
    YELLOW  = '\033[93m'
    RED     = '\033[91m'
    BOLD    = '\033[1m'
    DIM     = '\033[2m'
    END     = '\033[0m'

# ═══════════════════════════════════════════════════════════════════
#  ĐỊNH NGHĨA 4 GIAI ĐOẠN BẤT THƯỜNG
# ═══════════════════════════════════════════════════════════════════

PHASES = [
    # ── Phase 1: Nhiệt độ tăng nhanh đột ngột ────────────────────
    {
        "name": "🔥 RAPID TEMPERATURE SPIKE",
        "description": "Nhiệt độ tăng từ bình thường (30°C) lên 55°C trong thời gian ngắn",
        "duration_steps": 20,
        "generator": lambda step, total: {
            # Tăng phi tuyến (exponential-like) từ 30 → 55°C
            "temperature": round(
                30.0 + 25.0 * (1 - math.exp(-3.5 * step / total))
                + random.uniform(-0.3, 0.5),  # Nhiễu lệch dương khi tăng
                1
            ),
            # Độ ẩm giảm nhẹ khi nhiệt độ tăng
            "humidity": round(
                65.0 - 15.0 * (step / total)
                + random.uniform(-1.0, 1.0),
                1
            ),
        },
        "color": Color.RED,
        "alert_icon": "🔺",
    },

    # ── Phase 2: Nhiệt độ cao kéo dài ────────────────────────────
    {
        "name": "🌡️  SUSTAINED HIGH TEMPERATURE",
        "description": "Nhiệt độ duy trì 50-58°C – vượt ngưỡng nguy hiểm kéo dài",
        "duration_steps": 25,
        "generator": lambda step, total: {
            # Dao động quanh 54°C với biên độ ±4°C
            "temperature": round(
                54.0
                + 4.0 * math.sin(2 * math.pi * step / 8)     # Dao động chậm
                + random.uniform(-0.5, 0.5),
                1
            ),
            # Độ ẩm thấp, dao động 30-40%
            "humidity": round(
                35.0
                + 5.0 * math.sin(2 * math.pi * step / 6)
                + random.uniform(-1.0, 1.0),
                1
            ),
        },
        "color": Color.RED,
        "alert_icon": "⚠️",
    },

    # ── Phase 3: Độ ẩm cực thấp – Nguy cơ cháy ──────────────────
    {
        "name": "🏜️  LOW HUMIDITY — FIRE RISK",
        "description": "Độ ẩm giảm xuống 8-15%, nhiệt độ 42-48°C → nguy cơ cháy rừng",
        "duration_steps": 25,
        "generator": lambda step, total: {
            # Nhiệt độ cao 42-48°C
            "temperature": round(
                45.0
                + 3.0 * math.sin(2 * math.pi * step / 10)
                + random.uniform(-0.3, 0.3),
                1
            ),
            # Độ ẩm cực thấp: giảm từ 25% → 8%
            "humidity": round(
                max(8.0,
                    25.0 - 17.0 * (step / total)
                    + random.uniform(-1.5, 0.5)  # Nhiễu lệch âm
                ),
                1
            ),
        },
        "color": Color.YELLOW,
        "alert_icon": "🔥",
    },

    # ── Phase 4: Dấu hiệu bão lũ ────────────────────────────────
    {
        "name": "🌊 STORM / FLOOD INDICATORS",
        "description": "Nhiệt độ giảm nhanh, độ ẩm tăng vọt >95% → áp thấp nhiệt đới",
        "duration_steps": 30,
        "generator": lambda step, total: {
            # Nhiệt độ giảm nhanh từ 35°C → 18°C (front lạnh)
            "temperature": round(
                35.0 - 17.0 * (step / total)
                + random.uniform(-0.8, 0.8)     # Gió mạnh → nhiễu lớn
                - 2.0 * math.sin(4 * math.pi * step / total),  # Biến động do gió giật
                1
            ),
            # Độ ẩm tăng vọt từ 70% → 99%
            "humidity": round(
                min(99.9,
                    70.0 + 29.0 * (1 - math.exp(-3.0 * step / total))
                    + random.uniform(-0.5, 1.5)  # Nhiễu lệch dương
                ),
                1
            ),
        },
        "color": Color.BLUE,
        "alert_icon": "🌧️",
    },
]


# ═══════════════════════════════════════════════════════════════════
#  HÀM CHUYỂN TIẾP GIỮA CÁC PHASE
# ═══════════════════════════════════════════════════════════════════

def lerp(a, b, t):
    """Linear interpolation"""
    return a + (b - a) * t


def generate_transition(prev_temp, prev_hum, next_phase, steps=TRANSITION_STEPS):
    """Tạo dữ liệu chuyển tiếp mượt giữa 2 phase"""
    # Lấy giá trị đầu tiên của phase tiếp theo
    target = next_phase["generator"](0, next_phase["duration_steps"])
    target_temp = target["temperature"]
    target_hum = target["humidity"]

    data_points = []
    for i in range(steps):
        t = (i + 1) / steps
        data_points.append({
            "temperature": round(lerp(prev_temp, target_temp, t) + random.uniform(-0.2, 0.2), 1),
            "humidity": round(lerp(prev_hum, target_hum, t) + random.uniform(-0.3, 0.3), 1),
        })
    return data_points


# ═══════════════════════════════════════════════════════════════════
#  HIỂN THỊ DASHBOARD
# ═══════════════════════════════════════════════════════════════════

def print_header():
    print(f"\n{Color.BOLD}{'═' * 60}")
    print(f"  AnomalySensor — Mô phỏng dữ liệu thời tiết bất thường")
    print(f"  Target: TinyBroker ({BROKER}:{PORT})")
    print(f"  Topic:  {TOPIC}")
    print(f"{'═' * 60}{Color.END}\n")


def print_phase_banner(phase, phase_num):
    color = phase["color"]
    print(f"\n{color}{Color.BOLD}{'─' * 60}")
    print(f"  PHASE {phase_num}/4: {phase['name']}")
    print(f"  {phase['description']}")
    print(f"{'─' * 60}{Color.END}")


def print_data(step, total, temp, hum, phase, is_transition=False):
    """In dữ liệu kèm cảnh báo trực quan"""
    icon = phase["alert_icon"]
    color = phase["color"]
    label = "TRANSITION" if is_transition else f"Step {step}/{total}"

    # Thanh tiến trình
    progress = int(20 * step / total) if total > 0 else 0
    bar = f"[{'█' * progress}{'░' * (20 - progress)}]"

    # Xác định mức cảnh báo nhiệt độ
    if temp >= 50:
        temp_alert = f"{Color.RED}🔴 CRITICAL"
    elif temp >= 42:
        temp_alert = f"{Color.RED}🟠 DANGER"
    elif temp >= 35:
        temp_alert = f"{Color.YELLOW}🟡 WARNING"
    elif temp <= 20:
        temp_alert = f"{Color.BLUE}🔵 COLD FRONT"
    else:
        temp_alert = f"{Color.GREEN}🟢 NORMAL"

    # Xác định mức cảnh báo độ ẩm
    if hum <= 15:
        hum_alert = f"{Color.RED}🔴 FIRE RISK"
    elif hum <= 30:
        hum_alert = f"{Color.YELLOW}🟡 DRY"
    elif hum >= 95:
        hum_alert = f"{Color.BLUE}🔵 FLOOD RISK"
    elif hum >= 85:
        hum_alert = f"{Color.CYAN}🔵 VERY HUMID"
    else:
        hum_alert = f"{Color.GREEN}🟢 NORMAL"

    print(
        f"  {color}{icon} {label:>15}{Color.END}  "
        f"{bar}  "
        f"T={temp:5.1f}°C {temp_alert}{Color.END}  │  "
        f"H={hum:5.1f}% {hum_alert}{Color.END}"
    )


def print_summary(all_data):
    """In tổng kết cuối"""
    temps = [d["temperature"] for d in all_data]
    hums = [d["humidity"] for d in all_data]

    print(f"\n{Color.BOLD}{'═' * 60}")
    print(f"  📊 TỔNG KẾT MÔ PHỎNG")
    print(f"{'═' * 60}{Color.END}")
    print(f"  📦 Tổng số message:  {len(all_data)}")
    print(f"  🌡️  Nhiệt độ:  min={min(temps):.1f}°C  max={max(temps):.1f}°C  avg={sum(temps)/len(temps):.1f}°C")
    print(f"  💧 Độ ẩm:      min={min(hums):.1f}%    max={max(hums):.1f}%    avg={sum(hums)/len(hums):.1f}%")
    print(f"  ⏱️  Thời gian:   ~{len(all_data) * PUBLISH_INTERVAL}s ({len(all_data) * PUBLISH_INTERVAL / 60:.1f} phút)")
    print()


# ═══════════════════════════════════════════════════════════════════
#  MQTT CALLBACKS
# ═══════════════════════════════════════════════════════════════════

def on_connect(client, userdata, flags, rc):
    status = {
        0: "✅ Connected successfully",
        1: "❌ Incorrect protocol version",
        2: "❌ Invalid client identifier",
        3: "❌ Server unavailable",
        4: "❌ Bad username or password",
        5: "❌ Not authorized",
    }
    print(f"  📡 TinyBroker: {status.get(rc, f'Unknown rc={rc}')}")


def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"  ⚠️  Disconnected unexpectedly (rc={rc})")


# ═══════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════

def main():
    print_header()

    # ── Kết nối MQTT ──────────────────────────────────────────────
    client = mqtt.Client("AnomalySensor-Simulator")
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect

    try:
        client.connect(BROKER, PORT)
        client.loop_start()
    except Exception as e:
        print(f"  ❌ Cannot connect to TinyBroker: {e}")
        print(f"  💡 Hãy chạy TinyBroker.py trước!")
        sys.exit(1)

    time.sleep(1)  # Chờ kết nối ổn định

    all_data = []           # Lưu tất cả dữ liệu đã gửi
    msg_count = 0
    prev_temp = 30.0        # Nhiệt độ bình thường ban đầu
    prev_hum = 65.0

    try:
        # ── Gửi vài data bình thường trước ────────────────────────
        print(f"\n  {Color.GREEN}📋 Khởi tạo baseline — dữ liệu bình thường...{Color.END}")
        for i in range(5):
            temp = round(30.0 + random.uniform(-1.0, 1.0), 1)
            hum = round(65.0 + random.uniform(-2.0, 2.0), 1)

            payload = json.dumps({"temperature": temp, "humidity": hum})
            client.publish(TOPIC, payload)
            all_data.append({"temperature": temp, "humidity": hum})
            msg_count += 1

            print(f"  🟢 Baseline {i+1}/5  │  T={temp:.1f}°C  H={hum:.1f}%")
            prev_temp, prev_hum = temp, hum
            time.sleep(PUBLISH_INTERVAL)

        # ── Chạy qua 4 Phase ─────────────────────────────────────
        for phase_idx, phase in enumerate(PHASES):
            phase_num = phase_idx + 1
            total_steps = phase["duration_steps"]

            print_phase_banner(phase, phase_num)

            # ── Chuyển tiếp mượt từ phase trước ──────────────────
            print(f"  {Color.DIM}↘ Transitioning...{Color.END}")
            transition_data = generate_transition(prev_temp, prev_hum, phase)
            for t_data in transition_data:
                payload = json.dumps(t_data)
                client.publish(TOPIC, payload)
                all_data.append(t_data)
                msg_count += 1

                print_data(
                    0, total_steps,
                    t_data["temperature"], t_data["humidity"],
                    phase, is_transition=True
                )
                time.sleep(PUBLISH_INTERVAL)

            # ── Dữ liệu chính của phase ──────────────────────────
            for step in range(total_steps):
                data = phase["generator"](step, total_steps)
                temp = data["temperature"]
                hum = data["humidity"]

                payload = json.dumps({"temperature": temp, "humidity": hum})
                result = client.publish(TOPIC, payload)
                all_data.append({"temperature": temp, "humidity": hum})
                msg_count += 1

                print_data(step + 1, total_steps, temp, hum, phase)

                if result.rc != 0:
                    print(f"  {Color.RED}❌ Publish failed (rc={result.rc}){Color.END}")

                prev_temp, prev_hum = temp, hum
                time.sleep(PUBLISH_INTERVAL)

            print(f"\n  {Color.GREEN}✅ Phase {phase_num} completed — {total_steps} data points sent{Color.END}")

        # ── Trở về bình thường ────────────────────────────────────
        print(f"\n  {Color.GREEN}📋 Trở về trạng thái bình thường...{Color.END}")
        for i in range(5):
            t = (i + 1) / 5
            temp = round(lerp(prev_temp, 30.0, t) + random.uniform(-0.5, 0.5), 1)
            hum = round(lerp(prev_hum, 65.0, t) + random.uniform(-1.0, 1.0), 1)

            payload = json.dumps({"temperature": temp, "humidity": hum})
            client.publish(TOPIC, payload)
            all_data.append({"temperature": temp, "humidity": hum})
            msg_count += 1

            print(f"  🟢 Recovery {i+1}/5  │  T={temp:.1f}°C  H={hum:.1f}%")
            prev_temp, prev_hum = temp, hum
            time.sleep(PUBLISH_INTERVAL)

        # ── Tổng kết ─────────────────────────────────────────────
        print_summary(all_data)
        print(f"  {Color.GREEN}{Color.BOLD}🎉 Simulation completed successfully!{Color.END}")
        print(f"  📦 Total messages published: {msg_count}")

    except KeyboardInterrupt:
        print(f"\n\n  {Color.YELLOW}🛑 Simulation interrupted by user.{Color.END}")
        if all_data:
            print_summary(all_data)

    finally:
        client.loop_stop()
        client.disconnect()
        print(f"  📡 Disconnected from TinyBroker.\n")


if __name__ == "__main__":
    main()
