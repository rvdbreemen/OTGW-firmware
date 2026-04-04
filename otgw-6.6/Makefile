# -*- make -*-

PIC = p16f1847
SCRIPT = 16f1847.lkr
PROJECT = gateway
OTGWOBJECTS = gateway.o ds1820.o selfprog.o
DIAGOBJECTS = diagnose.o selfprog.o
INTFOBJECTS = interface.o selfprog.o
OUTPUT = $(PROJECT).hex
COD = $(PROJECT).cod
GPASM = gpasm
GPLINK = gplink
GPSIM = gpsim
TCLSH = tclsh

$(OUTPUT) $(COD): $(OTGWOBJECTS) $(SCRIPT)
	$(GPLINK) --map -s $(SCRIPT) -o $@ $(OTGWOBJECTS)

diagnose.hex diagnose.cod: $(DIAGOBJECTS) $(SCRIPT)
	$(GPLINK) --map -s $(SCRIPT) -o $@ $(DIAGOBJECTS)

interface.hex: $(INTFOBJECTS) $(SCRIPT)
	$(GPLINK) --map -s $(SCRIPT) -o $@ $(INTFOBJECTS)

%.o: %.asm
	$(GPASM) $(CFLAGS) -c $<

gateway.o: build.asm

build.asm: gateway.asm ds1820.asm
	$(TCLSH) tools/build.tcl

test: export PIC ::= $(PIC)
test: $(COD)
	GPSIM=$(GPSIM) $(TCLSH) tests/all.tcl $(TESTFLAGS)

clean:
	rm -f *~ *.o *.lst *.map *.hex *.cod

.PHONY: clean test

# # Include a local make file, if it exists
-include Makefile-local.mk
