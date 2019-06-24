#include <cstdio>
#include <miosix.h>
#include <functional>
#include "button.h"
#include "microphone.h"
#include "miosix/arch/common/drivers/serial_stm32.h"

#define BAUD_RATE 115200

using namespace std;
using namespace miosix;

void sendStartSignal(STM32Serial &serial);
void sendStopSignal(STM32Serial &serial);
void scanAudio(short* data, unsigned int size);


int main() {
    STM32Serial serial(2, BAUD_RATE);
    UserButton& button = UserButton::getInstance();
    Microphone& microphone = Microphone::getInstance();
    
    button.wait();
    sendStartSignal(serial);
    microphone.start(bind(scanAudio, placeholders::_1, placeholders::_2));
    
    button.wait();
    microphone.stop();
    sendStopSignal(serial);
    
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