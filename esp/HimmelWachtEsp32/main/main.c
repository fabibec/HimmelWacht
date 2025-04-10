#include "sdkconfig.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi-stack.h"
#include "mqtt-stack.h"

#include "freertos/task.h"

#include "driver/uart.h"
#include "ds4-driver.h"
#include "vehicle-control.h"
#include "motor-driver.h"
#include "diff-drive.h"
#include "platform-control.h"
#include "fire-control.h"

#define PWM_CHANNEL 0

// Define GPIO pins for motors
#define RIGHT_MOTOR_PWM_GPIO 23
#define RIGHT_MOTOR_DIR_GPIO 22

#define LEFT_MOTOR_PWM_GPIO 27
#define LEFT_MOTOR_DIR_GPIO 26

#define MAX_INPUT_VALUE 512

#define TAG "main"

// Enter the Wi-Fi credentials here
#define WIFI_SSID "TI Roboter"
#define WIFI_PASSWORD "ITRobot!"

void app_main(void)
{
    esp_err_t ret = wifi_stack_init(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network");
    }

    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "Wi-Fi station interface not initialized");
    }
    else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGE(TAG, "Wi-Fi station is not connected");
    }

    ESP_LOGI(TAG, "Wi-Fi stack successfully initialized");

    // Configure MQTT component
    mqtt_config_t mqtt_config = {
        .broker_uri = "mqtt://172.16.3.105:1883",  // Broker IP
        .topic = "vehicle/turret/cmd",               // Configurable topic
        .client_id = "esp32_vehicle_01",             // Unique client ID
        .keepalive = 60,                              // Keep alive interval
        .network_timeout_ms = 5000,          // Network timeout in milliseconds
        .reconnect_timeout_ms = 5000,       // Reconnect timeout in milliseconds
        .queue_timeout_ticks = 10,                // Queue timeout in ticks
    };

    // Initialize MQTT component
    ret = mqtt_stack_init(&mqtt_config);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to start MQTT component: %s", esp_err_to_name(ret));
        return;
    }

    // I²C configuration of PCA9685
    pca9685_config_t pwm_board_cfg = {
        .device_address = 0x40,
        .freq = 50,
        .i2c_port = 0,
        .sda_port = 18,
        .scl_port = 19,
        .internal_pullup = true,
    };

    // Platform interface configuration
    platform_config_t platform_cfg = {
        .pwm_board_config = pwm_board_cfg,
        .platform_x_channel = 2,
        .platform_x_start_angle = 0,
        .platform_x_left_stop_angle = -90,
        .platform_x_right_stop_angle = 90,
        .platform_y_channel = 1,
        .platform_y_start_angle = 48,
        .platform_y_left_stop_angle = 0,
        .platform_y_right_stop_angle = 80};

    // Configuration for vehicle control interface
    vehicle_control_config_t manual_control_cfg = {
        .button_hold_threshold_us = 1500000, // 1.5 seconds
        .max_deg_per_sec_x = 300,
        .max_deg_per_sec_y = 150,
        .input_processing_freq_hz = 60,
        .deadzone_x = 30,
        .deadzone_y = 100,
        .deadzone_drive_update = 10,
        .core = 1};

    // Differential drive configuration
    diff_drive_config_t diff_drive_config = {
        .max_input = MAX_INPUT_VALUE,
        .cmd_queue_size = 10,
        .recovery_time_ms = 1000,
        .task_priority = 0,
        .task_stack_size = 4096,
        .task_core_id = 0,
        .task_delay_ms = 50,
        .queue_timout_ms = 10,
    };

    // Left motor configuration
    motor_config_t left_motor_config = {
        .mcpwm_unit = MCPWM_UNIT_0,
        .timer_num = MCPWM_TIMER_0,
        .generator = MCPWM_OPR_A,
        .pwm_signal = MCPWM0A,
        .pwm_gpio_num = LEFT_MOTOR_PWM_GPIO,
        .dir_gpio_num = LEFT_MOTOR_DIR_GPIO,
        .pwm_frequency_hz = 20000,
        .ramp_rate = 5,            // Adjust as needed
        .ramp_intervall_ms = 10,   // Adjust as needed
        .direction_hysteresis = 5, // Adjust as needed
        .pwm_duty_limit = 100,
        .mynr = 0};

    // Right motor configuration
    motor_config_t right_motor_config = {
        .mcpwm_unit = MCPWM_UNIT_0,
        .timer_num = MCPWM_TIMER_1,
        .generator = MCPWM_OPR_A,
        .pwm_signal = MCPWM1A,
        .pwm_gpio_num = RIGHT_MOTOR_PWM_GPIO,
        .dir_gpio_num = RIGHT_MOTOR_DIR_GPIO,
        .pwm_frequency_hz = 20000,
        .ramp_rate = 5,            // Adjust as needed
        .ramp_intervall_ms = 10,   // Adjust as needed
        .direction_hysteresis = 5, // Adjust as needed
        .pwm_duty_limit = 100,
        .mynr = 1};


    platform_init(&platform_cfg);

    // Fire control config
    fire_control_config_t fire_control_cfg = {
        .gun_arm_channel = PWM_CHANNEL,  // Set the channel for the gun arm
        .flywheel_control_gpio_port = 5, // Set the GPIO port for the flywheel control
        .run_on_core = 1                 // Set the core to run on
    };

    fire_control_init(&fire_control_cfg);

    // Initialize differential drive
    diff_drive_handle_t *diff_drive = diff_drive_init(&diff_drive_config,
                                                      &left_motor_config,
                                                      &right_motor_config);

    // Initialize the DS4 controller
    ds4_init();

    // Initialize vehicle control on core 1
    vehicle_control_init(&manual_control_cfg, diff_drive);
}
