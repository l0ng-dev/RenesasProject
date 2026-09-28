#include "dht11.h"
#include "mpu6050.h"

/* Optional test hook. It is compiled, but production code does not call it. */
fsp_err_t SensorTest_CheckConfiguredModules (void)
{
    if (FSP_ERR_UNSUPPORTED != DHT11_Init())
    {
        return FSP_ERR_INVALID_MODE;
    }

    return MPU6050_Init();
}
