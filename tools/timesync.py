# Sync a Doobie's clock to this computer's local time over USB serial.
# Usage: python3 tools/timesync.py /dev/cu.usbmodemXXXX   (needs pyserial)
import serial, time, sys
port = sys.argv[1]
p = serial.Serial(port, 115200, timeout=0.3)
p.dtr = False
time.sleep(0.3)
lt = time.localtime()
local_epoch = int(time.time()) + lt.tm_gmtoff
p.write(f"T{local_epoch}\n".encode())
t0 = time.time(); buf = b""
while time.time() - t0 < 2.5:
    buf += p.read(2048)
for line in buf.decode("utf-8", "replace").splitlines():
    if "clock set" in line: print(port, line.strip())
