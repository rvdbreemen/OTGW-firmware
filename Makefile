# -*- make -*-

PROJ = OTGW-firmware
SRCDIR = src/$(PROJ)
SOURCES = $(wildcard $(SRCDIR)/*.ino $(SRCDIR)/*.cpp $(SRCDIR)/*.h)
FSDIR = $(SRCDIR)/data
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
LIBRARIES := libraries/WiFiManager libraries/ArduinoJson libraries/PubSubClient libraries/TelnetStream libraries/AceCommon libraries/AceSorting libraries/AceTime libraries/OneWire libraries/DallasTemperature libraries/WebSockets libraries/Time
BOARDS := arduino/package_esp8266com_index.json
# PORT can be overridden by the environment or on the command line. E.g.:
# export PORT=/dev/ttyUSB2; make upload, or: make upload PORT=/dev/ttyUSB2
PORT ?= /dev/ttyUSB0
BAUD ?= 460800

INO = $(SRCDIR)/$(PROJ).ino
MKFS = $(wildcard arduino/packages/esp8266/tools/mklittlefs/*/mklittlefs)
TOOLS = $(wildcard arduino/packages/esp8266/hardware/esp8266/*/tools)
ESPTOOL = python3 $(TOOLS)/esptool/esptool.py
BOARD = $(PLATFORM):d1_mini
FQBN = $(BOARD):eesz=4M2M,xtal=160
# Arduino-cli output path logic is complex, simplified here for 'build' target if CLI puts it in build/ relative to sketch
IMAGE = build/$(PROJ).ino.bin
FILESYS = build/$(PROJ).littlefs.bin

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
# Retry up to 3 times with exponential backoff to handle transient network errors
##
update_indexes: | $(CFGFILE)
	@echo "Updating package indexes..."
	@for i in 1 2 3; do \
		if $(CLICFG) core update-index; then \
			echo "✓ Core index updated successfully"; \
			break; \
		else \
			if [ $$i -lt 3 ]; then \
				wait_time=$$((2 ** $$i)); \
				echo "⚠ Core index update failed (attempt $$i/3), retrying in $${wait_time}s..."; \
				sleep $$wait_time; \
			else \
				echo "✗ Core index update failed after 3 attempts"; \
				exit 1; \
			fi; \
		fi; \
	done
	@for i in 1 2 3; do \
		if $(CLICFG) lib update-index; then \
			echo "✓ Library index updated successfully"; \
			break; \
		else \
			if [ $$i -lt 3 ]; then \
				wait_time=$$((2 ** $$i)); \
				echo "⚠ Library index update failed (attempt $$i/3), retrying in $${wait_time}s..."; \
				sleep $$wait_time; \
			else \
				echo "✗ Library index update failed after 3 attempts"; \
				exit 1; \
			fi; \
		fi; \
	done

$(LIBRARIES): | update_indexes

$(BOARDS): | update_indexes
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

libraries/AceCommon:
	$(CLICFG) lib install AceCommon@1.6.2

libraries/AceSorting:
	$(CLICFG) lib install AceSorting@1.0.0

libraries/AceTime:
	$(CLICFG) lib install AceTime@2.0.1

libraries/Time:
	$(CLICFG) lib install Time@1.6.1

libraries/OneWire:
	$(CLICFG) lib install OneWire@2.3.8

libraries/DallasTemperature: | libraries/OneWire
	$(CLICFG) lib install DallasTemperature@3.9.0

libraries/WebSockets:
	$(CLICFG) lib install WebSockets@2.3.5

$(IMAGE): $(BOARDS) $(LIBRARIES) $(SOURCES)
	$(info Build code)
	$(CLICFG) compile --fqbn=$(FQBN) --warnings default --verbose --libraries src/libraries --build-path build --build-property compiler.cpp.extra_flags="$(CFLAGS)" $(SRCDIR)

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

# Run workspace evaluation
evaluate:
	python3 evaluate.py --report

# Quick evaluation check
check:
	python3 evaluate.py --quick

.PHONY: binaries platform publish clean upload upload-fs install debug filesystem evaluate check

### Allow customization through a local Makefile: Makefile-local.mk

# Include the local make file, if it exists
-include Makefile-local.mk
