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

#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <functional>

using namespace std;

class Microphone {
public:

    Microphone() = delete;
    
    /**
     * Start the recording.
     *
     * @param callback      function to be called when the samples are ready (the function will
     *                      receive, as parameters, the samples data pointer and its length)
     * @param bufferSize    how many samples to collect before executing the callback
     *
     * @return true if the recording process has started successfully; false otherwise
     */
    static bool start(function<void (short*, unsigned int)> callback, unsigned int bufferSize);
    
    /**
     * Stop the recording.
     */
    static void stop();

};

#endif /* MICROPHONE_H */
