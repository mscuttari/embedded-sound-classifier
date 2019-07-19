/**************************************************************************
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

#include <cstdio>
#include <miosix.h>
#include <functional>
#include <termios.h>
#include "fft/fft.h"
#include "fft/window.h"
#include "neural-network/network.h"
#include "neural-network/network_data.h"
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



// FFT
#define FFT_SIZE 1024
static FFT* fft;


// Neural network
static ai_handle network = AI_HANDLE_NULL;
static ai_u8 nn_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
static ai_buffer nn_input[AI_NETWORK_IN_NUM] = { AI_NETWORK_IN_1 };
static ai_buffer nn_output[AI_NETWORK_OUT_NUM] = { AI_NETWORK_OUT_1 };
static ai_float nn_outData[AI_NETWORK_OUT_1_SIZE];

typedef enum {NONE, SILENCE, WHISTLE, CLAP} state_t;
static state_t state;


int main() {
    try {
        // Initialize the FFT structure
        static FFT mFFT(FFT_SIZE);
        fft = &mFFT;

    } catch (exception &e) {
        printf("%s\r\n", e.what());
        while (true);
    }

    // Peripherals setup
    Crc::init();
    setRawStdout();

    // Neural network setup
    ai_error aiError = ai_network_create(&network, (ai_buffer*) AI_NETWORK_DATA_CONFIG);

    if (aiError.type == AI_ERROR_NONE) {
        printf("Neural network created\r\n");
    } else {
        printf("Neural network creation error - type = %lu, code = %lu\r\n", aiError.type, aiError.code);
        while (true);
    }

    const ai_network_params networkParams = AI_NETWORK_PARAMS_INIT(
            AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
            AI_NETWORK_DATA_ACTIVATIONS(nn_activations)
    );

    if (ai_network_init(network, &networkParams)) {
        printf("Neural network initialized\r\n");
    } else {
        ai_error error = ai_network_get_error(network);
        printf("Neural network initialization error - type = %lu, code = %lu\r\n", error.type, error.code);
        while(true);
    }

    nn_input[0].n_batches = 1;
    nn_input[0].data = AI_HANDLE_PTR(fft->getBins());
    nn_output[0].n_batches = 1;
    nn_output[0].data = AI_HANDLE_PTR(nn_outData);

    // Main loop
    while (true) {
        // Start the recording on user button press
        UserButton::wait();
        state = NONE;
        sendStartSignal();
        function<void (short*, unsigned int)> callback = bind(scanAudio, placeholders::_1, placeholders::_2);
        Microphone::start(callback, fft->getSize());

        // Stop on second button press
        UserButton::wait();
        Microphone::stop();
        sendStopSignal();
    }
}


void setRawStdout() {
    struct termios t{};
    tcgetattr(STDOUT_FILENO, &t);
    t.c_lflag &= ~(ISIG | ICANON);
    tcsetattr(STDOUT_FILENO,TCSANOW, &t);
}


void sendStartSignal() {
    printf("#start\r\n");

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
        printf("#stop\r\n");
    #endif
}


void scanAudio(short* data, unsigned int n) {
    static HannWindow hann(FFT_SIZE);

    for (unsigned int i = 0; i < n; i++) {
        float value = normalize<short>(data[i], true);
        value = hann.apply(value, i);
        fft->addSample(value);;
    }

    // If the data is not enough, fill with zeros
    for (int i = n; i < fft->getSize(); i++) {
        fft->addSample(0);
    }

    fft->process();

    #ifdef TRAINING
        int s = FFT_SIZE / 2 * sizeof(float);
        write(STDOUT_FILENO, &s, sizeof(int));
        write(STDOUT_FILENO, fft->getBins(), s);
    #else
        ai_network_run(network, &nn_input[0], &nn_output[0]);

        if (nn_outData[0] > nn_outData[1] && nn_outData[0] > nn_outData[2]) {
            if (state != SILENCE) {
                state = SILENCE;
            }

        } else if (nn_outData[1] > nn_outData[0] && nn_outData[1] > nn_outData[2]) {
            if (state != WHISTLE) {
                state = WHISTLE;
                printf("Whistle\r\n");
            }

        } else {
            if (state != CLAP) {
                state = CLAP;
                printf("Clap\r\n");
            }
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
        for (unsigned int j = 0; j < 8; j++) {
            *((char*) normalizationFactor + i) |= 1u << j;
        }
    }

    // Set the first bit to 0 in case of signed type
    if (sign) {
        *normalizationFactor &= ~(1ull << (8 * sizeof(T) - 1));
    }

    // Normalize
    result = value / ((float) *normalizationFactor + 1);

    free(normalizationFactor);
    return result;
}