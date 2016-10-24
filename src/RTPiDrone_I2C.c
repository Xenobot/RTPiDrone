/*! \file RTPiDrone_I2C.c
    \brief Manage all of the I2C devices
 */

#include "RTPiDrone_I2C.h"
#include "RTPiDrone_Device.h"
#include "RTPiDrone_I2C_CaliInfo.h"
#include "RTPiDrone_I2C_Device_ADXL345.h"
#include "RTPiDrone_I2C_Device_L3G4200D.h"
#include "RTPiDrone_I2C_Device_HMC5883L.h"
#include "RTPiDrone_I2C_Device_BMP085.h"
#include "RTPiDrone_I2C_Device_PCA9685PW.h"
#include "Common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <sched.h>
#include <pthread.h>
#include <bcm2835.h>
#include <gsl/gsl_statistics.h>

#define FILENAMESIZE            64
#define N_SAMPLE_CALIBRATION    2000
#define NUM_CALI_THREADS        4
#define NDATA_ADXL345           3
#define NDATA_L3G4200D          3
#define NDATA_HMC5883L          3
#define NDATA_BMP085            1

/*!
 * \brief Private tempCali type
 * This structure allow to generate a single thread for calibration of a single device.
 */
typedef struct {
    Drone_I2C*      i2c;
    Drone_I2C_CaliInfo*  i2c_cali;
    int (*func)(Drone_I2C*);
    float* data;
    int nSample;
    int nData;
    char* name;
} tempCali;

static atomic_int i2c_stat = 0;                   //!< \private Drone_I2C: Indicate if I2C is occupied
static struct timespec tp1, tp2;                    //!< \private Drone_I2C: Internal Timer
static int Calibration_Single_L3G4200D(Drone_I2C*); //!< \private \memberof Drone_I2C: Calibration step for L3G4200D
static int Calibration_Single_ADXL345(Drone_I2C*);  //!< \private \memberof Drone_I2C: Calibration step for ADXL345
static int Calibration_Single_HMC5883L(Drone_I2C*); //!< \private \memberof Drone_I2C: Calibration step for HMC5883L
static int Calibration_Single_BMP085(Drone_I2C*);   //!< \private \memberof Drone_I2C: Calibration step for BMP085
static void* Calibration_Single_Thread(void*);      //!< \private \memberof tempCali: Template for calibration

/*!
 * \struct Drone_I2C
 * \brief Drone_I2C structure
 */
struct Drone_I2C {
    Drone_I2C_Device_ADXL345*       ADXL345;    //!< \private ADXL345 : 3-axis accelerometer
    Drone_I2C_Device_L3G4200D*      L3G4200D;   //!< \private L3G4200D : 3-axis gyroscope
    Drone_I2C_Device_HMC5883L*      HMC5883L;   //!< \private HMC5883L : 3-axis digital compass
    Drone_I2C_Device_BMP085*        BMP085;     //!< \private BMP085 : Barometric Pressure/Temperature/Altitude
    Drone_I2C_Device_PCA9685PW*     PCA9685PW;  //!< \private PCA9685PW : Pulse Width Modulator
    Drone_I2C_CaliInfo*             accCali;    //!< \private Parameters for the calibration of ADXL345
    Drone_I2C_CaliInfo*             gyrCali;    //!< \private Parameters for the calibration of L3G4200D
    Drone_I2C_CaliInfo*             magCali;    //!< \private Parameters for the calibration of HMC5883L
    Drone_I2C_CaliInfo*             barCali;    //!< \private Parameters for the calibration of BMP085
};

int Drone_I2C_Init(Drone_I2C** i2c)
{
    *i2c = (Drone_I2C*)malloc(sizeof(Drone_I2C));
    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626);
    if (ADXL345_setup(&(*i2c)->ADXL345)) {
        perror("Init ADXL345");
        return -1;
    }
    if (L3G4200D_setup(&(*i2c)->L3G4200D)) {
        perror("Init L3G4200D");
        return -2;
    }
    if (HMC5883L_setup(&(*i2c)->HMC5883L)) {
        perror("Init HMC5883L");
        return -3;
    }
    if (BMP085_setup(&(*i2c)->BMP085)) {
        perror("Init BMP085");
        return -4;
    }
    if (PCA9685PW_setup(&(*i2c)->PCA9685PW)) {
        perror("Init PCA9685PW");
        return -5;
    }

    Drone_I2C_Cali_Init(&(*i2c)->accCali, NDATA_ADXL345);
    Drone_I2C_Cali_Init(&(*i2c)->gyrCali, NDATA_L3G4200D);
    Drone_I2C_Cali_Init(&(*i2c)->magCali, NDATA_HMC5883L);
    Drone_I2C_Cali_Init(&(*i2c)->barCali, NDATA_BMP085);

    return 0;
}

int Drone_I2C_Calibration(Drone_I2C* i2c)
{
    pthread_t thread_i2c[NUM_CALI_THREADS];
    tempCali accTemp = {i2c, i2c->accCali, Calibration_Single_ADXL345,
                        Drone_Device_GetData((Drone_Device*)(i2c->ADXL345)), N_SAMPLE_CALIBRATION, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->ADXL345))
                       };
    pthread_create(&thread_i2c[0], NULL, Calibration_Single_Thread, (void*) &accTemp);

    tempCali gyrTemp = {i2c, i2c->gyrCali, Calibration_Single_L3G4200D,
                        Drone_Device_GetData((Drone_Device*)(i2c->L3G4200D)), N_SAMPLE_CALIBRATION, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->L3G4200D))
                       };
    pthread_create(&thread_i2c[1], NULL, Calibration_Single_Thread, (void*) &gyrTemp);

    tempCali magTemp = {i2c, i2c->magCali, Calibration_Single_HMC5883L,
                        Drone_Device_GetData((Drone_Device*)(i2c->HMC5883L)), N_SAMPLE_CALIBRATION/2, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->HMC5883L))
                       };
    pthread_create(&thread_i2c[2], NULL, Calibration_Single_Thread, (void*) &magTemp);

    tempCali barTemp = {i2c, i2c->barCali, Calibration_Single_BMP085,
                        Drone_Device_GetData((Drone_Device*)(i2c->BMP085)), N_SAMPLE_CALIBRATION/10, 1,
                        Drone_Device_GetName((Drone_Device*)(i2c->BMP085))
                       };
    pthread_create(&thread_i2c[3], NULL, Calibration_Single_Thread, (void*) &barTemp);


    for (int i=0; i<NUM_CALI_THREADS; ++i) pthread_join(thread_i2c[i],NULL);
    return 0;
}

void Drone_I2C_Start(Drone_I2C* i2c)
{
    puts("I2C Start");
}

int Drone_I2C_End(Drone_I2C** i2c)
{
    // Shut down the device (if necessary).

    if (Drone_Device_End((Drone_Device*)(*i2c)->PCA9685PW)) {
        perror("End PCA9685PW Error");
        return -1;
    }
    if (Drone_Device_End((Drone_Device*)(*i2c)->ADXL345)) {
        perror("End ADXL345 Error");
        return -2;
    }
    if (Drone_Device_End((Drone_Device*)(*i2c)->L3G4200D)) {
        perror("End L3G4200D Error");
        return -3;
    }
    if (Drone_Device_End((Drone_Device*)(*i2c)->HMC5883L)) {
        perror("End HMC5883L Error");
        return -4;
    }
    if (Drone_Device_End((Drone_Device*)(*i2c)->BMP085)) {
        perror("End BMP085 Error");
        return -5;
    }

    bcm2835_i2c_end();

    // Clean the file structures
    Drone_I2C_Cali_Delete(&(*i2c)->accCali);
    Drone_I2C_Cali_Delete(&(*i2c)->gyrCali);
    Drone_I2C_Cali_Delete(&(*i2c)->magCali);
    Drone_I2C_Cali_Delete(&(*i2c)->barCali);

    ADXL345_delete(&(*i2c)->ADXL345);
    L3G4200D_delete(&(*i2c)->L3G4200D);
    HMC5883L_delete(&(*i2c)->HMC5883L);
    BMP085_delete(&(*i2c)->BMP085);
    PCA9685PW_delete(&(*i2c)->PCA9685PW);

    free(*i2c);
    *i2c = NULL;
    return 0;
}

static int Calibration_Single_ADXL345(Drone_I2C* i2c)
{
    while (i2c_stat) sched_yield() ;
    atomic_fetch_add_explicit(&i2c_stat, 1, memory_order_seq_cst);
    int ret = Drone_Device_GetRawData((Drone_Device*)(i2c->ADXL345));
    atomic_fetch_sub(&i2c_stat, 1);
    ret += Drone_Device_GetRealData((Drone_Device*)(i2c->ADXL345));
    _usleep(3000);
    return ret;
}

static int Calibration_Single_L3G4200D(Drone_I2C* i2c)
{
    while (i2c_stat) sched_yield() ;
    atomic_fetch_add_explicit(&i2c_stat, 1, memory_order_seq_cst);
    int ret = Drone_Device_GetRawData((Drone_Device*)(i2c->L3G4200D));
    atomic_fetch_sub(&i2c_stat, 1);
    ret += Drone_Device_GetRealData((Drone_Device*)(i2c->L3G4200D));
    _usleep(3000);
    return ret;
}

static int Calibration_Single_HMC5883L(Drone_I2C* i2c)
{
    while (i2c_stat) sched_yield() ;
    atomic_fetch_add_explicit(&i2c_stat, 1, memory_order_seq_cst);
    int ret = Drone_Device_GetRawData((Drone_Device*)(i2c->HMC5883L));
    atomic_fetch_sub(&i2c_stat, 1);
    ret += Drone_Device_GetRealData((Drone_Device*)(i2c->HMC5883L));
    _usleep(6000);
    return ret;
}

static int Calibration_Single_BMP085(Drone_I2C* i2c)
{
    static const int sleepTime[] = {25500, 4500};
    int ret, ret2;
    for (int i=0; i<2; ++i) {
        while (i2c_stat) sched_yield() ;
        atomic_fetch_add_explicit(&i2c_stat, 1, memory_order_seq_cst);
        ret = Drone_Device_GetRawData((Drone_Device*)(i2c->BMP085));
        atomic_fetch_sub(&i2c_stat, 1);
        ret2 = Drone_Device_GetRealData((Drone_Device*)(i2c->BMP085));
        _usleep(sleepTime[ret]);
    }
    return ret2;
}

static void* Calibration_Single_Thread(void* temp)
{
    Drone_I2C* i2c = ((tempCali*)temp)->i2c;
    Drone_I2C_CaliInfo* cali = ((tempCali*)temp)->i2c_cali;
    int (*f)(Drone_I2C*) = ((tempCali*)temp)->func;
    int nSample = ((tempCali*)temp)->nSample;
    int nData = ((tempCali*)temp)->nData;
    char* name = ((tempCali*)temp)->name;
    char fileName[FILENAMESIZE];
    strcpy(fileName, name);
    strcat(fileName, "_calibration.log");
    FILE *fout = fopen(fileName, "w");
    unsigned long startTime, procesTime;
    float deltaT;
    float** vCali = (float**) malloc(sizeof(float*)*nData);
    for (int i=0; i<nData; ++i) {
        vCali[i] = (float*) calloc(nSample, sizeof(float));
    }

    float* data = ((tempCali*)temp)->data;
    for (int i=0; i<nSample; ++i) {
        clock_gettime(CLOCK_REALTIME, &tp1);
        startTime = tp1.tv_sec*1000000000 + tp1.tv_nsec;
        if (!f(i2c)) {
            clock_gettime(CLOCK_REALTIME, &tp2);
            procesTime = tp2.tv_sec*1000000000 + tp2.tv_nsec - startTime;
            deltaT = (float)procesTime/1000000000.0;
            fprintf(fout, "%f\n", deltaT);
            //printf("%d th: ", i);
            for (int j=0; j<nData; ++j) {
                vCali[j][i] = data[j];
                //printf("%f, ", data[j]);
            }
            //puts("");
        } else {
            fprintf(fout, "===========\n");
            --i;
        }
    }

    fclose(fout);
    float* mean = Drone_I2C_Cali_getMean(cali);
    float* sd = Drone_I2C_Cali_getSD(cali);
    for (int i=0; i<nData; ++i) {
        mean[i] = (float) gsl_stats_float_mean(&vCali[i][0], 1, nSample);
        sd[i] = (float) gsl_stats_float_sd(&vCali[i][0], 1, nSample);
        free(vCali[i]);
    }

    printf("Mean :");
    for (int i=0; i<nData; ++i) {
        printf("%f, ", mean[i]);
    }
    puts("");

    printf("SD :");
    for (int i=0; i<nData; ++i) {
        printf("%f, ", sd[i]);
    }
    puts("");

    free(vCali);
    pthread_exit(NULL);
}

