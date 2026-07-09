from log_parser import get_log_vals
import matplotlib.pyplot as plt
import config
from datetime import datetime

timestamp, vals = get_log_vals("C:/Users/daswyga/Downloads/log.txt")
xVals = [-i * config.update_interval_mins / 60 for i in reversed(range(len(vals)))]


midnight = datetime.strptime("00", "%H")
noon = datetime.strptime("12", "%H")
noonDelta = (timestamp-noon).total_seconds()/60/60 % 24 * -1
nightDelta = (timestamp-midnight).total_seconds()/60/60 % 24 * -1

plt.plot(xVals, vals)
plt.axvline(noonDelta, color="yellow", linestyle="dotted", linewidth=1.5)
plt.axvline(nightDelta, color="red", linestyle="dotted", linewidth=1.5)
plt.xlabel(f"Hours since {timestamp.strftime('%H:%M:%S.%f')[:-3]}")
plt.ylabel("Battery (V)")
plt.grid(True, alpha=0.3)
plt.show()
