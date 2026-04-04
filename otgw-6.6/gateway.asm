		title		"OpenTherm Gateway"
		list		p=16F1847, b=8, r=dec

;Copyright (c) 2022 - Schelte Bron

#define		version		"6.6"
#define		phase		"."	;a=alpha, b=beta, .=production
;#define	patch		"8"	;Comment out when not applicable
;#define	bugfix		"1"	;Comment out when not applicable
#include	build.asm

;See the file "license.txt" for information on usage and redistribution of
;this file, and for a DISCLAIMER OF ALL WARRANTIES.

		__config	H'8007', B'00101111111100'
		__config	H'8008', B'11101011111111'

		errorlevel	-302

;The OpenTherm Gateway is located between the thermostat and the boiler of a
;central heating system. In monitor mode it passes its inputs unmodified to
;the outputs using only hardware functions. In back to back mode the messages
;from the thermostat are passed to the boiler under firmware control. In some
;cases the responses from the boiler may be overridden by the gateway and
;external data is injected into the messages. If no thermostat, or a simple
;on/off thermostat is connected, the gateway will generate its own opentherm
;messages to communicate with the Opentherm boiler.
;
;The gateway starts in back to back mode. It switches to monitor mode if for
;some reason the firmware locks up, resulting in a watchdog timeout.
;
;Interaction with the gateway takes place via a serial interface. The interface
;uses 9600 baud, 8 bits, no parity.
;
;Messages on the OpenTherm link are reported as a letter follwed by 8 hex
;digits. The letter indicates if the message was received from the boiler (B),
;or from the thermostat (T). If the gateway modifies the message to the boiler
;in any way, the modified request is also reported (R), as is a modified answer
;back to the thermostat (A). The hex digits represent the message contents.
;
;The serial interface also allows commands to be sent to the gateway. Commands
;consist of two letters, an equals-sign, and an argument. A command must be
;terminated by a carriage-return character (13, 0xd). The gateway understands
;the following commands:
;OT=+15.4	Specify the outside temperature
;TT=18.5	Change the temperature setpoint (temporary). Use 0 (+0.0) to
;		return to the thermostat's normal programming.
;TC=+16.0	Change the temperature setpoint (continuously). Use +0.0 to
;		return to the thermostat's normal programming.
;SB=15.5	Configure the setback temperature to use in combination with
;		GPIO functions HOME (5) and AWAY (6).
;CS=10.0	Override the control setpoint from the thermostat.
;CH=1		Override the CH enable bit when CS != 0
;VS=25		Override the ventilation setpoint from the thermostat.
;MM=40		Override the maximum relative modulation from the thermostat
;SH=72.0	Set the maximum central heating setpoint.
;SW=60.0	Set the domestic hot water setpoint.
;SC=14:45/6	Set the clock and the day of the week, 1 = Monday, 7 = Sunday
;VR=5		Adjust the reference voltage between 0.8V and 2.0V
;HW=1		Control DHW. P to start manual DHW push. A or T for thermostat.
;CL=62.5	Override the cooling control signal from the thermostat.
;CE=1		Override the cooling enable bit when CL != 0
;RR=2		Issue a remote request to the boiler.
;MW=3		Set the Remote Override Operating Mode DHW.
;MH=4		Set the Remote Override Operating Mode for CH1.
;M2=5		Set the Remote Override Operating Mode for CH2.
;LA=F		Configure the function for LED A, defaults to Flame on
;LB=X		Configure the function for LED B, defaults to Transmitting
;LC=O		Configure the function for LED C, defaults to Setpoint Override
;LD=M		Configure the function for LED D, defaults to Maintenance
;LE=P		Configure the function for LED E, defaults to Raised Power
;LF=C		Configure the function for LED F, defaults to Comfort Mode
;		Possible function codes to use for the LEDs are:
;		R: Receiving an Opentherm message from the thermostat or boiler
;		X: Transmitting an Opentherm message to the thermostat or boiler
;		T: Transmitting or receiving a message on the master interface
;		B: Transmitting or receiving a message on the slave interface
;		O: Remote setpoint override is active
;		F: Flame is on
;		H: Flame is on for central heating
;		W: Flame is on for hot water
;		C: Comfort mode (Domestic Hot Water Enable) is on
;		E: Communication error has been detected
;		M: Boiler requires maintenance
;		P: Raised power mode
;GA=3		Configure the function for GPIO pin A, defaults to No Function
;GB=4		Configure the function for GPIO pin B, defaults to No Function
;		Possible GPIO functions are:
;		0 (input): NONE: No GPIO function.
;		1 (output): GND: Port is constantly pulled low.
;		2 (output): VCC: Port is constantly pulled high.
;		3 (output): LEDE: Port outputs the LED E function.
;		4 (output): LEDF: Port outputs the LED F function.
;		5 (input): HOME: High - Cancel remote room setpoint override
;				 Low  - Sets thermostat to setback temperature
;		6 (input): AWAY: High - Sets thermostat to setback temperature
;				 Low  - Cancel remote room setpoint override
;		7 (in/out): DS1820: Drive DS18[SB]20 for outside temperature
;TS=O		Select temperature sensor function. Possible values are:
;		O: Outside temperature
;		R: Return water temperature
;PR=L		Print selected information. Supported information sources are:
;		A: About opentherm gateway (prints the welcome message)
;		B: Build date and time
;		C: Clock speed of the PIC
;		G: Report the GPIO pin configuration
;		I: Report the GPIO pin input states
;		L: Configured LED functions. This prints a string like FXOMPC
;		M: Report the gateway mode
;		O: Setpoint override
;		P: Current Smart-Power mode.
;		R: The state of the automatic Remeha thermostat detection.
;		S: Print the configured setback temperature
;		T: Tweak settings: Ignore Transition and Override in High Byte
;		V: Report the reference voltage setting
;		W: Domestic hot water setting
;PS=1		Select whether to only print a summary line (1) on request, or
;		every received/transmitted opentherm message (0).
;AA=42		Add alternative message to send to the boiler instead of a
;		message that is known not to be supported by the boiler.
;DA=42		Delete a single occurrence from the list of alternative messages
;		to send to the boiler
;SR=18:1,230	Set the response for a DataID. Either one or two data bytes
;		must be provided.
;CR=18		Clear the response for a DataID.
;UI=9		Tell the gateway which DataID's are not supported by the boiler,
;		so the gateway can inject its own message.
;KI=9		Remove a DataID from the list of ID's that have been configured
;		as unsupported by the boiler.
;RS=HBS		Reset boiler counter. Supported counters are:
;		HBS: Central heating burner starts
;		HBH: Central heating burner operation hours
;		HPS: Central heating pump starts
;		HPH: Central heating pump operation hours
;		WBS: Domestic hot water burner starts
;		WBH: Domestic hot water burner operation hours
;		WPS: Domestic hot water pump starts
;		WPH: Domestic hot water pump operation hours
;GW=1		Switch between monitor (0) and gateway (1) mode. GW=R reset.
;IT=1		Control whether multiple mid-bit line transitions are ignored.
;		Normally more than one such transition results in Error 01.
;OH=0		Copy the low byte of MsgID 100 also into the high byte to work
;		with thermostats that look at the wrong byte (Honeywell Vision)
;FT=D		Force thermostat model to Remeha Celcia 20 (C) or iSense (I).
;DP=115		Set the debug pointer to the specified file register

;#######################################################################
; Peripheral use
;#######################################################################
;Comparator 1 is used for requests. The input is connected to the thermostat.
;In monitor mode, the output is connected to the boiler.
;Comparator 2 is used for responses. The input is connected to the Boiler.
;In monitor mode, the output is connected to the thermostat.

;Timer 0 uses a 1:64 prescaler. It is used to measure the 10ms line idle-time
;and is the time base for lighting up the error LED for 2 seconds
;Timer 1 has a 1:8 prescaler, which gives it a period of 524288us, or roughly
;half a second. It is used for flashing the override LED.
;Timer 2 is used to create a 250uS time base. When the first low to high
;transition on one of the comparators is detected the timer is started with
;a 100us head start. A variable is used to count timer 2 roll-overs. Any
;transition of the comparator while the lowest two bits of the rollover
;counter are binary 10 marks a real bit. These bits are shifted into a 32 bit
;message buffer. Also timer 2 is resynchronized. When 34 bits have been
;received, the gateway will send a report to the serial interface. The report
;consists of either a 'T' or a 'B' and eight hex digits. The first letter
;indicates whether the message was received from the thermostat or the
;boiler. The hex digits represent the value of the 32-bits of data in the
;OpenTherm message.

;The AUSART is configured for full duplex asynchronous serial communication at
;9600 baud, 8 bits, no parity. It is used for reporting about opentherm messages
;and receiving commands.

;Analog input 0 is used to measure the voltage level on the opentherm line to
;the thermostat. This way the code can distinguish between a real opentherm
;thermostat and a simple on/off thermostat. An opentherm thermostat will keep
;the line at a few volts (low) or between 15 and 18 volts (high). An on/off
;thermostat will either short-circuit the line (0 volts) or leave the line open
;(20+ volts).

#include	"p16f1847.inc"
include		<coff.inc>

;Define the speed of the PIC clock
#define		mhzstr	"4"

;Define the time (in microseconds) the line must be stable to consider it idle
;after an error was detected.
#ifndef IDLETIME
#define		IDLETIME 10000
#endif

;BAUD contains the SPBRG value for 9600 baud
		constant BAUD=25

;PERIOD is the value to be loaded into PR2 to obtain a 250uS period for TMR2
		constant PERIOD=249

;PRELOADT0 gives TMR0 a head start so the next rollover (with a 1:64 prescaler)
;will happen after roughly IDLETIME microseconds.

		constant PRELOADT0=256 - IDLETIME / 64

;ONESEC contains the number of timer 4 overflows (with a 1:16 prescaler, 1:5
;postscaler and a period of 250) that happen in 1 second. This controls the
;frequency for generating Opentherm requests to the boiler when no Opentherm
;thermostat is connected.
		constant ONESEC=50	;50 * 16 * 250 * 5 = 1000000
;TWOSEC contains the number of timer 0 overflows (with a 1:64 prescaler)
;that happen in roughly 2 seconds. This is the time the error LED will stay
;lit after an error has been detected.
		constant TWOSEC=122	;122 * 64 * 256 = 1998848

;Comparator configuration
		;Bit 7 CxON = 1 - Comparator enabled
		;Bit 6 CxOUT = R/O
		;Bit 5 CxOE = 0 - CxOUT internal only / 1 - output on CxOUT pin
		;Bit 4 CxPOL = 0 - Not inverted
		;Bit 3 Unimplemented
		;Bit 2 CxSP = 1 - Normal power
		;Bit 1 CxHYS = 1 - Hysteresis enabled
		;Bit 0 CxSYNC = 0 - Asynchronous output
		constant MONITORMODE=b'10100110'
		constant GATEWAYMODE=b'10000110'

;Opentherm message types
		constant T_READ=0x00
		constant T_WRITE=0x10
		constant T_INV=0x20
		constant B_RACK=0x40
		constant B_WACK=0x50
		constant B_INV=0x60
		constant B_UNKN=0x70

;Opentherm Data IDs
		constant MSG_STATUS=0
		constant MSG_CTRLSETPT=1
		constant MSG_REMOTECMD=4
		constant MSG_FAULTCODE=5
		constant MSG_REMOTEPARAM=6
		constant MSG_SETPTOVERRIDE=9
		constant MSG_MAXMODULATION=14
		constant MSG_ROOMSETPOINT=16
		constant MSG_MODULATION=17
		constant MSG_CHPRESSURE=18
		constant MSG_DAYTIME=20
		constant MSG_BOILERTEMP=25
		constant MSG_OUTSIDETEMP=27
		constant MSG_RETURNTEMP=28
		constant MSG_DHWBOUNDS=48
		constant MSG_MAXCHBOUNDS=49
		constant MSG_DHWSETPOINT=56
		constant MSG_MAXCHSETPOINT=57
		constant MSG_OPERMODES=99
		constant MSG_FUNCOVERRIDE=100

;Opentherm remote requests
		constant RCMD_TELEFOON=170

;Some default values to use if the boiler hasn't specified its actual limits.
		constant MIN_DHWSETPOINT=20
		constant MAX_DHWSETPOINT=80
		constant MIN_CHSETPOINT=10
		constant MAX_CHSETPOINT=90

;Line voltage thresholds
;To be independent of the supply voltage, the values defined here are relative
;to the Fixed Voltage Reference of 2.048V. Unfortunately it is not possible to
;use the fixed voltage reference directly as the positive voltage reference
;for the A/D converter, because the input voltage may exceed this value. So,
;the supply voltage must be used as the A/D converter's positive reference. To
;get the correct threshold for the used supply voltage, the values need to be
;multiplied with the value obtained from measuring the fixed voltage reference.
;Supply voltage = 5V: Measuring FVR = 2.048 results in 105.
;Supply voltage = 3.3V: Measuring FVR = 2.048 results in 159.
;8 * 105 / 128 = 7		8 * 159 / 128 = 10
;86 * 105 / 128 = 71		86 * 159 / 128 = 107
;156 * 105 / 128 = 128		156 * 159 / 128 = 194

		constant V_SHORT=6	;0.8V * 4.7 / 37.7 * 128 / 2.048
		constant V_LOW=86	;11V * 4.7 / 37.7 * 128 / 2.048
		constant V_OPEN=156	;20V * 4.7 / 37.7 * 128 / 2.048

;Variables accessible from all RAM banks
		UDATA_SHR
s_timer2	res	1	;Store timer 2 value, used in interrupt routine

temp		res	1
temp2		res	1

byte1		res	1
byte2		res	1
byte3		res	1			;Must be in shared RAM
byte4		res	1			;Must be in shared RAM
#define		MsgParity	byte1,7
#define		MsgResponse	byte1,6
#define		MsgUnknown	byte1,5
#define		MsgWriteOp	byte1,4

originaltype	res	1
originalreq	res	1
float1		res	1			;Must be in shared RAM
float2		res	1			;Must be in shared RAM
loopcounter	res	1

modeflags	res	0	;Alias for mode for debugging
mode		res	1
MONMODEBIT	equ	0	;Position of the bit returned by CheckBoolean
#define		MonitorMode	mode,MONMODEBIT
#define		ChangeMode	mode,1
#define		AlternativeUsed	mode,2		;Must be in shared RAM
#define		FailSafeMode	mode,3		;Must be outside wiped RAM

Bank0data	UDATA
;Variables for temporary storage
		res	1	;Free
tempvar0	res	1
tempvar1	res	1
tempvar2	res	1
databyte1	res	1
databyte2	res	1

;Variables for longer lasting storage
rxpointer	res	1
txpointer	res	1
bitcount	res	1	;Count of bits remaining in a message
celciasetpoint	res	1	;Setpoint times 5 as needed for a Celcia 20
setpoint1	res	1
setpoint2	res	1
outside1	res	1
outside2	res	1
clock1		res	1
clock2		res	1
#define		InvalidTime	clock2,7
dhwsetpoint1	res	1
dhwsetpoint2	res	1
maxchsetpoint1	res	1
maxchsetpoint2	res	1
controlsetpt1	res	1
controlsetpt2	res	1
#define		SysCtrlSetpoint	controlsetpt1,7
controlsetpt3	res	1	;Control Setpoint for CH2 high byte
controlsetpt4	res	1	;Control Setpoint for CH2 low byte
#define		SysCH2Setpoint	controlsetpt3,7
coolinglevel1	res	1
coolinglevel2	res	1
#define		SysCoolLevel	coolinglevel1,7
ventsetpoint	res	1
DebugPointerL	res	1
DebugPointerH	res	1
outputmask	res	1	;This var can quite easily be freed, if needed
messagetype	res	1
alternativecmd	res	1
valueindex	res	1
txavailable	res	1
resetflags	res	1
prioritymsgid	res	1
dhwsetpointmin	res	1
dhwsetpointmax	res	1
chsetpointmin	res	1
chsetpointmax	res	1
requestcode	res	1
operatingmode1	res	1
operatingmode2	res	1
#define		manualdhwpush	operatingmode1,4;Manual DHW push requested
minutetimer	res	1
#define		Expired		minutetimer,7
fakesetpoint1	res	1
fakesetpoint2	res	1
#define		NoFakeSetpoint	fakesetpoint1,7
SecCounter	res	1
MessagePtr	res	1
RemOverrideFunc	res	1
#define		ManChangePrio	RemOverrideFunc,0
#define		ProgChangePrio	RemOverrideFunc,1
#define		TempChangePrio	RemOverrideFunc,5

MaxModLevel	res	1
#define		SysMaxModLevel	MaxModLevel,7
GPIOFunction	res	1			;Funtions assigned to GPIO pins

flags		res	1			;General bit flags
#define		OutsideTemp	flags,0
#define		SensorInvalid	flags,1
#define		SummaryRequest	flags,2
#define		BoilerAlive	flags,3
#define		BoilerFault	flags,4
#define		Overrun		flags,5
#define		NegativeTemp	flags,6
#define		LeadingZero	flags,7

stateflags	res	1			;General bit flags
#define		MessageRcvd	stateflags,0
#define		Unsolicited	stateflags,1
#define		HideReports	stateflags,2
#define		CommandComplete	stateflags,3
#define		NoAlternative	stateflags,4
#define		AlternativeDone	stateflags,5
#define		OverrideUsed	stateflags,6
#define		BoilerResponse	stateflags,7

msgflags	res	1	;Used in interrupt routine
#define		ComparatorT	msgflags,0	;Must match CMOUT:MC1OUT
#define		ComparatorB	msgflags,1	;Must match CMOUT:MC2OUT
#define		MidBit		msgflags,2
#define		NextBit		msgflags,3
#define		Transmit	msgflags,4
#define		Intermission	msgflags,5
#define		Transition	msgflags,6
#define		Parity		msgflags,7	;Saves an instruction, if bit 0
#define		NextBitMask	1 << 3

quarter		res	1	;Count of 1/4 ms, used in interrupt routine
#define		LineStable	quarter,0
#define		RealBit		quarter,1
#define		BitClkCarry	quarter,3

tstatflags	res	1	;Used in- and outside interrupt routine
#define		ReceiveModeT	tstatflags,0	;Must match CMOUT:MC1OUT
#define		ReceiveModeB	tstatflags,1	;Must match CMOUT:MC2OUT
#define		Request		tstatflags,3	;Event on the thermostat line
#define		TransmitMode	tstatflags,4	;Must match MasterOut
#define		SmartPower	tstatflags,5	;Thermostat supports Smart Power
#define		PowerChange	tstatflags,6
#define		PowerReport	tstatflags,7

override	res	1
#define		OverrideAct	override,7	;Override activity has happened
#define		OverrideWait	override,6	;Override has not yet been sent
#define		OverrideFunc	override,5	;ID100 has not yet been sent
#define		OverrideClr	override,4	;Cancel override request
#define		OverrideReq	override,3	;Override temp was requested
#define		OverrideSet	override,2	;Override accepted by thermostat
;Bits 0&1 are used as a counter to allow 3 attempts for the override to stick

initflags	res	1
;----- initflags bits -------------------------------------------------
INITSW		equ	0	;WaterSetpoint	;Must match ID6:HB0
INITSH		equ	1	;MaxHeatSetpoint;Must match ID6:HB1
INITRR		equ	2	;RemoteReq
INITOM		equ	3	;OperModes
INITHW		equ	4	;InitHotWater	;Match swapped ID6:HB0?
INITCH		equ	5	;InitHeating	;Match swapped ID6:HB1?
INITRP		equ	6	;InitParameter
INITNR		equ	7	;NoResetUnknown

onoffflags	res	1			;General bit flags
#define		dhwupdate	onoffflags,0	;Must match ID6:LB0
#define		maxchupdate	onoffflags,1	;Must match ID6:LB1
#define		NoThermostat	onoffflags,2
#define		HeatDemand	onoffflags,3
#define		ExternalSensor	onoffflags,4	;External sensor configured
#define		SendUserMessage	onoffflags,5	;User message in standalone mode
#define		PriorityMsg	onoffflags,6
#define		MsgTrigger	onoffflags,7	;Time to send the next message
;When the thermostat is reconnected, the NoThermostat, HeatDemand, and
;SendUserMessage bits must be cleared
		constant	CONNECTMASK=b'11010011'

statusflags	res	1			;Master status flags
#define		CHModeOff	statusflags,0	;Must match ID0:HB0
#define		CoolModeOff	statusflags,2	;Must match ID0:HB2
#define		CH2ModeOff	statusflags,4	;Must match ID0:HB4
#define		DHWBlocking	statusflags,6	;Must match ID0:HB6

gpioflags	res	1			;General bit flags
#define		HighPower	gpioflags,0	;Must match ReceiveModeT
#define		VirtualLedE	gpioflags,1	;Must be bit 1
#define		VirtualLedF	gpioflags,2	;Must be bit 2
#define		PrioMsgWrite	gpioflags,3
#define		RaisedPower	gpioflags,4	;Must match TransmitMode
#define		gpioaway	gpioflags,5
#define		gpio_port1	gpioflags,6	;Must match PORTA,6
#define		gpio_port2	gpioflags,7	;Must match PORTA,7
#define		gpio_mask	gpioflags

;Flags to work around quirks of remeha thermostats
remehaflags	res	1
#define		TStatRemeha	remehaflags,7	;Auto-detected Remeha Thermostat
#define		TStatISense	remehaflags,6	;Thermostat is Remeha iSense
#define		TStatCelcia	remehaflags,5	;Thermostat is Remeha Celcia 20
#define		TStatManual	remehaflags,4	;Thermostat model set manually

dhwpushflags	res	1
;Bits 0..2 are used as a counter to allow 8 attempts for the override to stick
#define		dhwpushstarted	dhwpushflags,3
#define		dhwpushspoof	dhwpushflags,4
#define		dhwpushpending	dhwpushflags,5
#define		dhwpushinit	dhwpushflags,6
#define		opermodewrite	dhwpushflags,7

heartbeatflags	res	1	;Can be moved to another bank, if necessary
#define		HeartbeatSC	heartbeatflags,0
#define		HeartbeatCS	heartbeatflags,1
#define		HeartbeatC2	heartbeatflags,2

errornum	res	1
errortimer	res	1

repeatcount	res	1	;Count same message from the thermostat

TSPCount	res	1
TSPIndex	res	1
TSPValue	res	1

settings	res	1			;Bits 0-4: voltage reference
#define		IgnoreErr1	settings,5	;For bouncing high<->low changes
#define		OverrideHigh	settings,6	;Copy MsgID100 data to high byte

boilercom	res	1

conf		res	1			;Saved in EEPROM
#define		MonModeSwitch	conf,MONMODEBIT
#define		TempSensorFunc	conf,2
#define		SummerMode	conf,3
#define		HotWaterSwitch	conf,4
#define		HotWaterEnable	conf,5
#define		NoFailSafe	conf,6	;No fall back to fail safe mode

;The ds1820 package also uses 4 bytes in bank 0:
;sensorstage	res	1
;lsbstorage	res	1
;msbstorage	res	1
;crc		res	1

		global	float1, float2, flags, temp, StoreTempValue

Bank1data	UDATA
;The functions that can be assigned to a LED are referred to by an uppercase
;letter. One byte is reserved for each possible function. Bits 3, 4, 6, and 7
;indicate if the function is assigned to one or more of the four LED outputs.
;Bit 0 is used to keep track of the current state of the function. That makes
;it possible to immediately set a LED to the correct state upon assignment of
;the function.
functions	res	26

;The gateway keeps track of which messages are not supported by the boiler.
;The code counts the times the boiler responds with an Unknown-DataID in two
;bits per DataID. If the counter for a specific DataID reaches 3, the gateway
;stops sending the DataID to the boiler. The next time the thermostat requests
;that information again, the gateway will actually send a different request to
;the boiler. This allows gathering of information that the thermostat does not
;normally request.
unknownmap	res	32	;2 bits per DataID for 128 DataID's

LineVoltage	res	1	;The measured voltage on the thermostat input
VoltShort	res	1	;Threshold for shorted line
VoltLow		res	1	;Threshold for low logical level
VoltOpen	res	1	;Threshold for open line

;If the serial receive pin is disconnected and it picks up some 50Hz noise, the
;AUSART would detect a BREAK every 20ms. To prevent the gateway from repeatedly
;resetting in this situation, wait for 32 TMR0 overflows (slightly over 0.5s)
;without a BREAK before acting on a BREAK again.
BreakTimer	res	1
#define		NoBreak		BreakTimer,5	;Set after 32 TMR0 overflows
resetreason	res	1

; 4 bytes remaining in Bank 1

Bank2data	UDATA
;Keep track of the data values of up to 40 DataID's
valuestorage	res	80	;2 bytes per DataID for up to 40 DataID's
#define		outsideval1	valuestorage + 30
#define		outsideval2	valuestorage + 31
#define		returnwater1	valuestorage + 32
#define		returnwater2	valuestorage + 33

Bank3data	UDATA
ResponseValues3	res	16	;Low byte of fake responses
ResponseValues2	res	16	;High byte of fake responses
ResponseValues1	res	16	;DataID's of fake responses
rxbuffer	res	16	;Serial receive buffer

Bank4data	UDATA
;Use all available RAM in bank 4 for the transmit buffer
		constant TXBUFSIZE=80
txbuffer	res	TXBUFSIZE

;I/O map
#define		MasterIn	CMCON,C1OUT
#define		SlaveIn		CMCON,C2OUT
#define		SlaveOut	PORTA,3
#define		MasterOut	PORTA,4
#define		RXD		PORTB,2
#define		TXD		PORTB,5
#define		CCP1		PORTB,0
#define		LED_A		PORTB,3
#define		LED_B		PORTB,4
#define		LED_C		PORTB,6
#define		LED_D		PORTB,7
#define		RSET		PORTB,1		;Used by self-programming

#define		SlaveMask	b'00001000'
#define		MasterMask	b'00010000'

Bank8data	UDATA
MsgInterval	res	1
IntervalCounter	res	1

package		macro	pkg
pkg		code
pkg
		endm

pcall		macro	rtn
		lcall	rtn
		pagesel	$
		endm

;Get the output of the active comparator into the carry bit
getcompout	macro
		lsrf	msgflags,W	;Get the state of the thermostat line
		btfss	Request		;A request goes through comparator 1
		lsrf	WREG,W		;Carry if the boiler line is active
		endm

;The first thing to do upon a reset is to allow the firmware to be updated.
;So no matter how buggy freshly loaded firmware is, if the first two command
;have not been messed up, the device can always be recovered without the need
;for a PIC programmer. The self-programming code times out and returns after
;a second when no firmware update is performed.
;
		extern	SelfProg, Temperature

ResetVector	code	0x0000
		lcall	SelfProg	;Always allow a firmware update on boot
		lgoto	Start		;Start the opentherm gateway program

;Handle interrupts. This piece of code saves the important registers and finds
;out which interrupt fired. It then calls a subroutine to handle that specific
;interrupt. Upon return from the subroutine, it restores the saved registers.
;
InterruptVector	code	0x0004
		banksel	TMR2		;Bank 0
		movfw	TMR2
		movwf	s_timer2	;Save the current value of timer2
		pagesel	Interrupt
		;Process the timer 2 interrupt before the comparator
		;interrupt to get the correct overflow count
		btfsc	PIR1,TMR2IF	;Check if timer 2 matched
		call	timer2int
		btfsc	PIR2,C1IF	;Check if thermostat input changed
		call	input1int
		btfsc	PIR2,C2IF	;Check if boiler input changed
		call	input2int
		retfie			;End of interrupt routine

;########################################################################
;Interrupt routines
;########################################################################
;
;The meaning of events on the opentherm interface depends on the logical levels
;and timing. The state is tracked via a number of bits and variables.
;
;TMR2ON==0 && CXOUT==1: Line idle
;TMR2ON==0 && CXOUT:=0: Start T2
;bitcount==0 && quarter<=3 && CXOUT:=1: Start of opentherm message. Restart T2
;bitcount>0 && quarter<=3 && CXOUT:=0: Midbit transition, ignore.
;bitcount>0 && quarter<=3 && CXOUT:=1: Midbit transition, ignore.
;bitcount>0 && quarter>3 && quarter<=6 && CXOUT:=0: Bit: 0. Restart T2
;bitcount>0 && quarter>3 && quarter<=6 && CXOUT:=1: Bit: 1. Restart T2

		package	Interrupt

;Comparator 2 interrupt routine
input2int	comf	tstatflags,W	;Get the inverted Smart power bits
		banksel	CMOUT		;Bank 2
		xorwf	CMOUT,W		;Read to end the mismatch condition
		banksel	PIR2		;Bank 0
		bcf	PIR2,C2IF	;Clear the interrupt
		xorwf	msgflags,W	;Check if the comparator has changed
		andlw	b'00000010'	;Filter out the comparator bit
		skpnz			;There actually was a change
		return			;False trigger
		xorwf	msgflags,F	;Update the saved state
		btfss	Request		;Listening to the thermostat?
		bra	inputmark	;Handle the signal from the boiler
		movlw	-1		;Suppress errors for a while
		movwf	errornum
		return

;Comparator 1 interrupt routine
input1int	comf	tstatflags,W	;Get the inverted Smart power bits
		banksel	CMOUT		;Bank 2
		xorwf	CMOUT,W		;Read to end the mismatch condition
		banksel	PIR2		;Bank 0
		bcf	PIR2,C1IF	;Clear the interrupt
		xorwf	msgflags,W	;Check if the comparator has changed
		andlw	b'00000001'	;Filter out the comparator bit
		skpnz			;There actually was a change
		return			;False trigger
		xorwf	msgflags,F	;Update the saved state
		btfsc	NoThermostat	;Opentherm thermostat attached?
		return
		btfsc	Request		;A new event from the thermostat?
		bra	inputmark	;Already listening to the thermostat
		bsf	Request		;Receiving from the thermostat
		tstf	bitcount	;Message in progress?
		skpnz
		btfsc	Transmit	;Transmission in progress?
		goto	inputabort	;Abort message
inputmark	tstf	bitcount	;Is a message in progress?
		skpnz
		goto	InputEvent	;Line state change outside message
		movlw	3		;3 overflows of timer 2 = 750 us
		subwf	quarter,W	;Check the time since the last bit
		skpc			;More than 750 us
		goto	midbitchange
		getcompout		;Get the comparator out
		decfsz	bitcount,F	;Is this the stop bit?
		goto	storebit	;If not, store the bit in the buffer
		skpnc			;Carry is set when the line is active
		goto	error02		;A stop bit must be logical 1
		btfsc	Parity		;Check for even parity
		goto	error04		;Parity error
		bsf	MessageRcvd	;Indicate that a message was received
		pagesel	Message
		movlw	'R'		;Receive function
		call	SwitchOffLED	;Switch off the receive LED
		movlw	'T'		;Thermostat function
		call	SwitchOffLED	;Switch off the thermostat LED
;		movlw	'B'		;Boiler function
		call	SwitchOffLED	;Switch off the boiler LED
		pagesel	Interrupt
		bcf	Request		;Thermostat is done
inputspike	bcf	T2CON,TMR2ON	;Stop timer 2
		goto	activity	;End of the interrupt routine

;The thermostat may try to change power levels while the gateway is in the
;process of transmitting a message, or receiving a message from the boiler.
;The same timer is used for all opentherm communication, so the gateway can't
;handle the two things at the same time. Ignoring the thermostat may result in
;incorrect power levels leading to user-visible artefacts (like a flashing
;backlight of the thermostat screen). So, for a smoother user experience, we
;abort the current operation when the thermostat asks for attention.
inputabort	clrf	bitcount	;Abort message
		bcf	Transmit	;Clear flag
		movlw	b'00011000'	;Default state of the outputs
		btfsc	ReceiveModeB
		movlw	b'00001000'	;Idle state of port 4 is high
		xorwf	PORTA,W		;Check against the current state
		andlw	b'00011000'	;Only consider port 3 and 4
		xorwf	PORTA,F		;Update the ports
		pagesel	Message
		movlw	'X'
		call	SwitchOffLED	;Switch off transmit LED
;		movlw	'B'
		call	SwitchOffLED	;Switch off boiler LED
		pagesel	Interrupt
		goto	restarttimer	;Reset T2 and 1/4 ms counter

storebit	;Store the current bit in the message buffer
		movlw	1 << 7		;Mask for the bit flag tracking parity
		skpnc
		xorwf	msgflags,F	;Update the parity
		rlf	byte4,F		;Shift the 32 bits receive buffer
		rlf	byte3,F		;Shift bits 8 through 15
		rlf	byte2,F		;Shift bits 16 through 23
		rlf	byte1,F		;Shift bits 24 through 31
		movlw	1 << 0
		xorwf	byte4,F		;Need to invert the new bit
		bcf	MidBit		;Mid-bit transitions are allowed again
restarttimer	clrf	quarter		;Restart 1/4 ms counter
		movfw	s_timer2	;Timer 2 value just after the interrupt
		subwf	TMR2,F		;Adjust the timer value
		movlw	6		;Difference between 255 and PR2
		skpc
		subwf	TMR2,F		;Compensate for early reset of T2
		bcf	PIR1,TMR2IF	;Interrupt routine is less than 250us.
activity	movlw	PRELOADT0	;Idle time preload value
		movwf	TMR0		;Restart idle timer
		return			;End of the interrupt routine

InputEvent	;The line state changed while not receiving a message. Need to
		;figure out what is going on.
		btfsc	T2CON,TMR2ON	;Timer2 is stopped when the line is idle
		goto	InputProcess
		;If an unreported error has happened, we're still waiting for
		;the line to become idle again.
		tstf	errornum	;Is there an unreported error?
		skpz
		goto	activity	;Restart the non-idle timer
		;Change of a previously idle line
		movlw	39		;Time since interrupt (plus 4us latency)
		movwf	TMR2		;Initialize timer 2
		bsf	T2CON,TMR2ON	;Start timer2
		clrf	quarter		;Reset the 1/4 ms counter
		banksel	PIE1		;Bank 1
		bsf	PIE1,TMR2IE	;Enable timer2 interrupts
		banksel	PIR1		;Bank 0
		goto	activity	;The change has no meaning

InputProcess	;A second line change happened shortly after the previous one.
		movlw	10
		subwf	quarter,W
		skpnc			;Less than 2.5 ms since the last change
		goto	InputSlow
InputFast	getcompout		;Get the comparator output
		skpnc			;Carry is set when the line is active
		goto	activity	;Not a start bit
StartBit	tstf	quarter		;Check if pulse was less than 250us
		skpnz
		goto	inputspike	;Ignore spikes
		pagesel	Message
		movlw	'R'		;Receive function
		call	SwitchOnLED	;Turn on the receive LED
;		movlw	'T'		;Thermostat function
		btfss	Request
		movlw	'B'		;Boiler function
		call	SwitchOnLED	;Switch on the boiler or thermostat LED
		pagesel	Interrupt
		movlw	33		;Message length: 32 bits + stop bit - 1
		movwf	bitcount	;Initialize the counter
		bcf	Parity		;Initialize the parity check flag
		bcf	MidBit		;No mid-bit transitions yet
		goto	restarttimer	;Significant transition of the start bit

InputSlow	movlw	50
		subwf	quarter,W
		skpnc			;Less than 12.5 ms since the last change
		goto	restarttimer
		btfsc	Request		;Event on the boiler interface?
		btfss	TransmitMode	;In High Power mode?
		goto	restarttimer
		btfsc	ReceiveModeT	;Normal receive mode?
		btfss	ComparatorT	;Thermostat line active?
		goto	restarttimer
;The thermostat input changed from high back to low after 10ms -> Medium Power
		movlw	1 << 0		;Mask for the ReceiveModeT bit
		xorwf	tstatflags,F	;Toggle the appropriate invert bit
		xorwf	msgflags,F	;Invert the saved state
		goto	restarttimer

;Timer 2 interrupt routine

timer2int	bcf	PIR1,TMR2IF	;Clear the interrupt flag
		incf	quarter,F	;Update the 1/4 ms counter
		btfsc	Transmit	;Transmitting a message?
		goto	timer2xmit
		tstf	bitcount	;Message in progress?
		skpz
		goto	timer2idle

		movfw	quarter
		sublw	20		;New state persisted for 5ms?
		skpnz
		goto	timer2invert
		movlw	80
		subwf	quarter,W	;20ms since last line event?
		skpc
		return
		bcf	T2CON,TMR2ON	;No line event for 20ms
		bcf	Request		;Thermostat is done
		btfsc	PowerChange	;Was there any Smart Power change?
		bsf	PowerReport	;Report the new Smart Power level
		bcf	PowerChange	;Clear for the next round
		return			;idle after transition to low power

timer2idle	btfsc	BitClkCarry	;Bit transition happened on time?
		goto	error03		;Report missing transition
		return

timer2xmit	btfsc	LineStable	;Nothing to do in a stable interval
		return
		clrw			;Initialize work register
		btfss	NextBit		;Find out the desired output level
		movlw	b'00011000'	;Set bits matching the opentherm outputs
		btfsc	TransmitMode	;Using raised power to the thermostat?
		xorlw	b'00010000'	;Invert the bit for the thermostat line
		xorwf	PORTA,W		;Compare with the current port levels
		andwf	outputmask,W	;Only apply for the relevant output
		xorwf	PORTA,F		;Update the output port
		btfsc	RealBit		;Check if this was the start of a bit
		goto	InvertBit	;Next time send the inverted value
		bcf	NextBit		;Start with assumption next bit is 0
		setc			;Prepare to shift in a stop bit
		rlf	byte4,F		;Shift the full 32 bit message buffer
		rlf	byte3,F		;Shift bits 8 through 15
		rlf	byte2,F		;Shift bits 16 through 23
		rlf	byte1,F		;Shift bits 24 through 31
		skpnc			;Test the bit shifted out of the buffer
		bsf	NextBit		;Next time, send a logical 1
		clrf	quarter
		decfsz	bitcount,F	;Test for the end of the message
		return			;Return from the interrupt routine
		bcf	T2CON,TMR2ON	;Stop timer 2
		bcf	Transmit	;Finished transmitting
		btfsc	outputmask,4	;Transmission to the thermostat?
		bsf	Intermission	;Message exchange is finished
		pagesel	Message
		movlw	'X'		;Transmit function
		call	SwitchOffLED	;Switch off the transmit LED
		movlw	'T'		;Thermostat function
		call	SwitchOffLED	;Switch off the thermostat LED
;		movlw	'B'		;Boiler function
		call	SwitchOffLED	;Switch off the boiler LED
		pagesel	Interrupt
		return			;Return from the interrupt routine

InvertBit	movlw	NextBitMask	;Load a mask for the NextBit flag
		xorwf	msgflags,F	;Invert the bit
		return			;Return from the interrupt routine

timer2invert	getcompout		;Line high (according to current mode)?
		skpc			;Carry represents the logical line state
		return			;Not interresting
;Always invert the receive mode when the other side does. Even if communication
;has not been established in low power mode, or Smart Power support has not yet
;been negotiated. This wil ensure that messages can still be understood.
		movlw	1 << 0		;Mask for the ReceiveModeT bit
		btfss	Request		;Communicating with the thermostat?
		movlw	1 << 1		;Mask for the ReceiveModeB bit
		xorwf	tstatflags,F	;Toggle the appropriate invert bit
		xorwf	msgflags,F	;Invert the saved state
		btfss	Request		;Slave-side Smart Power is not supported
		goto	smartpowerslave	;Handle smart power ack from boiler
		bsf	PowerChange	;Smart Power levels have changed
		btfsc	MonitorMode	;Currently in gateway mode
		goto	smartpowermon	;Smart power request in monitor mode
		btfss	SmartPower	;Smart Power support was negotiated?
		return			;Ignore the request
		comf	tstatflags,W	;Get the inverted flags
		andlw	b'00010001'	;Filter out the Receive/Transmit modes
		skpnz			;Not in High Power mode
		return			;No Ack when going from Medium to High
		movlw	MasterMask
		xorwf	PORTA,F		;Acknowledge the Smart Power request
		xorwf	tstatflags,F	;Invert the master transmit mode
smartpowerled	andwf	tstatflags,W	;Get the TransmitMode bit
		movlw	'P'
		pcall	SwitchLED	;Update the Raised Power LED
		return

smartpowermon	bcf	Request		;Don't ignore reaction from the boiler
		btfss	ComparatorB	;Boiler already raised the line?
		return			;Let the timer run to report power level
		goto	restarttimer	;Time the boiler response

smartpowerslave	btfss	MonitorMode	;In monitor mode?
		return			;Nothing to be done in gateway mode
		btfsc	ReceiveModeB	;Slave in normal receive mode? (W=0x80)
		movlw	MasterMask	;Mask for TransmitMode bit
		xorwf	tstatflags,W	;Compare against current value
		andlw	MasterMask	;Only consider the TransmitMode bit
		skpnz			;TransmitMode not equal to ReceiveModeB?
		return			;No change
		xorwf	tstatflags,F	;Copy ReceiveModeB to TransmitMode
		bsf	PowerChange	;Smart Power levels have changed
		goto	smartpowerled	;Update the P LED with the new status

midbitfirst	bsf	MidBit		;Remember a mid-bit transition happened
		goto	activity
midbitchange	btfsc	MidBit		;First mid-bit transition?
		btfsc	IgnoreErr1	;Or multiple transitions are ignored?
		goto	midbitfirst	;Register mid-bit transition

;Error 01: More than one mid-bit transition
error01		movlw	1
		goto	errorcleanup

;Error 02: Wrong stop bit
error02		movlw	2
		goto	errorcleanup

;Error 03: Bit not received in time
error03		movlw	3
		goto	errorcleanup

;Error 04: Parity error
error04		movlw	4

errorcleanup	tstf	errornum
		skpnz
		movwf	errornum
		getcompout		;Check the active interface
		skpc			;Line is not idle, restart timer 2
		bcf	T2CON,TMR2ON	;Stop timer 2
		clrf	bitcount
		clrf	quarter
		bcf	Transmit
		pagesel	Message
		movlw	'X'
		call	SwitchOffLED	;Switch off the transmit LED
		movlw	'R'
		call	SwitchOffLED	;Switch off the receive LED
;		movlw	'B'
		call	SwitchOffLED	;Switch off the boiler LED
		movlw	'T'
		call	SwitchOffLED	;Switch off the thermostat LED
		pagesel	$
		comf	errornum,W
		skpnz
		goto	errorskip	;Not a real error
		movlw	'E'
		pcall	SwitchOnLED	;Switch on the error LED
		movlw	TWOSEC		;Leave the error LED on for 2 seconds
		movwf	errortimer
errorskip	goto	activity	;Wait for the line to go idle

;########################################################################
; Main program
;########################################################################

		package	Main
Break
		banksel	BreakTimer	;Bank 1
		btfsc	NoBreak
		goto	Restart2
		clrf	BreakTimer	;Reset break-free timer
		movlb	0		;Bank 0
		goto	MainLoop	;continue running the mainloop
Restart1
		banksel	resetreason	;Bank 1
		bsf	resetreason,2	;Reset because we're stuck in a loop
Restart2	bsf	resetreason,1	;Reset due to BREAK on serial line
		pcall	SelfProg	;Jump to the self-programming code
Start
		banksel	LATB		;Bank 2
		clrf	LATB		;Flash the LED's on startup
		banksel	OSCCON		;Bank 1
		movlw	b'01101010'	;Internal oscillator set to 4MHz
		movwf	OSCCON		;Configure the oscillator

		;Configure I/O pins
		;Power on defaults all pins to inputs
		;Port A
		;Pins 0 and 1 are used as comparator inputs
		;Pin 2 is unused
		;Pins 3 and 4 are (comparator) ouputs
		;Pin 5 is master reset input
		;Pins 6 and 7 are GPIO
		banksel	ANSELA		;Bank 3
		movlw	b'00000111'	;A0 through A2 are used for analog I/O
		movwf	ANSELA

		;Port B
		;Pins 2 and 5 are used by the USART.
		;Pins 3, 4, 6, and 7 are used to indicate events for debugging
		clrf	ANSELB		;No analog I/O on port B

		banksel	APFCON0		;Bank 2
		bsf	APFCON1,TXCKSEL	;TX on RB5
		bsf	APFCON0,RXDTSEL	;RX on RB2

		banksel	TRISA		;Bank 1
		movlw	b'11100111'
		movwf	TRISA
		movlw	b'00000111'
		movwf	TRISB
		;Check for reset reason
		btfss	PCON,NOT_POR
		bsf	PCON,NOT_BOR
		comf	PCON,W
		btfsc	PCON,STKOVF
		movlw	5
		btfsc	PCON,STKUNF
		movlw	6
		andlw	b'00001111'
		skpnz
		incf	resetreason,W
		btfss	STATUS,NOT_TO
		clrw
		swapf	WREG,W
		movwf	mode
		bsf	FailSafeMode	;Assume fail safe mode
		btfss	STATUS,NOT_TO
		btfss	NoFailSafe	;Fail safe mode is disabled by user
		bcf	FailSafeMode	;Clear fail safe mode
		movlw	b'1111'
		movwf	PCON

		;Configure Timer 0 & Watchdog Timer
		;Bit 7 RBPU = 1 - Weak pull up disabled
		;Bit 6 INTEDG = 1 - Interrupt on rising edge
		;Bit 5 T0CS = 0 - Internal clock
		;Bit 4 T0SE = 1 - High to low transition
		;Bit 3 PSA = 0 - Prescaler assigned to Timer 0
		;Bit 2-0 PS = 5 - 1:64 Prescaler
		movlw	b'11010101'
		movwf	OPTION_REG

		;Configure Timer 2
		;Generate an interrupt every 250uS (one fourth of a bit)
		;Bit 6-3 TOUTPS = 0 - 1:1 Postscaler
		;Bit 2 TMR2ON = 0 - Timer disabled
		;Bit 1-0 T2CKPS = 0 - 1:1 Prescaler
		banksel	T2CON		;Bank 0
		movlw	PERIOD		;Fosc / 16000 - 1
		movwf	PR2
#ifndef T2PRESCALER
		movlw	b'00000000'
#else
		movlw	b'00000001'
#endif
		movwf	T2CON

		clrf	TMR1L
		clrf	TMR1H
		;Configure Timer 1
		;Used to blink LEDs approximately once per second
		;Bit 7-6 TMR1CS = 0 - Internal clock (Fosc/4)
		;Bit 5-4 T1CKPS = 3 - 1:8 Prescale value
		;Bit 3 T1OSCEN = 0 - Oscillator is shut off
		;Bit 2 T1SYNC = 0 - Bit is ignored because TMR1CS=0
		;Bit 0 TMR1ON = 1 - Enable Timer1
		movlw	b'110001'
		movwf	T1CON

		;Delay a few milliseconds to show the lit LEDs
		clrf	TMR0
		bcf	INTCON,TMR0IF
WaitTimer0	clrwdt
		btfss	INTCON,TMR0IF
		goto	WaitTimer0

		;Configure the serial interface
		banksel	SPBRGL		;Bank 3
		clrf	RCSTA		;Reset the serial port control register
		movlw	BAUD
		movwf	SPBRGL
		bsf	TXSTA,BRGH	;9600 baud
		bcf	TXSTA,SYNC	;Asynchronous
		bsf	TXSTA,TXEN	;Enable transmit
		bsf	RCSTA,SPEN	;Enable serial port

		;Configure fixed voltage reference
		;Bit 7 FVREN = 1 - FVR enabled
		;Bit 5 TSEN = 0 - Temperature indicator disabled
		;Bit 4 TSRNG = 0 - Temperature indicator low range
		;Bit 3-2 CDAFVR = 2 - DAC fixed voltage is 2.048
		;Bit 1-0 ADFVR = 2 - ADC fixed voltage is 2.048
		banksel	FVRCON		;Bank 2
		movlw	b'10001010'
		movwf	FVRCON

		;Configure the digital to analog converter
		;Bit 7 DACEN = 1 - DAC enabled
		;Bit 6 DACLPS = 1 - Positive reference source selected
		;Bit 5 DACOE = 0 - Output pin disconnected
		;Bit 3-2 DACPSS = 2 - Positive source is FVR
		;Bit 0 DACNSS = 0 - Negative source is Vss
		movlw	b'11001000'
		movwf	DACCON0

		;Wait for the Fixed Voltage Reference to be ready
WaitVoltRef	btfss	FVRCON,FVRRDY
		bra	WaitVoltRef

		;Configure A/D converter
		banksel	ADCON0		;Bank 1
		movlw	b'00010000'	;Left justified, Fosc/8, Vss, Vdd
		movwf	ADCON1
		movlw	b'01111101'	;A/D on, channel 31 - FVR buffer 1
		movwf	ADCON0

		;Setup interrupt sources (interrupts are not enabled yet)
		bsf	PIE2,C1IE	;Enable comparator 1 interrupt
		bsf	PIE2,C2IE	;Enable comparator 2 interrupt
		bsf	PIE1,TMR2IE	;Enable timer 2 interrupts

		nop			;Delay for the acquisition time
		nop
		bsf	ADCON0,GO	;Measure FVR buffer 1

		btfss	FailSafeMode
		goto	ClearRam
		movlw	TimeoutStr
		pcall	PrintStringNL	;Report the WDT reset
		btfsc	NoThermostat
		bcf	FailSafeMode	;Disable fail safe mode if no thermostat

		;Clear RAM in bank 0
ClearRam	movlw	0
		call	ClearRamBank
		;Clear RAM in bank 1
		movlw	1
		call	ClearRamBank
		;Clear RAM in bank 2
		movlw	2
		call	ClearRamBank
		;Clear RAM in bank 3
		movlw	3
		call	ClearRamBank

WaitConvert	btfsc	ADCON0,GO	;Check that A/D conversion is finished
		bra	WaitConvert	;Wait for A/D conversion to complete
		movlw	V_SHORT		;Short circuit threshold
		call	CalcThreshold	;Adjust for measured supply voltage
		movwf	VoltShort
		movlw	V_LOW		;Low level threshold
		call	CalcThreshold	;Adjust for measured supply voltage
		movwf	VoltLow
		movlw	V_OPEN		;Open line threshold
		call	CalcThreshold	;Adjust for measured supply voltage
		movwf	VoltOpen

		movlw	b'00000001'	;A/D on, channel 0 - pin AN0
		movwf	ADCON0

		movlw	SavedSettings
		call	ReadEpromData	;Get the setting from EEPROM
		movwf	settings	;Store a copy in RAM
		banksel	DACCON1		;Bank 1
		movwf	DACCON1		;Set the reference voltage

		movlw	Configuration
		call	ReadEpromData
		movwf	conf

		;Configure comparator module
		pcall	SetupCompModule

		call	ReadEpromNext
		movwf	dhwsetpoint1
		skpz
		bsf	initflags,INITSW
		call	ReadEpromNext
		movwf	dhwsetpoint2
		call	ReadEpromNext
		movwf	maxchsetpoint1
		skpz
		bsf	initflags,INITSH
		call	ReadEpromNext
		movwf	maxchsetpoint2

		banksel	RCSTA		;Bank 3
		btfss	FailSafeMode
		bsf	RCSTA,CREN	;Enable serial receive

		movlw	MessageInterval
		call	ReadEpromData

		;Configure timer 6, interrupt every 5 ms when running
		banksel	T6CON		;Bank 8
		movwf	MsgInterval
		movlw	249		;Reset every 250 counts
		movwf	PR6
		movlw	b'0100001'	;1:4 prescaler, 1:5 postscaler, off
		movwf	T6CON

		;Configure timer 4, interrupt every 20 ms
		movlw	249		;Reset every 250 counts
		movwf	PR4
		movlw	b'0100110'	;1:16 prescaler, 1:5 postscaler
		movwf	T4CON

		banksel	CMOUT		;Bank 2
		comf	CMOUT,W
		clrf	STATUS
		banksel	PIR2		;Bank 0
		clrf	PIR2		;Clear any comparator interrupt
		andlw	b'00000011'
		movwf	msgflags
		movlw	b'11111111'
		movwf	PORTB
		btfsc	ComparatorT
		andlw	b'11110111'	;Clear RA3
		btfsc	ComparatorB
		andlw	b'11101111'	;Clear RA4
		movwf	PORTA
		bsf	INTCON,PEIE
		btfss	FailSafeMode
		bsf	INTCON,GIE
		comf	alternativecmd,F;Compensate for initial increment
		;Some boilers support MsgID 48 and/or MsgID 49, while they
		;don't support MsgID 6 (For example: Bosch HRC 30)
		movlw	(1 << INITRP) | (1 << INITHW) | (1 << INITCH) | (1 << INITOM)
		iorwf	initflags,F	;Nothing has been initialized yet
		movlw	MIN_DHWSETPOINT
		movwf	dhwsetpointmin
		movlw	MAX_DHWSETPOINT
		movwf	dhwsetpointmax
		movlw	MIN_CHSETPOINT
		movwf	chsetpointmin
		movlw	MAX_CHSETPOINT
		movwf	chsetpointmax

		bsf	SysCtrlSetpoint	;No user defined control setpoint
		bsf	SysMaxModLevel	;No user defined max modulation
		bsf	SysCoolLevel	;No user defined cooling level
		bsf	SysCH2Setpoint	;No user defined CH2 control setpoint
		bsf	NoFakeSetpoint	;Don't send fake setpoint to boiler
		bsf	InvalidTime	;No user specified time information

		movlw	ONESEC
		movwf	SecCounter	;Initialize second counter

		movlw	TStatModel
		call	ReadEpromData
		movwf	remehaflags

		;Reinstall manually configured unsupported MsgID's
		movlw	16		;Size of flag data: 16 * 8 = 128 msgs
		movwf	loopcounter	;Initialize loop counter
		clrf	float1		;Message number
		movlw	UnknownFlags	;Start of bitmap in data EEPROM
		movwf	tempvar0	;Pointer to byte of bitmap
UnknownLoop1	movfw	tempvar0	;Get the bitmap pointer
		call	ReadEpromData	;Read the flags for 8 MsgID's
		movwf	float2		;Temporary storage of bit flags
		movfw	float1		;Number of the first of 8 MsgID's
UnknownLoop2	tstf	float2		;Check if any remaining flag is set
		skpnz
		goto	UnknownJump2	;Skip over the rest
		movwf	float1		;Update the message number
		lsrf	float2,F	;Shift a flag into the carry bit
		skpc
		goto	UnknownJump1	;Current message is not flagged
		pcall	UnknownMask2	;Get mask and pointer into unknownmap
		iorwf	INDF0,F		;Mark message as unsupported
UnknownJump1	incf	float1,W	;Next message
		goto	UnknownLoop2	;Repeat the loop
UnknownJump2	movlw	b'111'
		iorwf	float1,F	;The last of the current 8 MsgID's
		incf	float1,F	;The first of the next 8 MsgID's
		incf	tempvar0,F	;Also advance the bitmap pointer
		decfsz	loopcounter,F
		goto	UnknownLoop1	;Repeat the loop

		movlw	GreetingStr
		pcall	PrintStringNL

		;Initialize the LED functions
		clrf	temp
		movlw	FunctionLED1
		call	ReadEpromData
		pcall	SetLEDFunction
		incf	temp,F
		movlw	FunctionLED2
		call	ReadEpromData
		pcall	SetLEDFunction
		incf	temp,F
		movlw	FunctionLED3
		call	ReadEpromData
		pcall	SetLEDFunction
		incf	temp,F
		movlw	FunctionLED4
		call	ReadEpromData
		pcall	SetLEDFunction
		incf	temp,F
		movlw	FunctionLED5
		call	ReadEpromData
		pcall	SetLEDFunction
		incf	temp,F
		movlw	FunctionLED6
		call	ReadEpromData
		pcall	SetLEDFunction

		;Initialize the GPIO ports
		movlw	FunctionGPIO
		call	ReadEpromData
		movwf	GPIOFunction
		pcall	gpio_init

		btfss	FailSafeMode
		goto	MainLoop
		movlw	'M'
		pcall	SwitchOnLED

;************************************************************************

MainLoop	clrwdt
		tstf	txavailable	;Check if there is data to transmit
		skpz
		call	ProcessOutput

		btfsc	FailSafeMode
		goto	MainLoop

		;Check if an OpenTherm message was received
		btfsc	MessageRcvd
		call	OpenThermMsg

		btfsc	PowerReport
		call	ReportPower	;Report a power level change

		btfsc	INTCON,TMR0IF
		call	IdleTimer

		btfsc	PIR1,TMR1IF
		call	FlashTimer

		btfsc	PIR3,TMR4IF
		call	TickTimer

		btfsc	PIR3,TMR6IF
		call	MessageTimer

		btfsc	PIR1,ADIF
		call	CheckThermostat

		pcall	gpio_common

		btfsc	SummaryRequest	;Summary report requested/in progress?
		call	SummaryReport

		btfsc	repeatcount,6	;Reset after same message 64 times
		goto	Restart1

		btfss	PIR1,RCIF	;Activity on the serial receiver?
		goto	MainLoop

		;FERR is only relevant if RCIF is set
		banksel	RCSTA		;Bank 3
		btfss	RCSTA,FERR	;Check for framing error (break)
		bra	SerialEvent
		tstf	RCREG		;Check received char is 0
		skpnz
		goto	Break		;Real break
		movlb	0		;Bank 0
		goto	MainLoop	;Framing error (baud rate mismatch)

SerialEvent	btfss	RCSTA,OERR	;Check for overrun error
		goto	SerialEventJmp1
		bcf	RCSTA,CREN	;Procedure to clear an overrun error
		bsf	RCSTA,CREN	;Re-enable the serial interface
		movlb	0		;Bank 0
		bsf	Overrun		;Remember for later reporting
		goto	MainLoop
SerialEventJmp1	call	SerialReceive
		btfss	CommandComplete
		goto	MainLoop
		lcall	SerialCommand
		clrf	rxpointer	;Prepare for a new command
		bcf	CommandComplete
		pagesel	Print
		iorlw	0		;Check exit code
		skpz			;No need to print a string from EEPROM
		call	PrintString	;Print the result
		call	NewLine
		pagesel	$
		call	PrtDebugPointer
		goto	MainLoop

;Clear a RAM bank up to the shared data area
ClearRamBank	lsrf	WREG,W
		movwf	FSR0H
		clrw
		rrf	WREG,W
		iorlw	0x20
		movwf	FSR0L
ClearBankLoop	clrf	INDF0
		incf	FSR0L,F
		comf	FSR0L,W
		andlw	0x70
		skpz
		goto	ClearBankLoop
		clrf	FSR0H
		return

;Callers depend on this function never setting the carry bit
SerialReceive
		banksel	rxpointer	;Bank 0
		movlw	high rxbuffer
		movwf	FSR1H
		movfw	rxpointer
		addlw	rxbuffer
		movwf	FSR1L
		banksel	RCREG		;Bank 3
		movfw	RCREG
		banksel	stateflags	;Bank 0
		btfsc	CommandComplete
		return			;Not ready to accept a command now
		movwf	INDF1
		xorlw	0x0a		;Ignore linefeed characters
		skpnz
		return
		xorlw	0x0a ^ 0x0d	;Check for end of command
		skpnz
		goto	SerialCmdEnd
		movfw	rxpointer
		addlw	1
		skpdc			;All commands are less than 15 chars
		movwf	rxpointer
		return			;User is still typing the command
SerialCmdEnd	tstf	rxpointer	;Check command length
		skpnz
		return			;Ignore empty commands
		clrf	INDF1		;Indicate end of command
		bsf	CommandComplete
		return

OpenThermMsg	pcall	PrintRcvdMsg	;Report the received message
		bcf	MessageRcvd	;Message has been reported
		pcall	HandleMessage	;Apply special treatment
		btfsc	MonitorMode	;In monitor mode there is no need ...
		goto	MonModeCommOK	;... to explicitly transmit the message
		btfss	NoThermostat
		goto	SendMessage
		bsf	Intermission	;Message exchange is finished
		movfw	byte2
		xorwf	originalreq,W
		skpz
		goto	PrtDebugPointer	;Didn't get the expected answer
		incf	MessagePtr,F
		bcf	MessagePtr,4
MonModeCommOK	clrf	repeatcount	;Communication is working
		goto	PrtDebugPointer	;No need to send the message
SendMessage	movlw	1
		movwf	quarter		;Initialize the state counter
		bsf	NextBit		;Start bit is 1
		bsf	Transmit	;Starting to transmit a message
		movlw	'X'		;Transmit function
		pagesel	Message
		call	SwitchOnLED	;Switch on the transmit LED
;		movlw	'T'		;Thermostat function
		btfss	MsgResponse
SendBoiler	movlw	'B'		;Boiler function
		; In package Message
		call	SwitchOnLED	;Switch on the boiler or thermostat LED
		movlw	SlaveMask	;Transmitting to the boiler
		btfsc	MsgResponse	;Check message direction
		movlw	MasterMask	;Transmitting to the thermostat
		movwf	outputmask
		movlw	34		;A message is 32 bits + start & stop bit
		movwf	bitcount	;Load the counter
		banksel	PIE1		;Bank 1
		bsf	PIE1,TMR2IE	;Enable timer 2 interrupts
		banksel	TMR2		;Bank 1
		movlw	PERIOD
		movwf	TMR2		;Prepare timer 2 to overflow asap
		bsf	T2CON,TMR2ON	;Start timer 2
PrtDebugPointer	pcall	PrintDebug	;Report the debug value, if necessary
		return

ReportPower	bcf	PowerReport	;Print the report only once
		btfsc	TransmitMode	;Normal power?
		btfss	ReceiveModeT	;High power?
		goto	ReportPowerJ1	;Report normally
		banksel	LineVoltage	;Bank 1
		movfw	LineVoltage	;Check the line voltage
		subwf	VoltOpen,W	;Thermostat disconnected?
		movlb	0		;Bank 0
		skpc
		return			;Don't report the power change
ReportPowerJ1	movfw	tstatflags	;Get the current smart power level
		xorwf	gpioflags,W	;Compare against the last report
		andlw	b'00010001'	;Only check the smart power bits
		skpnz			;Was there any change?
		return
		xorwf	gpioflags,F	;Remember what will be reported
		lcall	PrintPowerlevel
		call	PrintStringNL	;Print the new power setting
		pagesel	$
		return

;TickTimer is called whenever timer 4 overflows (every 20 ms)
TickTimer	bcf	PIR3,TMR4IF	;Clear Timer 4 overflow flag
		decfsz	SecCounter,F	;Decrease second counter
		return
		btfsc	BoilerAlive	;Ever received a message from boiler?
		incf	repeatcount,F	;Keep track of repeated messages
		incfsz	boilercom,W	;Increment variable without rolling over
		movwf	boilercom
		movlw	5
		subwf	boilercom,W	;Check for 5 seconds without a message
		skpnc
		bcf	SlaveOut	;Prevent unwanted heating
TriggerMessage	bsf	MsgTrigger	;Trigger new message in stand-alone mode
		movlw	ONESEC
		movwf	SecCounter	;Reload second counter
		return

;MessageTimer is called whenever timer 6 overflows (every 5 ms)
MessageTimer	bcf	PIR3,TMR6IF	;Clear Timer 6 overflow flag
		banksel	T6CON		;Bank 8
		decf	IntervalCounter,F
		movlb	0		;Bank 0
		skpnz
		bra	TriggerMessage
		return

; IdleTimer is called whenever timer 0 overflows
IdleTimer	bcf	INTCON,TMR0IF	;Clear Timer 0 overflow flag
		banksel	BreakTimer	;Bank 1
		btfss	NoBreak
		incf	BreakTimer,F
		banksel	T2CON		;Bank 0
		btfss	T2CON,TMR2ON
		call	StartConversion	;Start A/D conversion
		pagesel	Serial
		btfsc	ChangeMode	;Is a request to change mode pending?
		call	SetMonitorMode
		pagesel	Print
		comf	errornum,W	;Suppressing errors?
		skpnz
		clrf	errornum	;Re-enable errordetection
		tstf	errornum	;Check if any errors were detected
		skpz
		call	PrintError	;Report the error on the serial intf
		pagesel	Main
		tstf	errortimer
		skpz
		decfsz	errortimer,F
		return
		movlw	'E'
		pcall	SwitchOffLED	;Switch off the error LED
		return

StartConversion
		banksel	ADCON0		;Bank 1
		bsf	ADCON0,GO	;Start A/D conversion
		movlb	0		;Bank 0
		return

FlashTimer	bcf	PIR1,TMR1IF	;Clear timer overrun flag
		btfss	Expired
		call	CommandExpiry
		btfsc	OverrideReq
		btfsc	OverrideSet
		return			;No override request pending
		;Override requested, but not active. Flash the override LED
		movlw	'O'
		pcall	ToggleLED
		return

CommandExpiry	decf	minutetimer,F	;Count down the expiry timer
		btfss	Expired
		return			;Timer has not expired
		btfss	HeartbeatSC	;SC command in the past minute?
		bsf	InvalidTime	;Mark the time as invalid
		movfw	controlsetpt1	;User defined control setpoint
		andlw	b'11111000'	;Ignore least significant bits
		btfss	HeartbeatCS	;CS command in the past minute?
		skpnz			;Control setpoint >= 8?
		bra	CommandExpiryJ1	;Keep using the user control setpoint
		bsf	SysCtrlSetpoint	;Invalidate the control setpoint
CommandExpiryJ1	movfw	controlsetpt3	;User defined control setpoint2
		andlw	b'11111000'	;Ignore least significant bits
		btfss	HeartbeatC2	;C2 command in the past minute?
		skpnz			;Control setpoint >= 8?
		bra	CommandExpiryJ2	;Keep using the user control setpoint
		bsf	SysCH2Setpoint	;Invalidate the CH2 control setpoint
CommandExpiryJ2	pagesel	StartTimer
		movlw	b'111'
		andwf	heartbeatflags,W
		skpz			;No commands in the past minute?
		call	StartTimer	;Restart the timer
		pagesel	$
		return			;Timer stopped

SummaryReport	movfw	txavailable	;Check if space is available in buffer
		sublw	TXBUFSIZE - 20	;(A value may consume up to 19 chars)
		skpc			;At least 20 bytes left?
		return			;Not enough space in the transmit buffer
		pcall	PrintStoredVal
		movlw	SUMMARYFIELDS
		subwf	valueindex,W
		skpnc			;Check if the summary is complete
		goto	SummaryDone
		movlw	','		;Separator
		pcall	PrintChar
		return
SummaryDone	bcf	SummaryRequest
		pcall	NewLine
		return

ProcessOutput	btfss	PIR1,TXIF	;Check if the transmit buffer is empty
		return
		movfw	txpointer	;Get the offset of the next character
		addlw	low txbuffer	;Add the start of the buffer
		movwf	FSR0L		;Setup indirect access
		bsf	FSR0H,1		;The buffer is in bank 4
		movfw	INDF0		;Get the character from the buffer
		clrf	FSR0H		;Restore the register to normal
		banksel	TXREG		;Bank 3
		movwf	TXREG		;Load the serial transmitter
		banksel	txpointer	;Bank 0
		incf	txpointer,F	;Move to the next character
		movlw	TXBUFSIZE
		xorwf	txpointer,W	;Check for the end of the buffer
		skpnz
		clrf	txpointer	;Wrap around to the start
		decf	txavailable,F	;Adjust character count
		return

;This code should not set the carry bit, it may clear the carry
EepromWait
		banksel	EECON1		;Bank 3
		btfss	EECON1,WR	;Check if a write is in progress
		return			;Safe to modify the EEPROM registers
		banksel	PIR1		;Bank 0
		btfss	PIR1,RCIF	;Check for serial receiver activity
		goto	EepromWait	;Wait some more
		movwf	temp2		;Preserve value in W register
		call	SerialReceive	;Prevent receiver overrun
		movfw	temp2		;Restore original W register value
		goto	EepromWait	;Wait some more

ReadEpromNext	banksel	EEADRL		;Bank 3
		incf	EEADRL,W
ReadEpromData	call	EepromWait	;Wait for any pending EEPROM activity
		clrf	EECON1		;Read from DATA EEPROM
		movwf	EEADRL		;Setup the EEPROM data address
		bsf	EECON1,RD	;Start the EEPROM read action
		movfw	EEDATL		;Read the data from EEPROM
		movlb	0		;Switch back to bank 0
		return

;The Opentherm specification mentions the following special circumstance:
;"The boiler unit must support an important installation feature which allows
;the terminals at the boiler to be short-circuited to simulate a heat demand
;such as can be done with existing on/off boilers. The boiler unit should
;interpret the short-circuit as a heat demand within15 secs of the short-
;circuit being applied. This must be supported by both OT/+ and OT/- boilers.
;
;It is allowable that this can implemented by a software-detection method. The
;software short-circuit condition is defined as a low-voltage state (Vlow) with
;no valid communications frame for at least 5 seconds."
;
;Without special precautions, this situation would occur when the thermostat is
;disconnected while the gateway is still powered up. Without a thermostat, the
;gateway does not generate any messages towards the boiler and the default idle
;level of the Opentherm signal is the mentioned low-voltage state.
;
;To prevent an unintended heat demand when the thermostat is disconnected, the
;thermostat interface is monitored periodically between messages. If the master
;interface is found to be at a high-voltage state, the slave interface is also
;put to the high-voltage state.

;Measure the voltage on the thermostat line. A value between 10 and 45 is the
;normal idle line value. Between 95 and 115 is the opentherm high level. We'll
;check for a value below 6 (short circuit) or above 122 (open) to detect an
;on/off thermostat (or no thermostat at all).

CheckThermostat	bcf	PIR1,ADIF	;Clear flag for next round
		banksel	ADRESH		;Bank 1
		movfw	ADRESH		;Get the A/D result
		xorwf	LineVoltage,W	;Swap values of W and LineVoltage
		xorwf	LineVoltage,F
		xorwf	LineVoltage,W	;--
		subwf	LineVoltage,W	;Calculate the difference
		skpc
		sublw	0		;Turn into absolute value
		andlw	b'11111100'	;Check for limited deviation
		skpz
		goto	Unstable
		movfw	LineVoltage	;Get the A/D result again
		subwf	VoltOpen,W	;Line open?
		skpc
		goto	OpenCircuit	;No opentherm thermostat connected
		movfw	LineVoltage	;Get the A/D result again
		subwf	VoltShort,W	;Line shorted?
		skpnc
		goto	ShortCircuit	;Heat demand from on/off thermostat
		movfw	LineVoltage	;Get the A/D result again
		subwf	VoltLow,W	;Determine the logical line level
		banksel	onoffflags	;Bank 0
		btfss	NoThermostat
		return
;Any opentherm thermostat will always start in low power mode, so we want to
;see a low line voltage before considering the thermostat to be reconnected.
		skpc			;Logical low level?
		goto	ThermostatEnd	;Not a logical low opentherm level
		movlw	CONNECTMASK	;Mask for flags to be cleared
		andwf	onoffflags,F	;Thermostat was reconnected
		movlw	TConnect
		pcall	PrintStringNL
		return
ShortCircuit
		banksel	onoffflags	;Bank 0
		bsf	HeatDemand
		goto	DumbThermostat

OpenCircuit
		banksel	onoffflags	;Bank 0
		bcf	HeatDemand
DumbThermostat	btfsc	NoThermostat
		goto	ThermostatEnd
Disconnect	bsf	NoThermostat	;Thermostat has been disconnected
		movlw	TDisconnect	;Report the thermostat was disconnected
		pcall	PrintStringNL
		bcf	T2CON,TMR2ON	;Stop timer 2
		movlw	b'10000000'
		andwf	tstatflags,F	;Forget all thermostat related info
		bsf	SlaveOut	;Output at idle level
		bsf	ComparatorT	;Smart power is off, line is high
		btfss	TStatManual	;Manually configured thermostat model
		clrf	remehaflags
		clrf	override
		clrf	setpoint1
		clrf	setpoint2
		movlw	'P'
		pcall	SwitchOffLED	;Switch off Raised Power LED
		banksel	onoffflags	;Bank 0
		return
Unstable
		banksel	onoffflags	;Bank 0
		btfss	NoThermostat
		return
ThermostatEnd	btfss	MonitorMode	;Can't send messages in monitor mode
		btfss	MsgTrigger	;Time to send the next message?
		return
StandAloneMsg	movlw	T_READ
		movwf	byte1
		clrf	byte3
		clrf	byte4
		clrf	originaltype
		bcf	SendUserMessage
		call	SelectMessage	;Get a message to send from the table
		movwf	byte2
		lcall	CreateRequest	;Apply special treatment
		;PCLATH may have been changed to the Print package by SetParity
		pagesel	Message		;Prepare for calling StoreValue
		btfsc	byte1,4		;For Write-Data messages, ...
		call	StoreValue	;... store the value we're sending
		movfw	byte2
		movwf	originalreq
		banksel	T6CON		;Bank 8
		bcf	T6CON,TMR6ON	;Prevent further triggers
		movlb	0		;Bank 0
		bcf	MsgTrigger	;Message prepared - clear trigger
		;Need lgoto because PCLATH points to the Message package
		lgoto	SendMessage	;Start sending the message

SelectMessage	movfw	MessagePtr
		brw
MessageTable	retlw	MSG_STATUS
		retlw	MSG_BOILERTEMP
		retlw	MSG_CTRLSETPT
		goto	UserMsg
		retlw	MSG_MODULATION
		retlw	MSG_OUTSIDETEMP
		retlw	MSG_RETURNTEMP
		retlw	MSG_MAXMODULATION
		goto	UserMsg
		retlw	MSG_STATUS
		retlw	MSG_BOILERTEMP
		retlw	MSG_CTRLSETPT
		retlw	MSG_DHWSETPOINT
		retlw	MSG_MAXCHSETPOINT
		goto	UserMsg
		retlw	MSG_CHPRESSURE

UserMsg		bsf	SendUserMessage	;Send a user selected message
		retlw	MSG_STATUS	;Default to the status message

CalcThreshold	movwf	byte1		;Store the multiplication factor
		movlw	8
		movwf	loopcounter	;Doing 8-bit multiplication
		movfw	ADRESH		;Get the A/D conversion result
		btfsc	ADRESL,7
		addlw	1		;Round the value up
;Multiplication from http://www.piclist.com/Techref/microchip/math/mul/8x8.htm
		clrf	byte2		;High byte of the product
		rrf	byte1,F
CalcThresholdL1	skpnc
		addwf	byte2,F
		rrf	byte2,F
		rrf	byte1,F
		decfsz	loopcounter,F
		bra	CalcThresholdL1
;--
		lslf	byte1,W		;Shift the result one bit to the left
		rlf	byte2,W		;So the high byte is the final result
		btfsc	byte1,6
		addlw	1		;Round up
		return

;************************************************************************

		package	Print
PrintString	lcall	ReadEpromData	;Read character from EEPROM
PrintStringNext	pagesel	$
		skpnz
		return			;End of string
		call	PrintChar	;Send character to serial port
		lcall	ReadEpromNext
		bra	PrintStringNext

PrintStringNL	call	PrintString
NewLine		movlw	0x0d
		call	PrintChar
		movlw	0x0a
		goto	PrintChar

PrintHex	movwf	temp2		;Temporarily store the byte
		swapf	temp2,W		;Get the high nibble
		call	PrintXChar	;And print it
		movf	temp2,W		;Next, get the low nibble
		;Fall through to the subroutine to print a hex character
PrintXChar	andlw	B'00001111'	;Extract the low nible
		addlw	-10		;Check if is a digit or alpha char
		skpnc
		addlw	'A' - '9' - 1	;Bridge the gap in the ASCII table
		addlw	10		;Re-add the amount subtracted earlier
PrintDigit	bsf	LeadingZero	;Don't skip any future zeroes
		addlw	'0'		;Convert to ASCII digit
PrintChar	tstf	txavailable	;Check if some data is already waiting
		skpnz
		btfss	PIR1,TXIF	;Check the transmit register
		goto	PrintBuffer	;Need to put the byte into the buffer
		banksel	TXREG		;Bank 3
		movwf	TXREG		;Can transmit immediately
		movlb	0		;Bank 0
		retlw	0		;Exit code for SerialCommand
PrintBuffer	movwf	FSR0L		;Abuse FSR0L for temporary storage
		movfw	txpointer	;Current position in the transmit buffer
		addwf	txavailable,W	;Add number of pending characters
		addlw	-TXBUFSIZE	;Check for wrap-around
		skpc
		addlw	TXBUFSIZE	;No wrap, recalculate original value
		addlw	low txbuffer	;Next free position in transmit buffer
		xorwf	FSR0L,W		;Exchange FSR0L and W
		xorwf	FSR0L,F
		xorwf	FSR0L,W		;--
		bsf	FSR0H,1		;Buffer is in bank 4
		movwf	INDF0		;Store byte in the buffer
		clrf	FSR0H		;Restore register to normal
		movfw	txavailable	;Get number of pending characters
		xorlw	TXBUFSIZE	;Compare against buffer size
		skpz			;Buffer full?
		incf	txavailable,F	;One more character in the buffer
		retlw	0		;Exit code for SerialCommand

PrintRcvdMsg	movlw	'T'
		btfsc	MsgResponse
		movlw	'B'
PrintMessage	btfsc	HideReports
		return			;Don't print the message
		call	PrintChar
		movfw	byte1
		call	PrintHex
		movfw	byte2
		call	PrintHex
		movfw	byte3
		call	PrintHex
		movfw	byte4
		call	PrintHex
		goto	NewLine

PrintError	btfsc	PowerReport	;Pending power change report?
		return			;Error probably due to the power change
		movlw	ErrorStr
		call	PrintString
		movfw	errornum
		call	PrintHex
		call	NewLine
		movlw	'E'
		btfss	errornum,0
		call	PrintMessage	;Print message for error 2 and 4.
		clrf	errornum
PrintDebug	movfw	DebugPointerL
		skpnz
		return
		movwf	FSR0L
		movfw	DebugPointerH
		movwf	FSR0H
		movfw	INDF0
		clrf	FSR0H
		movwf	temp
		movfw	DebugPointerH
		call	PrintXChar
		movfw	DebugPointerL
		call	PrintHex
		movlw	'='
		call	PrintChar
		movfw	temp
		call	PrintHex
		goto	NewLine

PrintIndexed	movwf	valueindex
PrintStoredVal	movfw	valueindex
		addwf	valueindex,W
		addlw	valuestorage
		movwf	FSR0L
		incf	FSR0H,F
		movfw	INDF0
		movwf	float1
		incf	FSR0L,F
		movfw	INDF0
		movwf	float2
		clrf	FSR0H
		movfw	valueindex
		incf	valueindex,F
		brw
StoreProcTable	goto	PrintStatus	;Message ID 0
		goto	PrintFloat	;Message ID 1
		goto	PrintStatus	;Message ID 6
		goto	PrintFloat	;Message ID 7
		goto	PrintFloat	;Message ID 8
		goto	PrintFloat	;Message ID 14
		goto	PrintBytes	;Message ID 15
		goto	PrintFloat	;Message ID 16
		goto	PrintFloat	;Message ID 17
		goto	PrintFloat	;Message ID 18
		goto	PrintFloat	;Message ID 19
		goto	PrintFloat	;Message ID 23
		goto	PrintFloat	;Message ID 24
		goto	PrintFloat	;Message ID 25
		goto	PrintFloat	;Message ID 26
		goto	PrintSigned	;Message ID 27
		goto	PrintFloat	;Message ID 28
		goto	PrintFloat	;Message ID 31
		goto	PrintSigShort	;Message ID 33
		goto	PrintBytes	;Message ID 48
		goto	PrintBytes	;Message ID 49
		goto	PrintFloat	;Message ID 56
		goto	PrintFloat	;Message ID 57
		goto	PrintStatus	;Message ID 70
		goto	PrintLowByte	;Message ID 71
		goto	PrintLowByte	;Message ID 77
		goto	PrintShort	;Message ID 116
		goto	PrintShort	;Message ID 117
		goto	PrintShort	;Message ID 118
		goto	PrintShort	;Message ID 119
		goto	PrintShort	;Message ID 120
		goto	PrintShort	;Message ID 121
		goto	PrintShort	;Message ID 122
		goto	PrintShort	;Message ID 123

PrintStatus	movfw	float1
		call	PrintBinary
		movlw	'/'
		call	PrintChar
		movfw	float2
PrintBinary	movwf	temp
		movlw	8
		movwf	loopcounter
PrintBinaryLoop	rlf	temp,F
		call	PrintBoolean
		decfsz	loopcounter,F
		goto	PrintBinaryLoop
		return

PrintBoolean	rlf	CPSCON0,W	;CPSCON0 is initialized to all 0's
		goto	PrintDigit

PrintNegative	comf	float1,F	;Negate the value
		comf	float2,F
		incf	float2,F
		skpnz
		incf	float1,F	;--
		movlw	'-'
		goto	PrintChar	;Print a minus sign

PrintSigned	btfsc	float1,7	;Check if value is negative
		call	PrintNegative
		;Fall through to print the absolute value
;If the low byte of the floating point value is 0xff, it would translate to a
;fraction of .996, which should be rounded up to the next full number. This
;needs to be checked before the integer part of the value can be printed.
PrintFloat	incfsz	float2,W	;Check if the 2nd byte is 0xff
		goto	PrintFloatJ1
		incf	float1,F	;Round up to the next integral number
		clrf	float2		;--
PrintFloatJ1	movfw	float1
		call	PrintByte
		movlw	100
		movwf	temp
		movfw	float2
		call	Multiply
		btfsc	float2,7
		incf	float1,F	;Rounding
		movfw	float1
		movwf	temp
		movlw	'.'
		goto	PrintFraction

PrintBytes	movfw	float1
		call	PrintByte
		movlw	'/'
		call	PrintChar
PrintLowByte	movfw	float2
PrintByte	movwf	temp
PrintTemp	movfw	temp
		bcf	LeadingZero
		addlw	-100
		skpc
		goto	PrintByteJ1
		movwf	temp
		addlw	-100
		skpc
		goto	Print100
		movwf	temp
		movlw	'2'
		goto	PrintFraction
Print100	movlw	'1'
PrintFraction	call	PrintChar
PrintDecimal	bsf	LeadingZero
PrintByteJ1	clrf	loopcounter
		movfw	temp
PrintByteL1	movwf	temp		;BCD
		addlw	-10
		skpc
		goto	PrintByteJ2
		incf	loopcounter,F
		goto	PrintByteL1
PrintByteJ2	movfw	loopcounter
		skpnz
		btfsc	LeadingZero
		call	PrintLDigit
		movfw	temp
PrintLDigit	bsf	LeadingZero
		goto	PrintDigit

PrintNibble	andlw	b'1111'
		bra	PrintByte

PrintSigShort	btfsc	float1,7
		call	PrintNegative
PrintShort	movlw	16
		movwf	loopcounter
		clrf	tempvar0
		clrf	tempvar1
		clrf	tempvar2
		goto	PrintShortJump
PrintShortLoop	movfw	tempvar2
		call	AdjustBCD
		movwf	tempvar2
		movfw	tempvar1
		call	AdjustBCD
		movwf	tempvar1
PrintShortJump	rlf	float2,F
		rlf	float1,F
		rlf	tempvar2,F
		rlf	tempvar1,F
		rlf	tempvar0,F
		decfsz	loopcounter,F
		goto	PrintShortLoop
		bcf	LeadingZero
		movfw	tempvar0
		call	PrintBCD
		movfw	tempvar1
		call	PrintBCD
		movfw	tempvar2
		call	PrintBCD
		btfsc	LeadingZero
		return
		goto	PrintDigit	;W is always 0 here
AdjustBCD	addlw	0x33
		movwf	temp
		btfss	temp,3
		addlw	-0x03
		btfss	temp,7
		addlw	-0x30
		return
PrintBCD	movwf	temp
		swapf	temp,W
		call	PrintBCDDigit
		movfw	temp
PrintBCDDigit	andlw	b'1111'
		skpnz
		btfsc	LeadingZero
		goto	PrintDigit
		return

PrintSetting	movfw	rxpointer	;Check that the command is 4 chars long
		sublw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]
		sublw	'Z'
		sublw	'Z' - 'A'
		skpc
		retlw	BadValue
		brw
PrintTable	retlw	PrintSettingA	;PR=A
		retlw	PrintSettingB	;PR=B
		retlw	PrintSettingC	;PR=C
		goto	PrintSettingD	;PR=D
		goto	PrintSettingE	;PR=E
		goto	PrintSettingF	;PR=F
		goto	PrintSettingG	;PR=G
		retlw	BadValue	;PR=H
		goto	PrintSettingI	;PR=I
		retlw	BadValue	;PR=J
		retlw	BadValue	;PR=K
		retlw	PrintSettingL	;PR=L
		goto	PrintSettingM	;PR=M
		goto	PrintSettingN	;PR=N
		goto	PrintSettingO	;PR=O
		goto	PrintSettingP	;PR=P
		goto	PrintSettingQ	;PR=Q
		goto	PrintSettingR	;PR=R
		goto	PrintSettingS	;PR=S
		goto	PrintSettingT	;PR=T
		retlw	BadValue	;PR=U
		goto	PrintSettingV	;PR=V
		goto	PrintSettingW	;PR=W
		retlw	BadValue	;PR=X
		retlw	BadValue	;PR=Y
		retlw	BadValue	;PR=Z

PrintSettingID	moviw	3[FSR1]
		call	PrintChar
		movlw	'='
		goto	PrintChar

PrintSettingV	call	PrintSettingID
PrintRefVoltage	lsrf	settings,W
		addlw	-6
		goto	PrintXChar

PrintSettingO	call	PrintSettingID
PrintOverride	tstf	override	;Any override flags set?
		movlw	'N'
		skpnz
		goto	PrintChar	;No override
		movfw	setpoint1	;Override setpoint set?
		iorwf	setpoint2,W
		movlw	'n'
		skpnz
		goto	PrintChar	;Waiting to clear the override
PrintOvrSetPt	swapf	RemOverrideFunc,W ;Get the current bits in lower nibble
		xorwf	RemOverrideFunc,W ;Determine the changed bits
		andlw	b'1111'		;Only consider the lower nibble
		btfss	OverrideFunc	;Lower nibble contains pending request
		xorwf	RemOverrideFunc,F ;Harmless to overwrite lower nibble
		movlw	'T'
		btfss	ProgChangePrio	;Either pending or current state
		movlw	'C'
		btfss	OverrideSet	;Override accepted by the thermostat?
		iorlw	0x20		;Switch to lowercase
		call	PrintChar	;Print the override type
		movfw	setpoint1	;Copy the setpoint
		movwf	float1
		movfw	setpoint2
		movwf	float2		;--
		goto	PrintFloat	;Print as a floating point value

PrintSettingM	call	PrintSettingID
PrintGateway	movlw	'G'
		btfsc	MonitorMode
		movlw	'M'
		goto	PrintChar

PrintSettingN	call	PrintSettingID
		banksel	MsgInterval	;Bank 8
		movfw	MsgInterval
		movlb	0		;Bank 0
PrintInterval	movwf	temp2
		lsrf	temp2,W
		call	PrintByte
		movlw	'0'
		btfsc	temp2,0
		movlw	'5'
		goto	PrintChar

PrintSettingQ	call	PrintSettingID
		swapf	mode,W
		andlw	b'1111'
		addlw	ResetReasonStr
		pcall	ReadEpromData
		goto	PrintChar

PrintSettingW	call	PrintSettingID
PrintHotWater	movlw	'0'
		btfsc	HotWaterEnable
		movlw	'1'
		btfss	HotWaterSwitch
		movlw	'A'
		goto	PrintChar

PrintSettingP	call	PrintSettingID
PrintPowerlevel	btfss	TransmitMode
		retlw	NormalPowerStr
		movlw	MediumPowerStr
		btfsc	ReceiveModeT
		movlw	HighPowerStr
		call	PrintString
		retlw	PowerStr

PrintSettingR	call	PrintSettingID
PrintRemeha	movlw	'D'
		btfsc	TStatManual
		movlw	'S'
		btfsc	TStatRemeha
		movlw	'R'
		btfsc	TStatCelcia
		movlw	'C'
		btfsc	TStatISense
		movlw	'I'
		goto	PrintChar

PrintSettingT	call	PrintSettingID
PrintTweaks	swapf	settings,W
		movwf	temp
		rrf	temp,F
		rrf	temp,F
		call	PrintBoolean
		rrf	temp,F
		goto	PrintBoolean

PrintSettingI	call	PrintSettingID
		rlf	PORTA,W
		addlw	b'10000000'
		call	PrintBoolean
		rlf	PORTA,W
		goto	PrintBoolean

PrintSettingG	call	PrintSettingID
PrintGPIO	swapf	GPIOFunction,W
		goto	PrintHex

PrintSettingS	call	PrintSettingID
PrintSetback	movlw	AwaySetpoint
		pcall	ReadEpromData
		movwf	float1
		pcall	ReadEpromNext
		movwf	float2
		goto	PrintFloat

PrintSettingD	call	PrintSettingID
		movlw	'O'
		btfsc	TempSensorFunc
		movlw	'R'
		goto	PrintChar

PrintSettingE	call	PrintSettingID
		movlw	'-'
		btfsc	ExternalSensor	;External sensor not configured?
		btfsc	SensorInvalid	;Sensor reading is valid?
		goto	PrintChar	;No value to report
		movlw	15		;Index for outsideval in valuestorage
		btfsc	TempSensorFunc	;Sensor configured for outside temp
		movlw	16		;Index for returnwater in valuestorage
		goto	PrintIndexed

PrintSettingF	call	PrintSettingID
		banksel	EEADRL		;Bank 3
		movlw	low 0x8008	;Configuration word 2
		movwf	EEADRL
		clrf	EEADRH
		bsf	EECON1,CFGS	;Select config space
		bcf	INTCON,GIE
		bsf	EECON1,RD
		nop
		nop
		bsf	INTCON,GIE
		movfw	EEDATH
		bcf	EECON1,CFGS	;Deselect config space
		movlb	0		;Bank 0
		andlw	1 << 5		;LVP bit
		skpz
		movlw	1		;LVP is enabled
		goto	PrintDigit

PrintSummary	movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]
		sublw	'1'
		sublw	'1' - '0'
		skpc
		retlw	BadValue
		skpnz
		goto	PrintSummaryOff
		clrf	valueindex
		bsf	SummaryRequest
		bsf	HideReports
		goto	PrintSummaryOn
PrintSummaryOff	bcf	SummaryRequest
		bcf	HideReports
PrintSummaryOn	lgoto	PrintDigit

;Multiply W by temp - result in float1:float2
Multiply	clrf	float1
		clrf	float2
		xorlw	8
		movwf	loopcounter
		xorlw	8
		xorwf	loopcounter,F
MultiplyLoop	lslf	float2,F
		rlf	float1,F
		rlf	temp,F
		skpnc
		addwf	float2,F
		skpnc
		incf	float1,F
		decfsz	loopcounter,F
		goto	MultiplyLoop
		return

;************************************************************************
;Process an incoming OpenTherm message
;************************************************************************
		package	Message
HandleMessage	btfsc	MsgResponse	;Treat requests and responses differently
		goto	HandleResponse
HandleRequest	btfsc	byte1,4		;For Write-Data messages, ...
		call	StoreValue	;... store the value from the thermostat
		call	StoreRequest	;Save the original type and data bytes
CreateRequest	movfw	byte2
		xorwf	originalreq,W	;Compare against previous message
		skpz
		clrf	repeatcount	;Message is different -> reset counter
		xorwf	originalreq,F	;Save the original message ID
		bcf	AlternativeUsed
		bsf	OverrideUsed	;Assume only data values will be changed
		call	TreatMessage	;Process the request from the thermostat
		clrf	boilercom	;Going to send a message to the boiler
		btfss	SendUserMessage	;Pick an alternative (Z bit is set)
		call	UnknownMask	;Check if ID is unsupported by boiler
		skpnz			;Data ID is not known to be unsupported
		goto	SendAltRequest
		btfss	AlternativeUsed	;Was the request modified in some way?
SendDefaultMsg	btfsc	NoThermostat	;Is a thermostat connected?
		goto	SetParity	;Calculate the parity of the new message
		return			;Send message unmodified to the boiler
SendAltRequest	btfsc	SendUserMessage	;Not sending a user message?
		call	StoreRequest	;Save the original type and data bytes
		bsf	AlternativeUsed	;Probably going to send a different msg
		bcf	OverrideUsed	;Changes will be more drastic
		call	Alternative	;Get the alternative message to send
		btfss	AlternativeUsed
		bra	SendDefaultMsg	;There were no other candidates
		bcf	AlternativeDone
		movwf	byte2		;Fill in the alternative Message ID
		call	TreatMessage	;Process the alternative message
		goto	SetParity	;Calculate the parity of the new message

UpdateRequest	btfsc	OverrideUsed
		bra	StoreDataBytes
		return
StoreRequest	movfw	byte1
		movwf	originaltype	;Save the original message type
StoreDataBytes	movfw	byte3	
		movwf	databyte1	;Save original data byte #1
		movfw	byte4
		movwf	databyte2	;Save original data byte #2
		return

;Vendor specific ID's can go through the code below without causing any trouble
;because the mask returned by UnknownMask will be 0.
HandleResponse	bsf	BoilerAlive	;Received a message from the boiler
		call	StartInterval	;Start the message interval timer
		movfw	byte2
		xorwf	prioritymsgid,W	;Response matches ID of priority msg?
		skpnz
		bcf	PriorityMsg	;Priority message answered
		btfss	byte1,4		;For Read-Ack messages, ...
		call	StoreValue	;... store the value from the boiler
		call	UnknownMask	;Check boiler support for the DataID
		btfsc	byte1,5
		btfss	byte1,4
		bra	MsgSupported	;No Unknown-DataID response
		skpnz
		bra	MsgUnsupported	;Already found 3 consecutive failures
		andlw	b'01010101'	;Get lower bit of the mask
		addwf	INDF0,F		;Increase failure count
		bra	HandleRespJump
MsgUnsupported	movfw	byte2		;Get the message ID
		pcall	DeleteAlt	;Remove from the list of alternatives
		bra	HandleRespJump
MsgSupported	xorlw	-1		;Invert the mask
		btfss	initflags,INITNR;Do nothing for priority requests
		andwf	INDF0,F		;Reset the failure count
HandleRespJump	bcf	initflags,INITNR
		bsf	BoilerResponse
		btfsc	Unsolicited
		goto	RemoteCommand
		btfsc	AlternativeUsed	;No alternative request was sent?
		bra	SendAltResponse
		call	TreatMessage	;Process the received response
		btfsc	AlternativeUsed	;Was the response modified in some way?
		btfsc	NoThermostat	;And a thermostat is connected?
		return
		bra	SetParity
SendAltResponse	call	TreatMessage	;Process the received response
		bsf	AlternativeDone
		btfsc	NoThermostat
		return
		movlw	b'01000000'	;Response bit
		btfss	OverrideUsed
		movlw	b'01110000'	;Boiler does not support the message
		call	CreateMessage	;Turn the request into a response
		bcf	BoilerResponse
		btfss	OverrideUsed	;Does the replacement need processing?
		call	TreatMessage	;Process the message to return
SetParity	movfw	byte4		;Get the last byte
		xorwf	byte3,W		;Calculate the sum of the last 2 bytes
		xorwf	byte2,W		;Calculate the sum of the last 3 bytes
		bcf	MsgParity	;Make sure the parity bit is cleared
		xorwf	byte1,W		;Calculate the sum of all four bytes
		movwf	temp		;Save in temporary storage space
		swapf	temp,F		;Swap the nibbles of the checksum
		xorwf	temp,W		;Calculate the sum of the two nibbles
		movwf	temp		;Save the result again
		rrf	temp,F		;Shift the checksum 2 bits
		rrf	temp,F
		xorwf	temp,F		;Combine two sets of two bits
		movlw	1		;Setup a mask to be used later
		btfsc	temp,1		;Check if bit 1 is set
		xorwf	temp,F		;If so, invert bit 0
		btfsc	temp,0		;Check the calculated parity
		bsf	MsgParity	;Fill in the parity bit for even parity
		btfsc	MonitorMode
		return
		movlw	'R'
		btfsc	MsgResponse
		movlw	'A'
		lgoto	PrintMessage

RemoteCommand	call	TreatMessage	;Process the received message
		movlw	B_WACK
		movwf	byte1
		movlw	MSG_REMOTECMD
		movwf	byte2
		movlw	RCMD_TELEFOON
		movwf	byte3
		movfw	celciasetpoint
		movwf	byte4
		bcf	Unsolicited
		goto	SetParity

StartInterval	banksel	T6CON		;Bank 8
		movfw	MsgInterval
		movwf	IntervalCounter
		clrf	TMR6
		bsf	T6CON,TMR6ON
		movlb	0		;Bank 0
		return

;Define special treatment based on the message ID. This code will be called in
;three situations:
;1. A request was received from the thermostat.
;2. A response was received from the boiler.
;3. An alternative message has been sent to the boiler and a fake response has
;	been created to be sent back to the thermostat.
;Which of the three situations applies can be determined by checking the
;MsgResponse and BoilerResponse flags. The flags will have the following states:
;1. MsgResponse = 0, BoilerResponse = X
;2. MsgResponse = 1, BoilerResponse = 1
;3. MsgResponse = 1, BoilerResponse = 0
;If the code modifies the message, it must set the AlternativeUsed flag. This
;will cause the parity to be recalculated and the modified message is reported
;on the serial interface.

TreatMessage	btfsc	byte2,7
		goto	WordResponse	;Limited support for Vender specific ID
		movfw	byte2
		brw
messagetable	goto	MessageID0	;Data ID 0
		goto	MessageID1	;Data ID 1
		goto	MessageID2	;Data ID 2
		goto	MandatoryWord	;Data ID 3
		goto	MessageID4	;Data ID 4
		goto	MessageID5	;Data ID 5
		goto	MessageID6	;Data ID 6
		goto	MessageID7	;Data ID 7
		goto	MessageID8	;Data ID 8
		goto	MessageID9	;Data ID 9
		goto	TSPBufferSize	;Data ID 10
		goto	TSPReadWrite	;Data ID 11
		goto	TSPBufferSize	;Data ID 12
		goto	TSPReadEntry	;Data ID 13
		goto	MessageID14	;Data ID 14
		goto	WordResponse	;Data ID 15
		goto	MessageID16	;Data ID 16
		goto	MandatoryWord	;Data ID 17
		goto	WordResponse	;Data ID 18
		goto	WordResponse	;Data ID 19
		goto	MessageID20	;Data ID 20
		goto	WordResponse	;Data ID 21
		goto	WordResponse	;Data ID 22
		goto	WordResponse	;Data ID 23
		goto	WordResponse	;Data ID 24
		goto	MandatoryWord	;Data ID 25
		goto	WordResponse	;Data ID 26
		goto	MessageID27	;Data ID 27
		goto	MessageID28	;Data ID 28
		goto	WordResponse	;Data ID 29
		goto	WordResponse	;Data ID 30
		goto	WordResponse	;Data ID 31
		goto	WordResponse	;Data ID 32
		goto	WordResponse	;Data ID 33
		goto	WordResponse	;Data ID 34
		goto	WordResponse	;Data ID 35
		goto	WordResponse	;Data ID 36
		goto	WordResponse	;Data ID 37
		goto	WordResponse	;Data ID 38
		goto	WordResponse	;Data ID 39
		goto	WordResponse	;Data ID 40
		goto	WordResponse	;Data ID 41
		goto	WordResponse	;Data ID 42
		goto	WordResponse	;Data ID 43
		goto	WordResponse	;Data ID 44
		goto	WordResponse	;Data ID 45
		goto	WordResponse	;Data ID 46
		goto	WordResponse	;Data ID 47
		goto	MessageID48	;Data ID 48
		goto	MessageID49	;Data ID 49
		goto	WordResponse	;Data ID 50
		goto	WordResponse	;Data ID 51
		goto	WordResponse	;Data ID 52
		goto	WordResponse	;Data ID 53
		goto	WordResponse	;Data ID 54
		goto	WordResponse	;Data ID 55
		goto	MessageID56	;Data ID 56
		goto	MessageID57	;Data ID 57
		goto	WordResponse	;Data ID 58
		goto	WordResponse	;Data ID 59
		goto	WordResponse	;Data ID 60
		goto	WordResponse	;Data ID 61
		goto	WordResponse	;Data ID 62
		goto	WordResponse	;Data ID 63
		goto	WordResponse	;Data ID 64
		goto	WordResponse	;Data ID 65
		goto	WordResponse	;Data ID 66
		goto	WordResponse	;Data ID 67
		goto	WordResponse	;Data ID 68
		goto	WordResponse	;Data ID 69
		goto	ByteResponse	;Data ID 70
		goto	MessageID71	;Data ID 71
		goto	WordResponse	;Data ID 72
		goto	WordResponse	;Data ID 73
		goto	WordResponse	;Data ID 74
		goto	WordResponse	;Data ID 75
		goto	WordResponse	;Data ID 76
		goto	WordResponse	;Data ID 77
		goto	WordResponse	;Data ID 78
		goto	WordResponse	;Data ID 79
		goto	WordResponse	;Data ID 80
		goto	WordResponse	;Data ID 81
		goto	WordResponse	;Data ID 82
		goto	WordResponse	;Data ID 83
		goto	WordResponse	;Data ID 84
		goto	WordResponse	;Data ID 85
		goto	WordResponse	;Data ID 86
		goto	WordResponse	;Data ID 87
		goto	TSPBufferSize	;Data ID 88
		goto	TSPReadWrite	;Data ID 89
		goto	TSPBufferSize	;Data ID 90
		goto	TSPReadEntry	;Data ID 91
		goto	WordResponse	;Data ID 92
		goto	StringCharacter	;Data ID 93
		goto	StringCharacter	;Data ID 94
		goto	StringCharacter	;Data ID 95
		goto	WordResponse	;Data ID 96
		goto	WordResponse	;Data ID 97
		goto	WordResponse	;Data ID 98
		goto	MessageID99	;Data ID 99
		goto	MessageID100	;Data ID 100
		goto	ByteResponse	;Data ID 101
		goto	WordResponse	;Data ID 102
		goto	WordResponse	;Data ID 103
		goto	WordResponse	;Data ID 104
		goto	TSPBufferSize	;Data ID 105
		goto	TSPReadWrite	;Data ID 106
		goto	TSPBufferSize	;Data ID 107
		goto	TSPReadEntry	;Data ID 108
		goto	WordResponse	;Data ID 109
		goto	WordResponse	;Data ID 110
		goto	WordResponse	;Data ID 111
		goto	WordResponse	;Data ID 112
		goto	WordResponse	;Data ID 113
		goto	WordResponse	;Data ID 114
		goto	WordResponse	;Data ID 115
		goto	WordResponse	;Data ID 116
		goto	WordResponse	;Data ID 117
		goto	WordResponse	;Data ID 118
		goto	WordResponse	;Data ID 119
		goto	WordResponse	;Data ID 120
		goto	WordResponse	;Data ID 121
		goto	WordResponse	;Data ID 122
		goto	WordResponse	;Data ID 123
		goto	WordResponse	;Data ID 124
		goto	MessageID125	;Data ID 125
		goto	MessageID126	;Data ID 126
		goto	WordResponse	;Data ID 127

messageinv	movlw	B_INV
		bra	setbyte1
messageack	movfw	originaltype	;Get original request type
		iorlw	b'11000000'	;Turn it into a matching acknowledgement
		btfss	byte1,7
		andlw	b'01111111'	;Match parity bit of the received msg
setbyte1	xorwf	byte1,W		;Check if the byte is different
		skpnz
		retlw	0
		bsf	AlternativeUsed	;Going to modify the message
		bcf	OverrideUsed	;Modifying more than the data bytes
		xorwf	byte1,F		;Set the byte
		retlw	0

setbyte3	xorwf	byte3,W		;Check if the byte is different
		skpz
		bsf	AlternativeUsed	;Going to modify the message
		xorwf	byte3,F		;Set the byte
		retlw	0

missingclock	btfss	byte1,5		;Slave didn't provide information?
		return			;Use response from the slave
missingdata	btfss	originaltype,4	;On a write, acknowledge received data
		goto	messageinv	;On a read, return: data invalid
		call	messageack
datarestore	movfw	databyte1
		call	setbyte3
		movfw	databyte2
setbyte4	xorwf	byte4,W		;Check if the byte is different
		skpz
		bsf	AlternativeUsed	;Going to modify the message
		xorwf	byte4,F		;Set the byte
		retlw	0

;DataID 0: Status
;The gateway may want to manipulate the status message generated by the
;thermostat for two reasons: The domestic hot water enable option specified
;via a serial command may differ from what the thermostat sent. And if a
;control setpoint has been specified via a serial command, central heating
;mode must be enabled for the setpoint to have any effect.
;When a status response comes back from the boiler, several LED states are
;updated. Also if the master status byte was changed before sending it to the
;boiler, the original master status must be restored before the message is
;returned to the thermostat, so it doesn't get confused
MessageID0	btfsc	MsgResponse	;Status request or response?
		goto	statusreturn
readstatus	call	MandatoryID	;Remove any blacklisting for the DataID
		comf	statusflags,W
		iorlw	b'11101010'	;Mask CHMode, Cooling, and CH2Mode bits
		andlw	b'11111101'	;Assume domestic hot water enable off
		btfss	HotWaterEnable
		btfsc	dhwpushspoof
		iorlw	b'00000010'	;Domestic hot water enable on
		movwf	temp		;Used as bit values
		clrw			;Used as mask
		btfsc	SysCtrlSetpoint
		btfsc	HeatDemand
		iorlw	1 << 0		;Enable central heating mode
		btfss	HotWaterSwitch
		btfsc	dhwpushspoof
		iorlw	1 << 1		;Manipulate hot water enable
		btfss	SysCoolLevel
		iorlw	1 << 2		;Manipulate cooling enable
		btfss	SysCH2Setpoint
		iorlw	1 << 4		;Enable central heating mode 2
		btfsc	SummerMode
		iorlw	1 << 5		;Summer mode active
		btfsc	DHWBlocking
		iorlw	1 << 6		;Enable DHW blocking
		andwf	temp,F		;Keep the bits that match the mask
		xorlw	-1		;Invert the mask
		andwf	byte3,W		;Drop the bits to be replaced
		iorwf	temp,W		;Fill in the modified bits
		call	setbyte3
		movfw	byte3
		andlw	b'00000010'	;Check the DHW enable bit
		movlw	'C'
		goto	SwitchLED	;Update the comfort mode LED

statusreturn	btfsc	byte1,5
		return			;Not a valid response from the boiler
		btfsc	dhwpushspoof
		call	dhwpushcheck
		movfw	byte4
		movwf	databyte2	;Copy slave status to response message
		andlw	b'01000001'	;Check fault bit and diagnostic bit
		skpnz
		bcf	BoilerFault	;Clear any previous boiler faults
		skpz
		btfsc	BoilerFault	;New boiler fault?
		goto	statusmaint
		bsf	BoilerFault	;Remember a fault was reported
		movlw	MSG_FAULTCODE	;ASF-flags/OEM-fault-code message
		call	SetPriorityMsg	;Schedule request for more information
statusmaint	movlw	'M'
		call	SwitchLED	;Update the maintenance LED
		movfw	byte4
		andlw	b'00001000'	;Check the flame status bit
		movlw	'F'
		call	SwitchLED	;Update the flame LED
		movfw	byte4
		andlw	b'00000100'	;Check the DHW mode bit
		movlw	'W'
		call	SwitchLED	;Update the hot water LED
		movfw	byte4
		andlw	b'00000010'	;Check the CH mode bit
		movlw	'H'
		goto	SwitchLED	;Update the central heating LED

dhwpushcheck	btfsc	dhwpushstarted
		bra	dhwpushstage2
		btfss	byte4,3		;Flame on?
		bra	dhwpushstage1
		btfsc	byte4,2		;DHW mode not active?
		bsf	dhwpushstarted	;DHW push has been activated
		return
dhwpushstage1	incf	dhwpushflags,F	;Count the unsuccessful try
		btfss	dhwpushstarted	;Counter overflow?
		return
		bra	dhwpushdone	;DHW push failed
dhwpushstage2	btfss	byte4,2		;DHW mode still active?
dhwpushdone	clrf	dhwpushflags	;DHW push finished
		bcf	manualdhwpush
		return

;DataID 1: Control Setpoint
;If a control setpoint has been specified via a serial command, the specified
;value is put into the message before sending it to the boiler.
;If the boiler indicates that the control setpoint is invalid, the value
;specified by the user is cleared.
MessageID1	btfsc	MsgResponse	;Request or response?
		goto	ctrlsetptreturn
		call	MandatoryID	;Prevent the DataID getting blacklisted
		btfss	NoThermostat	;Always modify if running stand-alone
		btfsc	AlternativeUsed	;Always modify if this is an alternative
		goto	ctrlsetptchange
		btfsc	SysCtrlSetpoint
		return			;Continue with normal processing
ctrlsetptchange	bsf	MsgWriteOp	;TSet must be a WriteData request
		btfsc	SysCtrlSetpoint	;Use user specified value
		btfss	HeatDemand	;Check if line is short-circuited
		goto	ctrlsetptuser
ctrlsetptmax	movfw	maxchsetpoint1	;Did the user specify a max setpoint?
		skpnz
		movfw	chsetpointmax	;Use the maximum control setpoint
		call	setbyte3	;Specify the desired control setpoint
		movfw	maxchsetpoint2
		goto	setbyte4
ctrlsetptuser	clrw
		btfss	SysCtrlSetpoint
		movfw	controlsetpt1
		call	setbyte3	;Specify the desired control setpoint
		btfss	SysCtrlSetpoint
		movfw	controlsetpt2
		goto	setbyte4

ctrlsetptreturn	btfss	byte1,5		;Received Data-Invalid response?
		return			;Setpoint acknowledged by boiler
		bsf	SysCtrlSetpoint	;Zap the invalid value
		return

;DataID 2: Master Configuration
;The gateway will provide Smart Power support, even if the boiler doesn't.
MessageID2	btfss	MsgResponse
		goto	BrandDetect	;Detect thermostat manufacturer
		movlw	b'1'		;Don't know what the other bits are
		andwf	databyte1,W	;... so don't acknowledge those.
		call	setbyte3	;Acknowledge Smart Power support
		movfw	databyte2	;Make sure the master MemberID in ...
		call	setbyte4	;... the response matches the request
		goto	messageack	;Turn request into acknowledgement
BrandDetect	btfsc	byte3,0		;Smart power bit
		bsf	SmartPower	;Thermostat supports Smart Power
		btfsc	TStatManual
		return			;Don't auto-detect the thermostat model
		movfw	byte4		;MemberID code of the master
		sublw	11		;Remeha
		skpnz
		btfsc	TStatRemeha
		return
		bsf	TStatRemeha	;The master is some Remeha thermostat
		btfsc	SmartPower	;No Smart Power Support?
		bsf	TStatISense	;Probably an iSense (or qSense)
		;Without Smart Power support, the thermostat can be a Celcia20
		;or a c-Mix. The Celcia20 periodically sends a Master Product
		;Version message (MsgID 126). So assume the thermostat is not
		;a Celcia20 until it informs us otherwise.
		return

;DataID 4: Remote Request
;The remote request message with a request code of 170 is used by the Celcia
;20 to specify a remote setpoint override.
MessageID4	btfss	MsgResponse
		return			;Don't modify requests
		movfw	requestcode
		subwf	byte3,W
		skpz			;Response matches code requested?
		btfsc	byte1,5		;Read Ack or Write Ack?
		bcf	initflags,INITRR;No pending remote request
		movlw	RCMD_TELEFOON	;Check for the celcia override request
		subwf	byte3,W
		skpz
		return			;Todo: Turn UnkDataID into DataInvalid?
		call	messageack	;Turn request into acknowledgement
		movfw	celciasetpoint
		goto	setbyte4

;DataID 5: Application-specific flags
;The Remeha iSense RF doesn't always report a room setpoint, but it does ask for
;the Application-specific flags quite regularly. If such a message is received
;after the remote setpoint clear command has been sent, we're going to assume
;the setpoint clear has been accepted.
MessageID5	btfsc	MsgResponse	;Only interested in requests
		goto	WordResponse	;Default handling for responses
		btfsc	OverrideClr	;No pending clear ...
		btfsc	OverrideWait	;... or clear command has not been sent?
		return			;Do nothing
		goto	roomsetptclear	;Consider setpoint clear accepted

;DataID 6: Remote Boiler Parameter flags
;If the boiler doesn't support the DHW Setpoint or Max CH Setpoint parameters,
;the gateway will simulate read access.
;Some boilers (Vaillant hrSolide VHR NL 24-28C/3 C, Bulex FAS, Windhager BWE
;100 Exklusiv Bj2006) will indicate they don't support the Max CH setpoint
;parameter. Yet they return resonable data for MsgID 49. For this reason the
;bounds are still collected, even if MsgID 6 says they are not supported.
MessageID6	btfss	MsgResponse
		return
		clrw			;Assume no support for remote-parameters
		btfss	byte1,5
		btfsc	byte1,4
		goto	parameterupdate	;No ReadAck
		movfw	byte4		;Get the remote-parameter r/w flags
		iorlw	~b'11'		;Only consider the DHW and max CH flags
		btfsc	initflags,INITRP;Parameter initialization already done?
		andwf	initflags,F	;Don't request unsupported parameters
;		movfw	byte4		;Get the remote-parameter r/w flags
parameterupdate	xorwf	onoffflags,W
		andlw	b'11'
		xorwf	onoffflags,F
		call	messageack	;Turn request into acknowledgement
		bcf	initflags,INITRP;Parameter initialization started
		movfw	byte3
		iorlw	b'11'		;The gateway will simulate read access
		goto	setbyte3

MessageID7	btfss	MsgResponse	;Don't modify responses
		btfsc	SysCoolLevel	;User provided cooling level?
		return
		bsf	MsgWriteOp	;CoolingControl is a WriteData request
		movfw	coolinglevel1
		call	setbyte3
		movfw	coolinglevel2
		goto	setbyte4
		
MessageID8	btfsc	MsgResponse
		goto	WordResponse
		btfsc	SysCH2Setpoint	;Check if a setpoint has been specified
		btfsc	AlternativeUsed	;Always modify if this is an alternative
		goto	MsgID8Change
		return
MsgID8Change	bsf	MsgWriteOp	;TSetCH2 must be a WriteData request
		call	setbyte3
		movfw	controlsetpt4
		goto	setbyte4

;If a remote override setpoint has been specified, return it to the thermostat
MessageID9	btfss	MsgResponse
		return
		btfsc	OverrideReq
		goto	MsgID9Change
		btfss	byte1,5
		return
MsgID9Change	call	messageack	;Turn request into acknowledgement
		btfss	OverrideClr
		movfw	setpoint1
		call	setbyte3	;Data byte 1 has integer part
		btfss	OverrideClr
		movfw	setpoint2
		call	setbyte4	;Data byte 2 has the fraction
		btfsc	OverrideWait
		bsf	OverrideAct
		bcf	OverrideWait	;Override has been requested
		btfsc	TStatCelcia	;Remeha Celcia thermostat?
		btfss	OverrideReq	;And override requested?
		return
		btfss	OverrideSet	;And override not yet active?
		bsf	Unsolicited	;Then resend the request on next msg
		return

;Maximum relative modulation level
MessageID14	btfsc	MsgResponse	;Status request or response?
		return
		call	MandatoryID	;Prevent the DataID getting blacklisted
		btfsc	SysMaxModLevel
		btfsc	AlternativeUsed	;Always modify if this is an alternative
		goto	MsgId14Change
		btfss	NoThermostat
		return
MsgId14Change	bsf	MsgWriteOp	;MaxRelMod must be a WriteData request
		movfw	MaxModLevel	;Get the user specified max modulation
		btfsc	SysMaxModLevel
		movlw	100		;Use 100% if the user didn't specify
		call	setbyte3	;Set the max modulation level
		goto	setbyte4	;Clear the second data byte

;When the thermostat reports a room setpoint, compare it against the requested
;override setpoint. When it matches, the remote setpoint has been accepted. If
;at some later point it no longer matches, the thermostat has overriden the
;remote setpoint and it must be released.
MessageID16	btfsc	MsgResponse
		;We don't want an überclever thermostat to stop sending these
		;messages, so make sure we always return an acknowledgement
		goto	missingdata	;Turn request into acknowledgement
		btfsc	OverrideReq	;Check for pending override request
		btfsc	OverrideWait	;Must actually send the request first
		bra	roomsetptchange
		btfsc	OverrideClr
		bra	roomsetptclear
		;Another "nice" feature of the Remeha iSense thermostat: It
		;doesn't set the setpoint to the exact value requested. So we
		;have to allow for some variation. The delta chosen is 0.125
		;as that should be enough and is easy to do with bit operations
		movfw	setpoint2	;Calcualte the difference between ...
		subwf	byte4,W		;... the received value and the ...
		movwf	temp		;... desired setpoint
		movfw	setpoint1	;Get the units of the requested setpoint
		skpc			;Check for borrow from fractional part
		addlw	1		;Adjust the integer part
		subwf	byte3,W		;Calculate the difference
		skpc
		xorlw	-1		;Make the result positive
		skpz
		bra	roomsetptdiff	;Values differ more than 1 degree
		skpc			;Carry is set if setpoint <= value
		comf	temp,F		;Invert the fraction
		movlw	b'11100000'	;Only the lower 5 bits may be set
		andwf	temp,W		;Reset the lower 5 bits
		skpz			;Result should be 0 for a match
		bra	roomsetptdiff	;Values differ more than 0.125 degrees
		;Setpoint matches the request
		call	roomsetptchange
		btfsc	OverrideSet
		return			;Override remains in effect
		bsf	OverrideSet	;Override was picked up by thermostat
		movlw	'O'
		goto	SwitchOnLED	;Switch on the override LED
roomsetptclear	bcf	OverrideClr	;Override clear data has been sent
		movfw	setpoint1
		iorwf	setpoint2,W	;Check if there is a new setpoint
		skpnz
		bra	roomsetptreset
		bsf	OverrideWait	;The new setpoint must be sent next
roomsetptchange	btfsc	NoFakeSetpoint
		return
		movfw	fakesetpoint1
		call	setbyte3
		movfw	fakesetpoint2
		goto	setbyte4
roomsetptdiff	btfss	OverrideSet	;Override currently active?
		incf	override,F	;Make 3 attempts to set the override
		btfss	OverrideSet	;Time to abandon override?
		bra	roomsetptchange
roomsetptcancel	btfsc	TStatISense	;Special treatment for iSense thermostat
		bra	roomsetptreset
		clrf	setpoint1	;Reset the setpoint variables
		clrf	setpoint2
roomsetptreset	call	roomsetptchange
		clrf	override	;Stop requesting a setpoint override
		movlw	'O'
		goto	SwitchOffLED	;Switch off the override LED

;Return the current date and time, if specified by the user
MessageID20	btfss	MsgResponse
		return			;No need to process a request
		btfsc	InvalidTime	;Is the clock value valid?
		goto	missingclock	;Handle absence of time & day data
		call	messageack	;Turn request into acknowledgement
		movfw	clock1
		call	setbyte3
		movfw	clock2
		goto	setbyte4

;Return the outside temperature, if specified via the serial interface
MessageID27	btfsc	ExternalSensor	;Not using an external sensor
		btfsc	TempSensorFunc	;Sensor is used for outside temp
		bra	UserOutsideTemp	;User provided the outside temperature
		btfsc	AlternativeUsed
		call	MandatoryID
		btfsc	SensorInvalid	;Last sensor reading was valid
		bra	OutsideInvalid	;Report invalid data
UserOutsideTemp	btfss	OutsideTemp	;Do nothing if no outside temp available
		return
		movfw	originaltype	;Get original request type
		iorlw	b'01000000'	;Make it a matching acknowledgement
		btfss	MsgResponse	;Sending a response
		movlw	T_WRITE		;Turn it into a "Write-Data" request
		call	setbyte1	;Set the message type
		movfw	outside1
		call	setbyte3
		movfw	outside2
		goto	setbyte4
OutsideInvalid	btfsc	MsgResponse	;Sending a request
		goto	messageinv	;Return "Data-Invalid" response
		movlw	T_INV
		goto	setbyte1	;Send "Invalid-Data" request

;Return the return water temperature, if obtained via a temperature sensor
MessageID28	btfss	MsgResponse	;Do not modify a request
		return
		btfsc	ExternalSensor	;Not using an external sensor
		btfss	TempSensorFunc	;Do nothing if no return water sensor
		return
		btfsc	SensorInvalid
		goto	messageinv
		call	messageack	;Turn request into acknowledgement
		banksel	returnwater1	;Bank 2
		movfw	returnwater1
		call	setbyte3
		movfw	returnwater2
		movlb	0		;Bank 0
		goto	setbyte4

;Keep track of the boundaries for domestic hot water reported by the boiler
MessageID48	btfss	MsgResponse
		return
		bcf	initflags,INITHW;Received Hot Water Boundaries response
		btfsc	byte1,5		;DataInvalid Or UnknownDataID?
		goto	WordResponse	;Ignore bad responses
		movfw	byte3
		movwf	dhwsetpointmax	;Remember upper boundary
		movfw	byte4
		movwf	dhwsetpointmin	;Remember lower boundary
		goto	WordResponse

;Keep track of the boundaries for central heating reported by the boiler
MessageID49	btfss	MsgResponse
		return
		bcf	initflags,INITCH;Received Heating Boundaries response
		btfsc	byte1,5		;DataInvalid Or UnknownDataID?
		goto	WordResponse	;Ignore bad responses
		movfw	byte3
		movwf	chsetpointmax	;Remember upper boundary
		movfw	byte4
		movwf	chsetpointmin	;Remember lower boundary
		goto	WordResponse

;Pass on user defined boundaries for domestic hot water to the boiler
MessageID56	btfsc	MsgResponse
		goto	hotwaterreturn
		tstf	dhwsetpoint1	;Check if a setpoint has been specified
		btfss	byte1,4		;Change on WriteData
		btfsc	initflags,INITSW;Or new setpoint
		skpnz			;And a setpoint exists
		return			;Continue with normal processing
SetWaterSet	movlw	T_WRITE
		call	setbyte1	;Turn into a write request, if necessary
		movfw	dhwsetpoint1
		call	setbyte3	;Load the data value high byte
		movfw	dhwsetpoint2
		call	setbyte4	;Load the data value low byte
		retlw	MSG_DHWSETPOINT	;Return MsgID when used as alternative
hotwaterreturn	bcf	initflags,INITSW
		btfss	byte1,5
		goto	UpdateRequest
		btfss	byte1,4
		goto	InvalidHotWater	;Boiler didn't respond with DataInvalid
		;Boiler does not support the message
		call	messageack	;Turn request into acknowledgement
		movfw	dhwsetpoint1
		skpnz
		movfw	dhwsetpointmax
		call	setbyte3
		movfw	dhwsetpoint2
		goto	setbyte4
InvalidHotWater	tstf	dhwsetpoint1
		skpnz
		return
		clrf	dhwsetpoint1	;Zap the invalid value
		clrf	dhwsetpoint2
		movlw	DHWSetting	;EEPROM address
		bra	ClearFloat

;Pass on user defined boundaries for central heating to the boiler
MessageID57	btfsc	MsgResponse
		goto	maxchsetptret
		tstf	maxchsetpoint1	;Check if a setpoint has been specified
		btfss	byte1,4		;Change on WriteData
		btfsc	initflags,INITSH;Or new setpoint
		skpnz			;And a setpoint exists
		return			;Continue with normal processing
SetHeatingSet	movlw	T_WRITE
		call	setbyte1	;Turn into a write request, if necessary
		movfw	maxchsetpoint1
		call	setbyte3	;Load the data value high byte
		movfw	maxchsetpoint2
		call	setbyte4	;Load the data value low byte
		retlw	MSG_MAXCHSETPOINT ;Return MsgID when used as alternative
maxchsetptret	bcf	initflags,INITSH
		btfss	byte1,5
		goto	UpdateRequest	;Boiler was happy with the message
		btfss	byte1,4
		goto	InvalidHeating
		;Boiler does not support the message
		call	messageack	;Turn request into acknowledgement
		movfw	maxchsetpoint1
		skpnz
		movfw	chsetpointmax
		call	setbyte3
		movfw	maxchsetpoint2
		goto	setbyte4
InvalidHeating	tstf	maxchsetpoint1
		skpnz
		return
		clrf	maxchsetpoint1	;Zap the invalid value
		clrf	maxchsetpoint2
		movlw	MaxCHSetting	;EEPROM address
ClearFloat	clrf	float1
		clrf	float2
		pcall	StoreFloat
		return

; Control setpoint ventilation / heat-recovery
MessageID71	lsrf	ventsetpoint,W	;Get the user defined setpoint
		btfss	AlternativeUsed	;Always modify if this is an alternative
		skpnc			;There is no user defined setpoint
		btfsc	MsgResponse	;Nothing to do for a response
		return			;Leave message unmodified
		bsf	MsgWriteOp	;VSet must be a WriteData request
		call	setbyte4	;Put the setpoint in the low byte
		goto	setbyte3	;Clear the high byte

MessageID99	swapf	byte1,W
		andlw	b'111'
		brw
		bra	ID99ReadData	;Read-Data
		bra	ID99WriteData	;Write-Data
		return			;Invalid-Data
		return			;-reserved-
		bra	ID99ReadAck	;Read-Ack
		bra	ID99WriteAck	;Write-Ack
		nop			;Data-Invalid
ID99Unknown	btfsc	manualdhwpush	;No pending DHW push
		bsf	dhwpushspoof	;Spoof DHW push
		bra	OperModeAck

ID99WriteAck	btfsc	dhwpushpending	;No pending request to start DHW push?
		btfsc	byte3,4		;DHW push request denied?
		bra	OperModeBoiler
		bsf	dhwpushspoof	;OTGW will spoof the DHW push
		bsf	byte3,4		;Overwrite the received DHW push bit
		bsf	AlternativeUsed	;Message is modified
OperModeBoiler	bcf	opermodewrite
		bcf	dhwpushpending	;DHW push attempt is completed
OperModeFlag	movfw	byte3
		xorwf	operatingmode1,W
		andlw	1 << 4		;DHW push bit
		xorwf	operatingmode1,F
OperModeAck	bcf	initflags,INITOM;Oper modes request has been answered
		call	messageack	;Turn request into corresponding ack
OperModeData	movfw	operatingmode1
		call	setbyte3
		movfw	operatingmode2
		goto	setbyte4

ID99ReadData	btfss	opermodewrite
		return
		movlw	T_WRITE
		call	setbyte1	;Turn request into a write
		bra	OperModeData

ID99ReadAck	btfsc	initflags,INITOM
		bra	ID99ReadInit	;Store the initial data from the boiler
		btfss	byte3,4		;Boiler performing a DHW push?	
		btfss	dhwpushspoof	;OTGW spoofing a DHW push?
		bra	OperModeFlag
		bsf	byte3,4		;Overwrite the received DHW push bit
		bsf	AlternativeUsed	;Message is modified
		bra	OperModeAck

ID99ReadInit	bcf	initflags,INITOM
ID99WriteData	movfw	byte3
		movwf	operatingmode1
		movfw	byte4
		movwf	operatingmode2
		btfsc	byte3,4
StartDHWPush	btfsc	manualdhwpush
		return			;DHW push is already in progress
		bsf	manualdhwpush
		call	PushOperModes	;Send to the oper modes to the boiler
		skpnz			;ID 99 is not known to be unsupported
		bsf	dhwpushspoof	;Need to spoof DHW push
		return

PushOperModes	movlw	99
		call	UnknownMask	;Check if the boiler supports ID 99
		skpnz			;Data ID is not known to be unsupported
		return
		movlw	99
		bsf	dhwpushpending	;Trying to initiate a DHW push
		bsf	opermodewrite	;Perform write operation for MsgID99
		goto	SetPriorityMsg	;Send ID 99 at the first opportunity

;Indicate under which conditions the remote override setpoint can be overridden
;by the thermostat. The opentherm specs are quite confusing regarding which byte
;the flags must be in. So the gateway has an option to put them in both bytes.
MessageID100	btfss	MsgResponse
		return
		btfsc	OverrideReq
		goto	MsgID100Change
		btfss	byte1,5
		return
MsgID100Change	btfsc	OverrideFunc	;There is a new set of function bits
		swapf	RemOverrideFunc,F ;Start using the new function bits
		bcf	AlternativeUsed	;Allow user to override the response
		call	WordResponse
		btfsc	AlternativeUsed
		goto	UserOverride	;---
		call	messageack	;Turn request into acknowledgement
		swapf	RemOverrideFunc,W
		andlw	b'11'		;Get the remote override flags
		call	setbyte4
		btfsc	OverrideHigh
		movfw	byte4		;Fix for confused manufacturers
		call	setbyte3	;Reserved, must be 0?
UserOverride	btfsc	TStatISense	;Special treatment for iSense thermostat
		btfss	OverrideFunc	;First time after setpoint change?
		goto	OverrideFuncEnd
		swapf	RemOverrideFunc,W
		xorwf	RemOverrideFunc,W ;Compare the upper and lower nibbles
		skpz
		btfsc	OverrideWait	;Func before setpoint
		goto	OverrideFuncEnd	;Previously sent fucntion bits are OK
		bsf	OverrideClr	;Clear the remote override setpoint
		bsf	OverrideReq	;Then set it again
		bsf	OverrideWait
OverrideFuncEnd	bcf	OverrideFunc	;Override func has been sent
		return

;The gateway implements version 3.0 of the opentherm protocol specification
MessageID125	btfss	MsgResponse
		return
		movfw	byte3		;Get the major version number
		btfsc	MsgUnknown	;Did the boiler return a version?
		call	messageack	;Turn request into acknowledgement W=0
		addlw	-3		;Compare against minimum version
		skpnc			;Boiler version below 3.0
		return			;Keep using the boiler response
		movlw	3
		call	setbyte3	;Major version number
		goto	setbyte4	;Minor version number

;Check if work-arounds are needed for bugs in certain thermostats
MessageID126	btfss	MsgResponse	;Only interested in requests
		btfss	TStatRemeha	;Only Remeha thermostats have known bugs
		return
		;c-Mix:	17/17
		;Celcia20: 20/3
		;iSense:	30/19, 32/23
		bcf	TStatISense	;Clear guess based on ID2 information
		movfw	byte3		;Master device product version number
		sublw	20		;Celcia 20
		skpnz
		bsf	TStatCelcia
		skpc
		bsf	TStatISense	;At least 30 and 32 are iSenses
		return

;Messages to read transparent slave parameters (TSPs) and fault history buffer
;(FHB) entries.
TSPBufferSize	btfss	MsgResponse	;Only interested in responses
		return
		btfsc	AlternativeUsed	;Info was not requested by the gateway
		btfss	BoilerResponse	;Response from the boiler
		goto	WordResponse	;Response going back to the thermostat
		movfw	byte3		;Get the number of entries
		btfss	byte1,5		;Check for valid response
		skpnz
		return			;No (valid) data available
		movwf	TSPCount	;Store the number of entries
		incf	byte2,W		;Reading entries is the next MsgID
SetPriorityMsg	movwf	prioritymsgid	;Start reading entries
		bsf	PriorityMsg
		bcf	PrioMsgWrite
		return

TSPReadWrite	btfss	MsgResponse	;Only interested in requests ...
		btfss	AlternativeUsed	;... not initiated by the thermostat
		bra	TSPReadEntry	;Apply normal treatment
		btfss	PrioMsgWrite
		bra	TSPReadEntry	;Apply normal treatment
		movlw	T_WRITE
		call	setbyte1	;Turn request into a write
		movfw	TSPValue
		goto	setbyte4	;Fill in the value
TSPReadEntry	btfss	MsgResponse	;Only interested in responses ...
		return
		btfsc	AlternativeUsed	;... not requested by the thermostat
		incf	TSPIndex,F	;Index of next entry
		movfw	TSPCount
		subwf	TSPIndex,W	;Check if more entries exist
		skpc
		bsf	PriorityMsg	;Prepare to read next entry
		return

StringCharacter	btfss	MsgResponse	;Only interested in responses ...
		return
		btfsc	AlternativeUsed	;... not requested by the thermostat
		incf	TSPIndex,F	;Index of next entry
		movfw	byte3		;Count is in the high byte
		subwf	TSPIndex,W	;Check if more entries exist
		skpc
		bsf	PriorityMsg	;Prepare to read next entry
		return

;Remove blacklisting for special DataIDs. Do this when receiving a request, so
;even (accidental) manual action cannot blacklist the DataID.
MandatoryID	btfsc	MsgResponse	;Only interested in requests
		return
		call	UnknownMask	;Get mask and pointer into unknownmap
		xorlw	-1		;Invert the mask
		andwf	INDF0,F		;Unmark the ID as unsupported
		return

Alternative	movlw	T_READ		;Alternative is probably a read command
		movwf	byte1
		clrf	byte3		;With a data value of 0
		clrf	byte4
InitCmdLoop	tstf	initflags	;Check if there are any init messages
		skpnz
		goto	SendPriorityMsg
		;Obtain some parameters from the boiler that are useful for the
		;internal operation of the gateway.
		clrf	tempvar0	;Use for tracking which flag was tested
		call	AdminMessages	;Select a message
		call	UnknownMask2	;Check whether the message is unknown
		skpz			;Admin message is unknown?
		goto	InitCmdFound
		comf	tempvar0,W	;Invert the bitmap of tested initflags
		andwf	initflags,F	;Reset the unknown initflag
		goto	InitCmdLoop	;Find another message
InitCmdFound	movfw	temp2		;MsgID was left in temp2
		return

AdminMessages	bsf	tempvar0,INITRP
		btfsc	initflags,INITRP
		retlw	MSG_REMOTEPARAM
		bsf	tempvar0,INITHW
		btfsc	initflags,INITHW
		retlw	MSG_DHWBOUNDS
		bsf	tempvar0,INITCH
		btfsc	initflags,INITCH
		retlw	MSG_MAXCHBOUNDS
		bsf	tempvar0,INITSW
		btfsc	initflags,INITSW
		goto	SetWaterSet
		bsf	tempvar0,INITSH
		btfsc	initflags,INITSH
		goto	SetHeatingSet
		bsf	tempvar0,INITOM
		btfsc	initflags,INITOM
		retlw	MSG_OPERMODES
		bsf	tempvar0,INITRR
		btfss	initflags,INITRR
		retlw	0
		movfw	requestcode
		movwf	byte3
		bsf	MsgWriteOp
		retlw	MSG_REMOTECMD

SendPriorityMsg	btfss	PriorityMsg
		goto	InitDone
		bsf	initflags,INITNR;Don't whitelist if command succeeds
		movfw	TSPIndex	;This is 0 when not requesting TSPs
		movwf	byte3
		movfw	prioritymsgid
		return
InitDone	clrf	initflags
		movfw	resetflags	;Check for pending reset requests
		skpz
		goto	ResetCommand	;Send a counter reset command
		btfsc	NoAlternative
		goto	RestoreRequest	;No alternatives available, W = 0
		movlw	32		;Size of the list of alternatives
		movwf	loopcounter	;Counter to prevent infinite loop
		btfsc	AlternativeDone	;Previous attempt did not get an answer?
AltLoop		incf	alternativecmd,F
		movfw	alternativecmd	;Next entry in the list of alternatives
		andlw	b'11111'
		addlw	AlternativeCmd	;Add the starting address of the list
		pcall	ReadEpromData	;Read a byte from EEPROM
		skpz			;Check if the slot is empty
		return			;Found an entry
		decfsz	loopcounter,F
		goto	AltLoop		;Continue searching
		bsf	NoAlternative	;List of alternatives is empty
RestoreRequest	bcf	AlternativeUsed	;Not providing an alternative after all
		;At this point W = 0
CreateMessage	iorwf	originaltype,W	;Combine W and the original message type
		movwf	byte1		;Set the message type for the response
		movfw	databyte1
		movwf	byte3		;Restore data byte #1
		movfw	databyte2
		movwf	byte4		;Restore data byte #2
		movfw	originalreq
		movwf	byte2		;Restore the message ID
		return

ResetCommand	movlw	T_WRITE
		movwf	byte1		;Initialize a Write-Data message
		movlw	116		;MsgID of first counter
		movwf	loopcounter	;Initialize loopcounter
		clrf	temp		;Use for mask
		setc			;Make sure to shift in a '1'
		goto	ResetCmdJump
ResetCmdLoop	incf	loopcounter,F
ResetCmdJump	rlf	temp,F		;Shift the mask one position
		movfw	temp		;Get the mask
		andwf	resetflags,W	;Check if the matching resetflag is set
		skpnz
		goto	ResetCmdLoop	;Find the first set bit
		xorwf	resetflags,F	;Clear the resetflag
		movfw	loopcounter	;Get the Message ID
		return

UnknownMask	movfw	byte2
UnknownMask2	movwf	temp2
		rrf	temp2,W
		rrf	WREG,W
		andlw	b'00011111'	;Get bit 2-7
		addlw	unknownmap
		movwf	FSR0L		;Pointer into the map of unknown ID's
		btfsc	temp2,7		;Check for msg ID's in the range 0..127
		retlw	0		;Don't blacklist vendor specific ID's
		movlw	b'11'
		btfsc	temp2,0
		movlw	b'1100'
		movwf	temp
		btfsc	temp2,1
		swapf	temp,F		;Calculate mask for the desired bit
		comf	INDF0,W
		andwf	temp,W		;Check if ID is unsupported by boiler
		swapf	temp,F
		swapf	temp,W		;Get the mask without changing the Z bit
		return			;Z = 1 if message is not supported

;************************************************************************
;Values to store are maintained in three tables:
;* StoreProcTable (in the Print package)
;* StoreFlagTable
;* StoreOffsTable
;Byte 0 : (00) 0 1 6 7			11000011
;Byte 1 : (04) 8 14 15			11000001
;Byte 2 : (07) 16 17 18 19 23		10001111
;Byte 3 : (0C) 24 25 26 27 28 31	10011111
;Byte 4 : (12) 33			00000010
;Byte 5 :				00000000
;Byte 6 : (13) 48 49			00000011
;Byte 7 : (15) 56 57			00000011
;Byte 8 : (17) 70 71			11000000
;Byte 9 : (19) 77			00100000
;Byte 10:				00000000
;Byte 11:				00000000
;Byte 12:				00000000
;Byte 13:				00000000
;Byte 14: (1A) 116 117 118 119		11110000
;Byte 15: (1E) 120 121 122 123		00001111

;Keep the three tables in sync!

LookupStoreFlag	movfw	temp
		brw
StoreFlagTable	retlw	b'11000011'
		retlw	b'11000001'
		retlw	b'10001111'
		retlw	b'10011111'
		retlw	b'00000010'
		retlw	b'00000000'
		retlw	b'00000011'
		retlw	b'00000011'
		retlw	b'11000000'
		retlw	b'00100000'
		retlw	b'00000000'
		retlw	b'00000000'
		retlw	b'00000000'
		retlw	b'00000000'
		retlw	b'11110000'
		retlw	b'00001111'

LookupStoreOffs	brw
StoreOffsTable	retlw	0
		retlw	4
		retlw	7
		retlw	12
		retlw	18
		retlw	19
		retlw	19
		retlw	21
		retlw	23
		retlw	25
		retlw	26
		retlw	26
		retlw	26
		retlw	26
		retlw	26
		retlw	30
		constant	SUMMARYFIELDS=34	;Maximum: 40

StoreValue	rlf	byte2,W
		skpc			;Ignore manufacturer specific ID's
		btfsc	byte1,5		;Only store valid data
		return
		movwf	temp
		swapf	temp,W
		andlw	b'00001111'	;Calculate table entry
		movwf	temp
		movfw	byte2
		andlw	b'00000111'
		movwf	loopcounter
		incf	loopcounter,F
		call	LookupStoreFlag
		xorwf	temp,W		;Exchange W and temp
		xorwf	temp,F
		xorwf	temp,W		;--
		clrf	tempvar0
StoreValueLoop	skpnc
		incf	tempvar0,F
		rrf	temp,F
		decfsz	loopcounter,F
		goto	StoreValueLoop
		skpc
		return			;Not storing this ID
		call	LookupStoreOffs
		addwf	tempvar0,F
		rlf	tempvar0,W
		addlw	valuestorage
		movwf	FSR0L
		incf	FSR0H,F
		movfw	byte3
		movwf	INDF0
		incf	FSR0L,F
		movfw	byte4
		movwf	INDF0
		clrf	FSR0H
		return

;The following code should only run for responses going back to the thermostat.
;A response to an alternative message coming back from the boiler (which will
;not be sent to the thermostat) can be recognized by both BoilerResponse and
;AlternativeUsed being set.
;The clever combination of bit tests in the following code will end up at a
;return command for requests or alternative responses.
ByteResponse	btfsc	BoilerResponse
		btfss	AlternativeUsed
		btfss	MsgResponse
		return			;Not a message towards the thermostat
		bra	ClearByteResp
MandatoryWord	call	MandatoryID	;Prevent the DataID getting blacklisted
WordResponse	btfsc	BoilerResponse
		btfss	AlternativeUsed
		btfss	MsgResponse
		return			;Not a message towards the thermostat
ClearWordResp	clrf	databyte1
ClearByteResp	clrf	databyte2
		movfw	byte2		;Get the DataID
		pcall	FindResponse
		skpz
		return			;No entry found for the DataID
		call	messageack
		moviw	-32[FSR1]	;Get databyte 2
		iorwf	databyte2,W
		call	setbyte4
		moviw	-16[FSR1]	;Get databyte 1
		iorwf	databyte1,W
		goto	setbyte3

;************************************************************************
; Switch LEDs on and off depending on their configured function
;************************************************************************

SwitchLED	skpnz			;Zero bit indicates LED state
		bra	SwitchOffLED
SwitchOnLED	addlw	functions - 'A'	;Point into the table of functions
		movwf	FSR0L		;Setup indirect addressing
DoSwitchOnLED	bsf	INDF0,0		;Remember that this function is on
		movlw	b'11111110'	;Mask off the state bit
		andwf	INDF0,W		;Get the LED(s) for the function
		skpnz
		retlw	'T'		;No LED is assigned to this function
		xorlw	-1		;Invert the bitmask
		andwf	PORTB,F		;Switch on the LED
		iorlw	~b'110'
		andwf	gpioflags,F	;Switch on virtual LEDs E/F
		retlw	'T'		;Save some movlw 'T' instructions

SwitchOffLED	addlw	functions - 'A'	;Point into the table of functions
		movwf	FSR0L		;Setup indirect addressing
DoSwitchOffLED	bcf	INDF0,0		;Remember that this function is off
		movfw	INDF0		;Get the LED for the function
		skpnz
		retlw	'B'		;No LED is assigned to this function
		iorwf	PORTB,F		;Switch off the LED
		andlw	b'110'
		iorwf	gpioflags,F	;Switch off virtual LEDs E/F
		retlw	'B'		;Save some movlw 'B' instructions

ToggleLED	addlw	functions - 'A' ;Point into the table of functions
		movwf	FSR0L		;Setup indirect addressing
		btfss	INDF0,0
		bra	DoSwitchOnLED
		bra	DoSwitchOffLED

;************************************************************************
; Parse commands received on the serial interface
;************************************************************************

		package	Serial
SerialCommand	btfsc	Overrun
		goto	SerialOverrun
SerialCmdSub	movlw	low rxbuffer
		movwf	FSR1L
		movlw	4
		subwf	rxpointer,W	;Check the length of the command >= 4
		skpc
		retlw	SyntaxError	;Invalid command
		moviw	2[FSR1]		;Third character of every command ...
		sublw	'='		;... must be '='
		skpz
		retlw	SyntaxError
		;Report the command back to the caller
		movfw	INDF1
		lcall	PrintChar
		moviw	1[FSR1]
		call	PrintChar
		movlw	':'
		call	PrintChar
		movlw	' '
		call	PrintChar
		;Indexed jump on the exclusive or of the 2 command characters
		pagesel	$
		moviw	1[FSR1]		;Get the second character of the command
		xorwf	INDF1,W		;Combine with the first character
		movwf	temp		;Save for later use
		andlw	~b'11111'	;First quick check for a valid command
		skpz
		goto	SerialCmdCH2	; C2, H2, M2 commands
		movfw	temp
		brw			;Make a quick initial selection
; Calculate table index with Tcl: tcl::mathop::^ {*}[scan SB %c%c]
SerialCmdTable	goto	SerialCmd00	; AA, MM, RR, TT commands
		goto	SerialCmd01	; RS, SR commands
		goto	SerialCmd02	; PR, KI commands
		goto	SerialCmd03	; PS command
		goto	SerialCmd04	; MI, SW, TP, VR commands
		goto	SerialCmd05	; DA, GB, MH, VS commands
		goto	SerialCmd06	; CE, GA commands
		goto	SerialCmd07	; OH, TS commands
		goto	SerialCmdLED	; LD command
		goto	SerialCmdLED	; LE command
		goto	SerialCmdLED	; LF command
		goto	SerialCmd0B	; CH command
		retlw	CommandNG
		goto	SerialCmdLED	; LA command
		goto	SerialCmdLED	; LB command
		goto	SerialCmd0F	; CL, LC commands
		goto	SerialCmd10	; CS, GW, SC commands
		goto	SerialCmd11	; BS, CR, SB commands
		goto	SerialCmd12	; FT command
#ifndef __DEBUG
		retlw	CommandNG
#else
		goto	$		; WD command
#endif
		goto	SerialCmd14	; DP command
		goto	SerialCmd15	; BW, FS command
		retlw	CommandNG
		goto	SerialCmd17	; TC command
		retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmd1A	; MW command
		goto	SerialCmd1B	; OT, SH commands
		goto	SerialCmd1C	; UI command
		goto	SerialCmd1D	; IT, PM commands
		goto	SerialCmd1E	; SM command
		goto	SerialCmd1F	; HW command

SerialOverrun	bcf	Overrun
		retlw	OverrunError

SerialCmd00	movfw	INDF1
		xorlw	'T'
		skpnz
		goto	SetTempSetPoint
		xorlw	'T' ^ 'A'
		skpnz
		goto	SetAlternative
		xorlw	'A' ^ 'M'
		skpnz
		goto	SetMaxModLevel
		xorlw	'M' ^ 'R'
		skpnz
		goto	RemoteRequest
		retlw	CommandNG

SerialCmd01	movfw	INDF1
		xorlw	'R'
		skpnz
		goto	ResetCounter
		xorlw	'R' ^ 'S'
		skpnz
		goto	SetResponse
		retlw	CommandNG

SerialCmd02	movfw	INDF1
		xorlw	'P'
		skpnz
		goto	ReportSetting
		xorlw	'P' ^ 'K'
		skpnz
		goto	KnownDataID
		retlw	CommandNG

SerialCmd03	movfw	INDF1
		xorlw	'P'
		skpnz
		goto	ReportSummary
		retlw	CommandNG

SerialCmd04	movfw	INDF1
		xorlw	'V'
		skpnz
		goto	SetVoltageRef
		xorlw	'V' ^ 'S'
		skpnz
		goto	SetHotWaterTemp
		xorlw	'S' ^ 'T'
		skpnz
		goto	SlaveParameter
		xorlw	'T' ^ 'M'
		skpnz
		goto	SetInterval
		retlw	CommandNG

SerialCmd05	movfw	INDF1
		xorlw	'D'
		skpnz
		goto	DelAlternative
		xorlw	'D' ^ 'V'
		skpnz
		goto	SetVentSetpoint
		xorlw	'V' ^ 'M'
		skpnz
		goto	SetOperModeHC1
		goto	SerialCmdGPIO

SerialCmd06	movfw	INDF1
		xorlw	'C'
		skpnz
		goto	SetCoolEnable
		goto	SerialCmdGPIO

SerialCmd07	movfw	INDF1
		xorlw	'O'
		skpnz
		goto	SetOverrideHigh
		xorlw	'O' ^ 'T'
		skpnz
		goto	SetSensorFunc
		retlw	CommandNG

SerialCmd0B	movfw	INDF1
		xorlw	'C'
		skpnz
		goto	SetCentralHeat
		retlw	CommandNG

SerialCmd0F	movfw	INDF1
		xorlw	'C'
		skpnz
		goto	SetCoolLevel
		goto	SerialCmdLED

SerialCmd10	movfw	INDF1
		xorlw	'S'
		skpnz
		goto	SetClock
		xorlw	'S' ^ 'G'
		skpnz
		goto	SetGatewayMode
		xorlw	'G' ^ 'C'
		skpnz
		goto	SetCtrlSetpoint
		retlw	CommandNG

SerialCmd11	movfw	INDF1
		xorlw	'C'
		skpnz
		goto	ClearResponse
		xorlw	'C' ^ 'S'
		skpnz
		goto	SetSetBack
		xorlw	'S' ^ 'B'
		skpnz
		goto	SetFakeSetpoint
		retlw	CommandNG

SerialCmd12	movfw	INDF1
		xorlw	'F'
		skpnz
		goto	SetTStatModel
		retlw	CommandNG

SerialCmd14	movfw	INDF1
		xorlw	'D'
		skpnz
		goto	SetDebugPtr
		retlw	CommandNG

SerialCmd15	movfw	INDF1
		xorlw	'B'
		skpnz
		goto	SetDHWBlock
		xorlw	'B' ^ 'F'
		skpnz
		goto	SetFailSafety
		retlw	CommandNG

SerialCmd17	movfw	INDF1
		xorlw	'T'
		skpnz
		goto	SetContSetPoint
		retlw	CommandNG

SerialCmd1A	movfw	INDF1
		xorlw	'M'
		skpnz
		goto	SetOperModeDHW
		retlw	CommandNG

SerialCmd1B	movfw	INDF1
		xorlw	'O'
		skpnz
		goto	SetOutsideT
		xorlw	'O' ^ 'S'
		skpnz
		goto	SetHeatingTemp
		retlw	CommandNG

SerialCmd1C	movfw	INDF1
		xorlw	'U'
		skpnz
		goto	UnknownDataID
		retlw	CommandNG

SerialCmd1D	movfw	INDF1
		xorlw	'I'
		skpnz
		goto	IgnoreError1
		xorlw	'I' ^ 'P'
		skpnz
		goto	SetPrioMessage
		retlw	CommandNG

SerialCmd1E	movfw	INDF1
		xorlw	'S'
		skpnz
		goto	SetSummerMode
		retlw	CommandNG

SerialCmd1F	movfw	INDF1
		xorlw	'H'
		skpnz
		goto	SetHotWater
		retlw	CommandNG

SerialCmdLED	movfw	INDF1
		xorlw	'L'
		skpz
		retlw	CommandNG	;LED commands must start with 'L'
		movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError	;Command length must be 4
		moviw	3[FSR1]
		movwf	temp		;Keep func code for storing in EEPROM
		sublw	'Z'
		sublw	'Z' - 'A'	;Check for valid function code
		skpc
		retlw	BadValue	;Valid functions are 'A' - 'Z'
		moviw	1[FSR1]
		addlw	-'A'		;Get LED number
		addlw	FunctionLED1	;Calculate EEPROM address
		call	WriteEpromData	;Save the LED configuration in EEPROM
		moviw	3[FSR1]
		pcall	PrintChar
		moviw	1[FSR1]
		addlw	-'A'		;Get LED number again
		movwf	temp
		moviw	3[FSR1]		;Get the function code
SetLEDFunction	movwf	float1		;Save temporarily
		movlw	b'00001000'	;Bit mask for LED 0
		btfsc	temp,1
		movlw	b'01000000'	;Bit mask for LED 2
		btfsc	temp,2
		movlw	b'00000010'	;Bit mask for LED 4
		movwf	tempvar0	;Save the bit mask
		btfsc	temp,0
		lslf	tempvar0,F	;Shift the bit mask for odd LEDs
		;Remove the old function for the LED
		movlw	functions	;Start of the function table
		movwf	FSR0L		;Setup indirect adressing
		movlw	26		;Size of the table
		movwf	loopcounter
		comf	tempvar0,W	;Invert the bit mask
SetLEDLoop	andwf	INDF0,F		;Remove the LED from the function flags
		incf	FSR0L,F		;Point to the next function
		decfsz	loopcounter,F
		goto	SetLEDLoop	;Repeat the loop
		;Setup the new function for the LED
		movfw	float1		;Name of the new function
		addlw	functions - 'A'	;Pointer into the function table
		movwf	FSR0L		;Setup indirect addressing
		movfw	tempvar0	;Reload the bit mask for the LED
		iorwf	INDF0,F		;Link the LED to the selected function
		movfw	float1		;Name of the new function, clears Z-bit
		btfss	INDF0,0		;Check the function state
		setz			;Function state is off
		lcall	SwitchLED	;Set the initial state of the LED
		retlw	0

SerialCmdGPIO	movfw	INDF1
		sublw	'G'
		skpz
		retlw	CommandNG	;GPIO commands must start with 'G'
		call	GetDecimalArg	;Get the GPIO function
		skpnc			;Valid number?
		return			;Report error
		tstf	INDF1		;Check for end of command
		skpz
		retlw	SyntaxError
		movwf	temp		;Keep func code for storing in EEPROM
		sublw	8		;Currently there are 8 GPIO functions
		skpc
		retlw	BadValue	;Invalid GPIO function
		movlw	rxbuffer + 1
		movwf	FSR1L		;Point to the GPIO port letter
		movlw	b'11110000'	;Mask to clear old GPIO A function
		btfsc	INDF1,0		;Configuring GPIO B?
		bra	SetGPIOFunction
		swapf	temp,F		;Move new function code to upper nibble
		movlw	b'1111'		;Mask to clear old GPIO B function
		btfsc	ExternalSensor	;Old GPIO B function was not DS1820?
		btfss	TempSensorFunc	;DS1820 used for outside temperature?
		bra	SetGPIOFunction
		bcf	OutsideTemp	;Forget outside temperature
SetGPIOFunction	andwf	GPIOFunction,W	;Clear the old function code
		iorwf	temp,W		;Insert the new function code
		movwf	GPIOFunction	;Store in RAM
		movwf	temp		;Value to write to EEPROM
		movlw	FunctionGPIO	;EEPROM address to write to
		call	WriteEpromData	;Save in EEPROM
		bsf	gpio_port1
		bcf	gpio_port2
		movlw	b'11000000'
		btfss	INDF1,0		;Configuring GPIO A?
		xorwf	gpio_mask,F
		lcall	gpio_initport
		skpnc
		return
		movfw	GPIOFunction
		btfss	INDF1,0		;Configuring GPIO A?
		swapf	GPIOFunction,W
		andlw	b'1111'
GoPrintDigit	lgoto	PrintByte

CheckBoolean	clrc			;Carry indicates success
		movfw	rxpointer
		xorlw	4		;Check the command is 4 characters long
		skpz
		retlw	SyntaxError	;Wrong length
		moviw	3[FSR1]		;Get the flag state
		sublw	'1'		;It must be either '0' or '1'
		sublw	'1' - '0'	;Only the two allowed values ...
		skpnc			;... result in a carry
		return			;Success
		retlw	BadValue	;Illegal boolean value

SetContSetPoint	call	GetPosFloatArg	;Parse floating point value
		skpnc
		return
		movlw	b'01'		;Only allow manual override
		goto	SetSerSetPoint
SetTempSetPoint	call	GetPosFloatArg	;Parse floating point value
		skpnc
		return
		movlw	b'11'		;Allow manual and program override
SetSerSetPoint	call	SetSetPoint	;Change the setpoint
		goto	CommandFloat	;Confirm the new setpoint

SetSetPoint	xorwf	RemOverrideFunc,W ;Store function bits in lower nibble
		andlw	b'1111'
		xorwf	RemOverrideFunc,F ;--
		movlw	0		;Clear all old state information
		btfss	TStatISense	;Special treatment for iSense thermostat
		goto	SetSetPointJ1
		movfw	setpoint1	;Check old setpoint
		iorwf	setpoint2,W
		skpz			;There is no old setpoint
		bsf	OverrideClr	;Clear old setpoint before setting new
		movlw	1 << 4		;Leave OverrideClr bit intact
SetSetPointJ1	iorlw	1 << 7		;Keep the OverrideAct bit
		andwf	override,F	;Clear old state information
		movfw	float2		;Copy the value to the setpoint vars
		movwf	setpoint2
		movfw	float1
		movwf	setpoint1	;--
		bsf	OverrideReq	;Start blinking the override LED
		bsf	OverrideWait
		bsf	OverrideFunc
		clrf	celciasetpoint
		btfsc	TStatCelcia
		bsf	Unsolicited	;Send an unsolicited 04-frame
		iorwf	setpoint2,W	;Check if both bytes are 0
		skpnz
		goto	ClearSetPoint
		btfss	TStatCelcia	;Special treatment for Celcia thermostat
		return
;The Remeha Celcia 20 thermostat does not react propperly to the value provided
;in a remote room setpoint override (MsgID=09). It needs to receive a remote
;command response (MsgID=04) to manipulate the room setpoint.
;Thanks to Rien Groot for figuring out this feature and providing the necessary
;information to add this option to the opentherm gateway firmware.
CelciaValue	movfw	setpoint1
		call	Multiply5	;Multiply the units by 5
		movfw	float2
		movwf	celciasetpoint
		movfw	setpoint2
		call	Multiply5	;Multiply the fraction by 5
		movfw	float1
		addwf	celciasetpoint,F;Add the two together
		btfsc	float2,7
		incf	celciasetpoint,F;Round up
		movfw	setpoint1
		movwf	float1
		movfw	setpoint2
		movwf	float2
		return
ClearSetPoint	btfss	OverrideAct
		bra	CancelSetPoint
		bsf	OverrideClr	;Override request to be cancelled
		bcf	ManChangePrio
		bcf	ProgChangePrio
		return
CancelSetPoint	clrf	override
		movlw	'O'
		pcall	SwitchOffLED
		return

SetSetBack	call	GetPosFloatArg	;Parse floating point value
		skpnc
		return
		skpnz			;Zero has special meaning in the code
		retlw	SyntaxError	;Setpoint cannot be zero or below
		movlw	AwaySetpoint	;EEPROM address
		call	StoreFloat	;Store a float value in EEPROM
		goto	CommandFloat

SetHotWaterTemp	call	GetFloatArg
		skpnc
		return
		skpnz
		goto	HotWaterReset
		movfw	dhwsetpointmin
		call	Compare
		skpc
		retlw	BadValue
		movfw	dhwsetpointmax
		call	Compare
		skpz
		skpc
		goto	HotWaterTempOK
		retlw	BadValue
HotWaterReset	bcf	initflags,INITSW
		goto	HotWaterFinish
HotWaterTempOK	btfsc	dhwupdate	;Check if boiler supports write access
		bsf	initflags,INITSW;Schedule to send command to the boiler
HotWaterFinish	movfw	float2
		movwf	dhwsetpoint2
		movfw	float1
		movwf	dhwsetpoint1
		movlw	DHWSetting	;EEPROM address
		call	StoreFloat	;Store a float value in EEPROM
		goto	CommandFloat

SetHeatingTemp	call	GetFloatArg
		skpnc
		return
		skpnz
		goto	HeatingReset
		movfw	chsetpointmin
		call	Compare
		skpc
		retlw	BadValue
		movfw	chsetpointmax
		call	Compare
		skpz
		skpc
		goto	HeatingTempOK
		retlw	BadValue
HeatingReset	bcf	initflags,INITSH
		goto	HeatingFinish
HeatingTempOK	btfsc	maxchupdate	;Check if boiler supports write access
		bsf	initflags,INITSH;Schedule to send command to the boiler
HeatingFinish	movfw	float2
		movwf	maxchsetpoint2
		movfw	float1
		movwf	maxchsetpoint1
		movlw	MaxCHSetting	;EEPROM address
		call	StoreFloat	;Store a float value in EEPROM
		goto	CommandFloat

SetCtrlSetpoint	call	GetPercentArg
		skpnc
		return
		movwf	controlsetpt1
		movfw	float2
		movwf	controlsetpt2
		iorwf	float1,W
		skpnz
		bra	ClrCtrlSetPoint
		bsf	HeartbeatCS
		call	StartTimer
		goto	CommandFloat
ClrCtrlSetPoint	bsf	SysCtrlSetpoint	;No user defined control setpoint
		goto	CommandFloat

StartTimer	btfss	Expired		;Timer not running?
		return
		movlw	~b'111'
		andwf	heartbeatflags,F;Clear command flags
		movlw	120
		movwf	minutetimer	;Start the timer
		return

SetVentSetpoint	call	GetDecimalArg	;Get the ventilation setpoint
		skpnc
		goto	ClrVentSetpoint	;Non-numeric value
		tstf	INDF1
		skpz
		retlw	SyntaxError	;Characters following the value
		sublw	100
		skpc
		retlw	OutOfRange	;Value must be in the range 0-100
		rlf	temp,W		;Shift a 1 into bit 0, value in bit 1-7
		movwf	ventsetpoint
		lgoto	PrintTemp	;Confirm the command
ClrVentSetpoint	clrf	ventsetpoint
		goto	ValueCleared

SetOutsideT	call	GetFloatArg
		skpnc
		bra	ClrOutsideT
		btfss	float1,7	;Temperature value negative?
		btfss	float1,6	;Temperature higher than 64 degrees?
		goto	StoreOutsideT
;		retlw	OutOfRange	;Value must be in the range -40..64
ClrOutsideT	bcf	OutsideTemp	;No (reasonable) outside temperature
ValueCleared	movlw	'-'
		lgoto	PrintChar
StoreOutsideT	call	StoreOutTemp
CommandFloat	lgoto	PrintSigned

StoreTempValue	bsf	SensorInvalid
		skpnc			;Carry indicates a failed reading
		return
		bcf	SensorInvalid
		movlw	returnwater1
		btfsc	TempSensorFunc
		bra	StoreTempBank2
StoreOutTemp	bsf	OutsideTemp	;Outside temperature available
		movlw	outside1
		call	StoreTemp
		movlw	outsideval1
StoreTempBank2	incf	FSR0H,F
StoreTemp	movwf	FSR0L
		movfw	float1
		movwi	FSR0++
		movfw	float2
		movwf	INDF0
		clrf	FSR0H
		return

SetFakeSetpoint	call	GetPercentArg	;Get the fake setpoint
		skpnc
		return
		movwf	fakesetpoint1
		movfw	float2
		movwf	fakesetpoint2
		iorwf	float1,W
		skpnz
		bsf	NoFakeSetpoint
		goto	CommandFloat

SetDebugPtr	call	CmdArgPointer
		movfw	rxpointer
		sublw	5		;Check for 2-digit address
		skpnz
		bra	ShortPointer
		incfsz	WREG,W		;Check for 3-digit address
		retlw	SyntaxError	;Incorrect command length
		call	GetHexDigit	;Get the digit for the high byte
		skpnc
		return
ShortPointer	movwf	DebugPointerH	;Store the high byte
		call	GetHexadecimal	;Get the low byte
		skpnc
		return
		movwf	DebugPointerL	;Store the low byte
		movfw	DebugPointerH
		lcall	PrintXChar
		movfw	DebugPointerL
		goto	PrintHex	;Indicate success

SetClock	call	GetDecimalArg	;Get the hours
		skpnc
		return
		movwf	clock1
		sublw	23
		skpc
		retlw	OutOfRange

		moviw	FSR1++
		sublw	':'
		skpz
		retlw	SyntaxError

		call	GetDecimal	;Get the minutes
		skpnc
		retlw	SyntaxError
		movwf	clock2
		sublw	59
		skpc
		retlw	OutOfRange

		moviw	FSR1++
		sublw	'/'
		skpz
		retlw	SyntaxError

		movlw	'0'		;Get the day of the week
		subwf	INDF1,W
		skpz
		skpc
		retlw	BadValue
		movwf	temp
		andlw	b'11111000'
		skpz
		retlw	BadValue
		moviw	++FSR1
		skpz
		retlw	SyntaxError
		swapf	temp,F
		lslf	temp,W
		iorwf	clock1,F
		bsf	HeartbeatSC
		call	StartTimer
		movfw	clock1
		andlw	b'11111'
		movwf	temp
		lcall	PrintDecimal
		movfw	clock2
		movwf	temp
		movlw	':'
		call	PrintFraction
		movlw	'/'
		call	PrintChar
		swapf	clock1,W
		movwf	temp
		rrf	temp,W
		andlw	b'111'
		goto	PrintDigit

SetHotWater	movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]
		sublw	'P'
		skpnz
		bra	SetHotWaterPush
		clrf	dhwpushflags	;Abort any manual DHW push
		bcf	manualdhwpush
		bcf	HotWaterSwitch
		bcf	HotWaterEnable
		addlw	'1' - 'P'
		skpnz
		bsf	HotWaterEnable
		andlw	~1
		skpnz
		bsf	HotWaterSwitch
		call	SaveConfig
		lgoto	PrintHotWater
SetHotWaterPush	lcall	StartDHWPush
		movlw	'P'
		lgoto	PrintChar

SetCentralHeat	call	CheckBoolean
		skpc
		return
		bcf	CHModeOff
		skpnz
		bsf	CHModeOff
		goto	GoPrintDigit

;Set the reference voltage:
;0 = 0.832V	1 = 0.960V	2 = 1.088V	3 = 1.216V	4 = 1.344V
;5 = 1.472V	6 = 1.600V	7 = 1.728V	8 = 1.856V	9 = 1.984V
SetVoltageRef	movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]		;Get the reference voltage setting
		sublw	'9'		;Valid values are '0' through '9'
		sublw	'9' - '0'	;Convert the value, while at the ...
		skpc			;... same time checking for valid input
		retlw	BadValue
		lslf	WREG,W
		addlw	13
		banksel	DACCON1		;Bank 2
		movwf	DACCON1
		movlb	0		;Bank 0
		xorwf	settings,W
		andlw	b'00011111'
		xorwf	settings,W
		movwf	settings
		call	SaveSettings
		lgoto	PrintRefVoltage

SetCoolEnable	call	CheckBoolean
		skpc
		return
		bcf	CoolModeOff
		skpnz
		bsf	CoolModeOff
		goto	GoPrintDigit

SetCoolLevel	call	GetPercentArg
		skpndc
		goto	ClrCoolLevel
		skpnc
		return
		movwf	coolinglevel1
		movfw	float2
		movwf	coolinglevel2
		goto	CommandFloat
ClrCoolLevel	bsf	SysCoolLevel
		goto	ValueCleared

SetOperModeHC1	call	GetDecimalArg	;Get the value
		skpnc
		return
		andlw	b'11110000'
		skpz
		retlw	OutOfRange
		movfw	temp
		xorwf	operatingmode2,W
		andlw	b'00001111'
		xorwf	operatingmode2,F
		call	GoPushOperModes	;Send to the oper modes to the boiler
		movfw	operatingmode2
		lgoto	PrintNibble

GoPushOperModes	lgoto	PushOperModes

SetOperModeDHW	call	GetDecimalArg	;Get the value
		skpnc
		return
		andlw	b'11110000'
		skpz
		retlw	OutOfRange
		movfw	temp
		xorwf	operatingmode1,W
		andlw	b'00001111'
		xorwf	operatingmode1,F
		call	GoPushOperModes	;Send to the oper modes to the boiler
		movfw	operatingmode1
		lgoto	PrintNibble

SlaveParameter	call	GetDecimalArg	;Get the ID
		skpnc
		return
		movwf	prioritymsgid
		bcf	PriorityMsg
		bcf	PrioMsgWrite
		;Check for a valid TSP or FHB message ID
		movlw	6		;6 possible values
		movwf	loopcounter
SlaveParamCheck	movfw	loopcounter
		decf	loopcounter,F
		brw
		retlw	BadValue	;Not a valid message ID
		addlw	108 - 106 + 1	;Message ID 108
		addlw	106 - 91 + 1	;Message ID 106
		addlw	91 - 89 + 1	;Message ID 91
		addlw	89 - 13 + 1	;Message ID 89
		addlw	13 - 11 + 1	;Message ID 13
		addlw	11 - 6		;Message ID 11
		subwf	prioritymsgid,W
		skpz
		bra	SlaveParamCheck

		moviw	FSR1++
		sublw	':'
		skpz
		retlw	SyntaxError

		call	GetDecimal	;Get the TSP index
		skpnc
		return
		movwf	TSPIndex	;Save the TSP index
		addlw	1
		movwf	TSPCount	;Pretend the index is the last TSP

		moviw	FSR1++
		skpnz
		bra	SlaveParamRead
		sublw	'='
		btfsc	loopcounter,0	;FHBs are not writable
		skpz
		retlw	SyntaxError

		call	GetDecimal	;Get the new value
		skpnc
		return
		tstf	INDF1		;Check end of string
		skpz
		retlw	SyntaxError
		movwf	TSPValue
		bsf	PrioMsgWrite
SlaveParamRead	bsf	PriorityMsg	;Request the TSP
		movfw	prioritymsgid	;Confirm the command
		lcall	PrintByte
		movlw	':'
		call	PrintChar
		movfw	TSPIndex
		btfss	PrioMsgWrite
		goto	PrintByte
		call	PrintByte
		movlw	'='
		call	PrintChar
		movfw	TSPValue
		goto	PrintByte

SetSummerMode	call	CheckBoolean
		skpc
		return
		bcf	SummerMode
		skpz
		bsf	SummerMode
		goto	SetModeDone	;Store in EEPROM and transmit ACK

SetDHWBlock	call	CheckBoolean
		skpc
		return
		bcf	DHWBlocking
		skpz
		bsf	DHWBlocking
		goto	GoPrintDigit

SetInterval	decf	rxpointer,W	;Pointer to the last character
		addwf	FSR1L,F
		movfw	INDF1		;Get the last character
		sublw	'9'
		sublw	'9' - '0'	;Check that the character is a digit
		skpc
		retlw	SyntaxError
		movwf	tempvar2	;Save for later use
		clrf	INDF1		;Remove the last char from the command
		call	CmdArgPointer
		tstf	INDF1		;Check that there is anything left
		skpnz
		retlw	OutOfRange	;1 digit is definitely out of range
		call	GetDecimal	;Get the value of the remaining chars
		skpnc
		return			;Return any error encountered
		tstf	INDF1		;Check for end of command
		skpz
		retlw	SyntaxError
		movwf	temp		;Save the original arg divided by 10
		addwf	temp,F		;Double the value
		skpnc
		retlw	OutOfRange
		btfsc	tempvar2,3	;Last digit was 8 or higher
		incf	temp,F
		movfw	tempvar2
		sublw	2		;Last digit was higher than 2
		skpc
		incf	temp,F		;The original arg divided by 5 (rounded)
		movlw	100 / 5		;Minimum interval is 100ms
		subwf	temp,W
		skpc
		retlw	OutOfRange
		movlw	MessageInterval
		call	WriteEpromData	;Store new message interval in EEPROM
		banksel	MsgInterval	;Bank 8
		movwf	MsgInterval
		movlb	0		;Bank 0
		lgoto	PrintInterval
		
SaveConfig	movfw	conf
		movwf	temp
		movlw	Configuration
		goto	WriteEpromData

StoreFloat	pcall	EepromWait	;Wait for any pending EEPROM activity
		movwf	EEADR		;Setup the EEPROM data address
		movfw	float1		;Get units
		call	StoreEpromData	;Store in EEPROM
		pcall	EepromWait	;Wait for any pending EEPROM activity
		incf	EEADR,F		;Move to next address
		movfw	float2		;Get fraction
		bra	StoreEpromData

SaveSettings	movwf	temp
		movlw	SavedSettings
;Write to data EEPROM. Data must be in temp, address in W
WriteEpromData	pcall	EepromWait	;Wait for any pending EEPROM activity
		movwf	EEADR		;Setup the EEPROM data address
StoreEpromTemp	movfw	temp		;Get the value to store in EEPROM
StoreEpromData
		banksel	EECON1		;Bank 3
		bsf	EECON1,RD	;Read the current value
		xorwf	EEDATL,W	;Check if the write can be skipped
		skpnz
		goto	StoreEpromSkip	;Prevent unnecessary EEPROM writes
		xorwf	EEDATL,F	;Load the data value
		bsf	EECON1,WREN	;Enable EEPROM writes
		bcf	INTCON,GIE	;The sequence should not be interrupted
		;Required sequence to write EEPROM
		movlw	0x55
		movwf	EECON2
		movlw	0xAA
		movwf	EECON2
		bsf	EECON1,WR	;Start the actual write
		;--
		bsf	INTCON,GIE	;Interrupts are allowed again
		bcf	EECON1,WREN	;Prevent accidental writes
StoreEpromSkip	movfw	EEDATL		;Return the byte that was written
		movlb	0		;Bank 0
		return

SetAlternative	call	GetDecimalArg	;Get the message number
		skpnc			;Valid decimal value?
		return			;Return the error code from GetDecimal
		skpnz			;Don't allow adding message ID=0 ...
		retlw	OutOfRange	;... because 0 indicates an empty slot
		tstf	INDF1		;Is this the end of the command?
		skpz
		retlw	SyntaxError
		bcf	NoAlternative	;There now is at least one alternative
		movlw	AlternativeCmd
SetAltLoop	pcall	ReadEpromData
		skpnz
		goto	StoreAlt	;Slot is empty, put the DataID in it
		banksel	EEADRL		;Bank 3
		incfsz	EEADRL,W	;Slot not empty, try next position
		goto	SetAltLoop
		movlb	0		;Bank 0
		retlw	NoSpaceLeft	;No empty slot available
StoreAlt	call	StoreEpromTemp
		lgoto	PrintByte

DelAlternative	call	GetDecimalArg	;Get the message number
		skpnc			;Valid decimal value?
		return			;Return the error code from GetDecimal
		skpnz			;Don't allow deleting message ID=0 ...
		retlw	OutOfRange	;... because 0 indicates an empty slot
		tstf	INDF1		;Is this the end of the command?
		skpz
		retlw	SyntaxError
		call	DeleteAlt
		skpz			;MessageID was actually deleted?
		return			;Return the error code
		lgoto	PrintTemp	;Print the message ID

DeleteAlt	movwf	temp
		movlw	AlternativeCmd	;Start of list of alternatives
DelAltLoop	pcall	ReadEpromData	;Read a byte from EEPROM
		xorwf	temp,W		;Compare to the message ID to delete
		skpnz
		goto	StoreEpromData	;Release the slot by writing 0 to it
		banksel	EEADR		;Bank 3
		incfsz	EEADR,W		;Proceed to the next slot
		goto	DelAltLoop	;Repeat the loop
		movlb	0		;Bank 0
		retlw	NotFound	;The requested message ID was not found

SetTStatModel	movfw	rxpointer
		sublw	4		;Command takes a single char argument
		skpz
		retlw	SyntaxError
		clrf	remehaflags	;Assume auto-detect
		moviw	3[FSR1]
		xorlw	'C'		;C=Celcia 20
		skpnz
		bsf	TStatCelcia
		xorlw	'C' ^ 'I'	;I=ISense
		skpnz
		bsf	TStatISense
		xorlw	'I' ^ 'S'	;S=Standard
		skpnz
		bsf	TStatManual
		tstf	remehaflags	;No set bits means auto-detection
		skpz
		bsf	TStatManual	;Thermostat model set manually
		movfw	remehaflags
		movwf	temp
		movlw	TStatModel	;EEPROM address for thermostat model
		call	WriteEpromData	;Save in EEPROM
		lgoto	PrintRemeha	;Report the selected model

SetGatewayMode	movfw	rxpointer	;Check the command is 4 characters long
		sublw	4
		skpz
		retlw	SyntaxError
		call	CheckBoolean
		skpc
		goto	ResetGateway
		bsf	MonModeSwitch	;Inverse logic: W=1 for gateway mode
		xorwf	conf,F
		xorwf	mode,W		;Compare against the current setting
		andlw	1 << MONMODEBIT
		skpz			;More inverse logic
		goto	SetModeDone	;Not really changing the mode
		bsf	ChangeMode	;Remember to change the gateway mode
		call	SetMonitorMode
		btfsc	ChangeMode	;Mode change done?
		movwi	3[FSR1]		;Store back in rxbuffer for reporting
SetModeDone	call	SaveConfig	;Store the requested mode in EEPROM
		goto	PrintBufChar3

SetMonitorMode
		banksel	T2CON		;Bank 0
		btfsc	T2CON,TMR2ON	;Check if timer 2 is running
		retlw	'P'		;Don't change while communicating
SetupCompModule	bcf	MonitorMode
		btfss	MonModeSwitch
		btfsc	FailSafeMode
		bsf	MonitorMode
		;Bit 7 CxINTP = 1 - CxIF set on positive edge
		;Bit 6 CxINTN = 1 - CxIF set on negative edge
		;Bit 5-4 = 1 - CxVP connects to DAC voltage reference
		;Bit 3-2 Unimplemented
		;Bit 1-0 = 0 - C12IN0- (RA0) / 1 - C12IN1- (RA1)
		banksel	CM1CON0		;Bank 2
		movlw	b'11010000'
		movwf	CM1CON1
		movlw	b'11010001'
		movwf	CM2CON1
		movlw	GATEWAYMODE	;Comparator setting for gateway mode
		btfsc	MonitorMode	;Check which mode to switch to
		movlw	MONITORMODE	;Comparator setting for monitor mode
		movwf	CM1CON0		;Configure comparator 1
		movwf	CM2CON0		;Configure comparator 2
		bcf	ChangeMode	;Change has been applied
		movlb	0		;Bank 0
		return

ResetGateway	moviw	3[FSR1]
		sublw	'R'		;GW=R resets the gateway
		skpz
		retlw	BadValue
		reset

SetSensorFunc	moviw	3[FSR1]
		xorlw	'O'		;TS=O for outside temperature
		skpnz
		bra	SetSensorFuncO
		xorlw	'O' ^ 'R'	;TS=R for return water temperature
		skpz
		retlw	BadValue
		bsf	TempSensorFunc
		goto	SetModeDone
SetSensorFuncO	bcf	TempSensorFunc
		goto	SetModeDone

SetMaxModLevel	call	GetDecimalArg	;Get the modulation level
		skpnc
		goto	ClrMaxModLevel
		tstf	INDF1
		skpz
		retlw	SyntaxError
		sublw	100
		skpc
		retlw	OutOfRange
		sublw	100
		movwf	MaxModLevel
		lgoto	PrintByte
ClrMaxModLevel	bsf	SysMaxModLevel
		goto	ValueCleared

ReportSetting	lgoto	PrintSetting

ReportSummary	lgoto	PrintSummary

SetResponse	call	GetDecimalArg	;Get the DataID
		skpnc
		return			;No decimal value specified
		skpnz			;DataID should not be 0
		retlw	BadValue
		movwf	temp2
		moviw	FSR1++
		xorlw	':'
		skpz
		retlw	SyntaxError
		clrf	float1
		call	GetDecimal	;Get byte value
		skpnc
		return			;Missing byte value
		tstf	INDF1
		skpnz
		goto	OneByteResponse
		movwf	float1		;Save as upper byte
		moviw	FSR1++
		xorlw	','
		skpz
		retlw	SyntaxError
		call	GetDecimal	;Get the lower byte value
		skpnc
		return			;Missing lower byte value
		tstf	INDF1		;Command should be finished here
		skpz
		retlw	SyntaxError
OneByteResponse	movwf	float2		;Save lower byte
		movfw	temp2		;Get specified DataID
		call	FindResponse	;Check if it is already in the list
		skpnz
		goto	UpdateResponse	;DataID found in the list
		clrw
		call	FindResponse	;Find an empty slot
		skpz
		retlw	NoSpaceLeft	;There is no empty slot
UpdateResponse	movfw	float2		;Get the lower byte
		movwi	-32[FSR1]	;Save in the slot matching the DataID
		movfw	float1		;Get the upper byte
		movwi	-16[FSR1]	;Save in the slot matching the DataID
		movfw	temp2
		movwf	INDF1		;Save the DataID
		lcall	PrintByte
		movlw	':'
		call	PrintChar
		goto	PrintBytes

ClearResponse	call	GetDecimalArg	;Get the DataID
		skpnc
		return			;No decimal value specified
		skpnz			;DataID should not be 0
		retlw	BadValue
		tstf	INDF1
		skpz
		retlw	SyntaxError
		call	FindResponse	;Find the slot for the DataID
		skpz
		retlw	NotFound	;The DataID was not in the list
		clrf	INDF1		;Release the slot
		lgoto	PrintTemp

FindResponse	movwf	temp		;Save the DataID to search for
		movlw	high ResponseValues1
		movwf	FSR1H
		movlw	ResponseValues1
		movwf	FSR1L
FindResLoop	movfw	INDF1		;Get the DataID for the response
		xorwf	temp,W
		skpnz			;Check if the slot matches
		return			;DataID found (carry is cleared)
		movlw	1
		addwf	FSR1L,F
		skpdc
		goto	FindResLoop
		retlw	0		;DataID not found (carry is set)

UnknownDataID	call	GetDecimalArg	;Get the DataID
		skpnc
		return			;No decimal value specified
		skpz			;DataID should not be 0
		movwf	float1
		btfsc	float1,7	;Only allow MsgID's in range 1..127
		retlw	BadValue
		tstf	INDF1		;Is this the end of the command?
		skpz
		retlw	SyntaxError
		pcall	UnknownMask2	;Get mask and pointer into unknownmap
		iorwf	INDF0,F		;Mark message as unsupported
		movfw	float1
		call	GetUnknownFlags
		iorwf	temp,W
StoreDataIdMask	call	StoreEpromData	;Update non-volatile storage
		movfw	float1
		lgoto	PrintByte

KnownDataID	call	GetDecimalArg	;Get the DataID
		skpnc
		return			;No decimal value specified
		skpz			;DataID should not be 0
		movwf	float1
		btfsc	float1,7	;Only allow MsgID's in range 1..127
		retlw	BadValue
		tstf	INDF1		;Is this the end of the command?
		skpz
		retlw	SyntaxError
		pcall	UnknownMask2	;Get mask and pointer into unknownmap
		xorlw	-1		;Invert the mask
		andwf	INDF0,F		;Mark message as supported
		movfw	float1
		call	GetUnknownFlags
		comf	temp,F
		andwf	temp,W
		goto	StoreDataIdMask

GetUnknownFlags	call	BitMask
		movwf	temp
		swapf	temp2,F
		rlf	temp2,W		;Move MSB into carry
		rlf	temp2,W		;Swap + rol => ror 3
		andlw	b'11111'	;Top 3 bits contain garbage
		addlw	UnknownFlags
		pcall	ReadEpromData
		return

BitMask		movwf	temp2
		movlw	b'1'
		btfsc	temp2,1
		movlw	b'100'
		btfsc	temp2,0
		lslf	WREG,F
		btfsc	temp2,2
		swapf	WREG,W
		return

;There have been reports that some equipment does not produce clean transitions
;between the two logical Opentherm signal levels. As a result the gateway may
;produce frequent Error 01 reports. By setting the IT flag, the requirement for
;a maximum of one mid-bit transition is no longer checked.
;When this flag is set, Error 01 will never occur.
IgnoreError1	call	CheckBoolean
		skpc
		return
		moviw	3[FSR1]
		lsrf	WREG,W
		movlw	1 << 5		;Mask for IgnoreErr1
SetSetting	skpnc
		iorwf	settings,F	;Set the option
		xorlw	-1		;Invert the mask
		skpc
		andwf	settings,F	;Clear the option
		movfw	settings
		call	SaveSettings	;Store the setting in EEPROM
PrintBufChar3	moviw	3[FSR1]
		lgoto	PrintChar

SetOverrideHigh	call	CheckBoolean
		skpc
		return
		moviw	3[FSR1]
		lsrf	WREG,W
		movlw	1 << 6		;Mask for the OverrideHigh bit
		goto	SetSetting

;Reset boiler counters
;Algorithm:
;First char:	H = 0, W = 2
;Second char: B = 0, P = 1
;Third char:	S = 0, H = 4
;If bit 1 is set, xor bit 0

ResetCounter	movfw	rxpointer
		sublw	6
		skpz
		retlw	SyntaxError
		movlw	1 << 0
		movwf	temp
		moviw	3[FSR1]
		xorlw	'H'
		skpnz
		goto	ResetCntJump1
		xorlw	'H' ^ 'W'
		skpz
		retlw	SyntaxError
		movlw	1 << 2
		movwf	temp
ResetCntJump1	moviw	4[FSR1]
		btfsc	temp,2
		xorlw	'B' ^ 'P'	;Turn a B into a P and a P into a B
		xorlw	'B'
		skpnz
		goto	ResetCntJump2
		xorlw	'B' ^ 'P'
		skpz
		retlw	SyntaxError
		lslf	temp,F
ResetCntJump2	moviw	5[FSR1]
		xorlw	'S'
		skpnz
		goto	ResetCntJump3
		xorlw	'S' ^ 'H'
		skpz
		retlw	SyntaxError
		swapf	temp,F
ResetCntJump3	movfw	temp
		iorwf	resetflags,F
		moviw	3[FSR1]
		lcall	PrintChar
		moviw	4[FSR1]
		call	PrintChar
		moviw	5[FSR1]
		goto	PrintChar

SetPrioMessage	call	GetDecimalArg	;Get the DataID
		skpnc
		return			;No decimal value specified
		tstf	INDF1		;Is this the end of the command?
		skpz
		retlw	SyntaxError
		movwf	prioritymsgid
		bsf	PriorityMsg
		bcf	PrioMsgWrite
		clrf	TSPCount
		clrf	TSPIndex
		lgoto	PrintByte

CmdArgPointer	movlw	rxbuffer + 3
		movwf	FSR1L
		return

;Returns the decimal value found. Carry is set on error
GetDecimalArg	call	CmdArgPointer
GetDecimal	movfw	INDF1
		sublw	'9'
		sublw	'9' - '0'
		skpc
		goto	CarrySyntaxErr
GetDecLoop	movwf	temp
		moviw	++FSR1
		sublw	'9'
		sublw	'9' - '0'
		skpc
		goto	GetDecReturn
		movwf	tempvar0
		movlw	26
		subwf	temp,W
		skpnc
		retlw	OutOfRange
		movfw	temp
		call	Multiply10
		addwf	tempvar0,W
		skpc
		goto	GetDecLoop
		retlw	OutOfRange
GetDecReturn	movfw	temp
		return

GetHexadecimal	call	GetHexDigit
		skpnc
		return
		movwf	temp
		swapf	temp,F
		call	GetHexDigit
		skpc
		iorwf	temp,W
		return

GetHexDigit	moviw	FSR1++
		addlw	-'0'
		skpc
		goto	CarrySyntaxErr
		addlw	-10
		skpc
		goto	GetHexDigitNum
		andlw	~h'20'
		addlw	-7
		skpc
		goto	CarrySyntaxErr
		addlw	-6
		skpnc
		retlw	SyntaxError
		addlw	6
GetHexDigitNum	addlw	10
		clrc
		return

GetPercentArg	call	GetPosFloatArg
		setdc			;DC indicates bad floating point value
		skpnc
		return
		addlw	-100
		clrdc
		skpc
		goto	ReturnFloatArg	;Value is below 100
		skpz
		retlw	OutOfRange	;Value is above 100
		tstf	float2
		skpz
		retlw	OutOfRange	;Value is 100 + some fraction
		goto	ReturnFloatArg	;Value is exacly 100.0

GetPosFloatArg	call	CmdArgPointer
		movfw	INDF1
		sublw	'-'
		bra	GetFloatPlus
GetFloatArg	call	CmdArgPointer
GetFloat	bsf	NegativeTemp	;Assume a negative value
		movfw	INDF1
		sublw	'-'		;Check if the assumption is right
		skpnz
		goto	GetSignedTemp
GetFloatPlus	bcf	NegativeTemp	;No minus sign. Must be positive then.
		sublw	'-' - '+'	;Check for a plus sign
		skpnz			;No sign specified
GetSignedTemp	incf	FSR1L,F		;Skip the sign
		call	GetDecimal	;Get the integer part
		skpnc			;Carry is set on error
		return			;Return the error code from GetDecimal
		clrf	float1		;Assume no fractional part
		movfw	INDF1		;Check for end of string
		skpnz
		goto	GetFloatValue	;No fractional part specified
		sublw	'.'
		skpz
		goto	CarrySyntaxErr	;The only allowed character is a '.'
		movlw	b'000000111'	;Action encoding
		movwf	loopcounter
FloatFracLoop	moviw	++FSR1		;Check for end of string
		skpnz
		goto	GetFloatValue	;End of fractional part
		sublw	'9'
		sublw	'9' - '0'	;Only digits are allowed at this point
		skpc
		goto	CarrySyntaxErr	;Found something other than a digit
		btfsc	loopcounter,2
		call	Multiply10	;Multiply the digit by 10
		btfsc	loopcounter,1
		addwf	float1,F
		lsrf	loopcounter,F
		tstf	loopcounter
		skpnz
		skpc
		goto	FloatFracLoop
		addlw	-5
		skpnc
		incf	float1,F
		goto	FloatFracLoop
GetFloatValue	btfss	NegativeTemp	;Check for negative values
		goto	GetFloatWrapUp
		comf	temp,F		;Invert the integer part
		tstf	float1
		skpnz
		goto	GetFloatNegInt
		movfw	float1
		sublw	100
		movwf	float1
GetFloatWrapUp	clrf	float2
		call	Divide100
		skpnc			;Carry is set if float1 was >= 100
		incf	temp,F
		tstf	tempvar1	;The honneywell thermostat always ...
		skpz			;... rounds down, so if the result ...
		addlw	1		;... is not exact, round up
		movwf	float2
		movfw	temp
		movwf	float1
ReturnFloatArg	movfw	float1
		skpnz
		tstf	float2		;Z bit indicates if float value is 0.0
		clrc
		return
GetFloatNegInt	incf	temp,F		;Add 1 to get negative value
		goto	GetFloatWrapUp

CarrySyntaxErr	setc
		retlw	SyntaxError

Multiply5	clrf	float1
		movwf	float2
		call	Multiply2
		call	Multiply2
		addwf	float2,F
		skpnc
		incf	float1,F
		return

Multiply2	lslf	float2,F
		rlf	float1,F
		return

Multiply10	movwf	tempvar1	;Store a copy in a temporary variable
		addwf	tempvar1,F	;Add again to get: value * 2(clears C)
		rlf	tempvar1,F	;Shift left gives: value * 4
		addwf	tempvar1,F	;Add original value: value * 5
		rlf	tempvar1,W	;Shift left for: value * 10
		return			;Return the result

;Divide float1:float2 by 100 - result in W
Divide100	movlw	16
		movwf	loopcounter
		clrf	tempvar0
		clrf	tempvar1
DivideLoop	rlf	float2,F
		rlf	float1,F
		rlf	tempvar1,F
		movlw	100
		subwf	tempvar1,W
		skpnc
		movwf	tempvar1
		rlf	tempvar0,F
		decfsz	loopcounter,F
		goto	DivideLoop
		movfw	tempvar0
		return

;Compare the value in float1:float2 to W, sets C and Z bit. C=1 if float >= W
Compare		subwf	float1,W
		skpz
		return
		tstf	float2
		return

SerialCmdCH2	moviw	1[FSR1]
		xorlw	'2'
		skpz
		retlw	CommandNG	;Command is not valid
		movfw	INDF1
		xorlw	'C'
		skpnz
		goto	SetCtrlSetpt2
		xorlw	'C' ^ 'H'
		skpnz
		goto	SetCentralHeat2
		xorlw	'H' ^ 'M'
		skpz
		retlw	CommandNG	;Command is not valid

SetOperModeHC2	call	GetDecimalArg	;Get the value
		skpnc
		return
		andlw	b'11110000'
		skpz
		retlw	OutOfRange
		swapf	temp,W
		xorwf	operatingmode2,W
		andlw	b'11110000'
		xorwf	operatingmode2,F
		call	GoPushOperModes	;Send to the oper modes to the boiler
		swapf	operatingmode2,W
		lgoto	PrintNibble

SetCentralHeat2	call	CheckBoolean
		skpc
		return
		bcf	CH2ModeOff
		skpnz
		bsf	CH2ModeOff
		goto	GoPrintDigit

SetCtrlSetpt2	call	GetPercentArg
		skpnc
		return
		movwf	controlsetpt3
		movfw	float2
		movwf	controlsetpt4
		iorwf	float1,W
		skpnz
		bra	ClrCtrlSetpt2
		bsf	HeartbeatC2
		call	StartTimer
		goto	CommandFloat
ClrCtrlSetpt2	bsf	SysCH2Setpoint	;No user defined control setpoint
		goto	CommandFloat

RemoteRequest	call	GetDecimalArg
		skpnc
		return
		movwf	requestcode
		bsf	initflags,INITRR
		lgoto	PrintByte

SetFailSafety	call	CheckBoolean
		skpc
		return
		bcf	NoFailSafe
		skpnz
		bsf	NoFailSafe	;Inverse logic
		call	SaveConfig
		goto	PrintBufChar3

;************************************************************************
; Implement the GPIO port functions
;************************************************************************

		package	gpio
		constant GPIO_NONE=0
		constant GPIO_GND=1
		constant GPIO_VCC=2
		constant GPIO_LEDE=3
		constant GPIO_LEDF=4
		constant GPIO_HOME=5
		constant GPIO_AWAY=6
		constant GPIO_DS1820=7
		constant GPIO_DHWBLK=8

gpio_init	bsf	gpio_port1	;Set up the port mask for gpio port 1
		bcf	gpio_port2
		call	gpio_initport	;Initialize the port funtion
		bcf	gpio_port1	;Set up the port mask for gpio port 2
		bsf	gpio_port2
gpio_initport	clrc
		movfw	GPIOFunction
		btfss	gpio_port2
		bra	gpio_initport1
		swapf	GPIOFunction,W
		bcf	ExternalSensor
gpio_initport1	andlw	b'1111'		;Use only the low nibble
		brw			;Jump into the jump table
gpio_inittab	goto	gpio_input	;GPIO_NONE
		goto	gpio_initgnd	;GPIO_GND
		goto	gpio_initvcc	;GPIO_VCC
		goto	gpio_initvcc	;GPIO_LEDE
		goto	gpio_initvcc	;GPIO_LEDF
		goto	gpio_input	;GPIO_HOME
		goto	gpio_input	;GPIO_AWAY
		goto	gpio_onewire	;GPIO_DS1820
		goto	gpio_input	;GPIO_DHWBLK
		bra	gpio_invalid	;9
		bra	gpio_invalid	;10
		bra	gpio_invalid	;11
		bra	gpio_invalid	;12
		bra	gpio_invalid	;13
		bra	gpio_invalid	;14
gpio_invalid	movlw	b'11110000'
		btfsc	gpio_port1
		andwf	GPIOFunction,F
		movlw	b'00001111'
		btfsc	gpio_port2
		andwf	GPIOFunction,F
		setc
		retlw	BadValue

gpio_onewire	btfss	gpio_port2	;Function is only supported on port 2
		bra	gpio_invalid	;Disable function on port 1
		bsf	ExternalSensor
gpio_input	movfw	gpio_mask	;Get the port mask
		andlw	b'11000000'	;Mask of the other bits
		banksel	TRISA		;Bank 1
		iorwf	TRISA,F		;Set the port to input
		movlb	0		;Bank 0
		return
gpio_output	comf	gpio_mask,W	;Get the inverted port mask
		iorlw	b'00111111'	;Set all other bits
		banksel	TRISA		;Bank 1
		andwf	TRISA,F		;Set the port to output
		movlb	0		;Bank 0
		return
gpio_initgnd	call	gpio_output	;Make the port an output
		andwf	PORTA,F		;Pull the output low
		return
gpio_initvcc	call	gpio_output	;Make the port an output
		xorlw	-1		;Invert the mask
		iorwf	PORTA,F		;Pull the output high
		return

gpio_common	bsf	gpio_port1	;Set up the port mask for gpio port 1
		bcf	gpio_port2
		call	gpio_apply	;Perform the port function
		bcf	gpio_port1	;Set up the port mask for gpio port 2
		bsf	gpio_port2
gpio_apply	movfw	GPIOFunction
		btfsc	gpio_port2
		swapf	GPIOFunction,W
		andlw	b'1111'		;Limit to 0-15
		brw			;Jump into the jump table
gpio_table	return			;GPIO_NONE
		return			;GPIO_GND
		return			;GPIO_VCC
		goto	gpio_lede	;GPIO_LEDE
		goto	gpio_ledf	;GPIO_LEDF
		goto	gpio_home	;GPIO_HOME
		goto	gpio_away	;GPIO_AWAY
		goto	gpio_temp	;GPIO_DS1820
		goto	gpio_block	;GPIO_DHWBLK
		
gpio_lede	movfw	PORTA		;Get current state of the GPIO pin
		btfsc	VirtualLedE	;Check the shadow LED state
		comf	PORTA,W		;Get inverted state of the GPIO pin
		andwf	gpio_mask,W	;Mask off the other GPIO pin
		andlw	b'11000000'	;Mask off the remaining bits
		xorwf	PORTA,F		;Update the GPIO port
		return
gpio_ledf	movfw	PORTA		;Get current state of the GPIO pin
		btfsc	VirtualLedF	;Check the shadow LED state
		comf	PORTA,W		;Get inverted state of the GPIO pin
		andwf	gpio_mask,W	;Mask off the other GPIO pin
		andlw	b'11000000'	;Mask off the remaining bits
		xorwf	PORTA,F		;Update the GPIO port
		return
gpio_temp	btfss	Intermission	;Wait for the gap between messages
		return
		tstf	bitcount	;Check that no line events have happened
		pcall	Temperature	;Do one temperature measurement step
		iorlw	0		;Check if measurement is complete
		skpnz
		bcf	Intermission	;No more steps to be done
		return
gpio_home	comf	PORTA,W		;Get the inverted state of PORT A
		goto	gpio_security
gpio_away	movfw	PORTA		;Get the normal state of PORT A
gpio_security	andwf	gpio_mask,W	;Apply the GPIO port mask
		andlw	b'11000000'	;Clear the irrelevant bits
		skpz
		goto	gpio_armed	;Security system is armed
		btfss	gpioaway	;Did the port change since last check?
		return			;Nothing to do
		bcf	gpioaway	;Security system is disarmed
		clrf	float1		;Cancel the remote setpoint
		clrf	float2
		goto	gpio_setpoint	;W is 0
gpio_armed	btfsc	gpioaway	;Did the port change since last check?
		return			;Nothing to do
		bsf	gpioaway	;Security system is armed
		movlw	AwaySetpoint	;EEPROM address of the away setpoint
		lcall	ReadEpromData	;Get the setpoint from EEPROM
		movwf	float1		;Configure the remote setpoint
		call	ReadEpromNext	;Get the setpoint from EEPROM
		movwf	float2		;Configure the remote setpoint
		movlw	b'01'		;Temperature override mode: Continue
gpio_setpoint	pcall	SetSetPoint	;Set setpoint
		return
gpio_block	comf	PORTA,W		;Get the inverted state of PORT A
		andwf	gpio_mask,W	;Apply the GPIO port mask
		andlw	b'11000000'	;Clear the irrelevant bits
		skpz			;Do not block DHW?
		movlw	-1		;Block DHW
		xorwf	statusflags,W	;Compare with the current state
		andlw	b'01000000'	;Mask off the DHWBlocking bit
		xorwf	statusflags,F	;Update the current state
		return

;########################################################################

;Initialize EEPROM data memory

		org	0xf000
SavedSettings	de	23 | 1 << 5 | 1 << 6	;Set IgnoreErr1 and OverrideHigh
FunctionGPIO	de	GPIO_NONE | GPIO_NONE << 4
AwaySetpoint	de	16, 0
PrintSettingL	de	"L="
FunctionLED1	de	'F'
FunctionLED2	de	'X'
FunctionLED3	de	'O'
FunctionLED4	de	'M'
FunctionLED5	de	'P'
FunctionLED6	de	'C'
NullString	de	0
TStatModel	de	0
Configuration	de	0
DHWSetting	de	0, 0	;Must immediately follow Configuration
MaxCHSetting	de	0, 0	;Must immediately follow DHWSetting
MessageInterval	de	200

PrintSettingA	de	"A="
GreetingStr	de	"OpenTherm Gateway ", version
#ifdef __DEBUG
		de	phase, patch, ".", build
#else
	#ifdef patch
		de	phase, patch
	#ifdef bugfix
		de	'.', bugfix
	#endif
	#endif
#endif
		de	0
TimeoutStr	de	"WDT reset!", 0
TDisconnect	de	"Thermostat disconnected", 0
TConnect	de	"Thermostat connected", 0
CommandNG	de	"NG", 0
SyntaxError	de	"SE", 0
NoSpaceLeft	de	"NS", 0
BadValue	de	"BV", 0
NotFound	de	"NF", 0
OutOfRange	de	"OR", 0
OverrunError	de	"OE", 0
MediumPowerStr	de	"Medium", 0
HighPowerStr	de	"High", 0
NormalPowerStr	de	"Low"
PowerStr	de	" power", 0
ErrorStr	de	"Error ", 0
PrintSettingB	de	"B="
TimeStamp	de	tstamp, 0
PrintSettingC	de	"C="
SpeedString	de	mhzstr, " MHz", 0
ResetReasonStr	de	'W'	;Watchdog timer time-out
		de	'B'	;Brown out (supply voltage dropped below 2.7V)
		de	'P'	;Power on
		de	'S'	;BREAK condition on the serial interface
		de	'C'	;By command (GW=R)
		de	'O'	;Stack overflow
		de	'U'	;Stack underflow
		de	'L'	;Stuck in a loop (same message for 64 sec)
		de	'E'	;External reset (using the reset button)

		org	0xf0D0
UnknownFlags	de	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		org	0xf0E0
AlternativeCmd	de	116,117,118,119,120,121,122,123
		de	0,0,0,0,0,0,0,0
		de	0,0,0,0,0,0,0,0
		de	0,0,0,0,0,0,0,0
		end
