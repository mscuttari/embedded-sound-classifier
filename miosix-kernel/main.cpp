#include <cstdio>
#include "miosix.h"
#include "button.h"
#include "miosix/arch/common/drivers/serial_stm32.h"

#define BAUD_RATE 115200

using namespace std;
using namespace miosix;

void sendStartSignal(STM32Serial &serial);
void sendStopSignal(STM32Serial &serial);

int main() {
    STM32Serial serial(2, BAUD_RATE);
    UserButton& button = UserButton::getInstance();
    
    button.wait();
    sendStartSignal(serial);
    
    button.wait();
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