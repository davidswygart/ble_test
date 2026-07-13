import re
from pathlib import Path
from typing import List, Optional, Tuple
from datetime import datetime, timedelta
import config
import pandas as pd

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

def hex2BatV (logVal):
    adc = log2ADC(logVal)
    return ADC2Voltage(adc)

def log2ADC(logVal):
    mult = 255 / (config.max_mV - config.min_mV)
    return logVal / mult + config.min_mV - 0.5

def ADC2Voltage(Esp_mV):
    return Esp_mV / config.v_div / 1000

def parse_log(log_file: str | Path, sample_period: float = 5*60, battery_UUID: str = "6e400004-b5a3-f393-e0a9-e50e24dcca9e", duty_UUID: str = "6e400005-b5a3-f393-e0a9-e50e24dcca9e") -> pd.DataFrame:
    """Extract the timestamp and decoded byte values from the last BLE read response entry in a log file."""
    path = Path(log_file)
    log_text = path.read_text(encoding="utf-8", errors="replace")


    # Read the battery values and convert to voltage
    (bat_time, battery_val) = get_char_vals(log_text, battery_UUID)
    if bat_time is None or battery_val is None:
        raise ValueError(f"No valid battery values found in log file {log_file}")
    battery_Voltage = [hex2BatV(val) for val in battery_val] # Convert hex values to battery voltage
    
    # Generate timestamps for each data point, given fixed sample period
    seconds_since_reading = [i*sample_period for i in reversed(range(len(battery_Voltage)))]
    sample_times = [bat_time - timedelta(seconds=s) for s in seconds_since_reading] # Generate timestamps for each sample
    
    # Read the duty values and convert to percent (may not be present in all logs)
    (_, duty_val) = get_char_vals(log_text, duty_UUID)
    if duty_val:
        duty_percent = [val / 255 * 100 for val in duty_val] # Convert hex values to duty cycle percentage]
    else:
        duty_percent = [None] * len(battery_Voltage) # If no duty values found, fill with None
    
    # Create a DataFrame with the extracted values
    data_table = pd.DataFrame({'sample_times': sample_times, 'battery': battery_Voltage, 'duty': duty_percent})
    
    # Remove leading rows with zero battery values
    first_nonzero = next((i for i, x in enumerate(battery_val) if x != 0), None)
    if first_nonzero is None:
        raise ValueError("No non-zero battery values found in log file.")
    data_table = data_table.loc[first_nonzero:]

    return data_table

def get_char_vals(log_text: str, char_id: str) -> Tuple[Optional[datetime], Optional[List[int]]]:
    """Extract the timestamp and decoded byte values for a specific characteristic ID from the last BLE read response entry in a log text."""
    char_line_id = f'Read Response received from {char_id}, value: (0x)'
    data_pattern = r'(?:[A-Z0-9]{2}-)+[A-Z0-9]{2}' # 2 uppercase letter/number hex values separated by dashes
    time_pattern = r'[0-9]{2}:[0-9]{2}:[0-9]{2}' # HH:MM:SS
    date_pattern = r'[0-9]{4}-[0-9]{2}-[0-9]{2}' # YYYY-MM-DD

    lines = log_text.splitlines()
    
    ## Find the line with the char label and split into time strings and data strings
    char_line = next((l for l in reversed(lines) if char_line_id in l), None)
    if not char_line:
        print(f"No read line from Characteristic ID: {char_id}")
        return None, None
    (time_str, data_str) = char_line.split(char_line_id)

    ## Parse the data to decimal values
    data_match = re.search(data_pattern, data_str)
    if not data_match:
        raise ValueError(f"Data string didn't match expected pattern: {data_str}")
    data = data_match.group(0).split('-')
    data = [int(v, 16) for v in data]

    ## Get the time string
    time_match = re.search(time_pattern, time_str)
    if not time_match:
        raise ValueError(f"Time string didn't match expected pattern: {time_str}")
    time_str = time_match.group(0)

    ## Get date from first line of log
    date_match = re.search(date_pattern, lines[0])
    if date_match:
        date_str = date_match.group(0)
    else:
        raise ValueError(f"No date found in first line of log")
    
    # Parse the time and data to datetime value
    timestamp = datetime.strptime(f"{date_str} {time_str}", "%Y-%m-%d %H:%M:%S")
    return timestamp, data