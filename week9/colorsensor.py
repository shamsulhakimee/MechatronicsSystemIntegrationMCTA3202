import serial
import re
import sys
import time

SERIAL_PORT = 'COM5'
BAUD_RATE = 9600

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"✅ Connected to Arduino on {SERIAL_PORT} at {BAUD_RATE} baud.")
    time.sleep(2)
except serial.SerialException as e:
    print(f"❌ Error: Could not open {SERIAL_PORT}.")
    print(f"   Details: {e}")
    sys.exit(1)

pattern = re.compile(r'R=(\d+)\s+G=(\d+)\s+B=(\d+)\s+L=(\d+)')


def detect_color(red_val, green_val, blue_val, light_val):
    if light_val < 15:
        return "⚫ Too dark / No object"

    total = red_val + green_val + blue_val
    if total == 0:
        return "⚫ Black"

    r_norm = red_val / total
    g_norm = green_val / total
    b_norm = blue_val / total

    if r_norm > 0.6:
        return "🔴 RED"
    elif g_norm > 0.6:
        return "🟢 GREEN"
    elif b_norm > 0.6:
        return "🔵 BLUE"
    elif r_norm > 0.4 and g_norm > 0.4:
        return "🟡 YELLOW"
    elif r_norm > 0.4 and b_norm > 0.4:
        return "🟣 MAGENTA"
    elif g_norm > 0.4 and b_norm > 0.4:
        return "🟢🔵 CYAN"
    else:
        max_val = max(red_val, green_val, blue_val)
        if max_val == red_val and red_val > 20:
            return "🟥 Reddish"
        elif max_val == green_val and green_val > 20:
            return "🟩 Greenish"
        elif max_val == blue_val and blue_val > 20:
            return "🟦 Bluish"
        else:
            return "⚪ Gray / Neutral"


try:
    print("\n🔍 Listening for color data... (Press Ctrl+C to exit)\n")
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if not line:
            continue

        match = pattern.search(line)
        if match:
            r_val = int(match.group(1))
            g_val = int(match.group(2))
            b_val = int(match.group(3))
            l_val = int(match.group(4))
            color_name = detect_color(r_val, g_val, b_val, l_val)

            print(f"RGB+L: R={r_val:3} G={g_val:3} B={b_val:3} L={l_val:3} → {color_name}")

except KeyboardInterrupt:
    print("\n🛑 Stopped by user.")
except Exception as e:
    print(f"💥 Unexpected error: {e}")
finally:
    ser.close()
    print("🔌 Serial connection closed.")
