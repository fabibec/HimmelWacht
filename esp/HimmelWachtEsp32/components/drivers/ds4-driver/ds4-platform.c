#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <uni.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include "ds4-common.h"

/* Prototype */
static void check_battery(uint8_t battery_state);

/*
    Called just once, just after boot time, and before Bluetooth gets initialized.

    @param argc: number of arguments (unused)
    @param argv: arguments (unused)

    @author Fabian Becker
*/
static void on_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ESP_LOGI("Bluepad32 Init", "Using default gamepad mappings");

    // Set default gamepad mappings
    uni_gamepad_mappings_t mappings = GAMEPAD_DEFAULT_MAPPINGS;
    uni_gamepad_set_mappings(&mappings);
}

/*
    Called when initialization finishes.

    @author Fabian Becker
*/
static void on_init_complete(void) {
    ESP_LOGI("Bluepad32 Init Complete", "Enabling Bluetooth for paired devices");

    // Since the MAC Address of the ESP32 in set on the Dualshock4 controller,
    // we don't need to scan for devices, but just allow incoming connections.
    uni_bt_allow_incoming_connections(true);
}

/*
    Called when a new device has been discovered.

    @author Fabian Becker
*/
static uni_error_t on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {

    // Filter for the linked DualShock4 Controller, ignore any other device
    if (memcmp(addr, ds4_address, sizeof(bd_addr_t)) == 0) {
        ESP_LOGI("Bluepad32 Device Discovered", "Found DS4: %02x:%02x:%02x:%02x:%02x:%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return UNI_ERROR_SUCCESS;
    }

    ESP_LOGI("Bluepad32 Device Discovered", "Found unknown device: %02x:%02x:%02x:%02x:%02x:%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return UNI_ERROR_IGNORE_DEVICE;
}

/*
    Called when the DualShock4 controller connects. But probably it is not ready to use.
    HID and/or other things might not have been parsed/init yet.

    @param d: the device that connected (unused)

    @author Fabian Becker
*/
static void on_device_connected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    ESP_LOGI("Bluepad32 Device Connected", "Found DS4");
}

/*
    Called when the DualShock4 controller disconnects.

    @param d: the device that disconnected (unused)

    @author Fabian Becker
*/
static void on_device_disconnected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    xEventGroupClearBitsFromISR(ds4_event_group, DS4_CONNECTED);
    ESP_LOGI("Bluepad32 Device Disconnected", "DS4 Disconnected");
}

/*
    Called when the DualShock4 controller is ready to be used.

    @param d: the device that is ready

    @author Fabian Becker
*/
static uni_error_t on_device_ready(uni_hid_device_t* d) {
    ESP_LOGI("Bluepad32 Device Ready", "DS4 Ready");

    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 150 /* duration ms */, 128 /* weak magnitude */,
                                          40 /* strong magnitude */);
    }

    xEventGroupSetBitsFromISR(ds4_event_group, DS4_CONNECTED, NULL);
    return UNI_ERROR_SUCCESS;
}

/*
    Called when a controller button, stick, gyro, etc. has changed.

    @param d: the device that is ready
    @param ctl: the controller data

    @author Fabian Becker
*/
static void on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl){
    static ds4_input_t current_input = {0};
    static int64_t last_input_time = 0;
    static uni_gamepad_t* gp;
    static uint8_t battery_level = 0xFF;

    // Limit the input processing rate to avoid flooding the event queue
    int64_t now = esp_timer_get_time();
    if (now - last_input_time < input_processing_interval_us) {
        return;
    }
    last_input_time = now;

    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            gp = &ctl->gamepad;

            current_input.leftTrigger = gp->brake;
            current_input.rightTrigger = gp->throttle;
            current_input.leftStickX = gp->axis_x;
            current_input.leftStickY = gp->axis_y;
            current_input.rightStickX = gp->axis_rx;
            current_input.rightStickY = gp->axis_ry;
            current_input.dpad = gp->dpad;
            current_input.buttons = (gp->buttons & (BUTTON_CROSS_MASK | BUTTON_CIRCLE_MASK | BUTTON_SQUARE_MASK | BUTTON_TRIANGLE_MASK));
            current_input.triggerButtons = (gp->buttons & (BUTTON_R1_MASK | BUTTON_L1_MASK)) >> 4;
            current_input.battery = ctl->battery;

            /*ESP_LOGI("DS4 Driver", "Input: l2 %d, r2 %d, lX %d, lY %d, rX %d, rY %d, dpad %d, triggers %d, buttons %d",
                current_input.leftTrigger,
                current_input.rightTrigger,
                current_input.leftStickX,
                current_input.leftStickY,
                current_input.rightStickX,
                current_input.rightStickY,
                current_input.dpad,
                current_input.triggerButtons,
                current_input.buttons);
            */

            // Add input to event queue
            xQueueOverwriteFromISR(ds4_input_queue, &current_input, NULL);

            // Update Battery
            battery_level = d->controller.battery;
            //ESP_LOGI("DS4 Driver", "Battery %d", battery_level);
            check_battery(battery_level);
            break;
        default:
            break;
    }
}

/*
    Called when a property is requested.

    @param idx: the property index

    @return: NULL since not supported/needed here

    @author Fabian Becker
*/
static const uni_property_t* get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

/*
    Called when an OOB event occurs. In case of the Dualshock4 controller, when the PS Button is pressed.

    @param event: the OOB event
    @param data: the data associated with the event

    @author Fabian Becker
*/
static void on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
}

/*
    Checks the battery level of the Dualshock4 Controller and sets an event, if the level is below a defined threshold.

    @param battery_state: absolute battery_level from gamepad (0 - 254, 255)

    @author Fabian Becker
*/
static void IRAM_ATTR check_battery(uint8_t battery_state) {
    EventBits_t bits = xEventGroupGetBitsFromISR(ds4_event_group);

    // Read battery level, set event when battery level low
    if (battery_state < low_battery_threshold && !(bits & DS4_BATTERY_LOW)){
        xEventGroupSetBitsFromISR(ds4_event_group, DS4_BATTERY_LOW, NULL);
    } else if (battery_state >= low_battery_threshold && (bits & DS4_BATTERY_LOW)) {
        xEventGroupClearBitsFromISR(ds4_event_group, DS4_BATTERY_LOW);
    }
}

struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "ds4-platform",
        .init = on_init,
        .on_init_complete = on_init_complete,
        .on_device_discovered = on_device_discovered,
        .on_device_connected = on_device_connected,
        .on_device_disconnected = on_device_disconnected,
        .on_device_ready = on_device_ready,
        .on_oob_event = on_oob_event,
        .on_controller_data = on_controller_data,
        .get_property = get_property,
    };

    return &plat;
}

