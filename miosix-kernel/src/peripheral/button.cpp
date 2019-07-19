/**************************************************************************
 * Copyright (C) 2017 Giovanni Beri                                       *
 * Copyright (C) 2019 Michele Scuttari, Marina Nikolic                    *
 *                                                                        *
 * This program is free software: you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation, either version 3 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/

#include "button.h"
#include <miosix.h>
#include <kernel/scheduler/scheduler.h>

using namespace miosix;

typedef Gpio<GPIOA_BASE, 0> button;

static bool initialized = false;
static bool justPushed = false;
static Thread *t;


void UserButton::initialize() {
    // Temporarily disable the interrupts in order to safely
    // configure the GPIO interface
    FastInterruptDisableLock dLock;

    button::mode(Mode::INPUT_PULL_DOWN);

    // Enable Interrupt and event generation on line 0
    EXTI->IMR |= EXTI_EMR_MR0;

    // Enable rising edge detection on line 0
    EXTI->RTSR |= EXTI_RTSR_TR0;

    // Set EXTI0 to the lowest priority
    NVIC_SetPriority(EXTI0_IRQn, 0x0f);

    // Enable EXTI0 Interrupt
    NVIC_EnableIRQ(EXTI0_IRQn);
}


/**
 * Interrupt handler
 */
void __attribute__((naked)) EXTI0_IRQHandler() {
    saveContext();
    asm volatile("bl _Z16EXTI0HandlerImplv");
    restoreContext();
}


/**
 * Implementation of the interrupt handler
 */
void __attribute__((used)) EXTI0HandlerImpl() {
    // EXTI->PR must be reset to 1 in order
    // to clear the pending edge event flag
    //
    // FROM DATASHEET page 205
    // Pending register (EXTI_PR)
    //
    // "This bit is set when the selected edge event
    // arrives on the external interrupt line.
    // This bit is cleared by writing a 1 to the bit
    // or by changing the sensitivity of the edge detector."
    if (EXTI->PR == EXTI_PR_PR0)
        EXTI->PR = EXTI_PR_PR0;

    // The 'justPushed' is used for software debounce
    if (t == nullptr || justPushed)
        return;

    // Wake up the sleeping thread
    t->IRQwakeup();

    if (t->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    // The waiting thread has finished waiting
    t = nullptr;
}


void UserButton::wait() {
    // Initialize the peripheral in case of first call
    if (!initialized) {
        initialize();
        initialized = true;
    }

    // Disable interrupts
    FastInterruptDisableLock dLock;
    
    // Set the thread that wants to wait for the button click
    t = Thread::IRQgetCurrentThread();
    
    while (t) {
        // Call a preemption, return only when someone wake me up (sw interrupt
        Thread::IRQwait();
        
        // Enable interrupt only in the scope
        FastInterruptEnableLock eLock(dLock);
        
        // Pause the thread
        Thread::yield();
    }
    
    // Software debouncing (the board doesn't have an hardware debouncing circuit)
    justPushed = true;
    delayMs(200); 
    justPushed = false;
}
