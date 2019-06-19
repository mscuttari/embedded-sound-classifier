#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <math.h>
#include <numeric>
#include "kiss_fft.h"

// Input file. Must be a 16 bit signed PCM audio file
#define FILE_NAME "1000hz.pcm"

// Set to 1 in case of stereo sound
#define STEREO 0

// Buffer size
#define BUFFER_SIZE 4096

// Sensibility for peak detection (the lower, the more sensible)
#define PEAK_THRESHOLD 3.95

// Peak distance (how much to wait, in number of samples, in order to identify two separate peaks)
#define PEAK_DISTANCE 512

// Size of the window for the FFT
#define FFT_WINDOW_SIZE 512


using namespace std;


class WindowFunction {
public:
	WindowFunction(size_t size) : size(size) {};
	
	size_t getSize() {
		return size;
	}
	
	virtual float apply(float value, int index) = 0;
	virtual float* apply(float* value) = 0;
	
protected:
	size_t size;
};


class HannWindow : public WindowFunction {
public:
	HannWindow(int size) : WindowFunction(size) {
		multipliers = (double*) malloc(size * sizeof(double));
		
		for (int i = 0; i < size; i++) {
			multipliers[i] = 0.5 * (1 - cos(2 * M_PI * i / (size - 1)));
		}
	}
	
	~HannWindow() {
		free(multipliers);
	}
	
	float apply(float value, int index) {
		return value * multipliers[index];
	}
	
	float* apply(float* values) {
		for (int i = 0; i < this->size; i++) {
			values[i] = apply(values[i], i);
		}
	}
	
private:
	double* multipliers;
};


/**
 * Get the mean of a set of values
 * 
 * @param v		values
 * @return mean
 */
template<typename T>
float mean(const vector<T> &v) {
	return accumulate(v.begin(), v.end(), 0.0) / v.size();
}


/**
 * Get the standard deviation of a set of values
 * 
 * @param v		values
 * @return standard deviation
 */
template<typename T>
float stdDev(const vector<T> &v) {
    vector<float> diff(v.size());
    float m = mean(v);
    
    for (size_t i = 0; i < v.size(); i++) {
        diff[i] = v[i] - m;
	}
    
    float sq_sum = inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    return sqrt(sq_sum / v.size());
}


/**
 * Detect the peaks in a data source
 *
 * @param input		input signal
 * @return positions of the peaks
 */
vector<size_t> getPeaks(vector<short> input) {   
    // Lag 5 for the smoothing functions
    int lag = 5;
    
    // Between 0 and 1, where 1 is normal influence, 0.5 is half
    float influence = 0.1;

    if (input.size() <= lag + 2) {
        vector<size_t> emptyVector;
        return emptyVector;
    }

    // Initialize variables
    vector<int> signals(input.size(), 0.0);
    vector<float> filteredY(input.size(), 0.0);
    vector<float> avgFilter(input.size(), 0.0);
    vector<float> stdFilter(input.size(), 0.0);
    
	float m1 = mean(input);		// Mean
    float m2 = stdDev(input);		// Standard deviation

	// Analyze data
    for (size_t i = lag + 1; i < input.size(); i++) {
        if (abs(input[i] - m1) > PEAK_THRESHOLD * m2) {
        	// Peak detected
            signals[i] = true;
            
            // Make influence lower
            //filteredY[i] = influence* input[i] + (1 - influence) * filteredY[i - 1];
            
        } else {
            signals[i] = false;
            filteredY[i] = input[i];
        }
        
        // Adjust the filters
        vector<float> subVec(filteredY.begin() + i - lag, filteredY.begin() + i);
        avgFilter[i] = mean(subVec);
        stdFilter[i] = stdDev(subVec);
    }
    
    // Remove false positives
    vector<size_t> peaks;
    long long start = -1, end = -1;
    
    for (size_t i = 0; i < signals.size(); i++) {
    	if (signals[i] && start == -1) {
    		// New peak
    		start = i;
    		end = i;
    		
		} else if (signals[i] && end != -1) {
			// A peak has been previously detected. Extend its boundaries
			end = i;
			
		} else if (!signals[i] && start != -1 && i - end > PEAK_DISTANCE) {
			// The previous peak has ended.
			// Save its position and reset the boundaries for the next one.
			peaks.push_back((start + end) / 2);
			
			start = -1;
			end = -1;
		}
	}
	
	if (end != -1) {
		// Last peak found
		peaks.push_back((start + end) / 2);
	}
    
    return peaks;
}


/**
 * Normalize a value according to its type
 * 
 * @param value		value to be normalized
 * @param sign		whether the value is signed or unsigned
 *
 * @return the value normalized (between -1 and 1, both excluded)
 */
template<typename T>
float normalize(T value, bool sign) {
	float result;
	
	// Determine the divisor
	T *normalizationFactor = (T*) malloc(sizeof(T));
	
	// Set all the bits to 1
	for (int i = 0; i < sizeof(T); i++) {
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


/**
 * Perform a FFT by first applying a window function to the data
 *
 * @param data				data to be processed
 * @param windowFunction	Hann window function to be applied
 *
 * @return FFT values
 */
template<typename T>
vector<float> fft(vector<T> data, HannWindow &windowFunction) {
	vector<float> fft;
	size_t windowSize = windowFunction.getSize();
	
	// FFT structures
	kiss_fft_cfg cfg = kiss_fft_alloc(windowSize, 0, NULL, NULL);
	
	kiss_fft_cpx cx_in[windowSize];
	kiss_fft_cpx cx_out[windowSize];
	
	// Fill with 0
	memset(cx_in, 0, windowSize * sizeof(kiss_fft_cpx));
	
	// Insert the data in the middle
	for (int i = (windowSize - data.size()) / 2; i < (windowSize + data.size()) / 2; i++) {
		float value = normalize<T>(data[i], true);	// Normalization
		value = windowFunction.apply(value, i);		// Window function
		
		cx_in[i].r = value;
	}
	
	// Perform the FFT
    kiss_fft(cfg, cx_in, cx_out);
	
    // Save the results
    for (size_t i = 0; i < windowSize / 2; i++) {
    	float result = sqrt(pow(cx_out[i].r, 2) + pow(cx_out[i].i, 2));
    	fft.push_back(result);
	}
	
	free(cfg);
	
	return fft;
}


void fftPeaks(FILE *in, FILE *out, FILE *audioFile) {
	vector<vector<float> > ffts;
	
	// Buffer (doubled in case of stereo sound)
	vector<short> buffer(!STEREO ? BUFFER_SIZE : BUFFER_SIZE * 2);
	
	vector<short> data;
	vector<short> dataOld(FFT_WINDOW_SIZE, 0);
	
	// Window function
	HannWindow hann(FFT_WINDOW_SIZE);
	
	// Number of samples read in each iteration
	int n;
	
	// Read samples chunk by chunk
	while ((n = fread(&buffer[0], sizeof(short), buffer.size(), in)) > 0) {
		int samples_amount = !STEREO ? n : n / 2;
		data.resize(samples_amount, 0);
		
		for (size_t i = 0; i < samples_amount; i++) {
			data[i] = buffer[i + i * STEREO];
		}
		
		// Extract the peaks
		vector<size_t> peaks = getPeaks(data);
		printf("# peaks: %d\n", peaks.size());
		
		// Write the peaks to file and silence all the rest
		for (int i = 0, k = 0; i < data.size(); i++) {
			if (k >= peaks.size()) {
				for (int j = 0; j < data.size(); j++) {	
					short zero = 1024;
					fwrite(&zero, sizeof(short), 1, audioFile);
				}
				
				break;
			}
			
			int peakIndex = peaks[k];
			
			if (i + FFT_WINDOW_SIZE / 2 >= peakIndex) {
				while (i < data.size() && i < peakIndex + FFT_WINDOW_SIZE / 2) {
					fwrite(&data[i], sizeof(short), 1, audioFile);
					i++;
				}
				
				k++;
				
			} else {
				short zero = 1024;
				fwrite(&zero, sizeof(short), 1, audioFile);
			}
		}
		
		// Do a FFT on each peak
		for (size_t i = 0; i < peaks.size(); i++) {
			bool overLeft = peaks[i] < FFT_WINDOW_SIZE / 2;
			bool overRight = data.size() < peaks[i] + FFT_WINDOW_SIZE / 2;
			
			vector<short> peak(FFT_WINDOW_SIZE);
			
			if (overLeft) {
				for (int j = 0; j < FFT_WINDOW_SIZE / 2 - peaks[i]; j++) {
					peak[j] = dataOld[dataOld.size() - (FFT_WINDOW_SIZE / 2 - peaks[i]) + j];
				}
				
				for (int j = 0; j < peaks[i]; j++) {
					peak[j + FFT_WINDOW_SIZE / 2 - peaks[i]] = data[j];
				}
			} else {
				for (int j = 0; j < peak[i]; j++) {
					peak[j] = data[j];
				}
			}
			
			if (overRight) {
				for (int j = 0; j < data.size() - peaks[i]; j++) {
					peak[j + FFT_WINDOW_SIZE / 2] = data[peaks[i] + j];
				}
				
				for (int j = 0; j < peaks[i] + FFT_WINDOW_SIZE / 2 - data.size(); j++) {
					peak[j + peaks[i] + FFT_WINDOW_SIZE / 2 - data.size()] = 0;
				}
				
			} else {
				for (int j = 0; j < FFT_WINDOW_SIZE / 2; j++) {
					peak[j + FFT_WINDOW_SIZE / 2] = data[peaks[i] + j];
				}
			}
			
			vector<float> result = fft(peak, hann);
			ffts.push_back(result);
		}
		
		data.swap(dataOld);
	}
	
	// Write results to file
	for (int i = 0; i < ffts.size(); i++) {
		vector<float> fft = ffts[i];
		
		for (int j = 0; j < fft.size(); j++) {
			fprintf(out, "%+f", fft[j]);
			
			if (j < fft.size() - 1)
				fprintf(out, "\t");
		}
		
		fprintf(out, "\n");
	}
	
}


void readFFT(FILE *in, FILE *out) {
	vector<vector<float> > ffts;
	
	// Buffer (doubled in case of stereo sound)
	vector<short> buffer(!STEREO ? FFT_WINDOW_SIZE : FFT_WINDOW_SIZE * 2);
	
	// Window function
	HannWindow hann(FFT_WINDOW_SIZE);
	
	// Number of samples read in each iteration
	int n;
	
	// Read samples chunk by chunk
	while ((n = fread(&buffer[0], sizeof(short), buffer.size(), in)) > 0) {
		int samples_amount = min(FFT_WINDOW_SIZE, !STEREO ? n : n / 2);
		vector<short> data(samples_amount);
		
		for (size_t i = 0; i < samples_amount; i++) {
			data[i] = buffer[i + i * STEREO];
		}
		
		vector<float> result = fft(data, hann);
		ffts.push_back(result);
	}
	
	// Write results to file
	for (int i = 0; i < ffts.size(); i++) {
		vector<float> fft = ffts[i];
		
		for (int j = 0; j < fft.size(); j++) {
			fprintf(out, "%+f", fft[j]);
			
			if (j < fft.size() - 1)
				fprintf(out, "\t");
		}
		
		fprintf(out, "\n");
	}
}


int main() {
	// Get the output file name
	char *outputFile = (char*) malloc((strlen(FILE_NAME) + 9) * sizeof(char));
	sprintf(outputFile, "%s_FFT.csv", FILE_NAME);
	
	char *audioOutFile = (char*) malloc((strlen(FILE_NAME) + 11) * sizeof(char));
	sprintf(audioOutFile, "%s_peaks.pcm", FILE_NAME);
	
	// Open the input and the output file
	FILE *in = fopen(FILE_NAME, "rb");
	FILE *out = fopen(outputFile, "w");
	FILE *debug = fopen(audioOutFile, "wb");
	
	// Perform the analysis
	fftPeaks(in, out, debug);
	
	// Close the resources
	fclose(in);
	fclose(out);
	fclose(debug);
	
	return 0;
}

