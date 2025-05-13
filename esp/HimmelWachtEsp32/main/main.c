#include "sdkconfig.h"
#include "platform-control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_log.h>

int app_main(void) {

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
        .platform_x_channel = channel_2,
        .platform_x_start_angle = 0,
        .platform_x_left_stop_angle = -90,
        .platform_x_right_stop_angle = 90,
        .platform_y_channel = channel_1,
        .platform_y_start_angle = 47,
        .platform_y_left_stop_angle = 0,
        .platform_y_right_stop_angle = 90
    };

    platform_init(&platform_cfg);
    ESP_LOGI("Platform", "Platform initialized");

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /*for( int8_t angle = -90; angle <= 90; angle += 10){
            platform_x_set_angle(angle);
            ESP_LOGI("Platform", "X Angle: %d", angle);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }*/

        for(int8_t angle = 90; angle >= 0; angle -= 10){
            platform_y_set_angle(angle);
            ESP_LOGI("Platform", "Y Angle: %d", angle);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    return 0;
}
