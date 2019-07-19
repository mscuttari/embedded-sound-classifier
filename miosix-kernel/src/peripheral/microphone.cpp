/**************************************************************************
 * Copyright (C) 2014 Riccardo Binetti, Guido Gerosa, Alessandro Mariani  *
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

#include "microphone.h"
#include <miosix.h>
#include <kernel/scheduler/scheduler.h>

using namespace miosix;

typedef Gpio<GPIOB_BASE, 10> clk;   // CLK pin
typedef Gpio<GPIOC_BASE, 3> dout;   // Microphone signal pin

static const int bufferSize = 512;
static const int bufferNumber = 2;
static BufferQueue<unsigned short, bufferSize, bufferNumber> *bq;

static Thread *waiting;
static bool enobuf = true;
static bool recording;                      // Whether the recording is in progress or not

static void* mainLoopLauncher(void* arg);   // Used for mainLoop thread creation.
static void mainLoop();                     // Function that manages the transcoding
static pthread_t mainLoopThread;            // Thread taking care of transcoding

// Double buffering
static short *readyBuffer;
static short *processingBuffer;
static bool isBufferReady;

static unsigned int PCMsize;    // How many PCM samples to collect before executing the callback
static unsigned int PCMindex;   // Transcoding progress index

static bool processPdm(const unsigned short *pdmBuffer, int size);              // Convert PDM buffer to PCM samples
static short PDMFilter(const unsigned short* pdmBuffer, unsigned int index);    // Get single PCM sample from PDM values

static void* callbackLauncher(void* arg);               // Used for execCallback thread creation
static void execCallback();                             // Function that executes the callback when the PCM samples are ready
static function<void (short*, unsigned int)> callback;  // Function called when the PCM samples are ready to be processed

static pthread_mutex_t bufMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cbackExecCond = PTHREAD_COND_INITIALIZER;


/**
 * Configure the DMA to do another transfer
 */
static void IRQdmaRefill() {
    unsigned short *buffer;
    
    if (!bq->tryGetWritableBuffer(buffer)) {
        enobuf = true;
        return;
    }
    
    DMA1_Stream3->CR = 0;
    DMA1_Stream3->PAR = reinterpret_cast<unsigned int>(&SPI2->DR);
    DMA1_Stream3->M0AR = reinterpret_cast<unsigned int>(buffer);
    DMA1_Stream3->NDTR = bufferSize;
    DMA1_Stream3->CR = DMA_SxCR_PL_1    |   // High priority DMA stream
                       DMA_SxCR_MSIZE_0 |   // Write 16bit at a time to RAM
                       DMA_SxCR_PSIZE_0 |   // Read 16bit at a time from SPI
                       DMA_SxCR_MINC    |   // Increment RAM pointer
                       DMA_SxCR_TCIE    |   // Interrupt on completion
                       DMA_SxCR_EN;         // Start the DMA
}


static void dmaRefill() {
    FastInterruptDisableLock dLock;
    IRQdmaRefill();
}


/**
 * Helper function that waits until a buffer is available for reading
 * 
 * @return a readable buffer from bq
 */
static const unsigned short *getReadableBuffer() {
    FastInterruptDisableLock dLock;
    
    const unsigned short *result;
    unsigned int size;
    
    while (!bq->tryGetReadableBuffer(result, size)) {
        Thread::IRQwait();
        
        {
            FastInterruptEnableLock eLock(dLock);
            Thread::yield();
        }
    }
    
    return result;
}


static void bufferEmptied() {
    FastInterruptDisableLock dLock;
    bq->bufferEmptied();
}


/**
 * DMA end of transfer interrupt
 */
void __attribute__((naked)) DMA1_Stream3_IRQHandler() {
    saveContext();
    asm volatile("bl _Z17I2SdmaHandlerImplv");
    restoreContext();
}


/**
 * DMA end of transfer interrupt actual implementation
 */
void __attribute__((used)) I2SdmaHandlerImpl() {
    DMA1->LIFCR = DMA_LIFCR_CTCIF3  |
                  DMA_LIFCR_CTEIF3  |
                  DMA_LIFCR_CDMEIF3 |
                  DMA_LIFCR_CFEIF3;
    
    bq->bufferFilled(bufferSize);
    IRQdmaRefill();
    waiting->IRQwakeup();
    
    if (waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority()) {
        Scheduler::IRQfindNextThread();
    }
}


bool Microphone::start(function<void (short*, unsigned int)> cb, unsigned int buffsize) {
    if (recording)
        return false;

    callback = cb;
    PCMsize = buffsize;
    recording = true;
    readyBuffer = (short *) malloc(buffsize * sizeof(short));
    processingBuffer = (short *) malloc(buffsize * sizeof(short));

    {
        FastInterruptDisableLock dLock;

        // Enable DMA1, SPI2, GPIOB and GPIOC
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

        // Configure GPIOs
        // (see chapter 8.3.2 of the datasheet for the alternate functions list)

        clk::mode(Mode::ALTERNATE);
        clk::alternateFunction(5);
        clk::speed(Speed::_50MHz);

        dout::mode(Mode::ALTERNATE);
        dout::alternateFunction(5);
        dout::speed(Speed::_50MHz);

        // Set the 32 kHz sampling rate
        // (see chapters 7.3.23 and 28.4.4 of the datasheet for further details)

        const unsigned int PLLI2S_N = 213u;
        const unsigned int PLLI2S_R = 2u;
        const unsigned int I2SDIV = 13u;

        SPI2->I2SPR = (I2SDIV << 0u);

        RCC->PLLI2SCFGR = (PLLI2S_R << 28u) | (PLLI2S_N << 6u);

        // Enable I2S
        RCC->CR |= RCC_CR_PLLI2SON;
    }

    // Wait for PLL to lock
    while ((RCC->CR & RCC_CR_PLLI2SRDY) == 0);

    // RX buffer not empty interrupt enable
    SPI2->CR2 = SPI_CR2_RXDMAEN;

    // Enable the master clock for I2S
    SPI2->I2SPR |= SPI_I2SPR_MCKOE;

    // Configure SPI
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD |                            // I2S mode selected
                    SPI_I2SCFGR_I2SE |                              // I2S enabled
                    SPI_I2SCFGR_I2SCFG_0 | SPI_I2SCFGR_I2SCFG_1;    // Mode: master receive

    // High priority for DMA
    NVIC_SetPriority(DMA1_Stream3_IRQn, 2);

    // Wait for the microphone to enable
    delayMs(10);

    pthread_create(&mainLoopThread, nullptr, mainLoopLauncher, nullptr);
    return true;
}


void Microphone::stop() {
    // Stop the software
    recording = false;
    
    // Wait for the last PCM processing to end
    pthread_join(mainLoopThread, nullptr);
    
    // Reset the configuration registers to stop the hardware
    NVIC_DisableIRQ(DMA1_Stream3_IRQn);
    delete bq;
    SPI2->I2SCFGR = 0;
    
    {
        FastInterruptDisableLock dLock;
        RCC->CR &= ~RCC_CR_PLLI2SON;
    }

    free(readyBuffer);
    free(processingBuffer);
}


void* mainLoopLauncher(void* arg) {
    mainLoop();
    return nullptr;
}


void mainLoop() {
    waiting = Thread::getCurrentThread();
    pthread_t cback;
    bq = new BufferQueue<unsigned short, bufferSize, bufferNumber>();
    NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    
    // Create the thread that will execute the callbacks 
    pthread_create(&cback, nullptr, callbackLauncher, nullptr);
    isBufferReady = false;
    
    // Variable used for swap of processing and ready buffer
    short *tmp;

    while (recording) {
        PCMindex = 0;

        // Process any new chunk of PDM samples
        for (;;) {
            if (enobuf) {
                enobuf = false;
                dmaRefill();
            }
            
            if (processPdm(getReadableBuffer(), bufferSize)){
                // Transcode until the specified number of PCM samples
                break;
            }
            
            bufferEmptied();
        }
        
        // Swap the ready and the processing buffer: allows double buffering
        // on the callback side

        tmp = readyBuffer;

        // Start critical section
        pthread_mutex_lock(&bufMutex);

        readyBuffer = processingBuffer;
        isBufferReady = true;

        pthread_cond_broadcast(&cbackExecCond);
        pthread_mutex_unlock(&bufMutex);
        // End critical section

        processingBuffer = tmp;
    }
    
    pthread_cond_broadcast(&cbackExecCond);
    pthread_join(cback, nullptr);
}


void* callbackLauncher(void* arg) {
    execCallback();
    return nullptr;
}


void execCallback() {
    while (recording) {
        pthread_mutex_lock(&bufMutex);
        
        while (recording && !isBufferReady)
            pthread_cond_wait(&cbackExecCond, &bufMutex);

        callback(readyBuffer, PCMsize);
        isBufferReady = false;

        pthread_mutex_unlock(&bufMutex);
    }
}


bool processPdm(const unsigned short *pdmBuffer, int size) {
    int remaining = PCMsize - PCMindex;
    int length = min(remaining, size);

    for (int i = 0; i < length; i++){
        processingBuffer[PCMindex++] = PDMFilter(pdmBuffer, i);
    }

    return PCMindex >= PCMsize;
}



short PDMFilter(const unsigned short *pdmBuffer, unsigned int index) {
    static const char filterOrder = 4;
    static unsigned short intReg[filterOrder] = {0,0,0,0};
    static unsigned short combReg[filterOrder] = {0,0,0,0};
    static signed char pdmLUT[] = {-1, 1};

    short combInput, combRes = 0;
    
    // Perform integration on the first word of the PDM chunk to be filtered
    for (unsigned short i = 0; i < 16; i++){
        // Integrate each single bit
        intReg[0] += pdmLUT[(pdmBuffer[index] >> (15u - i)) & 1u];
        
        for (short j=1; j < filterOrder; j++){
            intReg[j] += intReg[j-1];
        }
    }
    
    // The last cell of intReg contains the output of the integrator stage
    combInput = intReg[filterOrder-1];
    
    // Apply the comb filter (with delay 1):
    for (unsigned short &i : combReg){
        combRes = combInput - i;
        i = combInput;
        combInput = combRes;
    }
    
    return combRes;
}
