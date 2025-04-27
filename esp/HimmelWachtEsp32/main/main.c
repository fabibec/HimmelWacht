#include "sdkconfig.h"
#include "ds4-driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_log.h>

void test_ds4_output(void* arg) {

    vTaskDelay(7000 / portTICK_PERIOD_MS);
    bool rumble = 1;
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 0;

    while(1){
        if(ds4_is_connected()){
            if(rumble)
                ds4_rumble(0, 150, 128, 40);
            if(rumble){
                r = 0;
                g = 255;
                b = 0;
            } else {
                r = 255;
                g = 0;
                b = 0;
            }
            ds4_lightbar_color(r, g, b);
            rumble = !rumble;
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void test_ds4_input(void* arg) {

    while(1){
        ds4_input_t input_data;
        if (ds4_is_connected() && ds4_get_input(&input_data) == ESP_OK) {
            ESP_LOGI("DS4 Driver", "Input: l2 %d, r2 %d, lX %d, lY %d, rX %d, rY %d, dpad %d",
                input_data.leftTrigger,
                input_data.rightTrigger,
                input_data.leftStickX,
                input_data.leftStickY,
                input_data.rightStickX,
                input_data.rightStickY,
                input_data.dpad);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

int app_main(void) {

    ds4_init();

    xTaskCreatePinnedToCore(
        test_ds4_output,   /* Function to implement the task */
        "test_ds4_output", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        1);  /* Core where the task should run */

    xTaskCreatePinnedToCore(
        test_ds4_input,   /* Function to implement the task */
        "test_ds4_input", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        1);  /* Core where the task should run */

    return 0;
}
