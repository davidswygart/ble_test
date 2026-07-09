import re
from pathlib import Path
from typing import List, Tuple
from datetime import datetime, timedelta
import config
from itertools import dropwhile

def get_log_vals(log_file: str | Path) -> Tuple[datetime, List[float]]:
    timestamp, vals = parse_last_read_response(log_file)
    vals = [hex2BatV(val) for val in vals] # Convert hex values to battery voltage
    return timestamp, vals

def hex2BatV (logVal):
    adc = log2ADC(logVal)
    return ADC2Voltage(adc)

def log2ADC(logVal):
    mult = 255 / (config.max_mV - config.min_mV)
    return logVal / mult + config.min_mV - 0.5

def ADC2Voltage(Esp_mV):
    return Esp_mV / config.v_div / 1000

def parse_last_read_response(log_file: str | Path) -> Tuple[datetime, List[int]]:
    """Extract the timestamp and decoded byte values from the last BLE read response entry in a log file."""
    path = Path(log_file)
    log_text = path.read_text(encoding="utf-8", errors="replace")

    for line in reversed(log_text.splitlines()):
        if "Read Response received from" not in line:
            continue

        timestamp_match = re.search(r"(?P<timestamp>\d{2}:\d{2}:\d{2}\.\d{3})", line)
        if not timestamp_match:
            continue
        timestamp_str = timestamp_match.group("timestamp")
        timestamp = datetime.strptime(timestamp_str, "%H:%M:%S.%f")

        hex_str = line.strip().split(' ')[-1]
        hex_list = hex_str.split('-')
        vals = [int(value, 16) for value in hex_list]
        vals = list(dropwhile(lambda x: x == 0, vals)) # remove leading zeros

        return timestamp, vals

    raise ValueError("No read response entry found in the provided log file.")
