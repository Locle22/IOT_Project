import serial
import csv
import time
from datetime import datetime

COM_PORT = "COM15"  # Change to ESP32-B port
BAUD_RATE = 115200
CSV_FILENAME = "model_run_result.csv"
LOG_PREFIX = "[TINYML_LOG]"

def setup_csv():
    # Write header if file is new/empty
    try:
        with open(CSV_FILENAME, mode='x', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                "timestamp", "temperature", "humidity", "temp_rate", "humi_rate",
                "predicted_label", "real_label", "confidence_score", 
                "prob_bg", "prob_nuisance", "prob_fire", 
                "inference_time_ms", "memory_usage_bytes"
            ])
            print(f"Created new log file: {CSV_FILENAME}")
    except FileExistsError:
        print(f"Appending to existing file: {CSV_FILENAME}")

def main():
    setup_csv()
    
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
        print(f"Listening on {COM_PORT} at {BAUD_RATE} baud...")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        return

    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Filter only TinyML logs
                if line.startswith(LOG_PREFIX):
                    try:
                        # Extract data (skip the prefix)
                        # Format: [TINYML_LOG],temp,hum,temp_rate,humi_rate,pred_class,conf,p_bg,p_nuisance,p_fire,duration,arena
                        parts = line.split(',')
                        
                        if len(parts) == 12:
                            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                            temp = parts[1]
                            humi = parts[2]
                            temp_rate = parts[3]
                            humi_rate = parts[4]
                            pred_class = parts[5]
                            real_label = ""  
                            conf = parts[6]
                            p_bg = parts[7]
                            p_nuisance = parts[8]
                            p_fire = parts[9]
                            duration = parts[10]
                            arena = parts[11]
                            
                            # Write to CSV
                            with open(CSV_FILENAME, mode='a', newline='') as f:
                                writer = csv.writer(f)
                                writer.writerow([
                                    timestamp, temp, humi, temp_rate, humi_rate, pred_class, real_label, 
                                    conf, p_bg, p_nuisance, p_fire, duration, arena
                                ])
                            
                            print(f"[{timestamp}] Logged: T:{temp}C, H:{humi}%, Delta_temp:{temp_rate}C/s, Delta_humi:{humi_rate}%/s, Pred:{pred_class} (Conf:{conf})")
                            
                    except Exception as e:
                        print(f"Parse error: {e} | Raw line: {line}")
                        
    except KeyboardInterrupt:
        print("\nLogging stopped by user.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()