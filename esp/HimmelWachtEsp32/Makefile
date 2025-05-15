all: install flash monitor

install:
	. ../esp-idf-v5.4.1/export.sh

build:
	idf.py build

flash:
	idf.py -p /dev/ttyUSB0 flash

monitor:
	idf.py -p /dev/ttyUSB0 monitor

