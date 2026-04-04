		title		"Opentherm Gateway Diagnostics"
		list		p=16F1847, b=8, r=dec

;Copyright (c) 2022 - Schelte Bron

;This firmware can perform several tests for analyzing different problems with
;the Opentherm Gateway hardware. The following tests are currently available:
;1) LED test. The four LEDS will each light in turn, creating a scanning effect
;2) Measure and report pulse widths on the Opentherm Master interface
;3) Measure and report pulse widths on the Opentherm Slave interface
;4) Loop delay test. This test requires the two Opentherm interfaces to be
;   connected together.
;5) Measure and report voltages detected on the Opentherm interfaces
;6) Measure periods of no activity on the opentherm lines.
;
#define		version		"2.1.1"

		__config	H'8007', B'00101111111100'
		__config	H'8008', B'11101011111111'

		errorlevel	-302

#include "p16f1847.inc"

#define		RXD	PORTB,2
#define		TSTAT	PORTB,3

#define		EOS	26	;^Z
#define		ANALOG0	0 << 2
#define		ANALOG1	1 << 2
#define		ANALOG2	2 << 2
#define		DACVREF	30 << 2
#define		FVRBUF1	31 << 2

		constant V_SHORT=6	;0.8V * 4.7 / 37.7 * 128 / 2.048
		constant V_LOW=86	;11V * 4.7 / 37.7 * 128 / 2.048
		constant V_OPEN=156	;20V * 4.7 / 37.7 * 128 / 2.048

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

		udata_shr
byte1		res	1
byte2		res	1
byte3		res	1
byte4		res	1

tstatflags	res	1
#define		NoThermostat	tstatflags,0
#define		OneSecond	tstatflags,1
#define		NextBit		tstatflags,3	;Must match SlaveOut
ccpltemp	res	1
ccphtemp	res	1

Bank0data	udata
testnum		res	1
flags		res	1	;Flags to be used by different tests

txtemp		res	1
txhead		res	1
txtail		res	1

loopcounter	res	1
temp		res	1
oldvalueh	res	1
oldvaluel	res	1
newvalueh	res	1
newvaluel	res	1
bcdbuffer	res	4		;A 24-bit binary fits in 8 BCD digits

counter		res	1
valuel		res	1
valueh		res	1
valueu		res	1
valuex		res	1
accual		res	1
accuah		res	1
accuau		res	1
accubl		res	1
accubh		res	1
accubu		res	1
supplyl		res	1	;Power supply voltage
supplyh		res	1

seccount	res	1

quarter		res	1

Bank1data	udata
LineVoltage	res	1	;The measured voltage on the thermostat input
VoltShort	res	1	;Threshold for shorted line
VoltLow		res	1	;Threshold for low logical level
VoltOpen	res	1	;Threshold for open line

Bank2data	udata
input		res	80

Bank3data	udata
levels		res	0
levels1		res	16
levels2		res	16

Bank6data	udata
ccpsavel	res	1
ccpsaveh	res	1

;Variables used by test 1
testdata	udata_ovr
ledcounter	res	1
leds		res	1

;Variables used by test 2 & 3
testdata	udata_ovr
compsave	res	1
compmask	res	1
bufferhead	res	1
buffertail	res	1
#define		avail		flags,0
#define		measure		flags,1
#define		leadingzero	flags,2
#define		printpending	flags,3
#define		decimaldot	flags,4

;Variables used by test 5
testdata	udata_ovr
prevvalue	res	1
loopcounter2	res	1
default		res	1
reflevel	res	1
lowerindex	res	1
upperindex	res	1
lowersave	res	1
uppersave	res	1
gaph		res	1
gapl		res	1

;Variables used by test 6
testdata	udata_ovr
idleflags	res	1
millisecs1	res	1
millisecs2	res	1
#define		IdleComparator	idleflags,7
#define		IdleDataAvail	idleflags,6
#define		IdleSubsequent	idleflags,5

;The linker has trouble assigning linear memory space, so we hack it
;Linear memory address range is 0x2000 - 0x23EF
linear0		udata	0x2200		;32 bytes into bank 6
txbuffer	res	0		;Serial transmit buffer (256 bytes)
linear1		udata	0x2300		;48 bytes into bank 9
buffer		res	0		;Space for 120 transitions, 2 bytes each

#define		SlaveOut	PORTA,3
#define		SlaveMask	b'00001000'

		extern	SelfProg
		global	Voltages, BitLenMaster

ResetVector	code	0x0000
		pagesel	SelfProg
		call	SelfProg	;Always allow a firmware update on boot
		pagesel	Start
		goto	Start		;Start the opentherm measure program

InterruptVector	code	0x0004
		pagesel	MasterInt
		banksel	testnum		;Bank 0
		movfw	testnum
		brw
		retfie
		retfie
		bra	test2interrupt	;Measure pulses on the Master interface
		bra	test3interrupt	;Measure pulses on the Slave interface
		retfie
		bra	test5interrupt	;Measure voltage levels
		retfie

test2interrupt	btfsc	PIR3,CCP3IF
		call	MasterInt
		retfie
test3interrupt	btfsc	PIR3,CCP4IF
		call	SlaveInt
test5interrupt	btfsc	PIR1,TMR2IF
		call	timer2int
		retfie

MasterInt	bcf	PIR3,CCP3IF
		banksel	CCPR3L		;Bank 6
		movfw	CCPR3L
		movwf	ccpltemp
		movfw	CCPR3H
		movwf	ccphtemp
		movlw	b'1'
		xorwf	CCP3CON,F
StoreCapture
		banksel	bufferhead	;Bank 0
		clrf	TMR0
		movlw	high buffer
		movwf	FSR0H
		lslf	bufferhead,W
		movwf	FSR0L
		sublw	240
		skpz
		skpc
		return
		incf	bufferhead,F
		movfw	ccpltemp
		movwi	FSR0++
		movfw	ccphtemp
		movwi	FSR0++
		return

SlaveInt	bcf	PIR3,CCP4IF
		banksel	CCPR4L		;Bank 6
		movfw	CCPR4L
		movwf	ccpltemp
		movfw	CCPR4H
		movwf	ccphtemp
		movlw	b'1'
		xorwf	CCP4CON,F
		bra	StoreCapture

timer2int	bcf	PIR1,TMR2IF	;Clear the interrupt flag
		tstf	quarter
		skpnz			;Message in progress?
		return
		decf	quarter,F	;Update the 1/4 ms counter
		skpnz
		bcf	T2CON,TMR2ON	;Stop the timer
		btfsc	quarter,0
		return			;Nothing to do in a stable interval
		movfw	tstatflags	;Get the desired output level
		xorwf	PORTA,W		;Compare against the current port level
		andlw	SlaveMask	;Only apply for the relevant output
		xorwf	PORTA,F		;Update the output port
		movlw	SlaveMask
		xorwf	tstatflags,F	;Invert the level for next time
		btfsc	quarter,1
		return
		bcf	NextBit		;Start with assumption next bit is 0
		setc			;Prepare to shift in a stop bit
		rlf	byte4,F		;Shift the full 32 bit message buffer
		rlf	byte3,F		;Shift bits 8 through 15
		rlf	byte2,F		;Shift bits 16 through 23
		rlf	byte1,F		;Shift bits 24 through 31
		skpc			;Test the bit shifted out of the buffer
		bsf	NextBit		;Next time, send a logical 1
		return

Main		code
Start
		banksel	OSCCON		;Bank 1
		movlw	b'01101010'	;Internal oscillator set to 4MHz
		movwf	OSCCON		;Configure the oscillator

		;Configure digital I/O
		banksel	TRISA		;Bank 1
		movlw	b'11100111'
		movwf	TRISA
		movlw	b'00100111'
		movwf	TRISB

		;Configure analog I/O
		banksel	ANSELA
		movlw	b'00000111'
		movwf	ANSELA
		clrf	ANSELB

		;Configure fixed voltage reference
		banksel	FVRCON		;Bank 2
		movlw	b'10001010'	;Buf1 = 2.048V, Buf2 = 2.048V
		movwf	FVRCON
		;Configure the digital to analog converter
		movlw	b'10001000'	;Vsrc- = Vss, Vsrc+ = FVR Buf2
		movwf	DACCON0
		movlw	23		;2.048 * 23 / 32 = 1.472V
		movwf	DACCON1		;Set the reference voltage

		;Configure comparators
		clrf	tstatflags
		call	SetupCompModule

		;Configure A/D converter
		banksel	ADCON0		;Bank 1
		movlw	b'00010000'	;Left justified, Fosc/8, Vss, Vdd
		movwf	ADCON1
		movlw	b'01111101'	;A/D on, channel 31 - FVR buffer 1
		movwf	ADCON0

		;Configure the serial interface
		banksel	SPBRGL		;Bank 3
		movlw	25
		movwf	SPBRGL
		bsf	TXSTA,BRGH	;9600 baud
		bcf	TXSTA,SYNC	;Asynchronous
		bsf	TXSTA,TXEN	;Enable transmit
		bsf	RCSTA,SPEN	;Enable serial port
		bsf	RCSTA,CREN	;Enable receive

		banksel	ADCON0		;Bank 1
		bsf	ADCON0,GO	;Measure FVR buffer 1
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

		banksel	T1CON		;Bank 0
		clrf	T1CON
		clrf	txhead
		clrf	txtail

#define		MAXTEST	6

MainLoop	bcf	INTCON,GIE	;Disable interrupts
		;Configure timer 0
		banksel	OPTION_REG	;Bank 1
		movlw	b'11010101'	;1:64 prescaler
		movwf	OPTION_REG
		clrf	PIE1		;Disable all interrupt sources
		clrf	PIE2
		clrf	PIE2
		;Configure AD converter
		movlw	b'00010000'	;Left justified, Fosc/8, Vss, Vdd
		movwf	ADCON1
		movlw	b'00000001'	;A/D on, channel 0 - pin AN0
		movwf	ADCON0
		;Configure comparators
		call	SetupCompModule	;Bank 0
		;Stop timers (TMR0 can't be stopped)
		clrf	T1CON
		clrf	T2CON

		btfsc	NoThermostat
		bcf	TSTAT		;Indicate absence of thermostat

		clrf	seccount
		bcf	SlaveOut	;Prevent low-voltage heat demand

		;Display the menu
		movlw	Banner
		call	Print
		movlw	Prompt
		call	Print
		banksel	RCSTA		;Bank 3
		btfsc	RCSTA,OERR
		call	ClearOverrunErr
		movlb	0		;Bank 0
		call	GetString
		skpnc
		bra	BadTest
		movwf	testnum
		sublw	MAXTEST
		sublw	MAXTEST
		skpc
		goto	BadTest
		call	RunTest
		goto	MainLoop

BadTest		btfsc	RCSTA,FERR
		goto	Break
		movlw	InvalidTestStr
		call	Print
		goto	MainLoop

		;Clear overrun error
ClearOverrunErr	movfw	RCREG
		bcf	RCSTA,CREN
		bsf	RCSTA,CREN
		return

GetString	movlw	high input
		movwf	FSR0H
		movlw	low input
		movwf	FSR0L
GetStringLoop	clrwdt
		call	IdleTasks	;Perform standard tasks
		btfsc	PIR1,ADIF
		call	CheckThermostat
		btfsc	INTCON,TMR0IF
		call	IdleTimer
		btfss	PIR1,RCIF
		bra	GetStringLoop
		banksel	RCSTA		;Bank 3
		btfss	RCSTA,FERR
		bra	ReceivedChar
		movfw	RCREG
		movlb	0		;Bank 0
		skpz			;A BREAK condition occurred?
		bra	GetStringLoop	;Discard bad character
Break		clrwdt
		btfss	RXD		;Check the serial receive line
		bra	Break		;Wait for the BREAK condition to end
		reset

ReceivedChar	movfw	RCREG		;Get the received character
		movwf	INDF0
		movlb	0		;Bank 0
		andlw	~b'11111'
		skpnz
		bra	ControlChar
		movlw	input +	79
		subwf	FSR0L,W
		skpnc
		bra	GetStringLoop
		moviw	FSR0++
		call	PrintChar
		bra	GetStringLoop
ControlChar	movfw	INDF0
		xorlw	13		;Enter
		skpnz
		bra	Enter
		xorlw	13 ^ 8		;Backspace
		skpnz
		bra	BackSpace
		goto	GetStringLoop

BackSpace	movlw	low input
		subwf	FSR0L,W
		skpnz
		bra	GetStringLoop
		decf	FSR0L,F
		movlw	'\b'
		call	PrintChar
		call	PrintChar
		movlw	'\b'
		call	PrintChar
		bra	GetStringLoop

Enter		call	PrintNewline
		clrf	INDF0
		movlw	low input
		movwf	FSR0L
		clrf	temp
		setc
NumberLoop	movfw	temp
		tstf	INDF0
		skpnz
		return
		addlw	-26
		skpnc
		bra	OutOfRange
		; Multiply testnumber by 10
		movfw	temp		;1 x testnum
		addwf	temp,F		;2 x testnum
		rlf	temp,F		;4 x testnum
		addwf	temp,F		;5 x testnum
		rlf	temp,F		;10 x testnum
		; --
		moviw	FSR0++
		sublw	'9'
		sublw	'9' - '0'
		skpc
		bra	BadNumber
		addwf	temp,F
		skpc
		bra	NumberLoop
OutOfRange	clrz			;Entry invalid
		retlw	1		;Number out of range
BadNumber	setc			;No valid number entered
		retlw	2		;Not a number

IdleTasks	btfsc	printpending	;No characters waiting?
		btfss	PIR1,TXIF	;Transmit register empty?
		return
		goto	PrintFlush

IdleTimer	bcf	INTCON,TMR0IF	;Clear the interrupt flag
		banksel	ADCON0		;Bank 1
		bsf	ADCON0,GO	;Start A/D conversion
		movlb	0
		return

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

CheckThermostat	bcf	PIR1,ADIF
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
		bra	TCheckReturn	;Unstable line voltage
		movfw	LineVoltage	;Get the A/D result again
		subwf	VoltOpen,W	;Line open?
		skpc
		bra	OpenCircuit	;No opentherm thermostat connected
		movfw	LineVoltage	;Get the A/D result again
		subwf	VoltLow,W	;Check logical level
		skpnc			;Low level?
		btfss	NoThermostat
		bra	TCheckReturn
		bcf	NoThermostat	;Thermostat was reconnected
		bra	SetupCompModule
OpenCircuit	btfsc	NoThermostat
		bra	TCheckReturn
		bsf	NoThermostat	;Thermostat has been disconnected
SetupCompModule	banksel	CM1CON0		;Bank 2
		;Bit 7 CxINTP = 0 - No interrupt on positive edge
		;Bit 6 CxINTN = 0 - No interrupt on negative edge
		;Bit 5-4 = 1 - CxVP connects to DAC voltage reference
		;Bit 3-2 Unimplemented
		;Bit 1-0 = 0 - C12IN0- (RA0) / 1 - C12IN1- (RA1)
		movlw	b'00010000'
		movwf	CM1CON1
		movlw	b'00010001'
		movwf	CM2CON1
		movlw	MONITORMODE	;Comparator setting for monitor mode
		movwf	CM2CON0		;Configure comparator 2
		btfsc	NoThermostat	;Thermostat connected?
		movlw	GATEWAYMODE	;Comparator setting for gateway mode
		movwf	CM1CON0		;Configure comparator 1
TCheckReturn
		movlb	0		;Bank 0
		return

MsgGenerator	bsf	SlaveOut	;Make opentherm line low
		movlw	249		;Timer overflows every 250 microseconds
		movwf	PR2		;Set up period register
		banksel	PIE1		;Bank 1
		bsf	PIE1,TMR2IE	;Enable timer 2 interrupts
		banksel	PIR1		;Bank 0
		bcf	PIR1,TMR2IF	;Forget any old overflows
		movlw	2		;Between 131 and 196 milliseconds of
		movwf	seccount	; idle time before the first message
		return

MsgHandler	btfsc	NoThermostat
		btfss	INTCON,TMR0IF
		return
		bcf	INTCON,TMR0IF
		tstf	seccount
		skpz
		decfsz	seccount,F
		return
		movlw	15
		movwf	seccount
		;T80190000
		movlw	0x80		;Read-Data and parity bit
		movwf	byte1
		movlw	0x19		;MsgID 25: Boiler water temperature
		movwf	byte2
		clrf	byte3
		clrf	byte4
		movlw	34 * 4		;Number of quarter bits
		movwf	quarter
		bsf	T2CON,TMR2ON	;Start timer 2
		return

RunTest		brw
; Test 0: Reset the gateway to allow starting a firmware update
		reset
; Test 1: LED test. Light up the LEDS one by one until a CR is received on the
; serial interface.
		goto	LEDTest
; Test 2: Bit lengths master interface. Report bit lengths of the opentherm
; message received on the master interface.
		goto	BitLenMaster
; Test 3: Bit lengths slave interface. Report bit lengths of the opentherm
; message received on the slave interface.
		goto	BitLenSlave
; Test 4: Opto-coupler delay. Report the delays introduced by the opto-coupler.
; The delays for both transitions should be as close to eachother as possible.
; For this test the master and slave interface must be connected together.
		goto	LoopDelay
; Test 5: Measure various voltages
		goto	Voltages
; Test 6: Idle periods
		goto	IdlePeriods

;Test 1:
LEDTest		clrf	leds
		movlw	1
		movwf	ledcounter
		banksel	OPTION_REG	;Bank 1
		movlw	b'11010111'	;1:256 prescaler
		movwf	OPTION_REG	;Make timer 0 run as slow as possible
		banksel	ledcounter	;Bank 0
TestLoop1	clrwdt
		call	CheckReturn
		skpnz
		goto	LEDTestEnd
		btfss	INTCON,TMR0IF
		goto	TestLoop1
		bcf	INTCON,TMR0IF
		decfsz	ledcounter,F
		goto	TestLoop1
		movlw	1
		movwf	ledcounter
		clrc
		rlf	leds,F
		tstf	leds
		skpnz
		bsf	leds,3
		btfsc	leds,5
		rlf	leds,F
		movlw	b'11011000'
		iorwf	PORTB,F
		comf	leds,W
		andwf	PORTB,F
		goto	TestLoop1
LEDTestEnd	movlw	b'11011000'
		iorwf	PORTB,F
		return

;Test 2 & 3: Measure the length of the individual pulses and report them over
;the serial interface.
;Take advantage of the fact that the comparator outputs match the CCP3 and
;CCP4 pins. Using a CCP module in capture mode takes care of copying the TMR1
;registers without having to deal with possible overflows between reading
;TMR1L and TMR1H. The data sheet is vague about what happens when the CCP pin
;is configured as an output, but in practice it works just fine.

;Test 2
BitLenMaster	call	BitLenInit
		movwf	CCP3CON		;Capture mode: every falling edge
		banksel	PIR3		;Bank 0
		bcf	PIR3,CCP3IF
		banksel	PIE3		;Bank 1
		bsf	PIE3,CCP3IE
		bra	BitLengths

;Test 3
BitLenSlave	call	BitLenInit
		movwf	CCP4CON		;Capture mode: every falling edge
		banksel	PIR3		;Bank 0
		bcf	PIR3,CCP4IF
		btfsc	NoThermostat	;Thermostat connected?
		call	MsgGenerator	;Generate messages to the boiler
		banksel	PIE3		;Bank 1
		bsf	PIE3,CCP4IE

BitLengths	movlw	high buffer
		movwf	FSR0H
		banksel	OPTION_REG	;Bank 1
		movlw	b'11010111'	;1:256 prescaler
		movwf	OPTION_REG	;Make timer 0 run as slow as possible
		banksel	bufferhead	;Bank 0
		bsf	INTCON,PEIE	;Enable peripheral interrupts
		bsf	INTCON,GIE	;Enable all interrupts
		clrf	accuau
BitLenStart	clrf	bufferhead
		clrf	FSR0L
BitLenLoop	clrwdt
		call	IdleTasks
		call	MsgHandler
		call	CheckReturn
		skpnz
		bra	BitLenCleanUp
		lslf	bufferhead,W
		skpnz
		bra	BitLenLoop	;No data has been collected
		tstf	FSR0L
		skpnz
		bra	BitLenStore	;First transition was found
		btfsc	INTCON,TMR0IF
		bra	BitLenNewline	;Timeout waiting for a transition
		subwf	FSR0L,W
		skpnz
		bra	BitLenLoop	;No unprocessed events
		movfw	FSR0L
		xorlw	2
		movlw	','
		skpz			;Do not print a comma at the start
		call	PrintChar
		movfw	accubl
		subwf	INDF0,W
		movwf	accual
		moviw	1[FSR0]
		movwf	accuah
		movfw	accubh
		subwfb	accuah,F
		call	PrintDecimal
BitLenStore	bcf	INTCON,TMR0IF
		moviw	FSR0++
		movwf	accubl
		moviw	FSR0++
		movwf	accubh
		bra	BitLenLoop
BitLenNewline	movlw	'.'
		call	PrintChar
		call	PrintNewline
		bra	BitLenStart

BitLenInit	movlw	b'00000001'	;On, Fosc/4, 1:1 Prescaler
		movwf	T1CON		;Configure timer 1
		clrf	bufferhead
		banksel	OPTION_REG	;Bank 1
		movlw	b'11010101'	;Fosc/4, 1:64 Prescaler
		movfw	OPTION_REG
		banksel	CCP3CON		;Bank 6
		retlw	b'0100'		;Capture mode: every falling edge

BitLenCleanUp
		banksel	PIE3		;Bank 1
		clrf	PIE3
		banksel	CCP3CON		;Bank 6
		clrf	CCP3CON
		clrf	CCP4CON
		banksel	PIR3		;Bank 0
		return

;Test 4:
LoopDelay
		banksel	CM1CON0		;Bnak 2
		bcf	CM1CON0,C1OE	;Disconnect comparator 1 output
		bcf	CM2CON0,C2OE	;Disconnect comparator 2 output
		banksel	PORTA		;Bank 0
		;Set both interfaces to their high level. This situation does
		;not normally occur, so it's a fairly good way to check if the
		;interfaces have been looped together.
		bcf	PORTA,3
		bcf	PORTA,4
		;Use timer 1 as a guard timer. If the inputs haven't reached
		;their expected levels after 65ms something is wrong.
		clrf	T1CON
		clrf	TMR1L
		clrf	TMR1H		;Reset timer 1
		bcf	PIR1,TMR1IF
		bsf	T1CON,TMR1ON	;Start timer 1
LoopDelayWait	clrwdt
		btfsc	PIR1,TMR1IF	;Check for timeout
		bra	NoLoop
		banksel	CMOUT		;Bank 2
		tstf	CMOUT
		banksel	0		;Bank 0
		skpz
		bra	LoopDelayWait	;Repeat
		movlw	Symmetry0Str
		call	Print
		movlw	b'00000010'	;Gate is active low, Comparator 1
		call	Check		;Test OK1A high to low transition
		movlw	Symmetry1Str
		call	Print
		movlw	b'01000010'	;Gate is active high, Comparator 1
		call	Check		;Test OK1A low to high transition
		movlw	Symmetry2Str
		call	Print
		movlw	b'00000011'	;Gate is active low, Comparator 2
		call	Check		;Test OK1B high to low transition
		movlw	Symmetry3Str
		call	Print
		movlw	b'01000011'	;Gate is active high, Comparator 2
		call	Check		;Test OK1B low to hight transition
LoopCleanUp	clrf	T1CON
		clrf	T1GCON
		return
NoLoop		movlw	NoLoopError
		call	Print
		bra	LoopCleanUp

Check		clrf	T1CON
		clrf	TMR1L
		clrf	TMR1H
		iorlw	b'10000000'	;Enable timer1 gate
		movwf	T1GCON		;Set up timer 1 gate control
		movlw	b'01000000'	;Fosc
		movwf	T1CON
		movlw	1 << RA3
		btfsc	T1GCON,0
		movlw	1 << RA4
		bcf	PIR1,TMR1IF
		bsf	T1CON,TMR1ON	;Start the timer
		xorwf	PORTA,F		;Switch the output
DelayLoop	clrwdt
		btfsc	PIR1,TMR1IF	;Check for timer overflow
		goto	NoResponse
		btfsc	T1GCON,T1GVAL
		goto	DelayLoop
		lsrf	TMR1H,W
		movwf	accuah
		rrf	TMR1L,W
		movwf	accual
		lsrf	accuah,F
		rrf	accual,F
		clrf	accuau
		call	PrintDecimal
		movlw	'.'
		call	PrintChar
		movfw	TMR1L
		andlw	b'11'
		lslf	WREG,W
		btfsc	TMR1L,1
		iorlw	b'1'
		addlw	'0'
		call	PrintChar
		movlw	'0'
		btfsc	TMR1L,0
		movlw	'5'
		call	PrintChar
		movlw	MicroSecStr
		call	Print
		return

NoResponse	bcf	T1CON,TMR1ON
BadResponse	movlw	DelayLoopError
		call	Print
		return

; Test 5
;Measure FVR1 with NREF = Vss & PREF = Vdd to determine Vdd.
;Vdd = FVR2 * 4095 / ADRES
Voltages	btfsc	NoThermostat	;Thermostat connected?
		call	MsgGenerator	;Generate messages to the boiler
		;Enable interrupts
		bsf	INTCON,PEIE
		bsf	INTCON,GIE
		banksel	ADCON0		;Bank 1
		movlw	b'10010000'	;Right justified, Fosc/8
		movwf	ADCON1		;Configure the AD converter
		movlw	FVRBUF1		;Select FVR 1 (2.048V)
		call	SelectADChannel
;Calculate 2048 * 1024 / AccuB
		movlw	upper (2048 * 1024)
		movwf	valueu		;Load value
		clrf	valueh
		clrf	valuel
		call	AnalogValue	;Get measurement in accu B
		call	divide		;Calculate value / accub
		movlw	PowerStr
		call	Print
		;Save the measured supply voltage for future calculations
		movfw	accual
		movwf	supplyl
		movfw	accuah
		movwf	supplyh

		call	PrintDotted
		call	PrintNewline

		movlw	DACVREF
		call	SelectADChannel
		movlw	Analog2Str
		call	Print
		call	AnalogValue
;Calculate Supply * AccuB / 1024
		call	multiplysupply	;Supply * AccuB => Value
		call	divideby1024	;Value / 1024 => AccuA
		call	PrintDotted
		call	PrintNewline

		movlw	Analog0Str
		call	Print
		movlw	ANALOG0
		call	SelectADChannel
		movlw	levels1
		call	FindLevels	;Find all stable voltage levels
		movlw	Analog1Str
		call	Print
		movlw	ANALOG1
		call	SelectADChannel
		movlw	levels2
		call	FindLevels	;Find all stable voltage levels
		movlw	VoltRefStr
		call	Print
		call	FindVoltRef
		movwf	default
		call	PrintDigit
		movlw	VoltRefPrompt
		call	Print
		call	GetString
		skpz			;No error
		bra	BadVoltRef
		skpnc
		movfw	default
		sublw	9		;Check for value 0..9
		sublw	9
		skpc
		bra	BadVoltRef
SetVoltRef	lslf	WREG,W		;Multiply by 2
		addlw	13		;Minimum level is 0.832V
		banksel	DACCON1		;Bank 2
		movwf	DACCON1
		movlb	0		;Bank 0
		goto	MainLoop
BadVoltRef	movlw	InvalidValue
		call	Print		
		goto	MainLoop

SelectADChannel	;Common ADC configuration
		banksel	ADCON0		;Bank 1
		iorlw	1 << ADON
		movwf	ADCON0		;Select the A/D channel
		banksel	0		;Bank 0
		return

;The required acquisition time @20 degrees is less than 4us.
;Assume enough time has elapsed due to subroutine call overhead.
AnalogValue
		banksel	ADCON0		;Bank 1
		bsf	ADCON0,GO	;Start the conversion
Conversion	btfsc	ADCON0,GO
		goto	Conversion	;Wait for conversion to complete
		movfw	ADRESL
		banksel	accubl		;Bank 0
		movwf	accubl
		banksel	ADRESH		;Bank 1
		movfw	ADRESH
		banksel	accubh		;Bank 0
		movwf	accubh
		clrf	accubu
		return

FindLevels	movwf	FSR0L
		movlw	high levels
		movwf	FSR0H
		movlw	16
		movwf	loopcounter
		movlw	-1
FindLevelInit	movwi	FSR0++
		decfsz	loopcounter,F
		goto	FindLevelInit
		addfsr	FSR0,-16
		movlw	64
		movwf	loopcounter2
		banksel	ADCON1
		bcf	ADCON1,ADFM	;Left justify the result
		clrf	temp
		call	AnalogValue	;Read analog value
FindLevelLoop1	clrwdt
		call	IdleTasks
		call	MsgHandler
		movfw	accubh
		movwf	prevvalue	;Remember value for next round
		call	AnalogValue	;Read analog value
		movfw	accubh
		subwf	prevvalue,W	;Compare with value on last round
		skpc
		sublw	0
		sublw	2		;Difference must be less than 2
		skpc
		clrf	temp
		incf	temp,F
		btfss	temp,3		;Need 4 similar values
		goto	FindLevelNext1
		swapf	accubh,W	;Found a stable value
		andlw	b'1111'
		addwf	FSR0L,F
		swapf	accubh,W
		andlw	b'11110000'
		movwf	INDF0
		swapf	accubl,W
		iorwf	INDF0,F
		movlw	b'11110000'
		andwf	FSR0L,F		;Reset FSR to the start of the array
FindLevelNext1	call	MsgHandler
		decfsz	loopcounter,F
		goto	FindLevelLoop1
		decfsz	loopcounter2,F
		goto	FindLevelLoop1
		; Report the levels found
		movlw	':'
		movwf	temp
		movlw	16
		movwf	loopcounter2
FindLevelLoop2	btfsc	INDF0,0
		goto	FindLevelNext2
		movfw	temp
		call	PrintChar
		call	PrintChar
		movfw	FSR0L
		andlw	b'1111'
		lsrf	WREG,W
		movwf	accubh
		rrf	INDF0,W
		movwf	accubl
		lsrf	accubh,F
		rrf	accubl,F
;Calculate Supply * AccuB / 1024
		call	multiplysupply	;Supply * AccuB => Value
		call	divideby1024	;Value / 1024 => AccuA
		call	PrintDotted
		movlw	','
		movwf	temp
FindLevelNext2	incf	FSR0L,F
		decfsz	loopcounter2,F
		goto	FindLevelLoop2
		call	PrintNewline
		return

;Calculate the difference between 2 levels, W and reflevel
LevelGap	movwf	valueh		;Save high byte
		addlw	levels
		movwf	FSR0L
		movfw	INDF0
		movwf	valuel		;Save low byte
		movfw	reflevel
		addlw	levels
		movwf	FSR0L
		movfw	INDF0
		movfw	INDF0		;Low byte to be subtracted
		subwf	valuel,F	;Subtract the low byte
		movfw	reflevel	;High byte to be subtracted
		subwfb	valueh,F	;Subtract the high byte
		return

;Calculate the difference between two gaps
LevelDiff	movfw	gapl
		subwf	valuel,F
		movfw	gaph
		subwfb	valueh,F
		return

;Find the largest gap
FindVoltRef	clrf	loopcounter	;Start at slot 0
FindVoltRefMain	movlw	-1
		movwf	lowerindex	;No lower index found yet
		movwf	upperindex	;No upper index found yet
		movwf	reflevel	;No reference level yet
		clrf	gaph
		clrf	gapl
FindVoltRefLoop	movfw	loopcounter	;Index of the slot to check
		addlw	levels
		movwf	FSR0L		;Set up indirect addressing
		btfsc	INDF0,0		;Empty slot?
		goto	FindVoltRefNext
		btfsc	reflevel,7	;Previous levels found?
		goto	FindVoltRefJump
		movfw	loopcounter
		call	LevelGap	;Calculate the gap
		call	LevelDiff	;Get the difference between the gaps
		skpc			;New gap is bigger?
		goto	FindVoltRefSkip
		movfw	valuel
		addwf	gapl,F
		movfw	valueh
		addwfc	gaph,F
FindVoltRefJump	movfw	reflevel
		movwf	lowerindex
		movfw	loopcounter
		movwf	upperindex
FindVoltRefSkip	movfw	loopcounter
		movwf	reflevel
FindVoltRefNext	movlw	1
		addwf	loopcounter,F
		skpdc			;16 levels done?
		goto	FindVoltRefLoop

		;Only when supply is 3.3V?
		incf	lowerindex,W
		subwf	upperindex,W
		skpnz
		bsf	lowerindex,7	;Ignore levels that are close together

		btfsc	loopcounter,5	;Less than 32 levels done?
		goto	FindVoltRefEnd
		movfw	lowerindex
		movwf	lowersave
		movfw	upperindex
		movwf	uppersave
		goto	FindVoltRefMain
FindVoltRefEnd	btfss	lowerindex,7	;Fewer than 2 levels for the boiler?
		goto	FindVoltRefJ1
		btfsc	lowersave,7	;Found 2 levels for the thermostat?
		retlw	5		;Stick to the default
		movfw	lowersave	;Use the thermostat levels
		movwf	lowerindex
		movfw	uppersave
		movwf	upperindex
		goto	FindVoltRefJ2
FindVoltRefJ1	btfsc	lowersave,7	;Found 2 levels for the thermostat?
		goto	FindVoltRefJ2	;Use the boiler levels
		;Find the highest lower index
		movfw	lowerindex
		subwf	lowersave,W
		skpndc
		addwf	lowerindex,F
		;Find the lowest upper index
		movfw	upperindex
		subwf	uppersave,W
		skpdc
		addwf	upperindex,F
		;Check that the upper index is higher than the lower index
		movfw	upperindex
		subwf	lowerindex,W
		skpnc			;Upper index > lower index?
		retlw	5		;Upper index <= lower index
FindVoltRefJ2	;Get the average of the upper and lower index values
		movfw	lowerindex
		movwf	accubh
		bcf	accubh,4
		addlw	levels
		movwf	FSR0L
		movfw	INDF0
		movwf	accubl
		movfw	upperindex
		addlw	levels
		movwf	FSR0L
		movfw	INDF0
		addwf	accubl,F
		movfw	upperindex
		andlw	b'1111'
		addwfc	accubh,F
		lsrf	accubh,F
		rrf	accubl,F
;Calculate the real voltage of the average value
		call	multiplysupply	;AccuA * AccuB => Value
		call	divideby1024	;Value / AccuB => AccuA
		lsrf	accuah,W
		sublw	15		;Maximum acceptable value
		sublw	15 - 6		;Range of acceptable values
		skpnc			;Value out of range?
		return			;Return the selected value
		retlw	5		;Return the default value

; Test 6
IdlePeriods	movlw	b'00000001'	;On, Fosc/4, 1:1 Prescaler
		movwf	T1CON		;Configure timer 1
		banksel	OPTION_REG	;Bank 1
		movlw	b'11010101'	;Fosc/4, 1:64 Prescaler
		movwf	OPTION_REG	;Configure timer 0
		movlw	high CCP3CON
		movwf	FSR0H
IdlePerLoop1
		banksel	CCP3CON		;Bank 6
		movlw	b'0100'		;Capture mode: every falling edge
		movwf	CCP3CON		;Configure CCP3 (thermostat)
		movwf	CCP4CON		;Configure CCP4 (boiler)
		banksel	PIR3		;Bank 0
		clrf	PIR3
		clrf	TMR0
		clrf	idleflags
IdlePerWait	clrwdt
		call	IdleTasks
		call	CheckReturn
		skpnz
		bra	IdlePerCleanUp
		movlw	1 << CCP3IF | 1 << CCP4IF
		andwf	PIR3,W		;Check if a CCP tripped
		skpnz
		bra	IdlePerSkip
		movwf	idleflags	;Save for later use
		xorwf	PIR3,F		;Clear the CCP interrupt flag
		clrf	TMR0		;Restart timer 0
		bcf	INTCON,TMR0IF
IdlePerSkip	tstf	idleflags
		skpz			;Waiting for first activity
		btfss	INTCON,TMR0IF	;Timer 0 overflow means lines are idle
		bra	IdlePerWait	;Wait longer
		movlw	low CCPR3H
		btfss	idleflags,CCP3IF
		movlw	low CCPR4H
		movwf	FSR0L
;Determine if timer 1 has overflowed since the capture. Timer 1 overflows
;every 65536 us. This point is reached a little after 16384 us since the
;last capture. So an overflow can be determined by comparing the MSBs of the
;captured value and the current timer value.
;The TMR1IF bit is used to count subsequent overflows. But because the timer
;is still running, the bit can possibly be set between clearing the flag and
;checking the timer value. To rectify this, the flag is cleared once more if
;an overflow has been found (as indicated by a cleared Z-bit).
		movlw	1		;Value in case of timer 1 overflow
		bcf	PIR1,TMR1IF	;Can't depend on the interrupt flag
		btfss	TMR1H,7		;Current MSB set: No timer overflow
		btfss	INDF0,7		;Captured MSB set: Timer overflow
		clrw			;No timer 1 overflow since the capture
		movwf	accuau		;Initialize upper byte
		skpz			;No overflow was found?
		bcf	PIR1,TMR1IF	;In case the flag was set at a bad time
		moviw	-1[FSR0]	;CCPRxL
		movwf	accual		;Save the last captured value low byte
		moviw	0[FSR0]		;CCPRxH
		movwf	accuah		;Save the last captured value high byte
		banksel	CCP3CON		;Bank 6
		movlw	b'1'
		xorwf	CCP3CON,F	;Start looking for a rising edge
		xorwf	CCP4CON,F	;Start looking for a rising edge
		banksel	PIR3		;Bank 0
		clrf	PIR3		;Clear the CCP interrupt flags
IdlePerLoop2	clrwdt
		call	IdleTasks
		call	CheckReturn
		skpnz
		bra	IdlePerCleanUp
		movlw	1 << CCP3IF | 1 << CCP4IF
		andwf	PIR3,W		;Check if a CCP tripped
		skpz
		bra	IdlePerBusy
		btfss	PIR1,TMR1IF
		bra	IdlePerLoop2
		incf	accuau,F
		bcf	PIR1,TMR1IF
		skpz
		bra	IdlePerLoop2
		bra	IdlePerWait	;Idle time exceeded maximum (16 sec)
IdlePerBusy	movwf	idleflags
		movlw	low CCPR3L
		btfss	idleflags,CCP3IF
		movlw	low CCPR4L
		movwf	FSR0L
		movfw	accual
		subwf	INDF0,W
		movwf	accual
		incf	FSR0L,F
		movfw	accuah
		subwfb	INDF0,W
		movwf	accuah
		skpc
		decf	accuau,F
		incf	accuau,F
		btfsc	INDF0,7
		btfss	PIR1,TMR1IF
		decf	accuau,F
		movlw	'T'
		btfss	idleflags,CCP3IF
		movlw	'B'
		call	PrintChar
		movlw	':'
		call	PrintChar
		call	PrintChar
		call	PrintDotted
		call	PrintNewline
		bra	IdlePerLoop1
IdlePerCleanUp	bcf	T1CON,TMR1ON	;Stop timer 1
		banksel	CCP3CON		;Bank 6
		clrf	CCP3CON
		clrf	CCP4CON
		banksel	PIR3		;Bank 0
		return

CheckReturn	clrz
		btfss	PIR1,RCIF
		return
		banksel	RCREG
		movfw	RCREG
		banksel	0
		sublw	'\r'
		return

PrintNewline	movlw	'\r'
		call	PrintChar
		movlw	'\n'
		goto	PrintChar

Print
		banksel	EEADRH		;Bank 3
		movwf	EEADRL
		movlw	high Strings
		movwf	EEADRH
PrintString
		banksel	EECON1		;Bank 3
		clrf	EECON1
		bsf	EECON1,EEPGD
		bsf	EECON1,RD
		nop
		nop
		rlf	EEDATL,W
		rlf	EEDATH,W
		call	PrintStrChar
		skpnz
		bra	PrintStrDone
		banksel	EEDATL		;Bank 3
		movfw	EEDATL
		call	PrintStrChar
		skpnz
		bra	PrintStrDone
		banksel	EEADRL		;Bank 3
		incf	EEADRL,F
		skpnz
		incf	EEADRH,F
		bra	PrintString
PrintStrDone
		movlb	0
		return

PrintStrChar	andlw	b'01111111'
		xorlw	EOS
		skpnz
		return
		xorlw	EOS
		skpz
		call	PrintChar
		clrz
		return

PrintDotted	bsf	decimaldot
PrintDecimal	movlw	24
		banksel	loopcounter
		movwf	loopcounter
		clrf	bcdbuffer
		clrf	bcdbuffer + 1
		clrf	bcdbuffer + 2
		clrf	bcdbuffer + 3
		movlw	high bcdbuffer
		movwf	FSR1H
		bra	bcdskip
bcdloop		movlw	low bcdbuffer
		movwf	FSR1L
		call	bcdadjust
		call	bcdadjust
		call	bcdadjust
		call	bcdadjust
bcdskip		lslf	accual,F
		rlf	accuah,F
		rlf	accuau,F
		rlf	bcdbuffer,F
		rlf	bcdbuffer + 1,F
		rlf	bcdbuffer + 2,F
		rlf	bcdbuffer + 3,F
		decfsz	loopcounter,F
		bra	bcdloop
		bsf	leadingzero		;Skip zeroes
		movfw	bcdbuffer + 3
		call	PrintPackedBCD
		movfw	bcdbuffer + 2
		call	PrintPackedBCD
		movfw	bcdbuffer + 1
		btfss	decimaldot
		call	PrintPackedBCD
		btfsc	decimaldot
		call	PrintDottedBCD
		movfw	bcdbuffer + 0
		call	PrintPackedBCD
		movlw	'0'
		btfsc	leadingzero		;Any digit printed?
		call	PrintChar
		return

bcdadjust	movlw	0x33
		addwf	INDF1,W
		btfss	WREG,3
		addlw	-0x03
		btfss	WREG,7
		addlw	-0x30
		movwi	FSR1++
		return

PrintDottedBCD	bcf	decimaldot
		bcf	leadingzero	;Always print the digit
		movwf	temp
		swapf	temp,W
		call	PrintBCD
		movlw	'.'
		call	PrintChar
		movfw	temp
		bra	PrintBCD
PrintPackedBCD	movwf	temp
		swapf	temp,W
		call	PrintBCD
		movfw	temp
PrintBCD	andlw	b'1111'
		skpnz
		btfss	leadingzero	;Skip leading zero?
		bra	PrintDigit
		return

PrintFloat	movfw	valueh
		call	PrintByte
		movlw	100
		movwf	temp
		movfw	valuel
;		call	Multiply
		btfsc	valuel,7
		incf	valueh,F
		movfw	valueh
		movwf	temp
		movlw	'.'
		goto	PrintFraction

PrintByte	bsf	leadingzero
		movwf	temp
		addlw	-100
		skpc
		goto	PrintByteJ1
		movwf	temp
		movlw	'1'
PrintFraction	call	PrintChar
		bcf	leadingzero
PrintByteJ1	clrf	loopcounter
		movfw	temp
PrintByteL1	movwf	temp
		addlw	-10
		skpc
		goto	PrintByteJ2
		incf	loopcounter,F
		goto	PrintByteL1
PrintByteJ2	movfw	loopcounter
		skpnz
		btfss	leadingzero
		call	PrintDigit
		movfw	temp
PrintDigit	bcf	leadingzero
		addlw	'0'

PrintChar
		banksel	PIR1		;Bank 0
		btfss	printpending
		btfss	PIR1,TXIF
		bra	PrintBusy
PrintTransmit
		banksel	TXREG
		movwf	TXREG
		banksel	0
		retlw	' '
PrintBusy	movwf	txtemp		;Temporarily save the character
		incf	txhead,W
		subwf	txtail,W
		skpz			;Buffer full?
		bra	PrintQueue
PrintWait	clrwdt
		btfss	PIR1,TXIF	;Transmit register is empty?
		bra	PrintWait	;Wait until space becomes available
		call	PrintFlush	;Transmit a character from the queue
PrintQueue	movlw	high txbuffer
		movwf	FSR1H
		movfw	txhead
		movwf	FSR1L		;Point to the first free slot
		movfw	txtemp
		movwf	INDF1		;Put the character in the buffer
		incf	txhead,F
		bsf	printpending	;Some chars are waiting
		btfss	PIR1,TXIF	;Transmit register is empty?
		retlw	' '
PrintFlush	movlw	high txbuffer
		movwf	FSR1H
		movfw	txtail
		movwf	FSR1L		;Point to the first waiting character
		incf	txtail,F	;Move the pointer
		movfw	txtail
		subwf	txhead,W
		skpnz			;More characters waiting?
		bcf	printpending
		movfw	INDF1		;Get the first character in the queue
		bra	PrintTransmit	;Transmit the character

multiplysupply	movfw	supplyl
		movwf	accual
		movfw	supplyh
		movwf	accuah
;Double Precision Multiply ( 16x16 -> 32 )
multiply	movlw	16
		movwf	loopcounter
		clrf	valuex
		clrf	valueu
multiplyloop	rrf	accuah,F
		rrf	accual,F
		skpc
		bra	multiplyskip
		movfw	accubl
		addwf	valueu,F
		movfw	accubh
		addwfc	valuex,F
multiplyskip	rrf	valuex,F
		rrf	valueu,F
		rrf	valueh,F
		rrf	valuel,F
		decfsz	loopcounter,F
		bra	multiplyloop
		return

divideby1024	movlw	high 1024
		movwf	accubh
		clrf	accubl
divide		clrf	accuau
		clrf	accuah
		clrf	accual
		clrf	counter
dividealign	incf	counter,F
		btfsc	accubu,7
		bra	divideloop
		lslf	accubl,F
		rlf	accubh,F
		rlf	accubu,F
		bra	dividealign
divideloop	lslf	accual,F
		rlf	accuah,F
		rlf	accuau,F
		movfw	accubl
		subwf	valuel,W
		movfw	accubh
		subwfb	valueh,W
		movfw	accubu
		subwfb	valueu,W
		skpc
		bra	divideskip
		movfw	accubl
		subwf	valuel,F
		movfw	accubh
		subwfb	valueh,F
		movfw	accubu
		subwfb	valueu,F
		incf	accual,F
		skpnz
		incf	accuah,F
		skpnz
		incf	accuau,F
divideskip	lsrf	accubu,F
		rrf	accubh,F
		rrf	accubl,F
		decfsz	counter,F
		bra	divideloop
		return

;Put all strings in a fixed 256 word section to allow simple access
Strings		equ	0x1e00
		code	Strings
Banner		da	"\r\nOpentherm gateway diagnostics - Version ", version
		da	"\r\n\n\032"
Prompt		da	"1. LED test\r\n"
		da	"2. Bit timing thermostat\r\n"
		da	"3. Bit timing boiler\r\n"
		da	"4. Delay symmetry\r\n"
		da	"5. Voltage levels\r\n"
		da	"6. Idle times\r\n\n"
		da	"Enter test number: \032"
InvalidTestStr	da	"Invalid test\032"
NoLoopError	da	"### Error: Interfaces don't appear to be looped\032"
DelayLoopError	da	"### Error\r\n\032"
PowerStr	da	"Power supply: \032"
Analog0Str	da	"Thermostat\032"
Analog1Str	da	"Boiler\032"
Analog2Str	da	"Reference: \032"
Symmetry0Str	da	"OK1A high-to-low: \032"
Symmetry1Str	da	"OK1A low-to-high: \032"
Symmetry2Str	da	"OK1B high-to-low: \032"
Symmetry3Str	da	"OK1B low-to-high: \032"
MicroSecStr	da	"us\r\n\032"
VoltRefStr	da	"Reference voltage setting (0..9) [\032"
VoltRefPrompt	da	"]: \032"
InvalidValue	da	"Invalid value\032"

		end
