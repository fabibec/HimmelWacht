/**
 * @file
 * @brief DualShock 4 driver for Esp32
 *
 * This module provides support for the DualShock 4 controller on the Esp32 platform.
 * It utilizes the Bluepad32 library to manage Bluetooth connections and controller interactions.
 *
 * @author Fabian Becker
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <uni.h>

#include "include/ds4-driver.h"

static volatile bd_addr_t ds4_addr = {0x00};

/**
 * @brief [Provide a brief description of the function or code block here.]
 *
 * [Provide a more detailed explanation of what the function or code block does,
 * including any important details or context.]
 *
 * @param [param_name] [Description of the parameter, including its purpose and expected value.]
 * @param [param_name] [Description of the parameter, including its purpose and expected value.]
 * ...
 *
 * @return [Description of the return value, if applicable, including what it represents.]
 *
 * @note [Optional: Include any additional notes, warnings, or important information.]
 *
 * @example [Optional: Provide an example of how to use the function or code block.]
 */
void bluepad32_task(void* arg){
    // Defined in ds4-platform.c
    struct uni_platform* get_my_platform(void);

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // Must be called before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Init Bluepad32.
    uni_init(0, NULL);

    // Does not return.
    btstack_run_loop_execute();
}

void ds4_init(void){

    // Create the bluepad32 task
    xTaskCreatePinnedToCore(
        bluepad32_task,   /* Function to implement the task */
        "bluepad32_task", /* Name of the task */
        8192,       /* Stack size in words */
        NULL,       /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */
}
