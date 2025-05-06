#!/usr/bin/env python3
"""
mass_ota.py  –  build once with PlatformIO, discover every ESP32 advertising
                _arduino._tcp.local, and push the fresh firmware image to all
                of them in parallel via espota.py.
"""

import os, sys, subprocess, threading, time, socket
from queue import Queue
from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange

ENV_NAME    = "seeed_xiao_esp32s3"        # PlatformIO env to build
OTA_PORT    = 3232
OTA_PASS    = "otapass"      # must match build flag / sketch
ESPOTA_PATH = os.path.join(
    os.environ.get("HOME", "~"),
    ".platformio", "packages", "framework-arduinoespressif32",
    "tools", "espota.py",
)

def build_firmware() -> str:
    print("🔧  Building firmware …")
    if subprocess.call(["platformio", "run", "-e", ENV_NAME]) != 0:
        sys.exit("Build failed 👎")
    bin_path = f".pio/build/{ENV_NAME}/firmware.bin"
    if not os.path.exists(bin_path):
        sys.exit("❌ firmware.bin not found")
    return bin_path

class Listener:
    def __init__(self):
        self.targets = {}     # hostname → ip

    def remove_service(self, *args): pass

    def add_service(self, zc, typ, name):
        info = zc.get_service_info(typ, name)
        if info and info.addresses:
            ip = socket.inet_ntoa(info.addresses[0])
            self.targets[name[:-len('._arduino._tcp.local.')]] = ip

def discover(timeout=5):
    zc, listener = Zeroconf(), Listener()
    ServiceBrowser(zc, "_arduino._tcp.local.", listener)
    time.sleep(timeout)
    zc.close()
    return listener.targets

def ota_single(ip, binary, queue):
    cmd = [
        "python", ESPOTA_PATH,
        "-i", ip,
        "-p", str(OTA_PORT),
        "-f", binary,
        "-a", OTA_PASS,
        "-r"  # reboot after flash
    ]
    rc = subprocess.call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    queue.put((ip, rc == 0))

def mass_ota(binary, targets):
    print(f"🚀  Flashing {len(targets)} device(s)…")
    q, threads = Queue(), []
    for ip in targets.values():
        t = threading.Thread(target=ota_single, args=(ip, binary, q))
        t.start()
        threads.append(t)
    for t in threads:
        t.join()
    ok = [ip for (ip, success) in iter(q.get, None) if success]
    fail = [ip for (ip, success) in iter(q.get, None) if not success]
    print(f"✅  Successful: {len(ok)}   ❌ Failed: {len(fail)}")
    if fail:
        print("   Retry or check connectivity for:", ", ".join(fail))

if __name__ == "__main__":
    firmware = build_firmware()
    devs = discover()
    if not devs:
        sys.exit("No OTA‑enabled ESP32 found on the LAN.")
    print("Found devices:\n",
          "\n ".join(f"• {h:20s} {ip}" for h, ip in devs.items()))
    mass_ota(firmware, devs)