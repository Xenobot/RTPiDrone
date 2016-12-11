#include "RTPiDrone_DataExchange.h"
#include "Common.h"
#include <string.h>
#include <math.h>
#define LINESIZE        512
#define LINETEMPSIZE    128

static char LINE[LINESIZE], LINETEMP[LINETEMPSIZE];
static float T_temp;

int Drone_DataExchange_Init(Drone_DataExchange** data)
{
    *data = (Drone_DataExchange*)calloc(1, sizeof(Drone_DataExchange));
    return 0;
}

void Drone_DataExchange_End(Drone_DataExchange** data)
{
    free(*data);
    data = NULL;
}

void Drone_DataExchange_Print(Drone_DataExchange* data)
{
    printf("Acc: %f, %f, %f\n", data->acc[0], data->acc[1], data->acc[2]);
    printf("Gyr: %f, %f, %f\n", data->gyr[0], data->gyr[1], data->gyr[2]);
    printf("Mag: %f, %f, %f\n", data->mag[0], data->mag[1], data->mag[2]);
    printf("Attitide: %f, temperature: %f, pressure: %f\n", data->attitude, data->temperature, data->pressure);
    printf("dt: %f\n", data->dt);
}

void Drone_DataExchange_PrintAngle(Drone_DataExchange* data)
{
    printf("T = %f, Roll: %f, Pitch: %f, Yaw: %f\n", data->T, data->angle[0], data->angle[1], data->angle[2]);
}

void Drone_DataExchange_PrintTextFile(Drone_DataExchange* data, FILE *fp)
{
    sprintf(LINE, "%f\t%f\t%f\t", data->T, data->dt, data->T-T_temp);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->angle[0], data->angle[1], data->angle[2]);
    strcat(LINE, LINETEMP);

    float norm = getSqrt(data->acc, 3);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->acc[0]/norm, data->acc[1]/norm, data->acc[2]/norm);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->gyr[0], data->gyr[1], data->gyr[2]);
    strcat(LINE, LINETEMP);

    norm = getSqrt(data->mag, 3);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->mag[0]/norm, data->mag[1]/norm, data->mag[2]/norm);
    strcat(LINE, LINETEMP);

    norm = getSqrt(data->acc_est, 3);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->acc_est[0]/norm, data->acc_est[1]/norm, data->acc_est[2]/norm);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->gyr_est[0], data->gyr_est[1], data->gyr_est[2]);
    strcat(LINE, LINETEMP);

    norm = getSqrt(data->mag_est, 3);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->mag_est[0]/norm, data->mag_est[1]/norm, data->mag_est[2]/norm);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->attitude, data->att_est, data->volt);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%d\t%u\t", data->comm.switchValue, data->comm.power);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%d\t%d\t", data->comm.horDirection[0], data->comm.horDirection[1]);
    sprintf(LINETEMP, "%f\t%f\t%f\t", data->comm.angle_expect[0], data->comm.angle_expect[1], data->comm.angle_expect[2]);
    strcat(LINE, LINETEMP);
    sprintf(LINETEMP, "%u\t%u\t%u\t%u\t", data->power[0], data->power[1], data->power[2], data->power[3]);
    strcat(LINE, LINETEMP);
    fprintf(fp, "%s\n", LINE);
    T_temp = data->T ;
}

void Drone_DataExchange_PrintFile(Drone_DataExchange* data, FILE *fp)
{
    fwrite(data, sizeof(Drone_DataExchange), 1, fp);
    fflush(fp);
}

