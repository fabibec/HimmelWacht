#include "sdkconfig.h"

#include "platform-control.h"
#include "fire-control.h"
#include "ds4-driver.h"
#include "manual-control.h"

#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define PWM_CHANNEL 0

void app_main(void) {
    pca9685_config_t pwm_board_cfg = {
        .device_address = 0x40,
        .freq = 50,
        .i2c_port = 0,
        .sda_port = 18,
        .scl_port = 19,
        .internal_pullup = true,
    };

    platform_config_t platform_cfg = {
        .pwm_board_config = pwm_board_cfg,
        .platform_x_channel = 2,
        .platform_x_start_angle = 0,
        .platform_x_left_stop_angle = -90,
        .platform_x_right_stop_angle = 90,
        .platform_y_channel = 1,
        .platform_y_start_angle = 47,
        .platform_y_left_stop_angle = 0,
        .platform_y_right_stop_angle = 80
    };

    manual_control_config_t manual_control_cfg = {
        .button_hold_threshold_us = 1500000, // 1.5 seconds
        .max_deg_per_sec = 150,
        .input_processing_freq_hz = 60,
        .deadzone = 30,
        .core = 1
    };

    platform_init(&platform_cfg);
    ESP_LOGI("Platform", "Platform initialized");

    fire_control_config_t fire_control_cfg = {
        .gun_arm_channel = PWM_CHANNEL, // Set the channel for the gun arm
        .flywheel_control_gpio_port = 5, // Set the GPIO port for the flywheel control
        .run_on_core = 1 // Set the core to run on
    };

    fire_control_init(&fire_control_cfg);

    // Initialize the DS4 controller
    ds4_init();

    // Initialize manual control on core 1
    manual_control_init(&manual_control_cfg);
}
