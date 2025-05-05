#include "pca9685-driver.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/i2c_master.h>
#include <driver/i2c_types.h>
#include <soc/clk_tree_defs.h>
#include <math.h>

static i2c_master_dev_handle_t master_dev_handle;
static i2c_master_bus_handle_t bus_handle = NULL;
static uint16_t i2c_timeout_ms = 100;
static uint8_t i2c_buffer[5] = {0x00};

/*
    Initializes and configures the PCA9685 driver.
    The driver also initalizes the underlying I2C Infrastructure.

    @param freq PWM Frequency of the board

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t pca9685_init(uint32_t freq){
    esp_err_t ret;

    // I2C Master Bus Config
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_PORT_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true // 2.4k resistors for SDA and SCL
    };
    ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if(ret == ESP_FAIL){
        ESP_LOGI("PCA9685 Driver Init", "Unable to get I2C master handle");
    }

    // I2C Master Device Config
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x40,
        .scl_speed_hz = 100000,
    };
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &master_dev_handle);
    if(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_NO_MEM){
        ESP_LOGI("PCA9685 Driver Init", "Unable to get I2C master device");
    }

    // Configure PCA9685, Auto Increment Register, totem-pole output
    i2c_buffer[0] = 0x00;
    i2c_buffer[1] = (1 << 5); // Auto Increment
    i2c_buffer[2] = 0x01 << 2; // totem-pole output
    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 3, i2c_timeout_ms);
    if(ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_INVALID_ARG){
        ESP_LOGI("PCA9685 Driver Init", "Unable to configure PCA9685");
    }

    // Set PCA9685 Frequency
    uint32_t prescale = roundf(25000000.0f / (4096.0f * freq)) -1 ;
    i2c_buffer[0] = 0xFE;
    i2c_buffer[1] = prescale;

    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 2, i2c_timeout_ms);
    if(ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_INVALID_ARG){
        ESP_LOGI("PCA9685 Driver Init", "Unable to set PCA9685 Frequency");
    }

    return ret;
}

/*
    Set the PWM Duty cycle of a channel by setting on/off periods.

    @param channel the output channel of the board (0-15)
    @param on the phase the PWM signal should be on (Vcc) (0-4095)
    @param off the phase the PWM signal should be off (0) (0-4095)

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t pca9685_set_pwm_on_off(uint8_t channel, uint16_t on, uint16_t off){
    if (on > 4095) on = 4095;
    if (off > 4095) off = 4095;
    i2c_buffer[0] = 0x06 + 4 * channel; // LEDn_ON_L register address
    i2c_buffer[1] = on & 0xFF; // Low byte of ON value
    i2c_buffer[2] = on >> 8; // High byte of ON value
    i2c_buffer[3] = off & 0xFF; // Low byte of OFF value
    i2c_buffer[4] = off >> 8; // High byte of OFF value

    return i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 5, i2c_timeout_ms);
}

/*
    Set the PWM Duty cycle of a channel by setting the duty cycle.

    @param channel the output channel of the board (0-15)
    @param duty_cycle the duty cycle of the PWM signal (0.0 - 1.0)

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t pca9685_set_pwm_duty(uint8_t channel, float duty_cycle){
    esp_err_t ret;
    if (duty_cycle <= 0.0f){
        ret = pca9685_set_pwm_on_off(channel, 0, 4095);
    } else if (duty_cycle >= 1.0f){
        ret = pca9685_set_pwm_on_off(channel, 4095, 0);
    } else {
        ret = pca9685_set_pwm_on_off(channel, 0, roundf(4095.0f * duty_cycle));
    }
    return ret;
}

/*
    Turn a particular channel of the board off.

    @param channel the output channel of the board (0-15)

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t pca9685_set_off(uint8_t channel){
    return pca9685_set_pwm_on_off(channel, 0, 4095);
}


