#include <cstdio>
#include <miosix.h>
#include <functional>
#include "button.h"
#include "microphone.h"

using namespace std;
using namespace miosix;


/**
 * Write to the serial port
 *
 * @param str   text to be sent
 */
void print(const char* str);

/**
 * Write the start signal to the serial port
 */
void sendStartSignal();

/**
 * Write the stop signal to the serial port
 */
void sendStopSignal();

/**
 * Elaborate the recorded audio
 *
 * @param data  data chunk
 * @param size  chunk size
 */
void scanAudio(short* data, unsigned int size);


int main() {
    // Peripherals setup
    Microphone& microphone = Microphone::getInstance();

    // Start the recording on user button press
    UserButton::wait();
    sendStartSignal();
    microphone.start(bind(scanAudio, placeholders::_1, placeholders::_2));

    // Stop on second button press
    UserButton::wait();
    microphone.stop();
    sendStopSignal();

    // Endless loop
    while (true) {
        
    }
}


void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}


void sendStartSignal() {
    print("#start\n");
}


void sendStopSignal() {
    print("#stop\n");
}


void scanAudio(short* data, unsigned int size) {
    
}