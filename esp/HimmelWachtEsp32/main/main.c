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

    uint16_t steps[5] = {325, 120, 325};

    // Plattform x
    // 535 left -> right for the controller  90 deg
    // 210 ticks between -> / 90 = 2.333
    // 325 middle
    // 210 ticks between -> /90 = 2.333
    // 115 right -> left for the controller -90 deg

    /*
        to get proper values turn at 2 for every degree and 3 for every third degree
        formula (int8_t target):
        - -> left, + -> right
        const uint16_t zero_deg = 325;

        uint8_t 3_steps = fabs(target) / 3  // cutoff wanted!
        uint8_t 2_steps = target - 3_steps

        int16_t step_amount = 2_steps * 2 + 3_steps * 3;
        step_amount = (target > 0) ? step_amount : (step_amount * -1);

        pca9685(servo_channel, 0, zero_deg + step_amount);
    */

    pca9685_set_pwm_on_off(0, 0, 325);
    pca9685_set_pwm_on_off(2, 0, 325);
    vTaskDelay(1000/ portTICK_PERIOD_MS);
    pca9685_set_off(0);
    pca9685_set_off(2);



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
           //vTaskDelay(10000);
            /*pca9685_set_pwm_on_off(1, 0, 320);
           pca9685_set_pwm_on_off(2, 0, 325);
           vTaskDelay(500 / portTICK_PERIOD_MS);
           pca9685_set_off(1);
           pca9685_set_off(2);
           vTaskDelay(200000 / portTICK_PERIOD_MS);*/
    }

    return 0;
}
