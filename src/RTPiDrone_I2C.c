/*! \file RTPiDrone_I2C.c
    \brief Manage all of the I2C devices
 */
#include "RTPiDrone_header.h"
#include "RTPiDrone_I2C.h"
#include "RTPiDrone_Device.h"
#include "RTPiDrone_I2C_CaliInfo.h"
#include "RTPiDrone_I2C_Device_ADXL345.h"
#include "RTPiDrone_I2C_Device_L3G4200D.h"
#include "RTPiDrone_I2C_Device_HMC5883L.h"
#include "RTPiDrone_I2C_Device_BMP085.h"
#include "RTPiDrone_I2C_Device_PCA9685PW.h"
#include "RTPiDrone_I2C_Device_MS5611.h"
#include "RTPiDrone_Filter.h"
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
#define RAD_TO_DEG      (180/M_PI)
#define FILENAMESIZE            64
#define N_SAMPLE_CALIBRATION    3000
#define NUM_CALI_THREADS        5
#define NDATA_ADXL345           3
#define NDATA_L3G4200D          3
#define NDATA_HMC5883L          3
#define NDATA_BMP085            1
#define NDATA_MS5611            1
/*!
 * \struct tempCali
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

static atomic_int i2c_stat = 0;                     //!< \private Drone_I2C: Indicate if I2C is occupied
static int Calibration_Single_L3G4200D(Drone_I2C*); //!< \private \memberof Drone_I2C: Calibration step for L3G4200D
static int Calibration_Single_ADXL345(Drone_I2C*);  //!< \private \memberof Drone_I2C: Calibration step for ADXL345
static int Calibration_Single_HMC5883L(Drone_I2C*); //!< \private \memberof Drone_I2C: Calibration step for HMC5883L
static int Calibration_Single_BMP085(Drone_I2C*);   //!< \private \memberof Drone_I2C: Calibration step for BMP085
static int Calibration_Single_MS5611(Drone_I2C*);   //!< \private \memberof Drone_I2C: Calibration step for MS5611
static void* Calibration_Single_Thread(void*);      //!< \private \memberof tempCali: Template for calibration
static int PCA9685PW_ESC_Init(Drone_I2C*);          //!< \private \memberof Drone_I2C: Initialization of ESC
static void Drone_I2C_MagPWMCorrection(uint32_t*, float*);

static float magFitFunc(uint32_t, const float*);

static const float magCorr[][3][3] = {
    {
        {6.61611606211, -98.902117397,  364.170847984},
        {3.25212997028, -48.7697238694, 179.022788776},
        {-7.37160176497,    111.834418395,  -412.447306945}
    },
    {
        {5.50903764712, -82.0980156356, 301.453031647},
        {4.07467179477, -63.7918721595, 249.373180638},
        {3.24067398825, -50.4595212277, 190.858825857}
    },
    {
        {-13.3460228282,    200.930820024,  -739.962719004},
        {29.3057756656, -445.783984334, 1662.17393418},
        {19.629876404,  -295.721326047, 1091.7205143}
    },
    {
        {-14.6725557049,    217.001761933,  -786.753669073},
        {-17.2872454836,    259.179108995,  -952.302481154},
        {-21.5664086508,    323.717279288,  -1190.54567997}
    }
};


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
    Drone_I2C_Device_MS5611*        MS5611;
};

static float magFitFunc(uint32_t power, const float* t)
{
    return t[0]*sqrtf((float)power) + pow(power,0.25)*t[1] + t[2];
}

int Drone_I2C_Init(Drone_I2C** i2c)
{
    *i2c = (Drone_I2C*)calloc(1,sizeof(Drone_I2C));
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
    if (MS5611_setup(&(*i2c)->MS5611)) {
        perror("Init MS5611");
        return -5; 
    }   
    if (PCA9685PW_setup(&(*i2c)->PCA9685PW)) {
        perror("Init PCA9685PW");
        return -6;
    }

    return 0;
}

int Drone_I2C_Calibration(Drone_I2C* i2c)
{
    pthread_t thread_i2c[NUM_CALI_THREADS];
    tempCali accTemp = {i2c, ADXL345_getCaliInfo(i2c->ADXL345), Calibration_Single_ADXL345,
                        Drone_Device_GetData((Drone_Device*)(i2c->ADXL345)), N_SAMPLE_CALIBRATION, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->ADXL345))
                       };
    pthread_create(&thread_i2c[0], NULL, Calibration_Single_Thread, (void*) &accTemp);

    tempCali gyrTemp = {i2c, L3G4200D_getCaliInfo(i2c->L3G4200D), Calibration_Single_L3G4200D,
                        Drone_Device_GetData((Drone_Device*)(i2c->L3G4200D)), N_SAMPLE_CALIBRATION, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->L3G4200D))
                       };
    pthread_create(&thread_i2c[1], NULL, Calibration_Single_Thread, (void*) &gyrTemp);

    tempCali magTemp = {i2c, HMC5883L_getCaliInfo(i2c->HMC5883L), Calibration_Single_HMC5883L,
                        Drone_Device_GetData((Drone_Device*)(i2c->HMC5883L)), N_SAMPLE_CALIBRATION/5, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->HMC5883L))
                       };
    pthread_create(&thread_i2c[2], NULL, Calibration_Single_Thread, (void*) &magTemp);

    tempCali barTemp = {i2c, BMP085_getCaliInfo(i2c->BMP085), Calibration_Single_BMP085,
                        Drone_Device_GetData((Drone_Device*)(i2c->BMP085)), N_SAMPLE_CALIBRATION/10, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->BMP085))
                       };
    pthread_create(&thread_i2c[3], NULL, Calibration_Single_Thread, (void*) &barTemp);

    tempCali bar2Temp = {i2c, MS5611_getCaliInfo(i2c->MS5611), Calibration_Single_MS5611,
                        Drone_Device_GetData((Drone_Device*)(i2c->MS5611)), N_SAMPLE_CALIBRATION/10, 3,
                        Drone_Device_GetName((Drone_Device*)(i2c->MS5611))
                       };
    pthread_create(&thread_i2c[4], NULL, Calibration_Single_Thread, (void*) &bar2Temp);

    for (int i=0; i<NUM_CALI_THREADS; ++i) pthread_join(thread_i2c[i],NULL);

    return 0;
}

void Drone_I2C_Start(Drone_I2C* i2c)
{
    PCA9685PW_ESC_Init(i2c);
    _usleep(5000000);
#ifdef  HMC5883L_PWM_CALI
    HMC5883L_PWM_Calibration(i2c);
#endif
}

int Drone_I2C_End(Drone_I2C** i2c)
{
    // Clean the file structures
    PCA9685PW_delete(&(*i2c)->PCA9685PW);
    ADXL345_delete(&(*i2c)->ADXL345);
    L3G4200D_delete(&(*i2c)->L3G4200D);
    HMC5883L_delete(&(*i2c)->HMC5883L);
    MS5611_delete(&(*i2c)->MS5611);
    BMP085_delete(&(*i2c)->BMP085);

    bcm2835_i2c_end();
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
    ADXL345_inputFilter(i2c->ADXL345);
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
    L3G4200D_inputFilter(i2c->L3G4200D);
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
    HMC5883L_inputFilter(i2c->HMC5883L);
    _usleep(HMC5883L_PERIOD/1000);
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
    BMP085_inputFilter(i2c->BMP085);
    return ret2;
}

static int Calibration_Single_MS5611(Drone_I2C* i2c)
{
    static const int sleepTime2 = 10000;
    int ret;
    for (int i=0; i<2; ++i) {
        while (i2c_stat) sched_yield() ;
        atomic_fetch_add_explicit(&i2c_stat, 1, memory_order_seq_cst);
        ret = Drone_Device_GetRawData((Drone_Device*)(i2c->MS5611));
        atomic_fetch_sub(&i2c_stat, 1);
        ret += Drone_Device_GetRealData((Drone_Device*)(i2c->MS5611));
        _usleep(sleepTime2);
    }
    MS5611_inputFilter(i2c->MS5611);
    return ret;
}

static void* Calibration_Single_Thread(void* temp)
{
    Drone_I2C* i2c = ((tempCali*)temp)->i2c;
    Drone_I2C_CaliInfo* cali = ((tempCali*)temp)->i2c_cali;
    struct timespec tp1, tp2;
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
            fprintf(fout, "%f\t", deltaT);
            //printf("%d th: ", i);
            for (int j=0; j<nData; ++j) {
                vCali[j][i] = data[j];
                fprintf(fout, "%f\t", data[j]);
                //printf("%f, ", data[j]);
            }
            fprintf(fout, "\n");
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

#ifdef  DEBUG
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
#endif
    free(vCali);
    pthread_exit(NULL);
}

void Drone_I2C_DataInit(Drone_DataExchange* data, Drone_I2C* i2c)
{
    Drone_I2C_CaliInfo* c = ADXL345_getCaliInfo(i2c->ADXL345);
    for (int i=0; i<3; ++i) {
        data->acc[i] = data->acc_est[i] = Drone_I2C_Cali_getMean(c)[i];
        data->gyr[i] = 0.0f;
    }
    c = HMC5883L_getCaliInfo(i2c->HMC5883L);
    for (int i=0; i<3; ++i) {
        data->mag[i] = data->mag_est[i] = Drone_I2C_Cali_getMean(c)[i];
    }
    c = BMP085_getCaliInfo(i2c->BMP085);
    data->attitude = data->attitudeHT = data->att_est = data->attHT_est = 0.0f;
    data->temperature = Drone_I2C_Cali_getMean(c)[1];
    data->pressure = Drone_I2C_Cali_getMean(c)[2];
    data->angle[0] = atan2(data->acc[1], data->acc[2]) * RAD_TO_DEG;      // roll
    data->angle[1] = -atan2(data->acc[0], getSqrt(data->acc, 3)) * RAD_TO_DEG; //pitch
    data->angle[2] = acos(data->mag[1]/getSqrt(data->mag, 2)) * RAD_TO_DEG;    // yaw
    for (int i=0; i<4; ++i) data->power[i] = PWM_MIN;
}

int Drone_I2C_ExchangeData(Drone_DataExchange* data, Drone_I2C* i2c, uint64_t* lastUpdate, bool step)
{
    int ret = 0;
    if (!step) {
        ADXL345_getFilteredValue(i2c->ADXL345, lastUpdate, data->acc, data->acc_est);
        L3G4200D_getFilteredValue(i2c->L3G4200D, lastUpdate, data->gyr, data->gyr_est);
    } else {
        ret = PCA9685PW_write(i2c->PCA9685PW, data->power, lastUpdate);
        if (!ret) data->dt_accu += data->dt;
        else data->dt_accu = 0.0f;
        ret += HMC5883L_getFilteredValue(i2c->HMC5883L, lastUpdate, data->mag, data->mag_est);
        if (ret) Drone_I2C_MagPWMCorrection(data->power, data->mag_est);
        ret += BMP085_getFilteredValue(i2c->BMP085, lastUpdate, &data->attitude, &data->att_est);
        ret += MS5611_getFilteredValue(i2c->MS5611, lastUpdate, &data->attitudeHT, &data->attHT_est);
    }
    return ret;
}

void HMC5883L_PWM_Calibration(Drone_I2C* i2c)
{
    char fileName[FILENAMESIZE], num[FILENAMESIZE];
    const int nSample = 10;
    float data[3][nSample];
    float* f;
    float mean, sd;
    uint64_t lastUpdate;
    uint32_t power[] = {PWM_MIN,PWM_MIN,PWM_MIN,PWM_MIN};
    for (int i=0; i<4; ++i) {
        printf("HMC5883L_PWM_Calibration : %d\n", i);
        sprintf(num, "%d", i);
        strcpy(fileName, "HMC5883L_PWM_");
        strcat(fileName, num);
        strcat(fileName, ".log");
        FILE* fp = fopen(fileName, "w");
        for (uint32_t j=PWM_MIN; j<=PWM_MAX; ++j) {
            power[i] = j;
            if (!PCA9685PW_writeOnly(i2c->PCA9685PW, power)) {
                _usleep(60000);
                for (int k=0; k<nSample; ++k) {
                    _usleep(6000);
                    lastUpdate = get_nsec();
                    f = (float*)Drone_Device_GetRefreshedData((Drone_Device*)i2c->HMC5883L, &lastUpdate);
                    if (f) {
                        for (int l=0; l<3; ++l) {
                            data[l][k] = f[l];
                        }
                    } else {
                        --k;
                    }
                }
                fprintf(fp, "%u\t", j);
                for (int k=0; k<3; ++k) {
                    mean = gsl_stats_float_mean(data[k], 1, nSample);
                    sd = gsl_stats_float_sd(data[k], 1, nSample);
                    fprintf(fp, "%f\t%f\t", mean, sd);
                }
                fprintf(fp, "\n");
            }
        }
        power[i] = PWM_MIN;
        PCA9685PW_writeOnly(i2c->PCA9685PW, power);
        fclose(fp);
        _usleep(3000000);
    }
}

static int PCA9685PW_ESC_Init(Drone_I2C* i2c)
{
    int ret = 0;
    uint32_t power[] = {PWM_MIN,PWM_MIN,PWM_MIN,PWM_MIN};
    ret += PCA9685PW_writeOnly(i2c->PCA9685PW, power);
    _usleep(40000);
    for (int i=0; i<4; ++i) power[i] = PWM_MAX;
    ret += PCA9685PW_writeOnly(i2c->PCA9685PW, power);
    _usleep(70000);
    for (int i=0; i<4; ++i) power[i] = PWM_MIN;
    ret += PCA9685PW_writeOnly(i2c->PCA9685PW, power);
    _usleep(50000);
    return ret;
}

static void Drone_I2C_MagPWMCorrection(uint32_t* power, float* mag_est)
{
    for (int i=0; i<4; ++i) {
        if (power[i]>1800) {
            for (int j=0; j<3; ++j) {
                mag_est[j] -= magFitFunc(power[i], magCorr[i][j]);
            }
        }
    }
}

