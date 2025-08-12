# HimmelWacht

This repository contains all the files, documentation, and source code for the HimmelWacht project. The project integrates a Raspberry Pi for high-level processing, an ESP32 for real-time hardware control, and a laboratory computer for AI model inference.

## Project Description

This project develops a Bluetooth-controller-operated vehicle equipped with a manual and semi-automatic Nerf dart launcher. The launcher fires at a wooden glider attached to a stick and moved by a person. In manual mode, the vehicle, turret platform, and launcher are controlled via the Bluetooth controller. The semi-automatic mode uses artificial intelligence and computer vision to detect the wooden glider and automatically aim the launcher, ignoring other objects. Movement is possible along both X and Y axes, with vertical motion limited to maintain the camera’s field of view. The system assumes optimal indoor conditions for target detection and tracking.

For more details refer to the [final presentation](documents/praesi/gruppe_1_himmelwacht_praesentation.pdf)

## Repository Structure

This repository is organized into the following directories:

```
.
├── cad/                 # CAD files (FreeCAD, STL) for 3D-printed components.
├── documents/           # Project documentation, datasheets, and diagrams.
├── esp/                 # ESP32 firmware and related tools.
├── laboratory_computer/ # AI models, training scripts, and inference code.
├── raspberry/           # Software for the Raspberry Pi.
└── util/                # Miscellaneous utilities, screenshots, and test files.
```

Each directory contains a `README.md` file with more detailed information about its contents and setup instructions.

## Getting Started

To get started with a specific part of the project, please refer to the `README.md` file within the corresponding directory:

- **For the ESP32 firmware**: See `esp/HimmelWachtEsp32/README.md`
- **For the Raspberry Pi code**: See `raspberry/README.md`

General project documentation can be found in the `documents/` directory.

## Authors

- Fabian Becker
- Jendrik Jürgens
- Nicolas Koch
- Michael Specht
- Jonathan Wohlrab
