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
						baudrate = 9600,
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

# Start logging
print("Listening...")

message = ser.readline().decode()

while (message != "#stop"):
	print(message, end = '')
	message = ser.readline().decode()

print("\nStopped")

# Close the serial connection
ser.close()
