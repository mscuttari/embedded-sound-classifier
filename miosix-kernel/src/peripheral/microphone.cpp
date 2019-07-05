#include "microphone.h"
#include <kernel/scheduler/scheduler.h>

typedef Gpio<GPIOB_BASE, 10> clk;   // CLK pin
typedef Gpio<GPIOC_BASE, 3> dout;   // Microphone signal pin

static const int bufferSize = 512;
static const int bufferNumber = 2;
static BufferQueue<unsigned short, bufferSize, bufferNumber> *bq;

static Thread *waiting;
static bool enobuf = true;
static const char filterOrder = 4;
static unsigned short intReg[filterOrder] = {0,0,0,0};
static unsigned short combReg[filterOrder] = {0,0,0,0};
static signed char pdmLUT[] = {-1, 1};


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
        waiting->IRQwait();
        
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
    
    if (waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
}


Microphone& Microphone::getInstance() {
    static Microphone instance;
    return instance;
}


Microphone::Microphone() {

}


void Microphone::start(function<void (short*, unsigned int)> cb, unsigned int buffsize) {
    this->callback = cb;
    this->PCMsize = buffsize;
    this->recording = true;
    this->readyBuffer = (short*) malloc(PCMsize * sizeof(short));
    this->processingBuffer = (short*) malloc(PCMsize * sizeof(short));
    
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

        // Set the 16 kHz sampling rate
        // (see chapters 7.3.23 and 28.4.4 of the datasheet for further details)

        const unsigned int PLLI2S_N = 213;
        const unsigned int PLLI2S_R = 2;
        const unsigned int I2SDIV = 13;

        SPI2->I2SPR = (I2SDIV << 0);
        RCC->PLLI2SCFGR = (PLLI2S_R << 28) | (PLLI2S_N << 6);

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
    
    pthread_create(&mainLoopThread, nullptr, mainLoopLauncher, reinterpret_cast<void*>(this));
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


void* Microphone::mainLoopLauncher(void* arg) {
    reinterpret_cast<Microphone*>(arg)->mainLoop();
    return nullptr;
}


void Microphone::mainLoop() {
    waiting = Thread::getCurrentThread();
    pthread_t cback;
    bq = new BufferQueue<unsigned short, bufferSize, bufferNumber>();
    NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    
    // Create the thread that will execute the callbacks 
    pthread_create(&cback, nullptr, callbackLauncher, reinterpret_cast<void*>(this));
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


void* Microphone::callbackLauncher(void* arg) {
    reinterpret_cast<Microphone*>(arg)->execCallback();
    return nullptr;
}


void Microphone::execCallback() {
    while (recording) {
        pthread_mutex_lock(&bufMutex);
        
        while (recording && !isBufferReady)
            pthread_cond_wait(&cbackExecCond, &bufMutex);

        callback(readyBuffer, PCMsize);
        isBufferReady = false;

        pthread_mutex_unlock(&bufMutex);
    }
}


bool Microphone::processPdm(const unsigned short *pdmBuffer, int size) {
    int remaining = PCMsize - PCMindex;
    int length = min(remaining, size);

    for (int i = 0; i < length; i++){
        processingBuffer[PCMindex++] = PDMFilter(pdmBuffer, i);
    }

    return PCMindex >= PCMsize;
}



short Microphone::PDMFilter(const unsigned short *pdmBuffer, unsigned int index) {
    short combInput, combRes = 0;
    
    // Perform integration on the first word of the PDM chunk to be filtered
    for (short i=0; i < 16; i++){
        // Integrate each single bit
        intReg[0] += pdmLUT[(pdmBuffer[index] >> (15-i)) & 1];
        
        for (short j=1; j < filterOrder; j++){
            intReg[j] += intReg[j-1];
        }
    }
    
    // The last cell of intReg contains the output of the integrator stage
    combInput = intReg[filterOrder-1];
    
    // Apply the comb filter (with delay 1):
    for (short i=0; i < filterOrder; i++){
        combRes = combInput - combReg[i];
        combReg[i] = combInput;
        combInput = combRes;
    }
    
    return combRes;
}