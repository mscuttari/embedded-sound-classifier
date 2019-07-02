#ifndef NEURALNETWORK_WINDOW_H
#define NEURALNETWORK_WINDOW_H

#include <cstdio>
#include <cstdlib>
#include <math.h>

class WindowFunction {
public:
    explicit WindowFunction(size_t size) : size(size) {};

    /** Get the window size */
    size_t getSize();

    /** Apply the window function to a value given its index */
    virtual float apply(float value, int index) = 0;

protected:
    size_t size;
};


class HannWindow : public WindowFunction {
public:
    explicit HannWindow(int size);
    ~HannWindow();
    float apply(float value, int index) override;

private:
    double* multipliers;
};

#endif //NEURALNETWORK_WINDOW_H
