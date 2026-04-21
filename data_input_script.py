import csv
import sys
from pathlib import Path
from datetime import datetime

import serial
import serial.tools.list_ports

SCRIPT_DIR = Path(__file__).resolve().parent

WRITE_CSV_PATH = SCRIPT_DIR / "dispense_data.csv"
RECORD_CSV_PATH = SCRIPT_DIR / "Record_keeping.csv"
PORT = "COM7"
BAUD_RATE = 9600


def extract_number(text):
    numeric = ""
    for ch in text.strip():
        if ch.isdigit() or ch in ".-+":
            numeric += ch
        else:
            break
    return float(numeric)



def ensure_write_csv_header(file_path):
    if not file_path.exists():
        with open(file_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["mass_g", "temperature_K", "head_height_m", "dispense_time_ms"])


def ensure_record_csv_header(file_path):
    if not file_path.exists():
        with open(file_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["timestamp", "mass_g", "temperature_K", "head_height_m", "dispense_time_ms"])


def show_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No COM ports detected.")
    else:
        print("Available COM ports:")
        for p in ports:
            print(f"  {p.device} : {p.description}")


def parse_line(line):
    """
    Expected format:
    write, xxxx.xxg, xxx.xxK, x.xxxxm, xxxx.xxms
    or
    record, xxxx.xxg, xxx.xxK, x.xxxxm, xxxx.xxms
    """
    parts = [p.strip() for p in line.split(",")]

    if len(parts) != 5:
        return None

    command = parts[0].lower()
    if command not in ("write", "record"):
        return None

    try:
        mass_g = extract_number(parts[1])
        temperature_K = extract_number(parts[2])
        head_height_m = extract_number(parts[3])
        dispense_time_ms = extract_number(parts[4])
    except ValueError:
        return None

    return command, mass_g, temperature_K, head_height_m, dispense_time_ms


def append_write_row(file_path, mass_g, temperature_K, head_height_m, dispense_time_ms):
    with open(file_path, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([mass_g, temperature_K, head_height_m, dispense_time_ms])


def append_record_row(file_path, timestamp, mass_g, temperature_K, head_height_m, dispense_time_ms):
    with open(file_path, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, mass_g, temperature_K, head_height_m, dispense_time_ms])


def main():
    print("Write CSV:", WRITE_CSV_PATH)
    print("Record CSV:", RECORD_CSV_PATH)
    ensure_write_csv_header(WRITE_CSV_PATH)
    ensure_record_csv_header(RECORD_CSV_PATH)
    show_ports()

    print(f"\nConnecting to {PORT}...")

    try:
        with serial.Serial(PORT, BAUD_RATE, timeout=1) as ser:
            print("Connected successfully.\n")
            print("Expected formats:")
            print("  write, xxxx.xxg, xxx.xxK, x.xxxxm, xxxx.xxms")
            print("  record, xxxx.xxg, xxx.xxK, x.xxxxm, xxxx.xxms\n")

            while True:
                line = ser.readline().decode("utf-8", errors="ignore").strip()

                if not line:
                    continue

                parsed = parse_line(line)
                if parsed is None:
                    print("Ignored invalid input:", line)
                    continue

                command, mass_g, temperature_K, head_height_m, dispense_time_ms = parsed
                timestamp = datetime.now().isoformat(timespec="seconds")

                if command == "write":
                    append_write_row(
                        WRITE_CSV_PATH,
                        mass_g,
                        temperature_K,
                        head_height_m,
                        dispense_time_ms
                    )
                    print(
                        f"[WRITE] "
                        f"{mass_g:.1f} g, "
                        f"{temperature_K:.1f} K, "
                        f"{head_height_m:.4f} m, "
                        f"{dispense_time_ms:.1f} ms"
                    )

                elif command == "record":
                    append_record_row(
                        RECORD_CSV_PATH,
                        timestamp,
                        mass_g,
                        temperature_K,
                        head_height_m,
                        dispense_time_ms
                    )
                    print(
                        f"[RECORD] [{timestamp}] "
                        f"{mass_g:.2f} g, "
                        f"{temperature_K:.2f} K, "
                        f"{head_height_m:.4f} m, "
                        f"{dispense_time_ms:.2f} ms"
                    )

    except KeyboardInterrupt:
        print("\nProgram stopped")

    except Exception as e:
        print("Serial error:", e)
        sys.exit(1)


if __name__ == "__main__":
    main()
