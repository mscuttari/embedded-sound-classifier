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

#include "window.h"
#include <cstdlib>
#include <math.h>

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