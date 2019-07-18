#include <cstdio>
#include <miosix.h>
#include <functional>
#include <termios.h>
#include "fft/fft.h"
#include "fft/window.h"
#include "neural-network/network.h"
#include "neural-network/network_data.h"
#include "neural-network/app_x-cube-ai.h"
#include "peripheral/button.h"
#include "peripheral/microphone.h"
#include "peripheral/crc.h"


// Uncomment to switch to FFT data transfer mode.
// To be used to get the data to train the neural network.
//#define TRAINING


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


/**
 * Feed the neural network
 *
 * @param input         input buffer
 * @param output        output buffer
 *
 * @return number of input tensors processed
 */
int nnRun(const ai_float *input, const ai_float *output);


// FFT
#define FFT_SIZE 1024
static FFT_F32_t FFT;
static float fftInput[FFT_SIZE * 2];
static float fftOutput[FFT_SIZE];
static HannWindow hann(FFT_SIZE);


// Neural network
static ai_handle network = AI_HANDLE_NULL;
static ai_u8 nn_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
static ai_buffer nn_input[AI_NETWORK_IN_NUM] = { AI_NETWORK_IN_1 };
static ai_buffer nn_output[AI_NETWORK_OUT_NUM] = { AI_NETWORK_OUT_1 };


int main() {
    // Peripherals setup
    Microphone& microphone = Microphone::getInstance();
    Crc::init();
    setRawStdout();

    // FFT structure setup
    FFT_Init_F32(&FFT, FFT_SIZE, 0);
    FFT_SetBuffers_F32(&FFT, fftInput, fftOutput);

    // Neural network setup
    ai_error aiError = ai_network_create(&network, (ai_buffer*) AI_NETWORK_DATA_CONFIG);

    if (aiError.type != AI_ERROR_NONE) {
        printf("Network creation error - type = %lu, code = %lu\r\n", aiError.type, aiError.code);
        while(true);

    } else {
        ai_network_report report;
        ai_bool res = ai_network_get_info(network, &report);

        if (res) {
            printf("Network creation successful\r\n");
            printf("MACC: %lu\r\n", report.n_macc);
            printf("Nodes: %lu\r\n", report.n_nodes);
        }
    }

    const ai_network_params networkParams = AI_NETWORK_PARAMS_INIT(
            AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
            AI_NETWORK_DATA_ACTIVATIONS(nn_activations)
    );

    if (!ai_network_init(network, &networkParams)) {
        ai_error error = ai_network_get_error(&network);
        printf("Network initialization error - type = %lu, code = %lu\r\n", error.type, error.code);
        while(true);
    } else {
        printf("Network initialized\r\n");
    }


    while (true) {
        // Start the recording on user button press
        UserButton::wait();
        sendStartSignal();
        microphone.start(bind(scanAudio, placeholders::_1, placeholders::_2), FFT_SIZE);

        // Stop on second button press
        UserButton::wait();
        microphone.stop();
        sendStopSignal();
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

    // If the data is not enough, fill with zeros
    for (int i = n; i < FFT_SIZE; i++) {
        FFT_AddToBuffer(&FFT, 0);
    }

    FFT_Process_F32(&FFT);

    #ifdef TRAINING
        int s = FFT_SIZE / 2 * sizeof(float);
        write(STDOUT_FILENO, &s, sizeof(int));
        write(STDOUT_FILENO, fftOutput, s);
    #else
        ai_float output[3];
        nnRun(fftOutput, output);
        static float counter = 0;
        counter += output[0] + output[1] + output[2];
        printf("%.6f\r\n", counter);
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


int nnRun(const ai_float *input, const ai_float *output) {
    nn_input[0].n_batches = 1;
    nn_input[0].data = AI_HANDLE_PTR(input);

    nn_output[0].n_batches = 1;
    nn_output[0].data = AI_HANDLE_PTR(output);

    return ai_network_run(&network, &nn_input[0], &nn_output[0]);
}
