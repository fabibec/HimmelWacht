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




## Version & Dependencies
- C11
- ESP IDF v5.4.1
- Bluepad32 v4.2.0
