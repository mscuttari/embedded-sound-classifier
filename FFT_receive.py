#!/usr/bin/python3 
#
# Copyright (C) 2019 Michele Scuttari, Marina Nikolic
#
# Usage: FFT_receive.py filename
# Example: ./FFT_receive.py fft.csv

import sys, serial
from subprocess import call

# bytes representing the expected size of the audio data chunk
# what it does is read bytes until it reach the head of a new chunk of data
# i.e. the size of the chunk (that should be the expected batch size)
def recoveryProcedure(expectedBatchSize):
	""" tries to perform recovery in case of a communication error """
	s = ser.read(size=1)
	while True :
		if s != expectedBatchSize[0:1]:
			s = ser.read(size=1)
		else :
			s = ser.read(size=1)
			if s == expectedBatchSize[1:2] :
				s = ser.read(size=1)
				if s == expectedBatchSize[2:3]:
					s = ser.read(size=1)
					if s == expectedBatchSize[3:4]:
						return int.from_bytes(expectedBatchSize,byteorder='little', signed=True)

beginSignal = "#start"
defaultFileName = "fft.csv"

# Setup serial port (no timeout specified)
ser = serial.Serial("COM6",					\
					115200,							\
					stopbits = serial.STOPBITS_ONE,	\
					parity = serial.PARITY_NONE,	\
					bytesize = serial.EIGHTBITS,	\
					timeout = None, 				\
					rtscts = False,					\
					dsrdtr = False,					\
					xonxoff = False
					)

if len(sys.argv) == 1:
	fileName = defaultFileName
	print("Default name '" + defaultFileName + "' assigned.\nUse ./FFT_receive.py filename to specify a different output file name.")
else:
	fileName = sys.argv[1]

outputFile = open("fft.raw", "wb")

# Wait for the begin signal
print("Waiting for the recording to start...")
sample = ""

while sample != beginSignal:
	sample = ser.readline().decode()
	sample = sample.split("\n")[0]

print("Started recording.", flush=True)

# Get expected batch size
expectedBatchSize = ser.read(4)
expectedBatchSizeInt = int.from_bytes(expectedBatchSize, byteorder='little', signed=True)
print("Expected batch size: " + str(expectedBatchSizeInt) + ".")
nextBatchSizeInt = -1

while nextBatchSizeInt != 0:
	# Read size of next batch
	nextBatchSize = ser.read(4)
	nextBatchSizeInt = int.from_bytes(nextBatchSize, byteorder='little', signed=True)
	
	print("Batch size: " + str(nextBatchSizeInt))
	
	# Check if an error occured
	if nextBatchSizeInt != expectedBatchSizeInt and nextBatchSizeInt != 0 :
		nextBatchSizeInt = recoveryProcedure(expectedBatchSize)
		print('Error detected. Trying recovery.', flush=True)
	
	# Read the batch
	sample = ser.read(size=nextBatchSizeInt)
	
	# Write data to file
	outputFile.write(sample)

print("Recording completed with success!")

# decode adpcm to wav file
dataExtraction = call(["FFT_extract", "fft.raw", fileName])

if dataExtraction == 0:
	print("FFT extraction completed")
else:
	print("Error in FFT extraction")

call(["rm", "fft.raw"])
