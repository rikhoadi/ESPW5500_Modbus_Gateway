from pymodbus.client import ModbusTcpClient
import struct
import time
from datetime import datetime

TARGET_IP = "192.168.180.55"
PORT = 502

power_meter_map = {
    "m4m": {
        "voltage_l1": {"address": 0x5B02, "scaling": 0.1, "count": 2},
        "Frequency" : {"address": 0x5B32, "scaling": 0.01, "count": 1},
    },
    "m1m": {
        "voltage_l1": {"address": 0x5B02, "scaling": 0.1, "count": 2},
        "Frequency" : {"address": 0x5B32, "scaling": 0.01, "count": 1},
    }
}

slaves = {
    1: "m4m",
    2: "m1m",
}

def ensure_connected(client):
    """Pastikan client tetap connect, kalau tidak reconnect."""
    if client.is_socket_open() and client.connected:
        return True

    print(f"[{datetime.now().strftime('%H:%M:%S')}] 🔄 Connection lost, trying to reconnect...")
    client.close()
    for attempt in range(1, 6):
        if client.connect():
            print(f"[{datetime.now().strftime('%H:%M:%S')}] ✅ Reconnected on attempt {attempt}")
            return True
        print(f"[{datetime.now().strftime('%H:%M:%S')}] ❌ Reconnect attempt {attempt} failed, retrying...")
        time.sleep(2)
    return False

def read_scaled_register(client, address, scaling, count, slave):
    try:
        start_time = datetime.now()
        resp = client.read_holding_registers(address=address, count=count, slave=slave)
        end_time = datetime.now()
        duration = (end_time - start_time).total_seconds() * 1000  # ms

        if resp and not resp.isError():
            regs = resp.registers
            if count == 1:
                value = regs[0] * scaling
            elif count == 2:
                raw = struct.unpack(">I", struct.pack(">HH", regs[0], regs[1]))[0]
                value = raw * scaling
            else:
                value = float('nan')

            print(f"[{start_time.strftime('%H:%M:%S.%f')[:-3]}] Slave {slave} "
                  f"addr {hex(address)} done in {duration:.2f} ms → {value}")
            return value
        else:
            print(f"[{start_time.strftime('%H:%M:%S.%f')[:-3]}] ❌ Error response from slave {slave}, addr {hex(address)}")

    except Exception as e:
        print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] ❌ Exception reading slave {slave} address {hex(address)}: {e}")
        # langsung force reconnect
        client.close()
    return float('nan')

def main():
    client = ModbusTcpClient(TARGET_IP, port=PORT)

    if not client.connect():
        print(f"❌ Failed initial connect to {TARGET_IP}")
        return

    print(f"✅ Connected to {TARGET_IP}")

    try:
        while True:
            if not ensure_connected(client):
                print("❌ Could not reconnect, wait 10s before retrying...")
                time.sleep(10)
                continue

            loop_start = datetime.now()
            print(f"\n⏱️ Polling started at {loop_start.strftime('%H:%M:%S')}")

            for slave_id, tipe in slaves.items():
                registers = power_meter_map[tipe]
                for name, reg in registers.items():
                    read_scaled_register(client, reg["address"], reg["scaling"], reg["count"], slave=slave_id)

            loop_end = datetime.now()
            elapsed = (loop_end - loop_start).total_seconds()
            print(f"✅ Poll finished at {loop_end.strftime('%H:%M:%S')} (duration {elapsed:.2f} sec)")

            wait_time = max(0, 5 - elapsed)
            print(f"⏳ Waiting {wait_time:.2f} seconds before next poll...\n")
            time.sleep(wait_time)

    except KeyboardInterrupt:
        print("🛑 Stopped by user")
    finally:
        client.close()

if __name__ == "__main__":
    main()
