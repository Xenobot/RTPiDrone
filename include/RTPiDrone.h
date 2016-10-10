/*
    RTPiDrone -- RTPiDrone.h
    Copyright 2016 Wan-Ting CHEN (wanting@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef H_RTPIDRONE
#define H_RTPIDRONE

struct str_drone;
typedef struct str_drone Drone;

// Initialize the Drone
int Drone_init(Drone** rpiDrone);

// Calibrate the Drone
int Drone_Calibration(Drone* rpiDrone);

// If everything is fine, start the Drone
void Drone_Start(Drone* rpiDrone);

// If something abnormal happens or controller asks to stop, this step will terminate everything.
int Drone_End(Drone** rpiDrone);

#endif
