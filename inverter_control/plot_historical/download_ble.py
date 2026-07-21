import asyncio
from bleak import BleakScanner, BleakClient
import struct
import numpy as np

TARGET_NAME = "STREAM_TEST"   # The advertised name of your BLE device
CHAR_NOTIFY_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
CHAR_WRITE_UUID  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

async def main():
    global client

    print("Scanning for BLE devices...")
    devices = await BleakScanner.discover()

    target = next((d for d in devices if d.name == TARGET_NAME), None)
    if target is None:
        print(f"Device '{TARGET_NAME}' not found.")
        return

    print(f"Found device: {target.name} @ {target.address}")
    client = BleakClient(target.address)

    print("Connecting...")
    await client.connect()
    print("Connected!")

    data = await get_data(client)

    dt = np.dtype([
        ('timestamp', np.uint16),
        ('batt_mV', np.uint16),
        ('duty', np.uint8)
    ])
    sensor_history = np.frombuffer(data, dtype=dt)

    await client.disconnect()
    print("Disconnected.")

async def get_data(client):
    global data_complete
    data_complete = False
    full_data = []

    def notification_handler(sender, data):
        global data_complete
        fin, seq_num, payload = decode_ble_packet(data)
        if fin:
            data_complete = True
        full_data.append(payload)
        seq_bytes = seq_num.to_bytes(2, byteorder='little', signed=False) # Convert sequence back to bytes 
        asyncio.create_task(client.write_gatt_char(CHAR_WRITE_UUID, seq_bytes, response=False))

    await client.start_notify(CHAR_NOTIFY_UUID, notification_handler)
    print('Writing initial data to start transfer')
    await client.write_gatt_char(CHAR_WRITE_UUID, b'01', response=False)

    # Run until all data collected
    while not data_complete:
        await asyncio.sleep(1)
    print('All data collected')
    await client.stop_notify(CHAR_NOTIFY_UUID)
    return bytearray().join(full_data)

def decode_ble_packet(packet_bytes):
    """
    Decodes a 2-byte C bit-field header (15-bit seq_num, 1-bit fin)
    and extracts the remaining data payload.
    """
    if len(packet_bytes) < 2:
        raise ValueError("Packet is too short to contain a header")
    header_val, = struct.unpack('<H', packet_bytes[:2]) # first 2 bytes as a little-endian unsigned short ('<H')
    seq_num = header_val & 0x7FFF # seq_num is the lower 15 bits (0x7FFF = 0111 1111 1111 1111)
    fin = (header_val >> 15) & 0x01 # fin is the highest bit (shifted down 15 places)
    payload = packet_bytes[2:] # remaining bytes are the payload
    return fin, seq_num, payload


asyncio.run(main())