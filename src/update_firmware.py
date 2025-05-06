#!/usr/bin/env python3
"""
Builds the selected PlatformIO environment *once*,
then streams the resulting firmware.bin to the gateway’s /upload endpoint.
"""

import os, subprocess, sys, requests

ENV          = "seeed_xiao_esp32s3" # name of the env that builds the firmware
GATEWAY_IP   = "192.168.4.1"        # gateway default IP
UPLOAD_URL   = f"http://{GATEWAY_IP}/upload"

def build():
    print("🔧  Building …")
    if subprocess.call(["platformio", "run", "-e", ENV]) != 0:
        sys.exit("Build failed")
    path = f".pio/build/{ENV}/firmware.bin"
    if not os.path.exists(path):
        sys.exit("firmware.bin not found")
    return path

def upload(path):
    print(f"📤  Uploading {path} → {UPLOAD_URL}")
    with open(path, "rb") as f:
        r = requests.post(UPLOAD_URL,
                          data=f,
                          headers={"Content-Type": "application/octet-stream"})
    if r.ok:
        print("✓ Gateway accepted file - beacon sent 🚀")
    else:
        print("✗ Upload failed:", r.status_code, r.text)

if __name__ == "__main__":
    bin_path = build()
    upload(bin_path)