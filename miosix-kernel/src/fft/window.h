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

#ifndef WINDOW_H
#define WINDOW_H

#include <cstdio>

class WindowFunction {
public:
    explicit WindowFunction(size_t size) : size(size) {};

    /**
     * Get the window size
     */
    size_t getSize();

    /**
     * Apply the window function to a value given its index
     */
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

#endif /* WINDOW_H */
