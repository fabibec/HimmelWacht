#include "pca9685-driver.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>

#include <driver/i2c_master.h>
#include <driver/i2c_types.h>
#include <soc/clk_tree_defs.h>

#include <string.h>
#include <math.h>
#include <esp_log.h>

static i2c_master_dev_handle_t master_dev_handle;
static i2c_master_bus_handle_t bus_handle = NULL;
static uint16_t i2c_timeout_ms = 100;
static uint8_t i2c_buffer[5] = {0x00};

const char *COMPONENT_TAG = "PCA9865 Driver";

/*
    Initializes and configures the PCA9685 driver.
    The driver also initializes the underlying I2C Infrastructure.

    @param freq PWM Frequency of the board

    @return ESP_OK on success, ESP error code on failure

    @author Fabian Becker
*/
esp_err_t pca9685_init(pca9685_config_t* cfg){
    esp_err_t ret;
    const char* TAG = "Init";

    // I2C Master Bus Config
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = cfg->i2c_port,
        .scl_io_num = cfg->scl_port,
        .sda_io_num = cfg->sda_port,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = cfg->internal_pullup // 2.4k external resistors for SDA and SCL
    };
    ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to get I2C master handle. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
        return ret;
    }

    // I2C Master Device Config
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->device_address,
        .scl_speed_hz = 100000,
    };
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &master_dev_handle);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to get I2C master device. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
        return ret;
    }

    // Put PCA9685 to sleep to configure oscillator clock
    i2c_buffer[0] = 0x00;
    i2c_buffer[1] = (0x01 << 4); // SLEEP
    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 2, i2c_timeout_ms);
    if(ret != ESP_OK){
        ESP_LOGI(
            COMPONENT_TAG,
            "%s: Unable to put PCA9685 into sleep. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
        return ret;
    }

    // Set PCA9685 Frequency
    uint32_t prescale = roundf(25000000.0f / (4096.0f * cfg->freq)) -1;
    i2c_buffer[0] = 0xFE;
    i2c_buffer[1] = prescale;
    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 2, i2c_timeout_ms);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to set PCA9685 Frequency. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
    }

    // Wake PCA9685
    i2c_buffer[0] = 0x00;
    i2c_buffer[1] = 0x00;
    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 2, i2c_timeout_ms);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to wake PCA9685. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
    }

    // Wait for oscillator
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // Configure PCA9685, Auto Increment Register, totem-pole output
    i2c_buffer[0] = 0x00;
    i2c_buffer[1] = (0x01 << 5); // Auto Increment
    i2c_buffer[2] = (0x01 << 2); // totem-pole output
    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 3, i2c_timeout_ms);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable to configure PCA9685. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
    }

    return ret;
}

/*
    Set the PWM Duty cycle of a channel by setting on/off periods.

    @param channel the output channel of the board (0x0 - 0xF)
    @param on the time when the PWM signal should be turned on (0x0000 - 0x0FFF)
    @param off the time when the PWM signal should be turned off (0x0000 - 0x0FFF)

    @return ESP_OK on success, ESP error code on failure

    @note Values bigger than the permitted range will be clipped to the end of the range.

    @author Fabian Becker
*/
esp_err_t pca9685_set_pwm_on_off(uint8_t channel, uint16_t on, uint16_t off){
    esp_err_t ret;
    const char* TAG = "Set PWM";

    // Clip values if needed
    if (channel > 0xF){
        ESP_LOGW(
            COMPONENT_TAG,
            "%s: Channel value %d clipped to 0xF!",
            TAG,
            channel
        );
        channel = 0xF;
    }
    if (on > 0x0FFF){
        ESP_LOGW(
            COMPONENT_TAG,
            "%s: On value %d clipped to 0x0FFF!",
            TAG,
            on
        );
        on = 0x0FFF;
    }
    if (off > 0x0FFF){
        ESP_LOGW(
            COMPONENT_TAG,
            "%s: Off value %d clipped to 0x0FFF!",
            TAG,
            off
        );
        off = 0x0FFF;
    }

    // Send PWM update via I²C
    i2c_buffer[0] = 0x06 + 4 * channel; // LEDn_ON_L register address
    i2c_buffer[1] = on & 0xFF; // Low byte of ON value
    i2c_buffer[2] = on >> 8; // High byte of ON value
    i2c_buffer[3] = off & 0xFF; // Low byte of OFF value
    i2c_buffer[4] = off >> 8; // High byte of OFF value

    ret = i2c_master_transmit(master_dev_handle, &i2c_buffer[0], 5, i2c_timeout_ms);
    if(ret != ESP_OK){
        ESP_LOGE(
            COMPONENT_TAG,
            "%s: Unable transmit PWM data. Error: %s",
            TAG,
            esp_err_to_name(ret)
        );
    }

    return ret;
}

/*
    Utility function to turn a particular channel on the board off.

    @param channel the output channel of the board (0x0 - 0xF)

    @return ESP_OK on success, ESP error code on failure

    @note Values bigger than the permitted range will be clipped to the end of the range.

    @author Fabian Becker
*/
esp_err_t pca9685_set_off(uint8_t channel){
    return pca9685_set_pwm_on_off(channel, 0, 0x0FFF);
}


