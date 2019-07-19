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

#include "fft.h"
#include <stdexcept>
#include <stdlib.h>
#include <CMSIS/Include/arm_const_structs.h>


using namespace std;


/* Array with constants for CFFT module */
/* Requires ARM CONST STRUCTURES files */
const arm_cfft_instance_f32 CFFT_Instances[] = {
	{16, twiddleCoef_16, armBitRevIndexTable16, ARMBITREVINDEXTABLE__16_TABLE_LENGTH},
	{32, twiddleCoef_32, armBitRevIndexTable32, ARMBITREVINDEXTABLE__32_TABLE_LENGTH},
	{64, twiddleCoef_64, armBitRevIndexTable64, ARMBITREVINDEXTABLE__64_TABLE_LENGTH},
	{128, twiddleCoef_128, armBitRevIndexTable128, ARMBITREVINDEXTABLE_128_TABLE_LENGTH},
	{256, twiddleCoef_256, armBitRevIndexTable256, ARMBITREVINDEXTABLE_256_TABLE_LENGTH},
	{512, twiddleCoef_512, armBitRevIndexTable512, ARMBITREVINDEXTABLE_512_TABLE_LENGTH},
	{1024, twiddleCoef_1024, armBitRevIndexTable1024, ARMBITREVINDEXTABLE1024_TABLE_LENGTH},
	{2048, twiddleCoef_2048, armBitRevIndexTable2048, ARMBITREVINDEXTABLE2048_TABLE_LENGTH},
	{4096, twiddleCoef_4096, armBitRevIndexTable4096, ARMBITREVINDEXTABLE4096_TABLE_LENGTH}
};


FFT::FFT(uint16_t windowSize) {
    // Check for proper window size value
    size = 0;

    for (const auto & CFFT_Instance : CFFT_Instances) {
        if (windowSize == CFFT_Instance.fftLen) {
            // Valid window size found. Save the reference to CFFT instance
            size = windowSize;
            s = &CFFT_Instance;
            break;
        }
    }

    if (size == 0) {
        // Invalid window size
        throw invalid_argument("Invalid FFT size");
    }

    // Initially there are no samples
    count = 0;

    // Allocate input buffer. Its size is 2 * windowSize because each sample is composed by real
    // and imaginary part. The imaginary part is automatically set to 0 when adding the samples.
    input = (float32_t*) malloc((size * 2) * sizeof(float32_t));

    if (!input) {
        throw runtime_error("Input buffer allocation failed");
    }

    // Allocate output buffer
    output = (float32_t*) malloc(size * sizeof(float32_t));

    if (!output) {
        free(this->input);
        throw runtime_error("Output buffer allocation failed");
    }
}


FFT::~FFT() {
    if (input) {
        free(input);
    }

    if (output) {
        free(output);
    }
}


bool FFT::addSample(float32_t value) {
    // Check if memory available
    if (count < size) {
        // Add to buffer, real part
        input[2 * count] = value;

        // Imaginary part set to 0
        input[2 * count + 1] = 0;

        // Increase count
        count++;
    }

    // Check if buffer full
    return count >= size;
}


void FFT::process() {
    // Process input data
    arm_cfft_f32(s, input, 0, 1);

    // Process the data through the Complex Magnitude Module for calculating the magnitude at each bin
    arm_cmplx_mag_f32(input, output, size);

    // Reset count
    count = 0;
}


uint16_t FFT::getSize() {
    return size;
}


const float32_t* FFT::getBins() {
    return output;
}


float32_t FFT::getBin(uint16_t index) {
    return output[index];
}
