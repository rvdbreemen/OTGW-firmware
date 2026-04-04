		title		"Self Programming"
		list		p=16F1847, b=8, r=dec

;Copyright (c) 2022 - Schelte Bron

;########################################################################
; Self programming, based on Microchip AN851
;########################################################################
; This code has been optimized for size so it would fit in 0x100 code words.
; To achieve this, some special tricks had to be used. As a result the code
; may be a little hard to read
;
; The functionality differs from that described in AN851 in the following
; respects:
; 1. No auto baud detection. Communication must take place at 9600 baud.
; 2. Dropped the first <STX> that was used for auto baud detection.
; 3. Dropped the third address byte as it would always be 0.
; 4. The code does not check the last byte of EEPROM data memory to enter boot
;    mode. Instead it transmits a <ETX> character and waits up to 1 second for
;    a <STX> character.
; 5. The code is designed to be placed in the highest part of memory. That way
;    it doesn't interfere with the interrupt code address. It does rely on the
;    main program to call the self-program code at an appropriate time.
; 6. Due to the location of the boot loader code, it cannot be protected using
;    the code protection bits in the configuration word. This means that the
;    boot loader code can also be updated, if necessary.
; 7. The Erase Flash command has been implemented for the PIC16F device.
; 8. The device reset command is 0x08, not any command with a 0 length.
; 9. The version command can be called with a data length of 1-3 words. It will
;    report the following information:
;    1. Version
;    2. Boot loader start address
;    3. Boot loader end address
;    The software used to reflash the device can use the last two pieces of
;    information to determine which part of memory should not be touched.

#include	"p16f1847.inc"

		errorlevel	-302, -306

#define		MAJOR_VERSION	2
#define		MINOR_VERSION	0

#define		STX	0x0F
#define		ETX	0x04
#define		DLE	0x05

CHKSUM		equ	0x70
COUNTER		equ	0x71
RXDATA		equ	0x72
TXDATA		equ	0x73
TEMP		equ	0x74

DATA_BUFF	equ	0x20	;Start of receive buffer
COMMAND		equ	0x0	;Data offsets in receive buffer
DATA_COUNT	equ	0x1
ADDRESS_L	equ	0x2
ADDRESS_H	equ	0x3
PACKET_DATA	equ	0x4

#define		RX		PORTB,2
#define		TX		PORTB,5

#ifdef		SELFPROGNEW
  #define	SELFPROG	SelfProgNew
  #define	STARTADDRESS	0x1700
#else
  #define	SELFPROG	SelfProg
  #define	STARTADDRESS	0x1f00
#endif

		global	SELFPROG
SELFPROG	code	STARTADDRESS
SELFPROG
		;Do not go into selfprog mode after a watchdog timeout reset
		btfss	STATUS,NOT_TO
		return			;Let the main code handle a timeout

		bcf	INTCON,GIE	;Ignore interrupts

		banksel	OSCCON		;Bank 1
		movlw	b'01101010'	;Internal oscillator set to 4MHz
		movwf	OSCCON		;Configure the oscillator
		;Configure timer 0
		movlw	b'11010111'	;Pull-ups disabled, 1:256 prescaler
		movwf	OPTION_REG

		;Configure the comparators and voltage reference
		;Allow communication between thermostat and boiler to continue
		;while reprogramming the device
		banksel	LATB		;Bank 2
		movlw	b'11111111'
		movwf	LATB		;Clear the LEDs
		;Configure the fixed voltage reference
		movlw	b'10001000'	;DAC fixed voltage is 2.048V
		movwf	FVRCON
		;Configure the DAC
		movlw	b'11001000'	;DAC voltage between Vss and 2.048V
		movwf	DACCON0
		movlw	23		;DAC output voltage is 1.472V
		movwf	DACCON1
		;Configure comparator module
		movlw	b'00010000'	;INV = RA0, Non-INV = DAC
		movwf	CM1CON1
		movlw	b'00010001'	;INV = RA1, Non-INV = DAC
		movwf	CM2CON1
		movlw	b'10100110'	;Not inverted, output on CxOUT pin
		movwf	CM1CON0
		movwf	CM2CON0
		bsf	APFCON1,TXCKSEL	;TX on RB5
		bsf	APFCON0,RXDTSEL	;RX on RB2

		banksel	TRISA		;Bank 1
		movlw	b'11100111'	;Pins 3 and 4 are digital ouputs
		movwf	TRISA
		;Set the LEDS as outputs
		movlw	b'00000111'
		movwf	TRISB

		banksel	ANSELA		;Bank 3
		movlw	b'00000111'	;A0 through A2 are used for analog I/O
		movwf	ANSELA
		clrf	ANSELB		;No analog I/O on port B
		;Configure the serial interface
		movlw	25		;9600 baud
		movwf	SPBRG
		movlw	b'00100110'
		movwf	TXSTA		;High speed, Asynchronous, Enabled
		bsf	RCSTA,SPEN	;Enable serial port
		bsf	RCSTA,CREN	;Enable receive

;########################################################################
;Nearly all needed registers (EEPROM, EUSART) are in bank 3. Only PIR1
;and TMR0 are needed in bank 0. FSR1 will be used to access those.
;########################################################################

		clrf	FSR1H
		movlw	PIR1
		movwf	FSR1L
		clrw
		movwi	TMR0-PIR1[FSR1]	;Clear timer 0
		movlw	1
		call	Pause
;Notify the external programming software (Pause returns <ETX>)
		call	WrRS232
;Programmer should react within 1 second
WaitForSTX	movlw	16
		call	Pause
		btfss	INDF1,RCIF
;No data received, resume the normal program
		return
;Check that the received character is <STX>
		call	RdRS232
		skpz
;Other serial data received, resume normal program
		return

ReSync		movlw	high 0x2000
		movwf	FSR0H
		clrf	FSR0L
		clrf	CHKSUM
GetNextDat	call	RdRS232		;Get the data
		skpnz
		bra	ReSync		;Found unprotected STX
		xorlw	STX ^ ETX	;Check for ETX
		skpnz
		bra	CheckSum	;Yes, examine checksum
		xorlw	ETX ^ DLE	;Check for DLE
		skpnz
		call	RdRS232		;Yes, get the next byte
		movfw	RXDATA
		movwi	FSR0++		;Store the data
		addwf	CHKSUM,F	;Get sum
		bra	GetNextDat

CheckSum	tstf	CHKSUM
		skpz
		bra	StartOfLine
		clrf	FSR0L
		clrf	EECON1
		moviw	ADDRESS_L[FSR0]
		movwf	EEADRL
		moviw	ADDRESS_H[FSR0]
		movwf	EEADRH
		moviw	DATA_COUNT[FSR0]
		movwf	COUNTER
		movlw	PACKET_DATA
		addwf	FSR0L,F
CheckCommand	moviw	COMMAND-PACKET_DATA[FSR0]
		sublw	8
		sublw	8
		skpc
		bra	StartOfLine
		brw
		bra	ReadVersion	;0 Read Version Information
		bra	ReadProgMem	;1 Read Program Memory
		bra	WriteProgMem	;2 Write Program Memory
		bra	EraseProgMem	;3 Erase Program Memory
		bra	ReadEE		;4 Read EEDATA Memory
		bra	WriteEE		;5 Write EEDATA Memory
		bra	ReadConfig	;6 Read Config Memory
		bra	StartOfLine	;7 Write Config Memory
		reset			;8 Reset

VersionData	data	(MAJOR_VERSION << 8) | MINOR_VERSION
#ifdef SELFPROGNEW
		data	SELFPROG + 0x800, SelfProgEnd +	0x800
#else
		data	SELFPROG, SelfProgEnd
#endif

ReadConfig	bsf	EECON1,CFGS
		bra	ReadProgMem
ReadVersion	movlw	low VersionData
		movwf	EEADRL
#ifdef SELFPROGNEW
		movlw	high (VersionData + 0x800)
#else
		movlw	high VersionData
#endif
		movwf	EEADRH
		movlw	2
		movwf	FSR0L
ReadProgMem	bsf	EECON1,EEPGD
		bsf	EECON1,RD	;Initiate read
		nop
		nop
		movfw	EEDATL		;Get LSB of word
		movwi	FSR0++		;Store in user location
		movfw	EEDATH		;Get MSB of word
		movwi	FSR0++		;Store in user location
		incf	EEADRL,F
		skpnz
		incf	EEADRH,F
		decfsz	COUNTER,F
		bra	ReadProgMem
WritePacket	movfw	FSR0L
WriteAck	movwf	COUNTER
		bcf	EECON1,WREN	;Prevent accidental writes
		clrf	FSR0L
		movlw	STX
		call	WrRS232
		clrf	CHKSUM
SendNext	moviw	FSR0++
		addwf	CHKSUM,F
		call	WrData
		decfsz	COUNTER,F
		bra	SendNext
		comf	CHKSUM,W
		addlw	1
		call	WrData		;WrData returns <ETX>
		call	WrRS232
StartOfLine	call	RdRS232		;Look for a start of line
		skpnz
		bra	ReSync
		bra	StartOfLine

WriteProgMem	movlw	1 << EEPGD | 1 << LWLO | 1 << WREN
		movwf	EECON1
		moviw	FSR0++
		movwf	EEDATL
		moviw	FSR0++
		movwf	EEDATH
		comf	EEADRL,W
		andlw	b'11111'
		skpnz
		bra	CommitProgMem	;Last address of the row
		decf	COUNTER,W
		skpnz
CommitProgMem	bcf	EECON1,LWLO	;Last address in the command
		call	StartWrite	;W = 1
		decfsz	COUNTER,F
		bra	WriteProgMem
		bra	WriteAck

EraseProgMem	movlw	1 << EEPGD | 1 << FREE | 1 << WREN
		movwf	EECON1
		movlw	0x1F
		iorwf	EEADRL,F
		call	StartWrite	;W = 1
		decfsz	COUNTER,F
		bra	EraseProgMem
		bra	WriteAck

ReadEE		clrf	EECON1
		bsf	EECON1,RD
		movfw	EEDATL
		movwi	FSR0++
		incf	EEADRL,F
		decfsz	COUNTER,F
		bra	ReadEE
		bra	WritePacket

WriteEE		movlw	1 << WREN
		movwf	EECON1
		moviw	FSR0++
		movwf	EEDATL
		call	StartWrite
		decfsz	COUNTER,F
		bra	WriteEE
		bra	WriteAck

WrData		movwf	TXDATA
		xorlw	STX
		skpnz
		bra	WrDLE
		xorlw	STX ^ ETX
		skpnz
		bra	WrDLE
		xorlw	ETX ^ DLE
		skpz
		bra	WrNext
WrDLE		movlw	DLE
		call	WrRS232
WrNext		movfw	TXDATA
WrRS232		clrwdt
WaitTxEmpty	btfss	INDF1,TXIF
		bra	WaitTxEmpty
		movwf	TXREG
		retlw	ETX

RdRS232		btfsc	RCSTA,OERR
		reset
RdLp1		clrwdt
		btfss	INDF1,RCIF
		bra	RdLp1
		movfw	RCREG
		movwf	RXDATA
		xorlw	STX
		return

StartWrite	clrwdt
		movlw	0x55
		movwf	EECON2
		movlw	0xAA
		movwf	EECON2
		bsf	EECON1,WR
WaitEEWrite	btfsc	EECON1,WR	;Skipped when writing program memory
		bra	WaitEEWrite	;Skipped when writing program memory
		incf	EEADR,F
		skpnz
		incf	EEADRH,F
		retlw	1		;Return length of acknowledgement

Pause		clrwdt
		btfsc	INDF1,RCIF
		return
		btfss	INTCON,TMR0IF
		bra	Pause
		bcf	INTCON,TMR0IF
		decfsz	WREG,W
		bra	Pause
SelfProgEnd	retlw	ETX

		end
