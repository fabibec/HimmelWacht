#ifndef _VEHICLE_CONTROL_H_
#define _VEHICLE_CONTROL_H_
#include <esp_err.h>
#include "diff-drive.h"

typedef struct {
    int64_t button_hold_threshold_us; // Threshold for button hold in microseconds
    int16_t max_deg_per_sec_x; // Maximum degrees per second for the x platform servo
    int16_t max_deg_per_sec_y; // Maximum degrees per second for the x platform servo
    int8_t input_processing_freq_hz; // Input processing frequency in Hz
    int8_t deadzone_x; // X Deadzone for the joystick input
    int8_t deadzone_y; // Y Deadzone for the joystick input
    int8_t core; // Core to run the control on
    int8_t deadzone_drive_update; // Deadzone for the drive update
} vehicle_control_config_t;

esp_err_t vehicle_control_init(vehicle_control_config_t* cfg, diff_drive_handle_t *diff_drive);

#endif // _VEHICLE_CONTROL_H_
