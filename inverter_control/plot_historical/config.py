update_interval_mins = 10

sample_period: float = 10
battery_UUID: str = "6e400004-b5a3-f393-e0a9-e50e24dcca9e"
duty_UUID: str = "6e400005-b5a3-f393-e0a9-e50e24dcca9e"

def byte2BatV (logVal):
    return logVal * .0238 + 8.981