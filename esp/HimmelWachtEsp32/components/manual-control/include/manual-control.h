#ifndef _MANUAL_CONTROL_H_
#define _MANUAL_CONTROL_H_
#include <esp_err.h>
#include "diff-drive.h"

typedef struct {
    int64_t button_hold_threshold_us; // Threshold for button hold in microseconds
    int16_t max_deg_per_sec; // Maximum degrees per second for the platform servos
    int8_t input_processing_freq_hz; // Input processing frequency in Hz
    int8_t deadzone; // Deadzone for the joystick input
    int8_t core; // Core to run the control on
} manual_control_config_t;

esp_err_t manual_control_init(manual_control_config_t* cfg, diff_drive_handle_t *diff_drive);

#endif // _MANUAL_CONTROL_H_
