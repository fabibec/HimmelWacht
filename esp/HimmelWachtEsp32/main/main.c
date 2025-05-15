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

#define UART_PORT UART_NUM_0
#define BUF_SIZE 1024
#define PWM_MAX 400
#define STEP 10
#define PWM_CHANNEL 0

// Function to check for button (serial input)
static void wait_for_button_and_move(void* arg) {
    uint8_t data[BUF_SIZE];

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT, &uart_config);
    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);

    printf("Waiting for button press (send any key over serial)...\n");

    while (1) {
        esp_err_t ret;
        int len = uart_read_bytes(UART_PORT, data, 1, 20 / portTICK_PERIOD_MS);

        if (len > 0) {
            ret = fire_control_trigger_shot(); // Trigger the shot
            if (ret == ESP_ERR_NOT_FINISHED) {
                printf("Shot not finished yet, ignoring request.\n");
            } else {
                printf("Shot triggered!\n");
            }

            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

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
        .platform_y_start_angle =  70,//47,
        .platform_y_left_stop_angle = 0,
        .platform_y_right_stop_angle = 90
    };

    platform_init(&platform_cfg);
    ESP_LOGI("Platform", "Platform initialized");

    fire_control_config_t fire_control_cfg = {
        .gun_arm_channel = PWM_CHANNEL, // Set the channel for the gun arm
        .flywheel_control_gpio_port = 5, // Set the GPIO port for the flywheel control
        .run_on_core = 1 // Set the core to run on
    };

    fire_control_init(&fire_control_cfg);

    xTaskCreatePinnedToCore(
        wait_for_button_and_move,   /* Function to implement the task */
        "fire_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0 /* Core where the task should run */
    );

    // Initialize the DS4 controller
    ds4_init();

    // Initialize manual control on core 1
    manual_control_init(1);
}
