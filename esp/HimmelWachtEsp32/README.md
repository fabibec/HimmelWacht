# HimmelWacht ESP32 Software

This repository contains the firmware for the ESP32 microcontroller used in the HimmelWacht project. It handles low-level hardware control, including motor operation and gamepad connectivity. The project is preconfigured for use with Visual Studio Code and the C/C++ extension.


## Getting Started

Follow these steps to set up the project, build the firmware, and flash it to the ESP32.

### Prerequisites

- **ESP-IDF v5.4.1**: This project depends on a specific version of the ESP-IDF, which is included as a Git submodule in the `esp/esp-idf-v5.4.1` directory.
- **Bluepad32 v4.2.0**: The Bluepad32 library is also included as a submodule in the `esp/bluepad32-v4.2.0` directory.

### Installation and Setup

1.  **Initialize Submodules**: If you have cloned the repository without the submodules, initialize them first:
    ```shell
    git submodule update --init --recursive
    ```

2.  **Set up ESP-IDF**: Configure the build environment by running the install and export scripts.
    ```shell
    # From the project root directory
    cd esp/esp-idf-v5.4.1
    ./install.sh
    . ./export.sh
    ```
    *Note: You may need to run the `export.sh` script in every new terminal session.*

3.  **Patch Bluepad32 and BTstack**: Apply the necessary patches to the BTstack library within Bluepad32.
    ```shell
    # From the project root directory
    cd esp/bluepad32-v4.2.0/external/btstack
    git apply ../patches/*.patch
    ```

4.  **Integrate BTstack**: Run the integration script to make the patched BTstack component available to the ESP-IDF.
    ```shell
    # From the project root directory
    cd esp/bluepad32-v4.2.0/external/btstack/port/esp32
    ./integrate_btstack.py
    ```

### Building and Flashing

After completing the setup, return to the main project directory and use the `idf.py` tool to build and flash the firmware.

1.  **Navigate to the project directory**:
    ```shell
    # From the project root directory
    cd esp/HimmelWachtEsp32
    ```

2.  **Build the project**:
    ```shell
    idf.py build
    ```

3.  **Flash the firmware**: Connect the ESP32 to your computer and flash the built firmware.
    ```shell
    idf.py flash
    ```

## Technical Details

### Motor Driver

The Pololu G2 High-Power Motor Driver 24v13 board is used to control the motors. It supports two primary control modes. The current implementation uses Sign-Magnitude mode.

#### Control Modes

1.  **Sign-Magnitude (S-M)**: The PWM duty cycle controls motor speed, and a separate digital pin (`DIR`) controls the direction.
2.  **Locked-Antiphase (L-A)**: The `PWM` pin is held high, and the PWM signal is sent to the `DIR` pin to control both speed and direction.

#### Locked-Antiphase Operation

| Duty Cycle (%) | Locked-Antiphase     |
| :------------- | :------------------- |
| 0              | Full speed reverse   |
| 25             | Medium speed reverse |
| 50             | Coast (motor off)    |
| 75             | Medium speed forward |
| 100            | Full speed forward   |

| PWM | DIR | OUTA | OUTB | Operation |
| :-- | :-- | :--- | :--- | :-------- |
| H   | H   | H    | L    | Forward   |
| H   | L   | L    | H    | Reverse   |
| L   | X   | L    | L    | Brake     |

#### Mode Comparison

| Feature                            | Sign-Magnitude             | Locked-Antiphase                         |
| :--------------------------------- | :------------------------- | :--------------------------------------- |
| **Ease of implementation**         | ✅ Very easy                | ❗ A bit more complex                     |
| **PWM Pin Use**                    | PWM → speed, DIR → direction | DIR → PWM waveform, PWM = always HIGH    |
| **Idle behavior (0% duty)**        | Outputs both LOW = brake   | DIR at 50% duty = coast                  |
| **PWM frequency required**         | Lower okay (~20 kHz)       | ⚠️ Needs higher freq (~50–100+ kHz)       |
| **EMI (Electromagnetic Interference)** | Lower                      | Can be noisier due to polarity switching |
| **Smoothness at low speeds**       | Good                       | ✅ Often smoother, more linear torque     |
| **Power efficiency**               | Slightly better            | Can generate more switching losses       |
| **Torque ripple**                  | Can be higher              | ✅ Lower, smoother torque transitions     |

## Dependencies

- C11
- ESP-IDF v5.4.1
- Bluepad32 v4.2.0
