#include "RTPiDrone_I2C_Device.h"
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*!
 * function : default function
 * \private \memberof Drone_I2C_Device
 */
static inline int dummyFunction(void* e)
{
    return 0;
}

/*!
 * function : default end function
 * \private \memberof Drone_I2C_Device
 */
static inline int dummyEndFunction(void* e)
{
    printf("%s END!\n", ((Drone_I2C_Device*)e)->name);
    return 0;
}

void Drone_I2C_Device_SetName(Drone_I2C_Device* dev, const char* str)
{
    strcpy(dev->name, str);
}

void Drone_I2C_Device_SetInitFunction(Drone_I2C_Device* dev, int (*init)(void*))
{
    dev->init_func = init;
}

void Drone_I2C_Device_SetRawFunction(Drone_I2C_Device* dev, int (*raw)(void*))
{
    dev->rawdata_func = raw;
}

void Drone_I2C_Device_SetRealFunction(Drone_I2C_Device* dev, int (*data)(void*))
{
    dev->data_func = data;
}

void Drone_I2C_Device_SetDataPointer(Drone_I2C_Device* dev, void* f)
{
    dev->getData = f;
}

void Drone_I2C_Device_SetEndFunction(Drone_I2C_Device* dev, int (*end)(void*))
{
    dev->end_func = end;
}


/* implementation */
void Drone_I2C_Device_Create(Drone_I2C_Device* i2c_dev)
{
    Drone_I2C_Device_SetInitFunction(i2c_dev, dummyFunction);
    Drone_I2C_Device_SetRawFunction(i2c_dev, dummyFunction);
    Drone_I2C_Device_SetRealFunction(i2c_dev, dummyFunction);
    Drone_I2C_Device_SetEndFunction(i2c_dev, dummyEndFunction);
    i2c_dev->getData = NULL;
}

int Drone_I2C_Device_Init(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->init_func(i2c_dev);
}

int Drone_I2C_Device_GetRawData(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->rawdata_func(i2c_dev);
}

int Drone_I2C_Device_GetRealData(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->data_func(i2c_dev);
}

int Drone_I2C_Device_End(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->end_func(i2c_dev);
}

float* Drone_I2C_Device_GetData(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->getData;
}

char* Drone_I2C_Device_GetName(Drone_I2C_Device* i2c_dev)
{
    return i2c_dev->name;
}

