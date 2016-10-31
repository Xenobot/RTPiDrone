#ifndef H_DRONE_EKF
#define H_DRONE_EKF
#include "RTPiDrone_Quaternion.h"
#include "RTPiDrone_DataExchange.h"

typedef struct Drone_EKF Drone_EKF;

int Drone_EKF_Init(Drone_EKF**);
void Drone_EKF_Delete(Drone_EKF**);
void Drone_EKF_Update(Drone_EKF*, Drone_DataExchange*);
#endif
