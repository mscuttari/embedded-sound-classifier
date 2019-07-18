#ifndef FFT_H
#define FFT_H

/**
 * The architecture is defined at compile time by using compile options.
 * See miosix/config/CMakeLists.txt for further details.
 */
#ifdef _ARCH_ARM7_LPC2000
#define ARM_MATH_CM7

#elif _ARCH_CORTEXM3_STM32
#define ARM_MATH_CM3

#elif _ARCH_CORTEXM4_STM32F4
#define ARM_MATH_CM4

#elif _ARCH_CORTEXM3_STM32F2
#define ARM_MATH_CM3

#elif _ARCH_CORTEXM3_STM32L1
#define ARM_MATH_CM3

#elif _ARCH_CORTEXM3_EFM32GG
#define ARM_MATH_CM3

#elif _ARCH_CORTEXM7_STM32F7
#define ARM_MATH_CM7

#elif _ARCH_CORTEXM7_STM32H7
#define ARM_MATH_CM7

#else
#error Invalid architecture
#endif


#include <interfaces/arch_registers.h>
#include <CMSIS/Include/arm_math.h>

class FFT {
public:

    /**
     * Constructor
     *
     * @param windowSize    number of samples to be used for FFT calculation
     */
    explicit FFT(uint16_t windowSize);


    /**
     * Destructor.
     * Frees the input and the output buffers.
     */
    ~FFT();


    /**
     * Add a new sample to the input buffer
     *
     * @param value     value to be added (real part; the imaginary part is automatically set to 0)
     * @return true if the input buffer is full and samples are ready to be processed;
     *         false if the input buffer is not full yet
     */
    bool addSample(float32_t value);


    /**
     * Process and calculate FFT from the input buffer and store the data in the output buffer.
     */
    void process();


    /**
     * Get FFT size in units of samples length.
     *
     * @return window size
     */
    uint16_t getSize();


    /**
     * Get the array of the output bins.
     *
     * @return output buffer
     */
    const float32_t* getBins();


    /**
     * Get value of a specific bin.
     *
     * @param index     index of the output buffer
     * @return bin value
     */
    float32_t getBin(uint16_t index);


private:
    uint16_t size;                      // Window size in units of samples. This parameter should be a value of 2^n, where n must between 4 and 12.
    float32_t* input;                   // Data input buffer. Its length is 2 * windowSize.
    float32_t* output;                  // Data output buffer. Its length is windowSize.
    const arm_cfft_instance_f32* s;     // Pointer to arm_cfft_instance_f32 structure.
    uint32_t count;                     // Number of samples in input buffer.
};

#endif /* FFT_H */
