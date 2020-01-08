#include "BMP180.h"

/***** Definitions *****/
#define I2C_ADDR (0xEE) // 1110111x

#define REG_ADDR_RESET (0xE0)
#define REG_ADDR_ID (0xD0)
#define REG_ADDR_CTRL (0xF4)
#define REG_ADDR_DATA (0xF6)
#define REG_ADDR_AC1 (0xAA)

#define CTRL_REG_TEMP (0x2E)
#define CTRL_REG_PRESS_0 (0x34)
#define CTRL_REG_PRESS_1 (0x74)
#define CTRL_REG_PRESS_2 (0xB4)
#define CTRL_REG_PRESS_3 (0xF4)

//******************************************************************************
BMP180::BMP180(I2C *i2c) : i2c_(i2c)
{
    avr_measure = 0.0;
}

//******************************************************************************
int8_t BMP180::init(void)
{
    char addr;
    char data[22];
    int i;

    if (checkId() != 0)
    {
        return -1;
    }

    addr = REG_ADDR_AC1;
    if (i2c_->write(I2C_ADDR, &addr, 1) != 0)
    {
        return -1;
    }

    if (i2c_->read(I2C_ADDR, data, 22) != 0)
    {
        return -1;
    }

    for (i = 0; i < 11; i++)
    {
        calib.value[i] = (data[2 * i] << 8) | data[(2 * i) + 1];
    }

    return 0;
}

//******************************************************************************
int BMP180::reset(void)
{
    char data;

    data = REG_ADDR_RESET;
    if (i2c_->write(I2C_ADDR, &data, 1) != 0)
    {
        return -1;
    }

    data = 0xB6;
    if (i2c_->write(I2C_ADDR, &data, 1) != 0)
    {
        return -1;
    }

    return 0;
}

//******************************************************************************
int BMP180::checkId(void)
{
    char addr;
    char data;

    addr = REG_ADDR_ID;
    if (i2c_->write(I2C_ADDR, &addr, 1) != 0)
    {
        return -1;
    }

    if (i2c_->read(I2C_ADDR, &data, 1) != 0)
    {
        return -1;
    }

    if (data != 0x55)
    {
        return -1;
    }

    return 0;
}

//******************************************************************************
int32_t BMP180::getPressure()
{
    char data[2];
    BMP180::oversampling_t oss = BMP180::ULTRA_HIGH_RESOLUTION;
    data[0] = REG_ADDR_CTRL;
    data[1] = CTRL_REG_PRESS_0 | ((oss & 0x3) << 6);
    oss_ = oss;

    i2c_->write(I2C_ADDR, data, 2);
    wait_ms(30);
    char addr = REG_ADDR_DATA, byte[3];
    int32_t up;
    int32_t b6, x1, x2, x3, b3, p;
    uint32_t b4, b7;

    i2c_->write(I2C_ADDR, &addr, 1);
    i2c_->read(I2C_ADDR, byte, 3);

    up = (((int32_t)byte[0] << 16) | ((int32_t)byte[1] << 8) | byte[2]) >> (8 - oss_);

    b6 = PressureCompensate - 4000;
    x1 = (calib.b2 * (b6 * b6) >> 12) >> 11;
    x2 = (calib.ac2 * b6) >> 11;
    x3 = x1 + x2;

    b3 = (((((int32_t)calib.ac1) * 4 + x3) << oss_) + 2) >> 2;

    x1 = (calib.ac3 * b6) >> 13;
    x2 = (calib.b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = (x1 + x2 + 2) >> 2;
    b4 = (calib.ac4 * (uint32_t)(x3 + 32768)) >> 15;
    b7 = ((uint32_t)up - b3) * (50000 >> oss_);
    p = ((b7 < 0x80000000) ? ((b7 << 1) / b4) : ((b7 / b4) << 1));
    x1 = p >> 8;
    x1 *= x1;
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p += (x1 + x2 + 3791) >> 4;

    return p;
}

//******************************************************************************
float BMP180::getTemperature()
{
    char data[2] = {REG_ADDR_CTRL, CTRL_REG_TEMP};
    i2c_->write(I2C_ADDR, data, 2);
    wait_ms(5);

    char addr = REG_ADDR_DATA, byte[2];
    int16_t ut;
    int32_t x1, x2;

    i2c_->write(I2C_ADDR, &addr, 1);
    i2c_->read(I2C_ADDR, byte, 2);

    ut = ((int16_t)byte[0] << 8) | byte[1];

    x1 = (((int32_t)ut - (int32_t)calib.ac6) * (int32_t)calib.ac5) >> 15;
    x2 = ((int32_t)calib.mc << 11) / (x1 + calib.md);
    PressureCompensate = x1 + x2;

    return (float)(PressureCompensate + 8) / 160;
}

float BMP180::calAltitude(float pressure, float pressure0)
{
    // float A = pressure / 101325;
    // float B = 1 / 5.25588;
    // float C = pow(A, B);
    // C = 1 - C;
    // C = C / 0.0000225577;
    // return C;
    float A = (44330.0 * (1 - pow(pressure / pressure0, 1 / 5.255)));
    avr_measure = avr_measure * 0.9 + A * 0.1;
    return avr_measure;
}