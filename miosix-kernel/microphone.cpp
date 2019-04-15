#include "microphone.h"
#include <miosix.h>
#include <functional>
#include "miosix/kernel/scheduler/scheduler.h"

// Keep only 1 / DECIMATION_FACTOR of audio samples
#define DECIMATION_FACTOR 4

// Number of samples to be processed at time
#define SAMPLES_AT_TIME 5000 

using namespace std;
using namespace miosix;

typedef Gpio<GPIOB_BASE, 10> clk;
typedef Gpio<GPIOC_BASE, 3> dout;

static const int bufferSize = 512; // Buffer RAM is 4 * bufferSize bytes
static const int bufNum = 2;
static Thread *waiting;
static BufferQueue<unsigned short, bufferSize, bufNum> *bq;
static bool enobuf = true;
static const char filterOrder = 4;
static const short oversample = 16;
static unsigned short intReg[filterOrder] = {0,0,0,0};
static unsigned short combReg[filterOrder] = {0,0,0,0};
static signed char pdmLUT[] = {-1, 1};


/**
 * Configure the DMA to do another transfer
 */
static void IRQdmaRefill() {
    unsigned short *buffer;
    
    if (bq->tryGetWritableBuffer(buffer) == false) {
        enobuf=true;
        return;
    }
    
    DMA1_Stream3->CR=0;
    DMA1_Stream3->PAR=reinterpret_cast<unsigned int>(&SPI2->DR);
    DMA1_Stream3->M0AR=reinterpret_cast<unsigned int>(buffer);
    DMA1_Stream3->NDTR=bufferSize;
    DMA1_Stream3->CR=DMA_SxCR_PL_1    | //High priority DMA stream
                     DMA_SxCR_MSIZE_0 | //Write 16bit at a time to RAM
                     DMA_SxCR_PSIZE_0 | //Read 16bit at a time from SPI
                     DMA_SxCR_MINC    | //Increment RAM pointer
                     DMA_SxCR_TCIE    | //Interrupt on completion
                     DMA_SxCR_EN;       //Start the DMA
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
    
    while (bq->tryGetReadableBuffer(result, size) == false) {
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
    DMA1->LIFCR=DMA_LIFCR_CTCIF3  |
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
    PCMsize = SAMPLES_AT_TIME * DECIMATION_FACTOR;
    compressed_buf_size_bytes = (PCMsize / DECIMATION_FACTOR) / 2;
}


void Microphone::start(function<void (unsigned char*, int)> callback) {
    this->callback = callback;
    recording = true;
    readyBuffer = (short*) malloc(sizeof(short) * PCMsize);
    processingBuffer = (short*) malloc(sizeof(short) * PCMsize);
    
    // I put 1 sample every DECIMATION_FACTOR - 1 samples
    decimatedReadyBuffer = (short*) malloc(sizeof(short) * PCMsize / DECIMATION_FACTOR);
    decimatedProcessingBuffer = (short*) malloc(sizeof(short) * PCMsize / DECIMATION_FACTOR);

    compressedBuf = (unsigned char*) malloc(compressed_buf_size_bytes*sizeof(char));
    
    {
        FastInterruptDisableLock dLock;
        
        // Enable DMA1 and SPI2/I2S2 and GPIOB and GPIOC
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;   
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
        
        // Configure GPIOs
        clk::mode(Mode::ALTERNATE);
        clk::alternateFunction(5);
        clk::speed(Speed::_50MHz);
        
        dout::mode(Mode::ALTERNATE);
        dout::alternateFunction(5);
        dout::speed(Speed::_50MHz);
        
        
        // I2S PLL Clock Frequency: 135.5 Mhz
        RCC->PLLI2SCFGR=(2<<28) | (271<<6);
        
        RCC->CR |= RCC_CR_PLLI2SON;
    }
    
    // Wait for PLL to lock
    while ((RCC->CR & RCC_CR_PLLI2SRDY) == 0);
    
    // RX buffer not empty interrupt enable
    SPI2->CR2 = SPI_CR2_RXDMAEN;  
    
    SPI2->I2SPR = SPI_I2SPR_MCKOE | 12;

    // Configure SPI
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_0 | SPI_I2SCFGR_I2SCFG_1 | SPI_I2SCFGR_I2SE;

    NVIC_SetPriority(DMA1_Stream3_IRQn, 2); // High priority for DMA

    // Wait for the microphone to enable
    delayMs(10);
    
    pthread_create(&mainLoopThread, NULL, mainLoopLauncher, reinterpret_cast<void*>(this));
}


void Microphone::stop() {
    // Stop the software
    recording = false;
    
    // Wait for the last PCM processing to end
    pthread_join(mainLoopThread, NULL);
    
    // Reset the configuration registers to stop the hardware
    NVIC_DisableIRQ(DMA1_Stream3_IRQn);
    delete bq;
    SPI2->I2SCFGR=0;
    
    {
        FastInterruptDisableLock dLock;
        RCC->CR &= ~RCC_CR_PLLI2SON;
    }
    
    free(readyBuffer);
    free(processingBuffer);
    free(decimatedProcessingBuffer);
    free(decimatedReadyBuffer);
}


void* Microphone::mainLoopLauncher(void* arg){
    reinterpret_cast<Microphone*>(arg)->mainLoop();
    return 0;
}


void Microphone::mainLoop(){
    waiting = Thread::getCurrentThread();
    pthread_t cback;
    bq = new BufferQueue<unsigned short,bufferSize, bufNum>();
    NVIC_EnableIRQ(DMA1_Stream3_IRQn);  
    
    // Create the thread that will execute the callbacks 
    pthread_create(&cback, NULL, callbackLauncher, reinterpret_cast<void*>(this));
    isBufferReady = false;
    
    // Variable used for swap of processing and ready buffer
    short *tmp, *decimatedTmp;

    while (recording) {
        PCMindex = 0;      
        // process any new chunk of PDM samples
        for (;;){
            if (enobuf) {
                enobuf = false;
                dmaRefill();
            }
            
            if (processPDM(getReadableBuffer(), bufferSize) == true){
                // Transcode until the specified number of PCM samples
                break;
            }
            
            bufferEmptied();
        }
        
        // Swap the ready and the processing buffer: allows double buffering
        // on the callback side
        tmp = readyBuffer;
        decimatedTmp = decimatedReadyBuffer;

        readyBuffer = processingBuffer;
        decimatedReadyBuffer = decimatedProcessingBuffer;
        // perform encoding using ADPCM codec
        encode(&state, decimatedReadyBuffer, PCMsize / DECIMATION_FACTOR, compressedBuf);

        // Start critical section
        pthread_mutex_lock(&bufMutex);
        isBufferReady = true;
        pthread_cond_broadcast(&cbackExecCond);
        pthread_mutex_unlock(&bufMutex);
        // End critical section
        
        processingBuffer = tmp;
        decimatedProcessingBuffer = tmp;
    }
    
    pthread_cond_broadcast(&cbackExecCond);
    pthread_join(cback, NULL);
}


void* Microphone::callbackLauncher(void* arg){
    reinterpret_cast<Microphone*>(arg)->execCallback();
    return 0;
}


void Microphone::execCallback() {
    while (recording) {
        pthread_mutex_lock(&bufMutex);
        
        while(recording && !isBufferReady)
            pthread_cond_wait(&cbackExecCond, &bufMutex);
        
        callback(compressedBuf, compressed_buf_size_bytes);
        isBufferReady = false;
        pthread_mutex_unlock(&bufMutex);
    }
}


bool Microphone::processPDM(const unsigned short *pdmbuffer, int size) {
    int decimatedIndex;
    int remaining = PCMsize - PCMindex;
    int length = remaining < size ? remaining : size; 
    short int s, sHigh, valueBeforeJump, jumpValue;
    bool jumpMade = false;

    for (int i=0; i < length; i++){    
        // Convert couples 16 pdm one-bit samples in one signed 16-bit PCM sample
        s = PDMFilter(pdmbuffer, i);
        processingBuffer[PCMindex] = s;
        decimatedIndex = PCMindex / DECIMATION_FACTOR;
        
        if(PCMindex - (decimatedIndex * DECIMATION_FACTOR) == 0) {
            decimatedProcessingBuffer[decimatedIndex] = s;
        }
        
        PCMindex++;
    }
    
    if (PCMindex < PCMsize) // If produced PCM sample are not enough 
        return false;
    
    return true;    
}


/*
 * This function takes care of the transcoding from 16 PDM bit to 1 PCM sample
 * via CIC filtering. Decimator rate: 16:1, CIC stages: 4.
 */
unsigned short Microphone::PDMFilter(const unsigned short* PDMBuffer, unsigned int index) {
    short combInput, combRes;
    
    // Perform integration on the first word of the PDM chunk to be filtered
    for (short i=0; i < 16; i++){
        // Integrate each single bit
        intReg[0] += pdmLUT[(PDMBuffer[index] >> (15-i)) & 1];
        
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