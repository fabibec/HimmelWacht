#include "sdkconfig.h"
#include "pca9685-driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_log.h>

int app_main(void) {

    pca9685_init(50); // 20 ms

    // Normal mg996r
    // 0.17 seconds / 60 degrees
    // Miuzei MG996R: Pulse range: 500 ~ 2500usec (0 - 180)
    // SG90 should work the same way according to: https://www.elektronik-kompendium.de/sites/praxis/bauteil_sg90.htm
    while(1){
        pca9685_set_pwm_on_off(0, 512, 0) // 180 (right)
        vTaskDelay(500 / portTICK_PERIOD_MS);
        pca9685_set_pwm_on_off(0, 102, 0) // 0 (left)
        vTaskDelay(500 / portTICK_PERIOD_MS);
        pca9685_set_pwm_on_off(0, 307, 0) // 90 (middle)
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    return 0;
}
