/**
 * @file
 * @brief PCA9685 PWM Board Driver for Esp32
 *
 * Esp introduced a new version of the I2C Driver in ESP IDF, this driver employs the new version.
 * The is converted from an already existing component that uses this board to control LEDs and was written in C++
 *
 * References:
 *  - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
 *  - https://github.com/supcik/idf-pca9685
 *
 * #TODO create servo interface for mg996r and sg90
 * Datasheet:
 *  - https://www.handsontec.com/dataspecs/motor_fan/MG996R.pdf
 *
 * @author Fabian Becker
 */
#ifndef _PCA9685_H_
#define _PCA9685_H_
#define I2C_MASTER_PORT_NUM 0
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 19
#include <esp_err.h>
#include <stdint.h>

esp_err_t pca9685_init(uint32_t freq);
esp_err_t pca9685_set_pwm_on_off(uint8_t channel, uint16_t on, uint16_t off);
esp_err_t pca9685_set_pwm_duty(uint8_t channel, float duty_cycle);
esp_err_t pca9685_set_off(uint8_t channel);

#endif //_PCA9685_H_
