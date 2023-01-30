# -*- make -*-

PROJ = $(notdir $(PWD))
SOURCES = $(wildcard *.ino *.cpp *.h)
FSDIR = data
FILES = $(wildcard $(FSDIR)/*)

# Don't use -DATOMIC_FS_UPDATE
CFLAGS_DEFAULT = -DNO_GLOBAL_HTTPUPDATE
CFLAGS = $(CFLAGS_DEFAULT)

CLI := arduino-cli
PLATFORM := esp8266:esp8266
CFGFILE := arduino-cli.yaml
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json
LIBRARIES := libraries/WiFiManager libraries/ArduinoJson libraries/pubsubclient libraries/TelnetStream libraries/Acetime libraries/Time libraries/OneWire libraries/DallasTemperature libraries/U8g2
BOARDS := arduino/package_esp8266com_index.json
# PORT can be overridden by the environment or on the command line. E.g.:
# export PORT=/dev/ttyUSB2; make upload, or: make upload PORT=/dev/ttyUSB2
PORT ?= /dev/ttyUSB0
BAUD ?= 460800

INO = $(PROJ).ino
MKFS = $(wildcard arduino/packages/esp8266/tools/mklittlefs/*/mklittlefs)
TOOLS = $(wildcard arduino/packages/esp8266/hardware/esp8266/*/tools)
ESPTOOL = python3 $(TOOLS)/esptool/esptool.py
BOARD = $(PLATFORM):d1_mini
FQBN = $(BOARD):eesz=4M1M
IMAGE = build/$(subst :,.,$(BOARD))/$(INO).bin
FILESYS = filesys.bin

export PYTHONPATH = $(TOOLS)/pyserial

binaries: $(IMAGE)

publish: $(PROJ)-fs.bin $(PROJ)-fw.bin

platform: $(BOARDS)

clean:
	find $(FSDIR) -name '*~' -exec rm {} +

$(CFGFILE):
	$(CLI) config init --dest-file $(CFGFILE)
	$(CLI) config set board_manager.additional_urls $(ESP8266URL)
	$(CLI) config set directories.data $(PWD)/arduino
	$(CLI) config set directories.downloads $(PWD)/staging
	$(CLI) config set directories.user $(PWD)
	$(CLI) config set sketch.always_export_binaries true
	$(CLI) config set library.enable_unsafe_install true

$(BOARDS): | $(CFGFILE)
	$(CLI) core update-index
	$(CLI) core install $(PLATFORM)

refresh: | $(CFGFILE)
	$(CLI) lib update-index

flush: | $(CFGFILE)
	$(CLI) cache clean

libraries/WiFiManager: | $(BOARDS)
	$(CLI) lib install WiFiManager@2.0.15-rc.1

libraries/ArduinoJson:
	$(CLI) lib install ArduinoJson@6.17.2

libraries/pubsubclient:
	$(CLI) lib install pubsubclient@2.8.0

libraries/TelnetStream:
	$(CLI) lib install TelnetStream@1.2.2

libraries/Acetime:
	$(CLI) lib install Acetime@2.0.1

libraries/U8g2:
	$(CLI) lib install u8g2@2.33.15 

libraries/Time:
	$(CLI) lib install --git-url https://github.com/PaulStoffregen/Time
	# https://github.com/PaulStoffregen/Time/archive/refs/tags/v1.6.1.zip

libraries/OneWire:
	$(CLI) lib install OneWire@2.3.6

libraries/DallasTemperature:
	$(CLI) lib install DallasTemperature@3.9.0

$(IMAGE): $(BOARDS) $(LIBRARIES) $(SOURCES)
	$(CLI) compile --config-file $(CFGFILE) --fqbn=$(FQBN) --warnings default --verbose --build-property compiler.cpp.extra_flags="$(CFLAGS)"

$(FILESYS): $(FILES) $(CONF) | $(BOARDS) clean
	$(MKFS) -p 256 -b 8192 -s 1024000 -c $(FSDIR) $@

$(PROJ)-fs.bin: $(FILES) $(CONF) | $(BOARDS) clean
	$(MKFS) -p 256 -b 8192 -s 1024000 -c $(FSDIR) $@

$(PROJ)-fw.bin: $(IMAGE)
	cp $(IMAGE) $@

$(PROJ).zip: $(PROJ)-fw.bin $(PROJ)-fs.bin
	rm -f $@
	zip $@ $^

# Build the image with debugging output
debug: CFLAGS = $(CFLAGS_DEFAULT) -DDEBUG
debug: $(IMAGE)
	
# Load only the sketch into the device
upload: $(IMAGE)
	$(ESPTOOL) --port $(PORT) -b $(BAUD) write_flash 0x0 $(IMAGE)

# Load only the file system into the device
upload-fs: $(FILESYS)
	$(ESPTOOL) --port $(PORT) -b $(BAUD) write_flash 0x300000 $(FILESYS)

# Load both the sketch and the file system into the device
install: $(IMAGE) $(FILESYS)
	$(ESPTOOL) --port $(PORT) -b $(BAUD) write_flash 0x0 $(IMAGE) 0x300000 $(FILESYS)

.PHONY: binaries platform publish clean upload upload-fs install debug

### Allow customization through a local Makefile: Makefile-local.mk

# Include the local make file, if it exists
-include Makefile-local.mk
