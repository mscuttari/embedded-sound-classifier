#include <cstdio>
#include <miosix.h>
#include <functional>
#include <termios.h>
#include "fft/fft.h"
#include "fft/window.h"
#include "peripheral/button.h"
#include "peripheral/microphone.h"


// Uncomment to switch to FFT data transfer mode.
// To be used to get the data to train the neural network.
#define TRAINING


using namespace std;
using namespace miosix;


/**
 * Configure stdout in RAW mode
 */
void setRawStdout();


/**
 * Write to the serial port
 *
 * @param str   text to be sent
 */
void print(const char* str);


/**
 * Write the start signal to the serial port
 */
void sendStartSignal();


/**
 * Write the stop signal to the serial port
 */
void sendStopSignal();


/**
 * Elaborate the recorded audio
 *
 * @param data  data chunk
 * @param n     samples amount
 */
void scanAudio(short* data, unsigned int n);


/**
 * Normalize a value according to its type
 *
 * @tparam T        data type
 * @param value     value to be normalized
 * @param sign      true if the data type is signed; false otherwise
 * @return normalized value
 */
template<typename T>
float normalize(T value, bool sign);


#define FFT_SIZE 1024               // FFT window size
static FFT_F32_t FFT;               // FFT data structure
static HannWindow hann(FFT_SIZE);   // Window function


int main() {
    FFT_Init_F32(&FFT, FFT_SIZE, 1);

    // Peripherals setup
    Microphone& microphone = Microphone::getInstance();
    setRawStdout();

    // Start the recording on user button press
    UserButton::wait();
    sendStartSignal();
    microphone.start(bind(scanAudio, placeholders::_1, placeholders::_2), FFT_SIZE);

    // Stop on second button press
    UserButton::wait();
    microphone.stop();
    sendStopSignal();

    // Endless loop
    while (true) {
        
    }
}


void setRawStdout() {
    struct termios t{};
    tcgetattr(STDOUT_FILENO, &t);
    t.c_lflag &= ~(ISIG | ICANON);
    tcsetattr(STDOUT_FILENO,TCSANOW, &t);
}


void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}


void sendStartSignal() {
    print("#start\n");

    #ifdef TRAINING
        int value = FFT_SIZE / 2 * sizeof(float);
        write(STDOUT_FILENO, &value, sizeof(int));
    #endif
}


void sendStopSignal() {
    #ifdef TRAINING
        int value = 0;
        write(STDOUT_FILENO, &value, sizeof(int));
    #else
        print("#stop\n");
    #endif
}


void scanAudio(short* data, unsigned int n) {
    for (unsigned int i = 0; i < n; i++) {
        float value = normalize<short>(data[i], true);
        value = hann.apply(value, i);
        FFT_AddToBuffer(&FFT, value);
    }

    for (int i = n; i < FFT_SIZE; i++) {
        FFT_AddToBuffer(&FFT, 0);
    }

    FFT_Process_F32(&FFT);

    #ifdef TRAINING
        int s = FFT_SIZE / 2 * sizeof(float);
        write(STDOUT_FILENO, &s, sizeof(int));

        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float value = FFT_GetFromBuffer(&FFT, i);
            write(STDOUT_FILENO, &value, sizeof(float));
        }
    #endif
}


template<typename T>
float normalize(T value, bool sign) {
    float result;

    // Determine the divisor
    T *normalizationFactor = (T*) malloc(sizeof(T));

    // Set all the bits to 1
    for (unsigned int i = 0; i < sizeof(T); i++) {
        for (int j = 0; j < 8; j++) {
            *((char*) normalizationFactor + i) |= 1 << j;
        }
    }

    // Set the first bit to 0 in case of signed type
    if (sign) {
        *normalizationFactor &= ~(1ULL << (8 * sizeof(T) - 1));
    }

    // Normalize
    result = value / ((float) *normalizationFactor + 1);

    free(normalizationFactor);
    return result;
}
