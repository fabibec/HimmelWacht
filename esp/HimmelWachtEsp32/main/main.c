#include "sdkconfig.h"
#include "pca9685-driver.h"
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

    pca9685_init(&pwm_board_cfg);

    // Normal mg996r
    // 0.17 seconds / 60 degrees
    // Miuzei MG996R: Pulse range: 500 ~ 2500usec (0 - 180)
    // SG90 should work the same way according to: https://www.elektronik-kompendium.de/sites/praxis/bauteil_sg90.htm

    // 512 all left
    // 409 left
    // 307 middle
    // 205 right
    // 105 all right

    uint16_t steps[5] = {514, 409, 307, 205, 104};

    while(1){
            for(uint8_t j = 0; j <= 4; j++){
                for(uint8_t i = 0; i <= 1; i++){
                    pca9685_set_pwm_on_off(i, 0, steps[j]); // 90 (left)
                }
                vTaskDelay(200 / portTICK_PERIOD_MS);
                for(uint8_t i = 0; i <= 1; i++){
                    pca9685_set_off(i);
                }
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
    }

    return 0;
}
