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
CFGFILE := $(PWD)/arduino/arduino-cli.yaml
# Add CLICFG command to add config file location to CLI command
CLICFG := $(CLI) --config-file $(CFGFILE)
# bug in http stream, fallback to 2.7.4
# ESP8266URL := https://github.com/esp8266/Arduino/releases/download/3.0.2/package_esp8266com_index.json
ESP8266URL := https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json
LIBRARIES := libraries/WiFiManager libraries/ArduinoJson libraries/PubSubClient libraries/TelnetStream libraries/AceTime libraries/OneWire libraries/DallasTemperature libraries/WebSockets
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
FQBN = $(BOARD):eesz=4M2M,xtal=160
IMAGE = build/$(INO).bin
FILESYS = build/$(INO).littlefs.bin

export PYTHONPATH = $(TOOLS)/pyserial

binaries: $(IMAGE)

publish: $(PROJ)-fs.bin $(PROJ)-fw.bin

platform: $(BOARDS)

clean:
	find $(FSDIR) -name '*~' -exec rm {} +

distclean: clean
	rm -f *~
	rm -rf arduino build libraries staging arduino-cli.yaml

$(CFGFILE):
	$(CLI) config init --dest-file $(CFGFILE)
	$(CLICFG) config set directories.data $(PWD)/arduino
	$(CLICFG) config set board_manager.additional_urls $(ESP8266URL)
	$(CLICFG) config set directories.downloads $(PWD)/staging
	$(CLICFG) config set directories.user $(PWD)
	$(CLICFG) config set sketch.always_export_binaries true
	$(CLICFG) config set library.enable_unsafe_install true

##
# Make sure CFG is updated before libraries are called.
##
$(LIBRARIES): | $(CFGFILE)

$(BOARDS): | $(CFGFILE)
	$(CLICFG) core update-index
	$(CLICFG) core install $(PLATFORM)

refresh: | $(CFGFILE)
	$(CLICFG) lib update-index

flush: | $(CFGFILE)
	$(CLICFG) cache clean

libraries/WiFiManager: | $(BOARDS)
	$(CLICFG) lib install WiFiManager@2.0.15-rc.1

libraries/ArduinoJson:
	$(CLICFG) lib install ArduinoJson@6.17.2

libraries/PubSubClient:
	$(CLICFG) lib install pubsubclient@2.8.0

libraries/TelnetStream:
	$(CLICFG) lib install TelnetStream@1.2.4

libraries/AceTime:
	$(CLICFG) lib install Acetime@2.0.1

# libraries/Time:
# 	$(CLI) lib install --git-url https://github.com/PaulStoffregen/Time
# 	# https://github.com/PaulStoffregen/Time/archive/refs/tags/v1.6.1.zip

libraries/OneWire:
	$(CLICFG) lib install OneWire@2.3.6

libraries/DallasTemperature: | libraries/OneWire
	$(CLICFG) lib install DallasTemperature@3.9.0

libraries/WebSockets:
	# WebSockets@2.3.6 is intentionally pinned for compatibility with ESP8266 Arduino Core 2.7.4.
	# Newer WebSockets releases have not been validated on this firmware and may introduce regressions.
	$(CLICFG) lib install WebSockets@2.3.6

$(IMAGE): $(BOARDS) $(LIBRARIES) $(SOURCES)
	$(info Build code)
	$(CLICFG) compile --fqbn=$(FQBN) --warnings default --verbose --build-property compiler.cpp.extra_flags="$(CFLAGS)"

filesystem: $(FILESYS)

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
	$(ESPTOOL) --port $(PORT) -b $(BAUD) write_flash 0x200000 $(FILESYS)

# Load both the sketch and the file system into the device
install: $(IMAGE) $(FILESYS)
	$(ESPTOOL) --port $(PORT) -b $(BAUD) write_flash 0x0 $(IMAGE) 0x200000 $(FILESYS)

.PHONY: binaries platform publish clean upload upload-fs install debug filesystem

### Allow customization through a local Makefile: Makefile-local.mk

# Include the local make file, if it exists
-include Makefile-local.mk
