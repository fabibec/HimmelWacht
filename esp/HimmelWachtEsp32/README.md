# HimmelWacht Esp32 Software
This project contains the software that runs on the Esp32. The project is also preconfigured to run in VsCode with the C/C++ Extension.

## ESP IDF Installation
The correct ESP-IDF version is included in this repository as a submodule.

## Bluepad32 Installation
For the DualShock4 Driver the library Bluepad32 and the BTstack Component is required.

1. Clone the GitHub Repo

    The Bluepad32 Repository is included in this project as a submodule.
    So just clone the submodules to get it installed at the right path.

2. Patch BTstack nd integrate it as a local component

    Patch it:
    ```shell
    cd ../bluepad32-v4.2.0/external/btstack
    git apply ../patches/*.patch
    ```

    Integrate it:
    ```shell
    cd ../bluepad32-v4.2.0/external/btstack/port/esp32
    ./integrate_btstack.py
    ```
Now you are able to run the project

## motor-driver

### Usage of driver board

In principal the driver board Pololu G2 High-Power Motor Driver 24v13 offers two modes to create control the motor:

1. **sign-magnitude (S-M)**: PWM duty cycle controls the speed of the motor. The DIR controls the direction.

2. **locked-antiphase (L-A)**: PWM is held high and the PWM signal itself is directed to DIR pin.

The following tables should explain how locked-antiphase works in detail:

| Duty Cycle (%) | Locked-Antiphase     |
| -------------- | -------------------- |
| 0              | Full speed reverse   |
| 25             | Medium speed reverse |
| 50             | Coast (motor off)    |
| 75             | Medium speed forward |
| 100            | Full speed forward   |

| PWM | DIR | OUTA | OUTB | Operation |
| --- | --- | ---- | ---- | --------- |
| H   | H   | H    | L    | Forward   |
| H   | L   | L    | H    | Reverse   |
| L   | X   | L    | L    | Brake     |

**Comparison S-M vs. L-A**

| Feature                            | Sign-Magnitude             | Locked-Antiphase                         |
| ---------------------------------- | -------------------------- | ---------------------------------------- |
| Ease of implementation             | ✅ Very easy                | ❗ A bit more complex                     |
| PWM Pin Use                        | PWM → speed DIR → direction | DIR → PWM waveformPWM = always HIGH      |
| Idle behavior (0% duty)            | Outputs both LOW = brake   | DIR at 50% duty = coast                  |
| PWM frequency required             | Lower okay (~20 kHz)       | ⚠️ Needs higher freq (~50–100+ kHz)       |
| EMI (Electromagnetic Interference) | Lower                      | Can be noisier due to polarity switching |
| Smoothness at low speeds           | Good                       | ✅ Often smoother, more linear torque     |
| Power efficiency                   | Slightly better            | Can generate more switching losses       |
| Torque ripple                      | Can be higher              | ✅ Lower, smoother torque transitions     |

The current version only supports S-M. L-A will be implemented if S-M does not fit our requirements.



## Version & Dependencies
- C11
- ESP IDF v5.4.1
- Bluepad32 v4.2.0
