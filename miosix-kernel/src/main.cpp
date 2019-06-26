#include <cstdio>
#include <miosix.h>
#include <functional>
#include <arch/common/drivers/serial.h>
#include "button.h"
#include "microphone.h"

#define BAUD_RATE 115200

using namespace std;
using namespace miosix;

void sendStartSignal(STM32Serial &serial);
void sendStopSignal(STM32Serial &serial);
void scanAudio(short* data, unsigned int size);


int main() {
    // Peripherals setup
    STM32Serial serial(2, BAUD_RATE);
    Microphone& microphone = Microphone::getInstance();

    // Start the recording on user button press
    UserButton::wait();
    sendStartSignal(serial);
    microphone.start(bind(scanAudio, placeholders::_1, placeholders::_2));

    // Stop on second button press
    UserButton::wait();
    microphone.stop();
    sendStopSignal(serial);

    // Endless loop
    while (true) {
        
    }
}


void sendStartSignal(STM32Serial &serial) {
    char msg[] = "#start\n";
    serial.writeBlock(msg, strlen(msg) * sizeof(char), 0);
}


void sendStopSignal(STM32Serial &serial) {
    char msg[] = "#stop\n";
    serial.writeBlock(msg, strlen(msg) * sizeof(char), 0);
}


void scanAudio(short* data, unsigned int size) {
    
}