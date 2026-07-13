from log_parser import get_log_vals
import matplotlib.pyplot as plt
import config
from datetime import datetime

table_list = get_log_vals("G:/My Drive/Solar")


fig, ax1 = plt.subplots()
ax1.set_xlabel("Datetime")
ax1.set_ylabel('Battery (V)', color='tab:blue')
ax1.tick_params(axis='y', labelcolor='tab:blue')

ax2 = ax1.twinx()
ax2.set_ylabel('Duty (%)', color='tab:grey')
ax2.tick_params(axis='y', labelcolor='tab:grey')

for t in table_list:
    ax2.plot(t.sample_times, t.duty, color='tab:grey')
    ax1.plot(t.sample_times, t.battery, color='tab:blue')
    
plt.grid(True, alpha=0.3)
plt.show()
print('hi')