# HimmelWacht Esp32 Software
This project contains the software that runs on the Esp32. The project is also preconfigured to run in VsCode with the C/C++ Extension.

## Bluepad32 Installation
For the DualShock4 Driver the library Bluepad32 and the BTstack Component is required.

1. Clone the GitHub Repo

    ```shell
    git clone --recursive https://github.com/ricardoquesada/bluepad32.git
    ```
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


## Version
- C11
- ESP IDF v5.4.1, install to ../esp-idf-v5.4.1
- Bluepad32 v4.2.0, install to ../bluepad32-v4.2.0
