		title		"OpenTherm Interface"
		list		p=16F1847, b=8, r=dec

;Copyright (c) 2022 Schelte Bron

#define		version		"2.0"
#define		phase		"."	;a=alpha, b=beta, .=production
;#define	patch		"2"	;Comment out when not applicable
;#define	bugfix		"1"	;Comment out when not applicable
#include	build.asm

;See the file "license.txt" for information on usage and redistribution of
;this file, and for a DISCLAIMER OF ALL WARRANTIES.

		__config	H'8007', B'00101111111100'
		__config	H'8008', B'11101011111111'

		errorlevel	-302, -312

;The OpenTherm Gateway is located between the thermostat and the boiler of a
;central heating system. This firmware reports any received opentherm messages
;to the serial interface. Each line consisting of 8 hexadecimal characters
;received on the serial interface is transmitted as an opentherm message on the
;appropriate opentherm interface.

#include	"p16f1847.inc"
#define		mhzstr "4"

;BAUD contains the SPBRG value for 9600 baud
		constant BAUD=25

;PERIOD is the value to be loaded into PR2/4/6 to obtain a 250uS period
		constant PERIOD=249

		constant PRELOADT0=178	;256 - 4 / 4 * 5000 / 64
		constant ONESEC=61	;0.999424s

;TWOSEC contains the number of timer 0 overflows (with a 1:64 prescaler)
;that happen in roughly 2 seconds. This is the time the error LED will stay
;lit after an error has been detected.
		constant TWOSEC=2*ONESEC

;Line voltage thresholds
;To be independent of the supply voltage, the values defined here are relative
;to the Fixed Voltage Reference of 2.048V. Unfortunately it is not possible to
;use the fixed voltage reference as the positive voltage reference for the A/D
;converter, because the input voltage may exceed this value. So, the supply
;voltage is used as the A/D converter's positive reference. To get the correct
;threshold for the used supply voltage, the values need to be multiplied with
;the value obtained from measuring the fixed voltage reference.
;Supply voltage = 5V: Measuring FVR = 2.048 results in 105.
;Supply voltage = 3.3V: Measuring FVR = 2.048 results in 159.
;8 * 105 / 128 = 7		8 * 159 / 128 =	10
;86 * 105 / 128 = 71		86 * 159 / 128 = 107
;156 * 105 / 128 = 128		156 * 159 / 128 = 194

		constant V_SHORT=8	;1V * 4.7 / 37.7 * 128 / 2.048
		constant V_LOW=86	;11V * 4.7 / 37.7 * 128 / 2.048
		constant V_OPEN=156	;20V * 4.7 / 37.7 * 128 / 2.048

;ReceiveModeT	Thermostat input is inverted
;ComparatorT	Thermostat input logical level
;SmartPowerT	Thermostat output is inverted
;ReceiveModeB	Boiler input is inverted
;ComparatorB	Boiler input logical level
;SmartPowerB	Boiler output is inverted

;Variables accessible from all RAM banks
		UDATA_SHR
		res	1	;Reserved for In-Circuit Debugger

temp		res	1
temp2		res	1

xmitbyte4	res	1
xmitbyte3	res	1
xmitbyte2	res	1
xmitbyte1	res	1
#define		XmitResponse	xmitbyte1,6

loopcounter	res	1

mode		res	1
#define		Overrun		mode,4

s_timer4	res	1	;Store timer 4 value, used in interrupt routine
s_timer6	res	1	;Store timer 6 value, used in interrupt routine

Bank0data	UDATA

RecvByteT4	res	1
RecvByteT3	res	1
RecvByteT2	res	1
RecvByteT1	res	1
#define		RecvResponseT	RecvByteT1,6

RecvByteB4	res	1
RecvByteB3	res	1
RecvByteB2	res	1
RecvByteB1	res	1
#define		RecvResponseB	RecvByteB1,6

;Variables for temporary storage
cmtemp		res	1
result		res	1
remainder	res	1
funcmask	res	1
pointer		res	1
BCD0		res	1
BCD1		res	1
BCD2		res	1
float1		res	1
float2		res	1

;Variables for longer lasting storage
bitcount1	res	1
bitcount2	res	1
bitcount	res	1	;Count of bits remaining in a message
outputmask	res	1
rxpointer	res	1
txpointer	res	1
txavailable	res	1

seccounter	res	1
linevoltage	res	1
nomessage	res	1

flags		res	1			;General bit flags
#define		NegativeTemp	flags,6
#define		LeadingZero	flags,7

stateflags	res	1			;More bit flags
#define		MessageRcvd	stateflags,0
#define		NoThermostat	stateflags,1
#define		Request		stateflags,2	;Must match swapped XmitResponse
#define		Receive		stateflags,3	;Activity on an Opentherm input
#define		CommandComplete stateflags,4
#define		Ready		stateflags,5	;Operation finsihed
#define		ChangeThis	stateflags,6	;Must match CMCON:C1OUT
#define		ChangeOther	stateflags,7	;Must match CMCON:C2OUT

msgflags1	res	1	;Used in interrupt routine
#define		ParityT		msgflags1,0
#define		MidBitT		msgflags1,2
#define		NextBitT	msgflags1,3
#define		PowerChangeT	msgflags1,4
#define		MessageRcvdT	msgflags1,5
#define		ReceiveModeT	msgflags1,6	;Must match CM1CON0:C1OUT
#define		ComparatorT	msgflags1,7	;Must match CM1CON0:C1OUT << 1

msgflags2	res	1	;Used in interrupt routine
#define		ParityB		msgflags2,0
#define		MidBitB		msgflags2,2
#define		NextBitB	msgflags2,3
#define		MessageRcvdB	msgflags2,5
#define		ReceiveModeB	msgflags2,6	;Must match CM2CON0:C2OUT
#define		ComparatorB	msgflags2,7	;Must match CM2CON0:C2OUT << 1

msgflags	res	1	;Used in interrupt routine
#define		Parity		msgflags,0
#define		Transition	msgflags,1
#define		MidBit		msgflags,2
#define		NextBit		msgflags,3
#define		Transmit	msgflags,4

#define		NextBitMask	1 << 3

quarter1	res	1
#define		BitClkCarryT	quarter1,3
quarter2	res	1
#define		BitClkCarryB	quarter2,3
quarter		res	1	;Count of 1/4 ms, used in interrupt routine
#define		LineStable	quarter,0
#define		RealBit		quarter,1
#define		BitClkCarry	quarter,3

tstatflags	res	1	;Used in- and outside interrupt routine
#define		HighPower	tstatflags,0	;Must match CMOUT:MC1OUT
#define		RaisedPower	tstatflags,1	;Must match CMOUT:MC2OUT
#define		OtherRequest	tstatflags,2	;Must match Request
#define		SmartPowerB	tstatflags,3	;Must correspond with SlaveOut
#define		SmartPowerT	tstatflags,4	;Must correspond with MasterOut
#define		OtherActive	tstatflags,5
#define		SmartPower	tstatflags,6	;Smart Power enabled
#define		PowerReport	tstatflags,7

active		res	1	;Active operations
pending		res	1	;Pending operations
;Active/pending bits
SMARTPOWER	equ	h'0000'
MEDIUMPOWER	equ	h'0001'
HIGHPOWER	equ	h'0002'
TSTATINVERT	equ	h'0003'
BOILERINVERT	equ	h'0004'
MESSAGE		equ	h'0007'

errornum	res	1
errortimer	res	1

settings	res	1			;Bits 0-4: voltage reference
#define		IgnoreErr1	settings,5	;For bouncing high<->low changes

debug		res	1
userleds	res	1

Bank1data	UDATA
		constant TXBUFSIZE=48
txbuffer	res	TXBUFSIZE
;The functions that can be assigned to a LED are referred to by an uppercase
;letter. One byte is reserved for each possible function. Bits 3, 4, 6, and 7
;indicate if the function is assigned to one or more of the four LED outputs.
;Bit 0 is used to keep track of the current state of the function. That makes
;it possible to immediately set a LED to the correct state upon assignment of
;the function.
functions	res	26
VoltShort	res	1
VoltLow		res	1
VoltOpen	res	1

Bank3data	UDATA
rxbuffer	res	16	;Serial receive buffer

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

#define		LEDMASK		b'11011000'

#define		SlaveMask	b'00001000'
#define		MasterMask	b'00010000'

#define		UserLEDAddress	(functions + ('U' - 'A'))

package		macro	pkg
pkg		code
pkg
		endm

pcall		macro	rtn
		lcall	rtn
		pagesel $
		endm

;The first thing to do upon a reset is to allow the firmware to be updated.
;So no matter how buggy freshly loaded firmware is, if the first two command
;have not been messed up, the device can always be recovered without the need
;for a PIC programmer. The self-programming code times out and returns after
;a second when no firmware update is performed.
;
ResetVector	code	0x0000
		extern	SelfProg
		lcall	SelfProg	;Always allow a firmware update on boot
		lgoto	Start		;Start the opentherm interface program

;Handle interrupts. Each type of interrupt is handled by its own subroutine.
;
InterruptVector code	0x0004
		banksel	TMR4		;Bank 8
		movfw	TMR4
		movwf	s_timer4	;Save the current value of timer4
		movfw	TMR6
		movwf	s_timer6	;Save the current value of timer6
		pagesel Interrupt
		;Process the timer interrupts before the comparator
		;interrupts to get the correct overflow count
		banksel	PIR3		;Bank 0
		btfsc	PIR3,TMR4IF	;Check if timer 4 matched
		call	timer4int
		btfsc	PIR3,TMR6IF	;Check if timer 6 matched
		call	timer6int
		btfsc	PIR2,C1IF	;Check if comparator 1 changed
		call	input1int
		btfsc	PIR2,C2IF	;Check if comparator 2 changed
		call	input2int
		btfsc	PIR1,TMR2IF	;Check if timer 2 matched
		call	timer2int
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

		package Interrupt

;Comparator interrupt routine
input1int	comf	msgflags1,W	;Get the inverted smart power bits
		banksel	CM1CON0		;Bank 2
		xorwf	CM1CON0,W	;Read the line state
		banksel	PIR2		;Bank 0
		bcf	PIR2,C1IF	;Clear the interrupt
		rlf	WREG,W		;Shift one position to the left
		xorwf	msgflags1,W	;Check that the comparator has changed
		andlw	b'10000000'	;Filter out the comparator bit
		skpnz			;Make sure there actually was a change
		return			;False trigger
		xorwf	msgflags1,F	;Update the saved state
		banksel	TMR4		;Bank 8
		btfsc	T4CON,TMR4ON	;Timer4 is stopped when the line is idle
		bra	input1mark
		movlw	30		;Time since interrupt (plus 4us latency)
		movwf	TMR4		;Initialize timer 4
		bsf	T4CON,TMR4ON	;Start timer 4
		banksel	quarter1	;Bank 0
		clrf	quarter1	;Reset the 1/4 ms counter
		return

input1mark	banksel	bitcount1
		tstf	bitcount1	;Is a message in progress?
		skpnz
		goto	input1process	;Line state change outside message
		movlw	3		;3 overflows of timer 4 = 750 us
		subwf	quarter1,W	;Check the time since the last bit
		skpc			;More than 750 us
		bra	input1midbit
		rlf	msgflags1,W	;Shift ComparatorT bit into carry
		decfsz	bitcount1,F	;Is this the stop bit?
		bra	storebit1	;If not, store the bit in the buffer
		clrf	quarter1	;Reset the 1/4ms counter
		btfsc	ComparatorT	;Check the logical line state
		goto	error02t	;A stop bit must be logical 1
		btfsc	ParityT		;Check for even parity
		goto	error04t	;Parity error
		bsf	MessageRcvdT	;A message was received from the master
		pagesel	Main
		movlw	'R'		;Receive function
		call	SwitchOffLED	;Switch off the receive LED
		movlw	'T'		;Thermostat function
		call	SwitchOffLED	;Switch off the thermostat LED
		pagesel	$
		return

input1midbit	btfsc	MidBitT		;First mid-bit transition?
		btfsc	IgnoreErr1	;Or multiple transitions are ignored?
		bsf	MidBitT		;Remember a mid-bit transition happened
		return

input1process	;A second line change happened shortly after the previous one.
		movlw	10
		subwf	quarter1,W
		skpnc			;Less than 2.5 ms since the last change
		bra	input1slow
input1fast	btfsc	msgflags1,7	;Check the logical line state
		return			;Not a start bit
		tstf	quarter1	;Check if pulse was less than 250us
		skpnz
		bra	input1spike	;Ignore spikes
		movlw	'R'		;Receive function
		call	SwitchOnLED	;Turn on the receive LED
		movlw	'T'
		call	SwitchOnLED	;Turn on the Thermostat LED
		pagesel	$
		movlw	33		;Message length: 32 bits + stop bit - 1
		movwf	bitcount1	;Initialize the counter
		bcf	ParityT		;Initialize the parity check flag
		bcf	MidBitT		;No mid-bit transitions yet
		goto	restarttimer4	;Significant transition of the start bit
input1slow	movlw	50
		subwf	quarter1,W
		skpnc			;Less than 12.5 ms since the last change
		goto	restarttimer4
		movlw	b'11000000'	;Bit mask for invert and logical level
		btfsc	ComparatorT	;Check the logical line state
		xorwf	msgflags1,F	;Toggle the invert and state bits
		return
input1spike
		banksel	TMR4
		bcf	T4CON,TMR4ON	;Stop timer 4
		banksel	PIR3
		return

storebit1	;Store the current bit in the message buffer
		lslf	msgflags1,W	;Put the line state into the carry bit
		movlw	1 << 0		;Mask for the bit flag tracking parity
		skpc			;Rising flank (i.e. 0 bit)?
		xorwf	msgflags1,F	;Update the parity
		rlf	RecvByteT4,F	;Shift the 32 bits receive buffer
		rlf	RecvByteT3,F	;Shift bits 8 through 15
		rlf	RecvByteT2,F	;Shift bits 16 through 23
		rlf	RecvByteT1,F	;Shift bits 24 through 31
		xorwf	RecvByteT4,F	;Need to invert the new bit
		bcf	MidBitT		;Mid-bit transitions are allowed again
restarttimer4	clrf	quarter1	;Restart 1/4 ms counter
		movfw	s_timer4	;Timer 4 value just after the interrupt
		banksel	TMR4		;Bank 8
		subwf	TMR4,F		;Adjust the timer value
		movlw	255 - PERIOD
		skpc
		subwf	TMR4,F		;Compensate for early reset of T4
		banksel	PIR3		;Bank 0
		bcf	PIR3,TMR4IF
		return

input2int	comf	msgflags2,W	;Get the inverted smart power bits
		banksel	CM2CON0		;Bank 2
		xorwf	CM2CON0,W	;Read the line states
		banksel	PIR2		;Bank 0
		bcf	PIR2,C2IF	;Clear the interrupt
		rlf	WREG,W		;Shift one position to the left
		xorwf	msgflags2,W	;Check that the comparator has changed
		andlw	b'10000000'	;Filter out the comparator bit
		skpnz			;Make sure there actually was a change
		return			;False trigger
		xorwf	msgflags2,F	;Update the saved state
		banksel	TMR6
		btfsc	T6CON,TMR6ON	;Timer6 is stopped when the line is idle
		bra	input2mark
		movlw	30		;Time since interrupt (plus 4us latency)
		movwf	TMR6		;Initialize timer 6
		bsf	T6CON,TMR6ON	;Start timer 6
		banksel	quarter2	;Bank 0
		clrf	quarter2	;Reset the 1/4 ms counter
		return

input2mark	banksel	bitcount2
		tstf	bitcount2	;Is a message in progress?
		skpnz
		goto	input2process	;Line state change outside message
		movlw	3		;3 overflows of timer 4 = 750 us
		subwf	quarter2,W	;Check the time since the last bit
		skpc			;More than 750 us
		bra	input2midbit
		rlf	msgflags2,W	;Shift ComparatorB bit into carry
		decfsz	bitcount2,F	;Is this the stop bit?
		bra	storebit2	;If not, store the bit in the buffer
		clrf	quarter2	;Reset the 1/4ms counter
		btfsc	ComparatorB	;Check the logical line state
		goto	error02b	;A stop bit must be logical 1
		btfsc	ParityB		;Check for even parity
		goto	error04b	;Parity error
		bsf	MessageRcvdB	;A message was received from the master
		pagesel	Main
		movlw	'R'		;Receive function
		call	SwitchOffLED	;Switch off the Receive LED
		movlw	'B'		;Thermostat function
		call	SwitchOffLED	;Switch off the Boiler LED
		pagesel	$
		return

input2midbit	btfsc	MidBitB		;First mid-bit transition?
		btfsc	IgnoreErr1	;Or multiple transitions are ignored?
		bsf	MidBitB		;Remember a mid-bit transition happened
		return

input2process	;A second line change happened shortly after the previous one.
		movlw	10
		subwf	quarter2,W
		skpnc			;Less than 2.5 ms since the last change
		bra	input2slow
input2fast	btfsc	msgflags2,7	;Check the logical line state
		return			;Not a start bit
		tstf	quarter2	;Check if pulse was less than 250us
		skpnz
		bra	input2spike	;Ignore spikes
		pagesel	Main
		movlw	'R'		;Receive function
		call	SwitchOnLED	;Turn on the Receive LED
		movlw	'B'
		call	SwitchOnLED	;Turn on the Boiler LED
		pagesel	$
		movlw	33		;Message length: 32 bits + stop bit - 1
		movwf	bitcount2	;Initialize the counter
		bcf	ParityB		;Initialize the parity check flag
		bcf	MidBitB		;No mid-bit transitions yet
		goto	restarttimer6	;Significant transition of the start bit
input2slow	btfsc	active,SMARTPOWER
		return
		movlw	50
		subwf	quarter2,W
		skpnc			;Less than 12.5 ms since the last change
		goto	restarttimer6
		btfss	ComparatorB	;Check the logical line state
		return			;Nothing to do
		movlw	b'11000000'	;Bit mask for invert and logical level
		xorwf	msgflags2,F	;Toggle the invert and state bits
		return
input2spike
		banksel	TMR6
		bcf	T6CON,TMR6ON	;Stop timer 4
		banksel	PIR3
		return
storebit2	;Store the current bit in the message buffer
		movlw	1 << 0		;Mask for the bit flag tracking parity
		skpc			;Rising flank (i.e. 0 bit)?
		xorwf	msgflags2,F	;Update the parity
		rlf	RecvByteB4,F	;Shift the 32 bits receive buffer
		rlf	RecvByteB3,F	;Shift bits 8 through 15
		rlf	RecvByteB2,F	;Shift bits 16 through 23
		rlf	RecvByteB1,F	;Shift bits 24 through 31
		xorwf	RecvByteB4,F	;Need to invert the new bit
		bcf	MidBitB		;Mid-bit transitions are allowed again
restarttimer6	clrf	quarter2	;Restart 1/4 ms counter
		movfw	s_timer6	;Timer 6 value just after the interrupt
		banksel	TMR6		;Bank 8
		subwf	TMR6,F		;Adjust the timer value
		movlw	255 - PERIOD
		skpc
		subwf	TMR6,F		;Compensate for early reset of T6
		banksel	PIR3		;Bank 0
		bcf	PIR3,TMR6IF
		return

;Timer 2 interrupt routine
timer2int	bcf	PIR1,TMR2IF	;Clear the interrupt flag
		incf	quarter,F	;Update the 1/4 ms counter
		btfsc	LineStable	;Nothing to do in a stable interval
		return
		tstf	bitcount
		skpnz
		bra	timer2nobits
		clrw			;Initialize work register
		btfss	NextBit		;Find out the desired output level
		movlw	b'00011000'	;Set bits matching the opentherm outputs
		xorwf	tstatflags,W	;Apply smart power
		xorwf	PORTA,W		;Compare with the current port levels
		andwf	outputmask,W	;Only apply for the relevant output
		xorwf	PORTA,F		;Update the output port
		btfsc	RealBit		;Check if this was the start of a bit
		goto	InvertBit	;Next time send the inverted value
		bcf	NextBit		;Start with assumption next bit is 0
		setc			;Prepare to shift in a stop bit
		rlf	xmitbyte4,F	;Shift the full 32 bit message buffer
		rlf	xmitbyte3,F	;Shift bits 8 through 15
		rlf	xmitbyte2,F	;Shift bits 16 through 23
		rlf	xmitbyte1,F	;Shift bits 24 through 31
		skpnc			;Test the bit shifted out of the buffer
		bsf	NextBit		;Next time, send a logical 1
		clrf	quarter
		decf	bitcount,F	;Test for the end of the message
		return			;Return from the interrupt routine

timer2nobits	btfsc	active,MESSAGE	;Message in progress?
		bra	timer2xmitend
		btfsc	active,SMARTPOWER
		bra	timer2power
		bcf	T2CON,TMR2ON	;Stop the timer
		return

timer2xmitend	bcf	active,MESSAGE	;Message operation has completed
		bcf	Transmit	;Finished transmitting
		bcf	T2CON,TMR2ON	;Stop the timer
		pagesel Main
		movlw	'X'		;Transmit function
		call	SwitchOffLED	;Switch off the transmit LED
		movlw	'T'		;Thermostat function
		call	SwitchOffLED	;Switch off the thermostat LED
		movlw	'B'		;Boiler function
		call	SwitchOffLED	;Switch off the boiler LED
		pagesel $
		return			;Return from the interrupt routine
timer2power	btfss	active,MEDIUMPOWER
		btfsc	active,HIGHPOWER
		bra	timer2raised
		btfsc	SmartPowerB	;Output not inverted?
		bra	timer2pulse
		movlw	40		;40 quarter milliseconds
		subwf	quarter,W
		skpc			;10 ms have elapsed?
		return
		call	ReadComparators	;Get the line levels
		btfss	WREG,MC2OUT	;Boiler line still inverted?
		clrf	msgflags2	;Reset the boiler flags
		bra	timer2pwrdone
timer2pulse	movlw	60		;60 quarter milliseconds
		subwf	quarter,W
		skpc			;15 ms have elapsed?
		return
		bsf	SlaveOut	;Return boiler line to idle
		bcf	SmartPowerB	;Line not inverted
		clrf	quarter
		return
timer2raised	movlw	40		;40 quarter milliseconds
		subwf	quarter,W
		skpc			;10 ms have elapsed?
		return
		call	ReadComparators	;Get the line levels
		btfss	WREG,MC2OUT	;Boiler line is high?
		bra	timer2lowpwr
		bsf	ReceiveModeB	;Invert signal from the boiler
		bcf	ComparatorB	;Line is idle
		btfss	active,MEDIUMPOWER	;Medium power request?
		bra	timer2pwrdone
timer2lowpwr	bsf	SlaveOut	;Return boiler line to idle
		bcf	SmartPowerB	;Line not inverted
timer2pwrdone	clrf	active		;Smart power request finished
		bcf	T2CON,TMR2ON	;Switch of timer
		return

ReadComparators	banksel	CMOUT
		comf	CMOUT,W		;Line levels are inverted
		banksel	0
		return

InvertBit	movlw	NextBitMask	;Load a mask for the NextBit flag
		xorwf	msgflags,F	;Invert the bit
		return			;Return from the interrupt routine

;Timer 4 interrupt routine - receiving an opentherm message from the thermostat
timer4int	bcf	PIR3,TMR4IF	;Clear the interrupt flag
		incf	quarter1,F	;Update the 1/4 ms counter
		movfw	bitcount1	;Message in progress?
		skpnz
		bra	timer4idle
		btfsc	BitClkCarryT	;Bit transition happened on time?
		goto	error03t
		return

timer4idle	movlw	20
		subwf	quarter1,W	;New state persisted for 5ms?
		skpnz
		bra	timer4idle5ms
		movlw	80
		subwf	quarter1,W
		skpc			;20ms since last line event?
		return
timer4idle20ms	btfsc	PowerChangeT
		bsf	PowerReport	;Report changed power levels
		bcf	PowerChangeT
		banksel	T4CON
		bcf	T4CON,TMR4ON    ;No line event for 20ms
		banksel	active
		return

;If the opentherm line has been stable and at the active logical level for 5ms,
;it is interpreted as a request to switch Smart Power mode. Switching receive
;mode must always be done, even if no communication was established in low
;power mode at start-up or if Medium or High power mode is not allowed.
;Switching the output power level is only supported on the thermostat interface.
timer4idle5ms	btfss	ComparatorT	;Get the active line level
		return			;Nothing to do
		movlw	1 << 6 | 1 << 7	;Bit mask
		xorwf	msgflags1,F	;Invert ReceiveModeT and ComparatorT
		btfss	SmartPower	;Smart Power support enabled?
		return			;Smart power is not supported.
		bsf	PowerChangeT	;Power levels have changed
		btfsc	ReceiveModeT	;In high power mode? (bit was inverted)
		btfss	SmartPowerT	;In medium power mode?
		bra	timer4invert	;Invert for high->low or low->high
		return			;Nothing to do in medium power mode
timer4invert	movlw	1 << RA4	;Bit mask for the thermostat output
		xorwf	PORTA,F		;Invert the thermostat output
		xorwf	tstatflags,F	;Toggle SmartPowerT
		andwf	tstatflags,W	;Test the TransmitMode bit
		movlw	'P'
		pcall	SwitchLED	;Update the Raised Power LED
		return

;Error 01: More than one mid-bit transition
error01t	movlw	1
		bra	errorcleanupt

;Error 02: Wrong stop bit
error02t	movlw	2
		bra	errorcleanupt

;Error 03: Bit not received in time
error03t	movwf	debug
		movlw	3
		bra	errorcleanupt

;Error 04: Parity error
error04t	movlw	4

errorcleanupt	tstf	errornum
		skpnz
		movwf	errornum
		comf	msgflags1,W
		banksel	CM1CON0		;Bank 2
		xorwf	CM1CON0,W
		banksel	T4CON		;Bank 8
		btfss	WREG,C1OUT	;Line is not idle, restart timer 4
		bcf	T4CON,TMR4ON	;Stop timer 4
		banksel	bitcount1	;Bank 0
		clrf	bitcount1
		clrf	quarter1
		bcf	Transmit
		pagesel Main
		movlw	'R'
		call	SwitchOffLED	;Switch off the receive LED
		movlw	'T'
		call	SwitchOffLED	;Switch off the thermostat LED
		comf	errornum,W
		skpnz
		bra	errcleanuptret	;Not a real error
		movlw	'E'
		call	SwitchOnLED	;Switch on the error LED
		movlw	TWOSEC		;Leave the error LED on for 2 seconds
		movwf	errortimer
errcleanuptret	pagesel $
		return			;Wait for the line to go idle

;Timer 6 interrupt routine - receiving an opentherm message from the boiler
timer6int	bcf	PIR3,TMR6IF	;Clear the interrupt flag
		incf	quarter2,F
		movfw	bitcount2	;Message in progress?
		skpnz
		bra	timer6idle
		btfsc	BitClkCarryB	;Bit transition happened on time?
		goto	error03b
		return

timer6idle	movfw	quarter2
		addlw	-80
		skpc			;20ms since last line event?
		return
		banksel	T6CON
		bcf	T6CON,TMR6ON    ;No line event for 20ms
		banksel	active
		clrf	active          ;Avoid getting stuck
		return

;Error 01: More than one mid-bit transition
error01b	movlw	1
		bra	errorcleanupb

;Error 02: Wrong stop bit
error02b	movlw	2
		bra	errorcleanupb

;Error 03: Bit not received in time
error03b	movwf	debug
		movlw	3
		bra	errorcleanupb

;Error 04: Parity error
error04b	movlw	4

errorcleanupb	tstf	errornum
		skpnz
		movwf	errornum
		banksel	CM2CON0		;Bank 2
		comf	CM2CON0,W	
		banksel	T6CON		;Bank 8
		btfss	WREG,C2OUT	;Line is not idle, restart timer 6
		bcf	T6CON,TMR6ON	;Stop timer 6
		banksel	bitcount2	;Bank 0
		clrf	bitcount2
		clrf	quarter2
		bcf	Transmit
		pagesel Main
		movlw	'R'
		call	SwitchOffLED	;Switch off the receive LED
		movlw	'B'
		call	SwitchOffLED	;Switch off the boiler LED
		comf	errornum,W
		skpnz
		bra	errcleanupbret	;Not a real error
		movlw	'E'
		call	SwitchOnLED	;Switch on the error LED
		movlw	TWOSEC		;Leave the error LED on for 2 seconds
		movwf	errortimer
errcleanupbret	pagesel $
		return			;Wait for the line to go idle

;########################################################################
; Main program
;########################################################################

		package Main
Break
		banksel	RCREG		;Bank 3
		tstf	RCREG		;Clear the RCIF interrupt flag
		banksel	PORTA
BreakWait	clrwdt
		btfss	RXD
		goto	BreakWait
		pcall	SelfProg
Start
		banksel	LATB		;Bank 2
		clrf	LATB		;Flash the LED's on startup
		bsf	MasterOut
		banksel	OSCCON		;Bank 1
		movlw	b'01101010'	;Internal oscillator set to 4MHz
		movwf	OSCCON		;Configure the oscillator

		;Configure I/O pins
		;Power on defaults all pins to inputs
		;Port A
		;Pins 0 and 1 are used as comparator inputs
		;Pin 2 is used as VREF (must be configured as input!)
		;Pins 3 and 4 are (comparator) ouputs
		;Pin 5 is master reset input
		;Pins 6 and 7 are GPIO
		banksel	ANSELA		;Bank 3
		movlw	b'00000111'	;A0 through A2 are used for analog I/O
		movwf	ANSELA

		;Port B
		;Pins 2 and 5 are used by the USART and don't need to be configured
		;Pins 3, 4, 6, and 7 are used to indicate events for debugging
		clrf	ANSELB		;No analog I/O on port B

		banksel	APFCON0		;Bank 2
		bsf	APFCON1,TXCKSEL	;TX on RB5
		bsf	APFCON0,RXDTSEL	;RX on RB2

		banksel	TRISA		;Bank 1
		movlw	b'11100111'
		movwf	TRISA
		movlw	b'00100111'
		movwf	TRISB

		;Check for reset from watchdog timeout
		clrf	mode

		;Configure Timer 0 & Watchdog Timer
		;Bit 7 RBPU = 1 - Port B pull up disabled
		;Bit 6 INTEDG = 1 - Interrupt on rising edge
		;Bit 5 T0CS = 0 - Internal clock
		;Bit 4 T0SE = 1 - High to low transition
		;Bit 3 PSA = 0 - Prescaler assigned to Timer 0
		;Bit 2-0 PS = 5 - 1:64 Prescaler
		movlw	b'11010101'
		movwf	OPTION_REG

		banksel	ANSELA		;Bank 3
		movlw	b'00000111'	;A0 through A2 are used for analog I/O
		movwf	ANSELA
		clrf	ANSELB		;No analog I/O on port B

		;Configure Timer 4 & 6
		;Generate an interrupt every 250uS (one fourth of a bit)
		;Bit 6-3 TOUTPS = 0 - 1:1 Postscaler
		;Bit 2 TMR2ON = 0 - Timer disabled
		;Bit 1-0 T2CKPS = 0 - 1:1 Prescaler
		banksel	TMR4		;Bank 8
		movlw	PERIOD		;Fosc / 16000 - 1
		movwf	PR4
		movwf	PR6
		clrf	T4CON
		clrf	T6CON

		;Configure fixed voltage reference
		;Bit 7 FVREN = 1 - FVR enabled
		;Bit 5 TSEN = 0 - Temperature indicator disabled
		;Bit 4 TSRNG = 0 - Temperature indicator low range
		;Bit 3-2 CDAFVR = 2 - DAC fixed voltage is 2.048
		;Bit 1-0 ADFVR = 2 - ADC fixed voltage is 2.048
		banksel FVRCON          ;Bank 2
		movlw   b'10001010'
		movwf   FVRCON

		;Configure the serial interface
		banksel	RCSTA		;Bank 3
		clrf	RCSTA		;Reset the serial port control register
		movlw	BAUD
		movwf	SPBRGL
		bsf	TXSTA,BRGH	;9600 baud
		bcf	TXSTA,SYNC	;Asynchronous
		bsf	TXSTA,TXEN	;Enable transmit
		bsf	RCSTA,SPEN	;Enable serial port
		bsf	RCSTA,CREN	;Enable receive

		;Configure A/D converter
		banksel	ADCON0		;Bank 1
		movlw	b'00010000'	;Left justified, Fosc/8, Vss, Vdd
		movwf	ADCON1
		movlw	b'01111101'	;A/D on, channel 31 - FVR buffer 1
		movwf	ADCON0

		;Setup interrupt sources
		bsf	PIE2,C1IE	;Enable comparator 1 interrupt
		bsf	PIE2,C2IE	;Enable comparator 2 interrupt
		bsf	PIE1,TMR2IE	;Enable timer 2 interrupts
		bsf	PIE3,TMR4IE	;Enable timer 4 interrupts
		bsf	PIE3,TMR6IE	;Enable timer 6 interrupts

		bsf	ADCON0,GO	;Start A/D conversion

		;Clear RAM in bank 0 & 1
		movlw	0x20
		movwf	FSR0L
		movlw	80
		movwf	loopcounter
		movlw	0x80
InitLoop0	clrf	INDF0
		xorwf	FSR0L,F
		clrf	INDF0
		incf	FSR0L,F
		decfsz	loopcounter,F
		goto	InitLoop0

WaitConvert	btfsc	ADCON0,GO
		bra	WaitConvert	;Wait for A/D conversion to complete
		movlw	V_SHORT
		call	CalcThreshold
		movwf	VoltShort
		movlw	V_LOW
		call	CalcThreshold
		movwf	VoltLow
		movlw	V_OPEN
		call	CalcThreshold
		movwf	VoltOpen

		movlw	b'00000001'	;A/D on, channel 0 - AN0
		movwf	ADCON0		;Configure the A/D converter

		;Configure the digital to analog converter
		;Bit 7 DACEN = 1 - DAC enabled
		;Bit 6 DACLPS = 1 - Positive reference source selected
		;Bit 5 DACOE = 0 - Output pin disconnected
		;Bit 3-2 DACPSS = 2 - Positive source is FVR
		;Bit 0 DACNSS = 0 - Negative source is Vss
		movlw   b'11001000'
		movwf   DACCON0

		movlw	SavedSettings
		call	ReadEpromData	;Get the settings from EEPROM
		movwf	settings	;Store a copy in RAM
		banksel	DACCON1		;Bank 1
		movwf   DACCON1         ;Set the reference voltage

		;Configure comparator module
		;Bit 7 CxINTP = 1 - CxIF set on positive edge
		;Bit 6 CxINTN = 1 - CxIF set on negative edge
		;Bit 5-4 = 1 - CxVP connects to DAC voltage reference
		;Bit 3-2 Unimplemented
		;Bit 1-0 = 0 - C12IN0- (RA0) / 1 - C12IN1- (RA1)
		movlw	b'11010000'
		movwf	CM1CON1
		movlw	b'11010001'
		movwf	CM2CON1
		;Bit 7 CxON = 1 - Comparator enabled
		;Bit 6 CxOUT = R/O
		;Bit 5 CxOE = 0 - CxOUT internal only
		;Bit 4 CxPOL = 0 - Not inverted
		;Bit 3 Unimplemented
		;Bit 2 CxSP = 1 - Normal power
		;Bit 1 CxHYS = 1 - Hysteresis enabled
		;Bit 0 CxSYNC = 0 - Asynchronous output
		movlw	b'10000110'
		movwf	CM1CON0
		movwf	CM2CON0

		;Delay a few milliseconds to show the lit LEDs
		banksel	TMR0		;Bank 0
		clrf	TMR0
		bcf	INTCON,TMR0IF
WaitTimer0	clrwdt
		btfss	INTCON,TMR0IF
		goto	WaitTimer0

		;Configure Timer 1
		;Used to blink LEDs approximately once per second
		;Bit 5-4 T1CKP = 3 - 1:8 Prescale value
		;Bit 3 T1OSCEN = 0 - Oscillator is shut off
		;Bit 2 T1SYNC = 0 - Bit is ignored because TMR1CS=0
		;Bit 1 TMR1CS = 0 - Internal clock (Fosc/4)
		;Bit 0 TMR1ON = 1 - Enable Timer1
		movlw	b'110001'
		movwf	T1CON

		;Configure Timer 2
		;Generate an interrupt every 250uS (one fourth of a bit)
		;Bit 6-3 TOUTPS = 0 - 1:1 Postscaler
		;Bit 2 TMR2ON = 0 - Timer disabled
		;Bit 1-0 T2CKPS = 0 - 1:1 Prescaler
		movlw	PERIOD		;Fosc / 16000 - 1
		movwf	PR2
		clrf	T2CON

		clrf	PIR2		;Clear any comparator interrupt
		movlw	b'11111111'
		movwf	PORTA		;Outputs at idle
		movwf	PORTB		;Switch off the LEDs
		bsf	INTCON,PEIE
		bsf	INTCON,GIE

		movlw	ONESEC
		movwf	seccounter
		movlw	5
		movwf	nomessage

		movlw	GreetingStr
		pcall	PrintStringNL

		;Initialize the LED functions
		movlw	b'00000010'	;Bit mask for LED 5
		movwf	funcmask
		movlw	FunctionLED5
		call	ReadEpromData
		pcall	SetLEDFunction
		lslf	funcmask,F
		movlw	FunctionLED6
		call	ReadEpromData
		pcall	SetLEDFunction
		lslf	funcmask,F
		movlw	FunctionLED1
		call	ReadEpromData
		pcall	SetLEDFunction
		lslf	funcmask,F
		movlw	FunctionLED2
		call	ReadEpromData
		pcall	SetLEDFunction
		lslf	funcmask,F
		lslf	funcmask,F
		movlw	FunctionLED3
		call	ReadEpromData
		pcall	SetLEDFunction
		lslf	funcmask,F
		movlw	FunctionLED4
		call	ReadEpromData
		pcall	SetLEDFunction

;************************************************************************

MainLoop	clrwdt

		tstf	txavailable	;Check if there is data to transmit
		skpz
		call	ProcessOutput

		;Check if an OpenTherm message was received
		btfss	MessageRcvdT
		btfsc	MessageRcvdB
		call	OpenThermMsg

		btfsc	PowerReport
		call	ReportPower	;Report a power level change

		btfsc	Ready
		call	IdleState

		btfsc	INTCON,TMR0IF
		call	IdleTimer

#ifdef ADCON0
		btfsc	PIR1,ADIF
		call	CheckThermostat
#endif
		btfss	PIR1,RCIF	;Activity on the serial receiver?
		goto	MainLoop
		banksel	RCSTA		;Bank 3
		btfss	RCSTA,FERR	;Check for framing error (break)
		bra	SerialEvent
		tstf	RCREG		;Check received char is 0
		skpnz
		goto	Break		;Real break
		banksel	0		;Bank 0
		goto	MainLoop	;Framing error (baud rate mismatch)

SerialEvent	btfss	RCSTA,OERR	;Check for overrun error
		goto	SerialEventJmp1
		bcf	RCSTA,CREN	;Procedure to clear an overrun error
		bsf	RCSTA,CREN	;Re-enable the serial interface
		banksel	0
		bsf	Overrun		;Remember for later reporting
		goto	MainLoop
SerialEventJmp1 banksel	0
		call	SerialReceive
		btfss	CommandComplete
		goto	MainLoop
		lcall	SerialCommand
		clrf	rxpointer	;Prepare for a new command
		bcf	CommandComplete
		pagesel Print
		xorlw	-1		;Nothing needs to be printed?
		skpnz
		goto	SerialEventRet
		xorlw	-1
		skpz
		call	PrintString	;Print the result
		call	NewLine
SerialEventRet	lgoto	MainLoop

;Callers depend on this function never setting the carry bit
SerialReceive	movlw	high rxbuffer
		movwf	FSR1H
		movfw	rxpointer
		addlw	low rxbuffer
		movwf	FSR1L
		banksel	RCREG
		movfw	RCREG
		banksel 0
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

OpenThermMsg	pagesel	Print
		btfsc	MessageRcvdT
		call	PrintRcvdMsgT	;Report the received message
		bcf	MessageRcvdT	;Message has been reported
		btfsc	MessageRcvdB
		call	PrintRcvdMsgB	;Report the received message
		bcf	MessageRcvdB	;Message has been reported
		pagesel	$
		return

IdleState	bcf	Ready
		call	RunOperation
		btfss	PowerReport	;Report a power level change?
		return

ReportPower	movlw	NormalPowerStr	;Normal Power
		btfss	SmartPowerT
		goto	ReportPowerJ2
		movlw	MediumPowerStr	;Medium Power
		btfss	ReceiveModeT
		goto	ReportPowerJ1
		movfw	linevoltage	;Check the line voltage
		sublw	V_OPEN		;Thermostat disconnected?
		skpc
		return			;Don't report the power change
		movlw	HighPowerStr	;High Power
ReportPowerJ1	lcall	PrintString
		movlw	PowerStr
ReportPowerJ2	pcall	PrintStringNL	;Print the new power setting
		bcf	PowerReport	;Print the report only once
		return

;IdleTimer is called whenever timer 0 overflows
IdleTimer	bcf	INTCON,TMR0IF	;Clear Timer 0 overflow flag
		decfsz	seccounter,F	;Decrease second counter
		goto	IdleTimerJ1
		movlw	ONESEC
		movwf	seccounter	;Reload second counter
		decfsz	nomessage,F
		return
		bcf	SlaveOut	;No message to the boiler for 5 seconds
		bcf	SmartPowerB	;Prevent unintended heat demand
		movlw	b'10000000'	;Line is high
		movwf	msgflags2	;Reset the boiler state
IdleTimerJ1
#ifdef ADCON0
		btfsc	T2CON,TMR2ON
		bra	IdleTimerJ2
		banksel	ADCON0
		bsf	ADCON0,GO	;Start A/D conversion
		banksel	T2CON
IdleTimerJ2
#endif
		btfss	OtherActive
		goto	IdleTimerJ3
;The other side inverted their output. Then the input on this side must also be
;inverted to be able to still understand the messages.
		bcf	OtherActive	;The change is being processed
		movlw	msgflags1	;Thermostat interface information
		btfss	OtherRequest	;Other interface is the thermostat?
		movlw	msgflags2	;Boiler interface information
		movwf	FSR0L		;Point to the relevant information byte
		btfss	INDF0,7		;Check that the line is actually active
		goto	IdleTimerJ3	;Unexpected result, skip the action
		movlw	b'11000000'	;Toggle the invert flag and saved state
IdleTimerJ3
		pagesel Print
		comf	errornum,W	;Suppressing errors?
		skpnz
		clrf	errornum	;Re-enable errordetection
		tstf	errornum	;Check if any errors were detected
		skpz
		call	PrintError	;Report the error on the serial intf
		pagesel Main
		tstf	errortimer
		skpz
		decfsz	errortimer,F
		return
		movlw	'E'
		call	SwitchOffLED	;Switch off the error LED
		return

ProcessOutput	btfss	PIR1,TXIF	;Check if the transmit buffer is empty
		return
		movfw	txpointer	;Get the offset of the next character
		addlw	low txbuffer	;Add the start of the buffer
		movwf	FSR0L		;Setup indirect access
		movfw	INDF0		;Get the character from the buffer
		banksel	TXREG		;Bank 3
		movwf	TXREG		;Load the serial transmitter
		banksel	txpointer
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
		banksel	PIR1
		btfss	PIR1,RCIF	;Check for serial receiver activity
		goto	EepromWait	;Wait some more
		movwf	temp2		;Preserve value in W register
		call	SerialReceive	;Prevent receiver overrun
		movfw	temp2		;Restore original W register value
		goto	EepromWait	;Wait some more

ReadEpromData	call	EepromWait	;Wait for any pending EEPROM activity
		clrf	EECON1		;Read from DATA EEPROM
		movwf	EEADRL		;Setup the EEPROM data address
		bsf	EECON1,RD	;Start the EEPROM read action
		movfw	EEDATL		;Read the data from EEPROM
		banksel	0		;Switch back to bank 0
		return

#ifdef ADCON0
CheckThermostat bcf	PIR1,ADIF	;Clear flag for next round
		movfw	ADRESH		;Get the A/D result
		xorwf	linevoltage,W	;Swap values of W and linevoltage
		xorwf	linevoltage,F
		xorwf	linevoltage,W	;--
		subwf	linevoltage,W	;Calculate the difference
		skpc
		sublw	0		;Turn into absolute value
		andlw	b'11111100'	;Check for limited deviation
		skpz
		return
		movfw	linevoltage	;Get the A/D result again
		sublw	V_OPEN		;Line open?
		skpc
		goto	OpenCircuit	;No opentherm thermostat connected
		movfw	linevoltage	;Get the A/D result once more
		sublw	V_OPEN - V_SHORT;Line shorted?
		skpnc			;Voltage too close to the limits
		btfss	NoThermostat
		return
Connect		bcf	NoThermostat	;Thermostat was reconnected
		movlw	TConnect
		pcall	PrintStringNL
		return
OpenCircuit	btfsc	NoThermostat
		return
Disconnect	bsf	NoThermostat	;Thermostat has been disconnected
		movlw	TDisconnect	;Report the thermostat was disconnected
		pcall	PrintStringNL
		banksel	T4CON
		bcf	T4CON,TMR4ON	;Stop timer 4
		banksel	msgflags1
		movlw	b'10000000'	;Line is high
		movwf	msgflags1	;Reset the message flags
		movlw	b'00001010'	;Mask for boiler related bits
		andwf	tstatflags,F	;Forget all thermostat related info
		return
#endif

;Check if there are any pending operations that can be executed
RunOperation	tstf	active		;Make sure no operation is in progress
		skpz
		retlw	0
		btfsc	pending,SMARTPOWER
		goto	SmartPowerOp
RunNextOp	btfss	pending,MESSAGE
		retlw	0

SendMessageOp	btfss	T2CON,TMR2ON	;Check idle state
		goto	SendOpAllowed
		swapf	stateflags,W	;Check which interface was last used
		xorwf	xmitbyte1,W	;Compare against the message direction
		andlw	b'01000000'
		skpnz			;Different interfaces, no need to wait
		retlw	0
		bcf	T2CON,TMR2ON	;Stop timer 2
SendOpAllowed	bsf	active,MESSAGE	;The operation has started
		bcf	pending,MESSAGE	;It is no longer pending

		movlw	1
		movwf	quarter		;Initialize the state counter
		bsf	NextBit		;Start bit is 1
		bsf	Transmit	;Starting to transmit a message
		movlw	'X'		;Transmit function
		call	SwitchOnLED	;Switch on the transmit LED
		movlw	'T'		;Thermostat function
		btfss	XmitResponse
		movlw	'B'		;Boiler function
		call	SwitchOnLED	;Switch on the boiler or thermostat LED
		movlw	SlaveMask	;Transmitting to the boiler
		btfsc	XmitResponse	;Check message direction
		movlw	MasterMask	;Transmitting to the thermostat
		movwf	outputmask
		movlw	34		;A message is 32 bits + start & stop bit
		movwf	bitcount	;Load the counter
		movlw	PERIOD
		movwf	TMR2		;Prepare timer 2 to overflow asap
		bsf	T2CON,TMR2ON	;Start timer 2

		movlw	5
		btfss	XmitResponse
		movwf	nomessage	;Message to the boiler

		lcall	PrintXmitMsg
		call	NewLine
		pagesel $
		retlw	0

SmartPowerOp	btfsc	T2CON,TMR2ON	;Check idle state
		retlw	0
		movfw	pending
		andlw	b'00011111'	;Check the type of Smart Power request
		xorwf	pending,F	;Reset the pending bits for this action
		movwf	active		;Action in progress
		andlw	b'00011110'	;Clear the SMARTPOWER bit
		skpnz			;Requesting Smart Power?
		goto	PowerOpLow	;Switch to low power
		btfsc	active,MEDIUMPOWER
		goto	PowerOpMedium	;Switch to medium power
		btfsc	active,HIGHPOWER
		goto	PowerOpHigh	;Switch to high power
		btfsc	active,BOILERINVERT
		goto	PowerOpForce	;Toggle the boiler interface output
		btfss	active,TSTATINVERT
		retlw	0		;Should never get here
		clrf	active		;No timed action is needed
		bsf	Request		;Operation on the thermostat interface
		movlw	1 << RA4	;Bit mask
		goto	PowerOpExecute	;Toggle the thermostat interface output
PowerOpHigh	btfss	ReceiveModeB	;Currently in some smart power mode?
		goto	PowerOpBoiler	;Toggle the boiler line output
		btfsc	SmartPowerB	;In medium power mode?
		goto	SmartPowerNoOp	;Nothing to do
;Switching from medium to high is just a matter of inverting the output
PowerOpForce	clrf	active		;No timed action is needed
		goto	PowerOpBoiler	;Toggle the boiler line output
PowerOpMedium	btfss	ReceiveModeB	;Currently in some smart power mode?
		goto	PowerOpBoiler	;Toggle the boiler line output
		btfss	SmartPowerB	;Currently in high power mode?
		goto	SmartPowerNoOp	;Nothing to do
;There is no direct way to go from High Power to Medium Power.
		xorwf	pending,F	;Put the request back in pending
		bcf	active,MEDIUMPOWER ;Go to Low Power mode first
PowerOpLow	btfss	ReceiveModeB	;Currently in raised power mode?
		goto	SmartPowerNoOp	;Nothing to do
PowerOpBoiler	bcf	Request		;Operation on the boiler interface
		movlw	1 << RA3	;Bit mask
PowerOpExecute	clrf	TMR2
		bsf	T2CON,TMR2ON	;Start timer 2
PowerOpDebug	xorwf	PORTA,F		;Invert the appropriate opentherm line
		xorwf	tstatflags,F	;Invert the appropriate smart power bit
		clrf	quarter
		retlw	0

SmartPowerNoOp	clrf	active		;Already in the requested power mode
		goto	RunNextOp	;Check for other pending actions

LEDFuncPointer	addlw	functions - 'A'	;Point into the table of functions
		movwf	FSR0L		;Setup indirect addressing
		return

SwitchLED	skpnz			;Zero bit indicates LED state
		goto	SwitchOffLED
SwitchOnLED	call	LEDFuncPointer
		bsf	INDF0,0
		goto	UpdateLED
SwitchOffLED	call	LEDFuncPointer
		bcf	INDF0,0
UpdateLED	movlw	UserLEDAddress	;Pointer to the User LED function
		xorwf	FSR0L,W
		movlw	LEDMASK		;All LEDs Off
		btfsc	INDF0,0
		movlw	0		;All LEDs On (clrw would set Z bit)
		skpnz			;Not the User LED?
		comf	userleds,W	;Get the inverted state of the User LEDs
		xorwf	PORTB,W		;Find which bits are different
		andwf	INDF0,W		;Mask with the assigned LEDs
		andlw	LEDMASK		;Only consider the LED outputs
		xorwf	PORTB,F		;Update the LEDs
		return

CalcThreshold	movwf	xmitbyte1	;Store the multiplication factor
		movlw	8
		movwf	loopcounter	;Doing 8-bit multiplication
		movfw	ADRESH		;Get the A/D conversion result
		btfsc	ADRESL,7
		addlw	1		;Round the value up
;Multiplication from http://www.piclist.com/Techref/microchip/math/mul/8x8.htm
		clrf	xmitbyte2	;High byte of the product
		rrf	xmitbyte1,F
CalcThresholdL1	skpnc
		addwf	xmitbyte2,F
		rrf	xmitbyte2,F
		rrf	xmitbyte1,F
		decfsz	loopcounter,F
		bra	CalcThresholdL1
;--
		lslf	xmitbyte1,W	;Shift the result one bit to the left
		rlf	xmitbyte2,W	;So the high byte is the final result
		btfsc	xmitbyte1,6
		addlw	1		;Round up
		return

;************************************************************************

		package Print
PrintString	pcall	ReadEpromData	;Read character from EEPROM
		skpnz
		return			;End of string
		call	PrintChar	;Send character to serial port
		banksel	EEADRL		;Bank 3
		incf	EEADRL,W	;Next address
		goto	PrintString

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
PrintDigit	addlw	'0'		;Convert to ASCII digit
PrintChar	tstf	txavailable	;Check if some data is already waiting
		skpnz
		btfss	PIR1,TXIF	;Check the transmit register
		goto	PrintBuffer	;Need to put the byte into the buffer
		banksel	TXREG		;Bank 3
		movwf	TXREG		;Can transmit immediately
		banksel	0		;Bank 0
		retlw	0
PrintBuffer	movwf	FSR0L		;Abuse FSR0L for temporary storage
		movfw	txavailable	;Get number of pending characters
		addwf	txpointer,W	;Add position in the transmit buffer
		addlw	-TXBUFSIZE	;Check for wrap-around
		skpc
		addlw	TXBUFSIZE	;No wrap, recalculate original value
		addlw	txbuffer	;Next free position in transmit buffer
		xorwf	FSR0L,W		;Exchange FSR0L and W
		xorwf	FSR0L,F
		xorwf	FSR0L,W		;--
		movwf	INDF0		;Store byte in the buffer
		movfw	txavailable	;Get number of pending characters
		xorlw	TXBUFSIZE	;Compare against buffer size
		skpz			;Buffer full?
		incf	txavailable,F	;One more character in the buffer
		retlw	0

PrintRcvdMsgT	movlw	'T'
PrintMessageT	call	PrintChar
		movfw	RecvByteT1
		call	PrintHex
		movfw	RecvByteT2
		call	PrintHex
		movfw	RecvByteT3
		call	PrintHex
		movfw	RecvByteT4
		call	PrintHex
		goto	NewLine

PrintRcvdMsgB	movlw	'B'
PrintMessageB	call	PrintChar
		movfw	RecvByteB1
		call	PrintHex
		movfw	RecvByteB2
		call	PrintHex
		movfw	RecvByteB3
		call	PrintHex
		movfw	RecvByteB4
		call	PrintHex
		goto	NewLine

PrintXmitMsg	movlw	'R'
		btfsc	XmitResponse
		movlw	'A'
		call	PrintChar
		movfw	xmitbyte1
		call	PrintHex
		movfw	xmitbyte2
		call	PrintHex
		movfw	xmitbyte3
		call	PrintHex
		movfw	xmitbyte4
		goto	PrintHex

PrintError	btfsc	PowerReport	;Pending power change report?
		return			;Error probably due to the power change
		movlw	ErrorStr
		call	PrintString
		movfw	errornum
		call	PrintHex
		call	NewLine
		movlw	'E'
		btfss	errornum,0
		call	PrintMessageT
		clrf	errornum
		return

PrintBoolean	rlf	CPSCON0,W	;CPSCON0 is initialized to all 0's
		goto	PrintDigit

PrintSetting	movfw	rxpointer
		xorlw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]
		xorlw	'A'
		skpnz
		retlw	PrintSettingA
		xorlw	'A' ^ 'T'
		skpnz
		goto	PrintSettingT
		xorlw	'T' ^ 'V'
		skpnz
		goto	PrintSettingV
		retlw	BadValue

PrintSettingID	moviw	3[FSR1]
		call	PrintChar
		movlw	'='
		goto	PrintChar

PrintSettingT	call	PrintSettingID
PrintTweaks	swapf	settings,W
		movwf	temp
		rrf	temp,F
		rrf	temp,F
		goto	PrintBoolean

PrintSettingV	call	PrintSettingID
PrintRefVoltage lsrf	settings,W
		addlw	-6
		goto	PrintXChar

;************************************************************************
; Parse commands received on the serial interface
;************************************************************************
		package Serial
SerialCommand	btfsc	Overrun
		goto	SerialOverrun
SerialCmdSub	movlw	4
		subwf	rxpointer,W	;Check the length of the command >= 4
		skpc
		retlw	SyntaxError	;Invalid command
		movlw	high rxbuffer
		movwf	FSR1H
		movlw	low rxbuffer
		movwf	FSR1L
		moviw	2[FSR1]		;Third character of every command ...
		sublw	'='		;... must be '='
		skpz
		goto	CmdMessage
		moviw	1[FSR1]		;Get the 2nd character of the command
		xorwf	INDF1,W		;Combine with the 1st character
		movwf	temp		;Save for later use
		andlw	~b'11111'	;First quick check for a valid command
		skpz
		retlw	CommandNG	;Command is not valid
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
		movfw	temp
		brw			;Make a quick initial selection
; Calculate table index with Tcl: tcl::mathop::^ {*}[scan SB %c%c]
SerialCmdTable	retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmd02	; PR command
		goto	SerialCmd03	; SP command
		goto	SerialCmd04	; VR command
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmdLED	; LD command
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmdLED	; LA command
		goto	SerialCmdLED	; LB command
		goto	SerialCmdLED	; LC command
		goto	SerialCmd10	; GW command
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmd14	; DP command
		goto	SerialCmd15	; EP command
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		retlw	CommandNG
		goto	SerialCmd1D	; IT command
		retlw	CommandNG
		retlw	CommandNG

SerialOverrun	bcf	Overrun
		retlw	OverrunError

SerialCmd02	movfw	INDF1
		sublw	'P'
		skpnz
		goto	ReportSetting
		retlw	CommandNG

SerialCmd03	movfw	INDF1
		sublw	'S'
		skpnz
		goto	SetSmartPower
		retlw	CommandNG

SerialCmd04	movfw	INDF1
		sublw	'V'
		skpnz
		goto	SetVoltageRef
		retlw	CommandNG

SerialCmd10	movfw	INDF1
		sublw	'G'
		skpnz
		goto	SetGatewayMode
		retlw	CommandNG

SerialCmd14	movfw	INDF1
		sublw	'D'
		skpnz
		goto	SetDebugPtr
		retlw	CommandNG

SerialCmd15	movfw	INDF1
		sublw	'E'
		skpnz
		goto	EnableSmartPwr
		retlw	CommandNG

SerialCmd1D	movfw	INDF1
		sublw	'I'
		skpnz
		goto	IgnoreError1
		retlw	CommandNG

SerialCmdLED	movfw	INDF1
		sublw	'L'
		skpz
		retlw	CommandNG	;LED commands must start with 'L'
		movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError	;Command length must be 4
		moviw	1[FSR1]
		addlw	-'A'		;Get LED number
		movwf	float1
		movlw	b'00001000'	;Bit mask for LED 0
		btfsc	float1,1
		movlw	b'01000000'	;Bit mask for LED 2
		movwf	funcmask	;Save the bit mask
		btfsc	float1,0
		addwf	funcmask,F	;Shift the bit mask for odd LEDs
		call	CheckBoolean
		skpnc
		goto	SerialCmdUsrLED
		moviw	3[FSR1]
		movwf	temp		;Keep func code for storing in EEPROM
		sublw	'Z'
		sublw	'Z' - 'A'	;Check for valid function code
		skpc
		retlw	BadValue	;Valid functions are 'A' - 'Z'
		movfw	float1
		addlw	FunctionLED1	;Calculate EEPROM address
		call	WriteEpromData	;Save the LED configuration in EEPROM
		moviw	3[FSR1]
		pcall	PrintChar
		;Remove the old function for the LED
		movlw	functions	;Start of the function table
		movwf	FSR0L		;Setup indirect adressing
		movlw	26		;Size of the table
		movwf	loopcounter
		comf	funcmask,W	;Invert the bit mask
SetLEDLoop	andwf	INDF0,F		;Remove the LED from the function flags
		incf	FSR0L,F		;Point to the next function
		decfsz	loopcounter,F
		goto	SetLEDLoop	;Repeat the loop
		;Setup the new function for the LED
		moviw	3[FSR1]		;Name of the new function
SetLEDFunction	addlw	functions - 'A'	;Pointer into the function table
		movwf	FSR0L		;Setup indirect addressing
		movfw	funcmask	;Reload the bit mask for the LED
		iorwf	INDF0,F		;Link the LED to the selected function
		lgoto	UpdateLED	;Set the initial state of the LED

SerialCmdUsrLED addfsr	FSR1,3
		movfw	INDF1
		pcall	PrintChar
		movfw	funcmask
		xorwf	userleds,W
		btfss	INDF1,0
		xorwf	funcmask,W
		andwf	funcmask,W
		xorwf	userleds,F
		movlw	UserLEDAddress	;Point to the User LED function
		movwf	FSR0L		;Setup indirect addressing
		lgoto	UpdateLED

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

ReportSetting	lgoto	PrintSetting

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
ShortPointer	movwf	temp2		;Save the high byte
		call	GetHexadecimal	;Get the low byte
		skpnc
		return
		movwf	FSR1L		;Store the low byte
		movfw	temp2
		movwf	FSR1H
		lcall	PrintXChar
		movfw	FSR1L
		call	PrintHex
		movlw	'='
		call	PrintChar
		movfw	INDF1
		goto	PrintHex

;Set the reference voltage:
;0 = 0.625V	1 = 0.833V	2 = 1.042V	3 = 1.250V	4 = 1.458V
;5 = 1.667V	6 = 1.875V	7 = 2.083V	8 = 2.292V	9 = 2.500V
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
		banksel	settings	;Bank 0
		xorwf	settings,W
		andlw	b'00011111'
		xorwf	settings,W
		movwf	settings
		call	SaveSettings
		lgoto	PrintRefVoltage

SaveSettings	movwf	temp
		movlw	SavedSettings
;Write to data EEPROM. Data must be in temp, address in W
WriteEpromData	pcall	EepromWait	;Wait for any pending EEPROM activity
		banksel	EEADR
		movwf	EEADR		;Setup the EEPROM data address
		movfw	temp		;Get the value to store in EEPROM
StoreEpromData
		banksel	EECON1
		bsf	EECON1,RD	;Read the current value
		banksel	EEDATL
		xorwf	EEDATL,W	;Check if the write can be skipped
		skpnz
		goto	StoreEpromSkip	;Prevent unnecessary EEPROM writes
		xorwf	EEDATL,F	;Load the data value
		banksel	EECON1
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
StoreEpromSkip
		banksel	0		;Switch back to bank 0
		movfw	temp		;Return the byte that was written
		return

BitMask		clrc
		movwf	temp2
		movlw	b'1'
		btfsc	temp2,1
		movlw	b'100'
		movwf	temp
		btfsc	temp2,0
		rlf	temp,F
		btfsc	temp2,2
		swapf	temp,F
		swapf	temp2,F
		rlf	temp2,W
		rlf	temp2,W
		andlw	b'11111'
		return

;There have been reports that some equipment does not produce clean transitions
;between the two logical Opentherm signal levels. As a result the gateway may
;produce frequent Error 01 reports. By setting the IT flag, the requirement for
;a maximum of one mid-bit transition is no longer checked.
;When this flag is set, Error 01 will never occur.
IgnoreError1	call	CheckBoolean
		skpc
		return
		movlw	1 << 5		;Mask for IgnoreErr1
SetSetting	addfsr	FSR1,3
		btfsc	INDF1,0
		iorwf	settings,F	;Set the option
		xorlw	-1		;Invert the mas
		btfss	INDF1,0
		andwf	settings,F	;Clear the option
		movfw	settings
		call	SaveSettings	;Store the setting in EEPROM
		movfw	INDF1
		lgoto	PrintChar

EnableSmartPwr	call	CheckBoolean
		skpc
		return
		skpz
		bsf	SmartPower
		skpnz
		bcf	SmartPower
		moviw	3[FSR1]
		lgoto	PrintChar

SetSmartPower	movfw	rxpointer
		sublw	4
		skpz
		retlw	SyntaxError
		moviw	3[FSR1]
		xorlw	'L'
		skpnz
		goto	SetSmartPowerL
		xorlw	'L' ^ 'M'
		skpnz
		goto	SetSmartPowerM
		xorlw	'M' ^ 'H'
		skpnz
		goto	SetSmartPowerH
		xorlw	'H' ^ 'T'
		skpnz
		goto	SetSmartPowerT
		xorlw	'T' ^ 'B'
		skpz
		retlw	BadValue
SetSmartPowerB	movlw	1 << BOILERINVERT
		bra	SetSmartPowerOK
SetSmartPowerT	movlw	1 << TSTATINVERT
		bra	SetSmartPowerOK
SetSmartPowerH	movlw	1 << HIGHPOWER
		bra	SetSmartPowerOK
SetSmartPowerM	movlw	1 << MEDIUMPOWER
SetSmartPowerL
SetSmartPowerOK iorwf	pending,F
		bsf	pending,SMARTPOWER
		moviw	3[FSR1]
		lcall	PrintChar
		lgoto	RunOperation

SetGatewayMode	movfw	rxpointer	;Check the command is 4 characters long
		sublw	4
		skpz
		retlw	SyntaxError
ResetGateway	moviw	3[FSR1]
		sublw	'R'		;GW=R resets the gateway
		skpz
		retlw	BadValue
		reset

CmdMessage	movfw	rxpointer
		xorlw	8		;Check the command is 8 characters long
		skpz
		retlw	SyntaxError	;Wrong length
		movlw	rxbuffer
		movwf	FSR1L
		call	GetHexadecimal
		skpnc
		return
		movwf	xmitbyte1
		call	GetHexadecimal
		skpnc
		return
		movwf	xmitbyte2
		call	GetHexadecimal
		skpnc
		return
		movwf	xmitbyte3
		call	GetHexadecimal
		skpnc
		return
		movwf	xmitbyte4
		bsf	pending,MESSAGE
		lcall	RunOperation
		retlw	-1

CmdArgPointer	movlw	rxbuffer + 3
		movwf	FSR1L
		return

;Returns the decimal value found. Carry is set on error
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
		movwf	result
		movlw	26
		subwf	temp,W
		skpnc
		retlw	OutOfRange
		movfw	temp
		call	Multiply10
		addwf	result,W
		skpc
		goto	GetDecLoop
		retlw	OutOfRange
GetDecReturn	movfw	temp
		return
CarrySyntaxErr	setc
		retlw	SyntaxError

GetHexArg	call	CmdArgPointer
GetHexadecimal	call	GetHexDigit
		skpnc
		return
		movwf	temp
		swapf	temp,F
		call	GetHexDigit
		skpc
		iorwf	temp,W
		return

GetHexDigit	movlw	'0'
		subwf	INDF1,W
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
		incf	FSR1L,F
		clrc
		return

Multiply10	movwf	remainder	;Store a copy in a temporary variable
		addwf	remainder,F	;Add again to get: value * 2(clears C)
		rlf	remainder,F	;Shift left gives: value * 4
		addwf	remainder,F	;Add original value: value * 5
		rlf	remainder,W	;Shift left for: value * 10
		return			;Return the result

;########################################################################

;Initialize EEPROM data memory

DEEPROM		code
SavedSettings	de	23 | 1 << 5	;Set IgnoreErr1
FunctionLED1	de	'U'
FunctionLED2	de	'U'
FunctionLED3	de	'U'
FunctionLED4	de	'U'
FunctionLED5	de	'U'
FunctionLED6	de	'U'
NullString	de	0
PrintSettingA	de	"A="
GreetingStr	de	"OpenTherm Interface ", version
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
TDisconnect	de	"Thermostat disconnected", 0
TConnect	de	"Thermostat connected", 0
CommandNG	de	"NG", 0
SyntaxError	de	"SE", 0
BadValue	de	"BV", 0
OutOfRange	de	"OR", 0
OverrunError	de	"OE", 0
MediumPowerStr	de	"Medium", 0
HighPowerStr	de	"High", 0
NormalPowerStr	de	"Low"
PowerStr	de	" power", 0
ErrorStr	de	"Error ", 0
TimeStamp	de	tstamp, 0
SpeedString	de	mhzstr, " MHz", 0
		end
