#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <functional>

using namespace std;

class Microphone {
public:

    Microphone() = delete;
    
    /**
     * Start the recording.
     *
     * @param callback      function to be called when the samples are ready (the function will
     *                      receive, as parameters, the samples data pointer and its length)
     * @param bufferSize    how many samples to collect before executing the callback
     *
     * @return true if the recording process has started successfully; false otherwise
     */
    static bool start(function<void (short*, unsigned int)> callback, unsigned int bufferSize);
    
    /**
     * Stop the recording.
     */
    static void stop();

};

#endif /* MICROPHONE_H */
