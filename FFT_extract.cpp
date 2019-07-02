#include <stdio.h>
#include <stdlib.h>

#define FFT_SIZE 1024
#define BIN_AMOUNT (FFT_SIZE / 2)

int main(int argc, char **argv) {
	char defaultInputName[] = "fft.raw";
	char defaultOutputName[] = "fft.csv";
	
	char *inputName, *outputName;
	
	if (argc >= 2) {
		inputName = argv[1];
	} else {
		printf("[INFO] No input file specified. Using %s.\n", defaultInputName);
		inputName = defaultInputName;
	}
	
	if (argc >= 3) {
		outputName = argv[2];
	} else {
		printf("[INFO] No output file specified. Using %s.\n", defaultOutputName);
		outputName = defaultOutputName;
	}
	
	FILE *input = fopen(inputName, "rb");
	
	if (input == NULL) {
		printf("[ERROR] Can't open %s.\n", inputName);
		exit(EXIT_FAILURE);
	}
	
	FILE *output = fopen(outputName, "w");
	
	if (output == NULL) {
		printf("[ERROR] Can't open %s.\n", outputName);
		exit(EXIT_FAILURE);
	}
	
	printf("Converting RAW FFT data to tab separated values.\n");
	
	float buffer[BIN_AMOUNT];
	int n;
	
	while ((n = fread(&buffer[0], sizeof(float), BIN_AMOUNT, input)) > 0) {
		for (int i = 0; i < n; i++) {
			fprintf(output, "%.6f\t", buffer[i]);
		}
		
		fprintf(output, "\n");
	}
	
	printf("Conversion finished.\n");
	
	return EXIT_SUCCESS;
}

