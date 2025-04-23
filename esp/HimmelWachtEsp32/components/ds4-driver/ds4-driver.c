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
#include "ds4-common.h"

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
        NULL,  /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */
}

void ds4_rumble_cb(void* context){
    uni_hid_device_t* d;
    ds4_rumble_context_t* ctx = (ds4_rumble_context_t*) context;

    d = uni_hid_device_get_instance_for_address((uint8_t*) ds4_address);

    // Safety checks in case the gamepad got disconnected while the callback was scheduled
    if (!d) return;
    if (!uni_bt_conn_is_connected(&d->conn)) return;

    if (d->report_parser.play_dual_rumble != NULL)
        d->report_parser.play_dual_rumble(d, ctx->start_delay_ms, ctx->duration_ms, ctx->weak_magnitude, ctx->strong_magnitude);
}

void ds4_rumble(uint16_t start_delay_ms, uint16_t duration_ms, uint8_t weak_magnitude, uint8_t strong_magnitude){
    // TODO
}

void ds4_set_lightbar_color(uint8_t r, uint8_t g, uint8_t b){
    // TODO
}
