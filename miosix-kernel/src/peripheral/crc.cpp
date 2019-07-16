#include "crc.h"
#include <miosix.h>

using namespace miosix;

void Crc::init() {
    {
        FastInterruptDisableLock dLock;

        // Enable the clock
        RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

        // Reset the peripheral
        CRC->CR = CRC_CR_RESET;
    }
}