#ifndef MICROPHONE_H
#define MICROPHONE_H

// IDE bug ("function" type not recognised)
#ifndef __GXX_EXPERIMENTAL_CXX0X__
#define __GXX_EXPERIMENTAL_CXX0X__
#endif

#include <miosix.h>
#include <functional>
#include "codec.h"

using namespace std;
using namespace miosix;

class Microphone {
public:
    
    /**
     * Get a singleton instance
     */
    static Microphone& getInstance();
    
    // Avoid copies (singleton instance)
    Microphone(Microphone const&) = delete;
    void operator = (Microphone const&) = delete;
    
    /**
     * Start the recording
     */
    void start(function<void (unsigned char*, int)> callback);
    
    /**
     * Stop the recording
     */
    void stop();

private:
    /**
     * Private constructor (singleton instance)
     */
    Microphone();
    
    volatile bool recording;
    
    pthread_t mainLoopThread;
    static void* mainLoopLauncher(void* arg);
    void mainLoop();
    
    function<void (unsigned char*, int)> callback;
    static void* callbackLauncher(void* arg);
    void execCallback();
    
    bool processPDM(const unsigned short *pdmbuffer, int size);
    unsigned short PDMFilter(const unsigned short* PDMBuffer, unsigned int index);
    
    // Variables used to track and store the transcoding progress
    unsigned int PCMsize;
    unsigned int PCMindex;
    
    // The buffers handling the double buffering "callback-side"
    short* readyBuffer;
    short* processingBuffer;
    
    volatile bool isBufferReady;
    pthread_mutex_t bufMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cbackExecCond = PTHREAD_COND_INITIALIZER; 
    
    // Buffers used to perform decimation 
    short int* decimatedReadyBuffer;
    short int* decimatedProcessingBuffer;
    
    // ADPCM encoder data
    unsigned char* compressedBuf;
    CodecState state;
    int compressed_buf_size_bytes;

};

#endif /* MICROPHONE_H */
