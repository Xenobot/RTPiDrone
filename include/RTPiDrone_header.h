/*!
 * \file    RTPiDrone_header.h
 * \brief   This file contains the basic parameters of the Drone.
 */

#ifndef H_DRONE_HEADER
#define H_DRONE_HEADER

#define DEBUG                                   /*! If DEBUG is defined, debug messages will be printed. */
//#define DEBUG_VALGRIND
//#define HMC5883L_PWM_CALI                     /*! If HMC5883L_PWM_CALI is defined, will calibrate PWM/MAG */
#define CONTROL_PERIOD      (4000000L)          /*! The period of one control cycle. */
#define KP                  (7.5f)              /*! PID -- P */
#define KI                  (0.7f)              /*! PID -- I */
#define KD                  (140.0f)            /*! PID -- D */
#define PWM_MAX             (3500)              /*! PWM Max value */
#define PWM_MIN             (1750)              /*! PWM Min value */
#define ADXL345_RATE        (400)               /*! ADXL345 data sampling rate */
#define L3G4200D_RATE       (400)               /*! L3G4200D data sampling rate */
//#define HMC5883L_SINGLEMEASUREMENT
#define HMC5883L_RATE       (75)
#define HMC5883L_PERIOD     (1000000000L/HMC5883L_RATE)
#define BMP085_PeriodLong   (25500000L)
#define BMP085_PeriodShort  (4500000L)
#define MS5611_Period       (10000000L)
#define PWM_CONTROLPERIOD   (2)
#endif
