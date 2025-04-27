#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_mac.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <uni.h>

#include "include/ds4-driver.h"
#include "ds4-common.h"

QueueHandle_t ds4_input_queue = NULL;

EventGroupHandle_t ds4_event_group = NULL;

static QueueHandle_t ds4_output_event_queue = NULL;

static SemaphoreHandle_t ds4_output_event_callback_sem = NULL;

static btstack_context_callback_registration_t output_event_callback_registration;

/*
    This task is responsible for initializing the Bluepad32 library and setting up the Bluetooth stack.

    @param arg: unused
    @author Fabian Becker

    @note This task does not return, it runs the main loop of the Bluepad32 library.
    @note The task is pinned to core 0.
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

/*
    Callback to be called when the rumble event is triggered.

    @param context: the context passed to the callback (ds4_rumble_t)

    @note This function should only be called the main BTstack thread, as part of a registered callback.
    @note DO NOT call this function yourself!

    @author Fabian Becker
*/
static void ds4_rumble_cb(void* context){

    ds4_rumble_t* rumble = (ds4_rumble_t*) context;
    // We only allow 1 connection at a time, so we can assume that the first device is the one we want to use
    uni_hid_device_t* d = uni_hid_device_get_first_device_with_state(UNI_BT_CONN_STATE_DEVICE_READY);

    // Safety checks in case the gamepad got disconnected while the callback was scheduled
    if(d){
        // Take the rumble event from the queue
        if (d->report_parser.play_dual_rumble != NULL)
        d->report_parser.play_dual_rumble(
            d,
            rumble->start_delay_ms,
            rumble->duration_ms,
            rumble->weak_magnitude,
            rumble->strong_magnitude
        );
    }

    // Unlock the output_event_callback_registration for a new callback
    xSemaphoreGiveFromISR(ds4_output_event_callback_sem, NULL);
}

/*
    Callback to be called when the lightbar event is triggered.

    @param context: the context passed to the callback (ds4_lightbar_color_t)

    @note This function should only be called the main BTstack thread, as part of a registered callback.
    @note DO NOT call this function yourself!

    @author Fabian Becker
*/
static void ds4_lightbar_cb(void* context){

    ds4_lightbar_color_t lightbar_color = *(ds4_lightbar_color_t*) context;
    // We only allow 1 connection at a time, so we can assume that the first device is the one we want to use
    uni_hid_device_t* d = uni_hid_device_get_first_device_with_state(UNI_BT_CONN_STATE_DEVICE_READY);

    // Safety checks in case the gamepad got disconnected while the callback was scheduled
    if(d){
        if (d->report_parser.set_lightbar_color != NULL)
        d->report_parser.set_lightbar_color(
            d,
            lightbar_color.red,
            lightbar_color.green,
            lightbar_color.blue
        );
    }

    // Unlock the output_event_callback_registration for a new callback
    xSemaphoreGiveFromISR(ds4_output_event_callback_sem, NULL);
}

/*
    Task to handle output events for the DS4 controller.
    This task runs in a loop and waits for events to be available in the queue.
    When an event is available, it calls the appropriate callback function.

    @param arg: unused

    @author Fabian Becker
*/
void ds4_output_event_task(void* arg){
    static ds4_output_event_t event;

    static ds4_rumble_t rumble;
    static ds4_lightbar_color_t lightbar_color;

    // Wait for an event to be available in the queue
    while (xQueueReceive(ds4_output_event_queue, &event, portMAX_DELAY) == pdTRUE) {

        // Call the appropriate function based on the event type
        switch (event.event_type) {
            case DS4_OUTPUT_EVENT_RUMBLE:

                // Block the output_event_callback_registration (freed after the callback is executed)
                xSemaphoreTake(ds4_output_event_callback_sem, portMAX_DELAY);
                rumble = event.event_params.rumble;
                output_event_callback_registration.callback = &ds4_rumble_cb;
                output_event_callback_registration.context = &rumble;

                btstack_run_loop_execute_on_main_thread(&output_event_callback_registration);
                break;
            case DS4_OUTPUT_EVENT_LIGHTBAR_COLOR:

                // If the controller signals low battery no events will be accepted
                if(!(xEventGroupGetBitsFromISR(ds4_event_group) & DS4_BATTERY_LOW)) {
                    // Block the output_event_callback_registration (freed after the callback is executed)
                    xSemaphoreTake(ds4_output_event_callback_sem, portMAX_DELAY);
                    lightbar_color = event.event_params.lightbar;
                    output_event_callback_registration.callback = &ds4_lightbar_cb;
                    output_event_callback_registration.context = &lightbar_color;

                    btstack_run_loop_execute_on_main_thread(&output_event_callback_registration);
                }
                break;
            default:
                break;
        }
    }
}

void ds4_low_battery_signal_task(void* arg){
    static uint8_t red = 0xFF;

    while(1){
        // Wait until low battery
        xEventGroupWaitBits(ds4_event_group, DS4_BATTERY_LOW, pdFALSE, pdFALSE, portMAX_DELAY);

        // We only allow 1 connection at a time, so we can assume that the first device is the one we want to use
        uni_hid_device_t* d = uni_hid_device_get_first_device_with_state(UNI_BT_CONN_STATE_DEVICE_READY);

        if (d && d->report_parser.set_lightbar_color != NULL) {
            d->report_parser.set_lightbar_color(d, red, 0x00, 0x00);
        }
        red = ~red;

        vTaskDelay(low_battery_blinking_interval_ms / portTICK_PERIOD_MS);
    }
}

/*
    Initialize the DS4 driver.
    The driver awaits for a DS4 controller with the same MAC address as the esp to be connected.

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t ds4_init(void){

    // Set the DS4 address
    esp_read_mac(ds4_address, ESP_MAC_BT);
    ESP_LOGI("DS4 Driver Init", "Set DS4 address to %02x:%02x:%02x:%02x:%02x:%02x",
        ds4_address[0], ds4_address[1], ds4_address[2],
        ds4_address[3], ds4_address[4], ds4_address[5]);

    // Create the event group for DS4 connection
    ds4_event_group = xEventGroupCreate();
    if(ds4_event_group == NULL){
        ESP_LOGE("DS4 Driver Init", "Failed to create event group");
        return ESP_FAIL;
    }

    // Create the queue for input events
    ds4_input_queue = xQueueCreate(1, sizeof(ds4_input_t));
    if(ds4_input_queue == NULL){
        ESP_LOGE("DS4 Driver Init", "Failed to create input queue");
        return ESP_FAIL;
    }

    // Create the queue for output events
    ds4_output_event_queue = xQueueCreate(output_event_queue_size, sizeof(ds4_output_event_t));
    if (ds4_output_event_queue == NULL) {
        ESP_LOGE("DS4 Driver Init", "Failed to create output event queue");
        return ESP_FAIL;
    }

    // Create the binary semaphore for the output event callback
    ds4_output_event_callback_sem = xSemaphoreCreateBinary();
    if(ds4_output_event_callback_sem == NULL){
        ESP_LOGE("DS4 Driver Init", "Failed to create output event callback semaphore");
        return ESP_FAIL;
    }
    xSemaphoreGive(ds4_output_event_callback_sem);

    BaseType_t task_created = pdFALSE;

    // Create the bluepad32 task
    task_created = xTaskCreatePinnedToCore(
        bluepad32_task,   /* Function to implement the task */
        "bluepad32_task", /* Name of the task */
        8192,       /* Stack size in words */
        NULL,  /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */
    if (task_created != pdTRUE) {
        ESP_LOGE("DS4 Driver Init", "Failed to create bluepad32 task");
        return ESP_FAIL;
    }

    task_created = pdFALSE;

    // Create the output event task
    task_created = xTaskCreatePinnedToCore(
        ds4_output_event_task,   /* Function to implement the task */
        "ds4_output_event_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */
    if(task_created != pdTRUE) {
        ESP_LOGE("DS4 Driver Init", "Failed to create output event task");
        return ESP_FAIL;
    }

    task_created = pdFALSE;

    // Create the low battery signal task
    task_created = xTaskCreatePinnedToCore(
        ds4_low_battery_signal_task,   /* Function to implement the task */
        "ds4_low_battery_signal_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */
    if(task_created != pdTRUE) {
        ESP_LOGE("DS4 Driver Init", "Failed to create low battery signal task");
        return ESP_FAIL;
    }

    ESP_LOGI("DS4 Driver Init", "DS4 driver initialized");
    return ESP_OK;
}

/*
    Send a rumble event to the DS4 controller.
    This function is thread-safe and can be called from any task.

    @param start_delay_ms: delay before the rumble starts
    @param duration_ms: duration of the rumble
    @param weak_magnitude: weak rumble magnitude (0-255)
    @param strong_magnitude: strong rumble magnitude (0-255)

    @return ESP_OK on success, ESP_FAIL on failure

    @author Fabian Becker
*/
esp_err_t ds4_rumble(uint16_t start_delay_ms, uint16_t duration_ms, uint8_t weak_magnitude, uint8_t strong_magnitude){

    esp_err_t ret = ESP_OK;

    ds4_rumble_t rumble = {
        .start_delay_ms = start_delay_ms,
        .duration_ms = duration_ms,
        .weak_magnitude = weak_magnitude,
        .strong_magnitude = strong_magnitude
    };
    ds4_output_event_t event = {
        .event_type = DS4_OUTPUT_EVENT_RUMBLE,
        .event_params.rumble = rumble
    };

    // Send the rumble event to the queue, if the queue is full, discard the event
    if (xQueueSendToBack(ds4_output_event_queue, (void *) &event, (TickType_t) 0) != pdTRUE) {
        ESP_LOGE("DS4 Driver", "Failed to send rumble event to queue");
        ret = ESP_FAIL;
    }

    return ret;
}

/*
    Set the lightbar color of the DS4 controller.
    This function is thread-safe and can be called from any task.

    @param r: red component (0-255)
    @param g: green component (0-255)
    @param b: blue component (0-255)

    @return ESP_OK on success, ESP_FAIL on failure

    @note If the controller has low battery these events will be ignored.

    @author Fabian Becker
*/
esp_err_t ds4_lightbar_color(uint8_t r, uint8_t g, uint8_t b){

    esp_err_t ret = ESP_OK;

    ds4_lightbar_color_t color = {
        .red = r,
        .green = g,
        .blue = b
    };
    ds4_output_event_t event = {
        .event_type = DS4_OUTPUT_EVENT_LIGHTBAR_COLOR,
        .event_params.lightbar = color
    };

    // Send the lightbar event to the queue, if the queue is full, discard the event
    if (xQueueSendToBack(ds4_output_event_queue, (void *) &event, (TickType_t) 0) != pdTRUE) {
        ret = ESP_FAIL;
    }

    return ret;
}

/*
    Check if the DS4 controller is connected.

    @return true if connected, false otherwise

    @author Fabian Becker
*/
inline bool ds4_is_connected(void){
    return xEventGroupGetBits(ds4_event_group) & DS4_CONNECTED ? true : false;
}

/*
    Wait for the DS4 controller to be connected.
    This function blocks until the DS4 controller is connected.

    @author Fabian Becker
*/
inline void ds4_wait_for_connection(void){
    // Wait for the DS4 controller to be connected, don't clear the bit
    xEventGroupWaitBits(ds4_event_group, DS4_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
}
