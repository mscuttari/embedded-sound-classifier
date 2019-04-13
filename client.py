#!/usr/bin/python3
#
# Copyright (C) 2019 Michele Scuttari & Marina Nikolic
#
# Usage: keylogger.py serial_port_name
# Example: py keylogger.py COM1

import sys, serial
from serial import SerialException

portName = "COM1"

if len(sys.argv) == 1:
	print("[WARNING] No serial port specified. Assuming %s" % portName)
else:
	portName = sys.argv[1]

# Connect to the board
try:
	ser = serial.Serial(port = portName,
						baudrate = 115200,
						stopbits = serial.STOPBITS_ONE,
						parity = serial.PARITY_NONE,
						bytesize = serial.EIGHTBITS,
						timeout = 1,
						rtscts = False,
						dsrdtr = False,
						xonxoff = False)

except ValueError:
	print("[ERROR] Invalid port configuration")
	sys.exit(-1)

except SerialException:
	print("[ERROR] Can't open port", portName)
	sys.exit(-1)

# Wait for the start message
print("Press the USER button on the board to start logging. Press it again to stop.")

message = ""

while (message != "#start"):
	message = ser.readline().decode()
	message = message.split("\n")[0]

# Start logging
print("Listening...")

somethingPrinted = False
message = ser.readline().decode()

while (message != "#stop"):
	print(message, end = '')
	somethingPrinted |= len(message) > 0;
	message = ser.readline().decode()
	message = message.split("\n")[0]

if (somethingPrinted):
	print("\nStopped")
else:
	print("Stopped")

# Close the serial connection
ser.close()
