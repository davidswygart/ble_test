import re
from pathlib import Path
from typing import List, Optional, Tuple
from datetime import datetime, timedelta
import pandas as pd
import numpy as np

def get_log_vals(log_dir: str | Path) -> List[pd.DataFrame]:
    # Try to parse all text file in log directory
    log_dir = Path(log_dir)
    table_list = []
    for log_file in sorted(log_dir.glob("*.txt")):
        try:
            table_list.append(parse_log(log_file))
        except Exception as e:
            print(f"Error parsing log file {log_file}: {e}")
            continue
    return table_list

def parse_log(log_file: str | Path) -> pd.DataFrame:
    """Extract the timestamp and decoded byte values from the last BLE read response entry in a log file."""
    path = Path(log_file)
    log_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()

    ## Get date from first line of log
    date_pattern = r'[0-9]{4}-[0-9]{2}-[0-9]{2}' # YYYY-MM-DD
    date_match = re.search(date_pattern, log_lines[0])
    if date_match:
        date_str = date_match.group(0)
    else:
        raise ValueError(f"No date found in first line of log")

    ## Extract data from indication read lines
    data_splitter = "Indication received from 6e400003-b5a3-f393-e0a9-e50e24dcca9e, value: (0x)"
    data_lines = [l for l in log_lines if data_splitter in l]
    time_pattern = r'[0-9]{2}:[0-9]{2}:[0-9]{2}' # HH:MM:SS
    data_pattern = r'(?:[A-Z0-9]{2}-)+[A-Z0-9]{2}' # 2 uppercase letter/number hex values separated by dashes

    ## Get the time string from the first data line
    time_match = re.search(time_pattern, data_lines[0])
    if not time_match:
        raise ValueError(f"Time string didn't match expected pattern: {data_lines[0]}")
    time_str = time_match.group(0)
    # Parse the time and data to datetime value
    timestamp = datetime.strptime(f"{date_str} {time_str}", "%Y-%m-%d %H:%M:%S")

    data_str = []
    for l in data_lines:
        l = l.split(data_splitter)[1].strip().split('-')
        for v in l:
            data_str.append(v)
    data_str = "".join(data_str)

    data_bytes = bytes.fromhex(data_str)



    # Define the exact matching data types
    dt = np.dtype([
        ('timestamp', np.uint16),
        ('batt_mV', np.uint16),
        ('duty', np.uint8)
    ])

    # Direct, zero-copy memory mapping
    sensor_history = np.frombuffer(data_bytes, dtype=dt)

    # Access columns directly as fast vector arrays
    print(sensor_history['timestamp'])
    print(sensor_history['batt_mV'])  # Array of all battery voltages
    print(sensor_history['duty'])
    

    # # Generate timestamps for each data point, given fixed sample period
    # seconds_since_reading = [i*update_interval_mins*60 for i in reversed(range(len(battery_Voltage)))]
    # sample_times = [bat_time - timedelta(seconds=s) for s in seconds_since_reading] # Generate timestamps for each sample
    
    
    # # Remove leading rows with zero battery values
    # first_nonzero = next((i for i, x in enumerate(battery_val) if x != 0), None)
    # if first_nonzero is None:
    #     raise ValueError("No non-zero battery values found in log file.")
    # data_table = data_table.loc[first_nonzero:]

    return sensor_history


if __name__ == "__main__":
    file = "G:/My Drive/Solar/streamTest.txt"
    table_list = parse_log(file)