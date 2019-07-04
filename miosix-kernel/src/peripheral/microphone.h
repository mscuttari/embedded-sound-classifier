#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <miosix.h>
#include <functional>

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
    void start(function<void (short*, unsigned int)> callback, unsigned int buffsize);
    
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

    //function<void (unsigned char*, unsigned int)> callback;
    function<void (short*, unsigned int)> callback;
    static void* callbackLauncher(void* arg);
    void execCallback();
    
    bool processPdm(const unsigned short *pdmBuffer, int size);
    short PDMFilter(const unsigned short* pdmBuffer, unsigned int index);
    
    // Variables used to track and store the transcoding progress
    unsigned int PCMsize;
    unsigned int PCMindex;
    
    // The buffers handling the double buffering "callback-side"
    short *readyBuffer;
    short *processingBuffer;
    
    volatile bool isBufferReady;
    pthread_mutex_t bufMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cbackExecCond = PTHREAD_COND_INITIALIZER;

};

#endif /* MICROPHONE_H */
