#include "window.h"

size_t WindowFunction::getSize() {
    return size;
}

HannWindow::HannWindow(int size) : WindowFunction(size) {
    multipliers = (double*) malloc(size * sizeof(double));

    for (int i = 0; i < size; i++) {
        multipliers[i] = 0.5 * (1 - cos(2 * M_PI * i / (size - 1)));
    }
}

HannWindow::~HannWindow() {
    free(multipliers);
}

float HannWindow::apply(float value, int index) {
    return value * multipliers[index];
}