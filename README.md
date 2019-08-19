# Embedded sound classifier

## Overview
This application uses the on-board MEMS microphone to collect audio samples, analyze them using a pre-trained neural network and send their classification on the serial port. A desktop python script takes care of reading the results.

## Requirements
- For sound clasification (normal usage):
  - Python 3. You can check your version with `python --V`
  - pyserial: `pip install pyserial`
  - A RS232-USB cable
- For neural network training:
  - Python 3, pyserial and RS232-USB cable as in previous case
  - Keras 2.2.4: `pip install keras==2.2.4`
  - Tensorflow 2.0.0-alpha0: `pip install tensorflow==2.0.0-alpha0`
  - GCC
- For pre-trained Keras model to C conversion:
  - [STMCubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)
  - X-CUBE-AI: can be installed from within STMCubeMX
- For embedded software compilation:
  - [Miosix toolchain](https://miosix.org/wiki/index.php?title=Miosix_Toolchain)
  - [GNU ARM embedded toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) (just for the linker)

## How to use
- For sound classification (normal usage):
  1. Connect the serial cable pins to PA2 (board TX) and PA3 (board RX)
  2. Connect the board through USB cable
  3. Launch the client with `python client.py serial_port_name`, replacing `serial_port_name` with the name of the serial port (i.e */dev/tty*, *COM1*)
  4. Press the board user button, do the desired sounds and press again the button to stop recording
- For neural network training:
  1. Compile the FFT extraction program with `gcc FFT_extract.c -o FFT_extract`
  2. Connect the cables as in previous case
  3. Launch the FFT reciever with `python FFT_receive.py serial_port_name`
  4. Press the user button, do the desired sounds and press again the button to stop recording
  5. The results will be in the file `fft.csv`. Values are separated by TAB and not by comma (as it would be in normal CSVs) for better cross-platform management. They must be separated into columns and manually classified according to what they are: one column has to be added at the end and it must contain value 0 for silence, 1 for whistles or 2 for claps
  6. Go into the `neural-network` folder, place the new data in `training_data.csv` and run `python trainer.py`. The pre-trained model will output to file `model.h5`
- For pre-trained Keras model to C library conversion: everything is explained in the `docs/x-cube-ai.pdf` file, provided by ST.
- For embedded software compilation: use command `make` in the root folder or compile using your preferred CMake compatible IDE