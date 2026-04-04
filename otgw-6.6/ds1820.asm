		title		"Temperature sensor"
		list		p=16F1847, b=8, r=dec

;Copyright (c) 2022 - Schelte Bron

#include	"p16f1847.inc"
		errorlevel	-302, -306

#define CHECKCRC
#ifdef NOCHECKCRC
#undefine CHECKCRC
#endif

#define		IOPORT	PORTA,7		;I/O port used for 1-wire line

;1-wire ROM commands
#define		READROM		0x33
#define		SKIPROM		0xCC
;1-wire FUNCTION commands
#define		CONVERTT	0x44
#define		READSCRATCHPAD	0xBE
;1-wire family codes for supported temperature sensors
#define		DS_FAM_DS18S20	0x10
#define		DS_FAM_DS18B20	0x28
#define		DS_FAM_DS1822	0x22
;Return codes, controlling when to perform the next step
#define		DONE		0	;Next step after one second
#define		CONTINUE	1	;Next step as soon as possible

Bank0data	UDATA
sensorstage	res	1
#define		ds18b20		sensorstage,7
lsbstorage	res	1
#ifdef CHECKCRC
msbstorage	res	1
crc		res	1
#endif
		extern	float1, float2, temp, StoreTempValue

		global	Temperature
Temperature	CODE
Temperature	skpz			;Zero bit indicates no message in prog
		goto	Restart		;Sequence interrupted by OT message
		;Split up the temperature measurement into several short steps,
		;each less than 1ms. That way the main loop is allowed to run
		;frequently enough for it to be able to do all the necessary
		;processing in time, like clearing out the serial receive
		;register.
		movfw	sensorstage
		andlw	b'11111'
		incf	sensorstage,F	;Determine next step of the process
		brw			;Jump to the current step
StageTable	goto	PresencePulse	;Step 0 - Transmit a reset pulse
		goto	SkipRom		;Step 1 - Address all devices on the bus
		goto	Measure		;Step 2 - Start a measurement
		retlw	DONE		;Step 3 - Allow time for measurement
		goto	PresencePulse	;Step 4 - Transmit a reset pulse
		goto	ReadRom		;Step 5 - Send Read ROM command
		goto	ReadFamily	;Step 6 - Read device family code
		goto	PresencePulse	;Step 7 - Transmit a reset pulse
		goto	SkipRom		;Step 8 - Address all devices
		goto	ReadCommand	;Step 9 - Send Read Scratchpad command
		goto	ReadLSB		;Step 10 - Read temperature value, LSB
		goto	ReadMSB		;Step 11 - Read temperature value, MSB
#ifdef CHECKCRC
		goto	ReadByte	;Step 12 - Read user byte 1
		goto	ReadByte	;Step 13 - Read user byte 2
		goto	ReadByte	;Step 14 - Read config register (18B20)
		goto	ReadByte	;Step 15 - Read reserved byte
		goto	ReadByte	;Step 16 - Read count remain (18S20)
		goto	ReadByte	;Step 17 - Read count per C (18S20)
		goto	ReadCRC		;Step 18 - Read CRC
#endif
Restart		setc			;Step 12/19 - Start over
		call	ReportResult	;Report the result back
		retlw	DONE

PresencePulse	bcf	IOPORT		;Prepare the output latch
		banksel	TRISA
		bcf	IOPORT		;Drive the output port low
		call	Delay480	;Wait for the minimum reset duration
		banksel	TRISA
		bsf	IOPORT		;Release the I/O port
		banksel	PORTA
		call	Delay60		;Allow the device time to respond
		btfsc	IOPORT		;Check the 1-wire line
		goto	Restart		;No device detected
		call	Delay240	;Wait for the presence pulse to end
		retlw	CONTINUE

Measure		movlw	CONVERTT	;Initiate temperature conversion
		call	WriteByte	;Send the command on the 1-wire bus
		retlw	DONE		;Give devices time to do the conversion

ReadFamily	call	ReadByte	;Read family code from the 1-wire bus
		movfw	temp
		xorlw	DS_FAM_DS18S20	;Check if the sensor is a DS18S20
		skpnz
		retlw	CONTINUE	;Found a DS18S20
		bsf	ds18b20
		xorlw	DS_FAM_DS18S20 ^ DS_FAM_DS18B20
		skpnz
		retlw	CONTINUE	;Found a DS18B20
		xorlw	DS_FAM_DS18B20 ^ DS_FAM_DS1822
		skpnz
		retlw	CONTINUE	;Found a DS1822
		goto	Restart		;Not a supported temperature sensor

ReadCommand	movlw	READSCRATCHPAD	;The scratchpad contains the temerature
		goto	WriteByte	;Send the command on the 1-wire bus

ReadLSB
#ifdef CHECKCRC
		clrf	crc
#endif
		call	ReadByte	;Read the first scratchpad byte
		movfw	temp
		movwf	lsbstorage	;Save the byte for later use
		retlw	CONTINUE

ReadMSB		call	ReadByte	;Read the second scratchpad byte
#ifdef CHECKCRC
		movfw	temp
		movwf	msbstorage
		retlw	CONTINUE
#endif
CalcTemperature	btfsc	ds18b20		;Check which sensor type was found
		goto	HighPrecision	;A DS18B20 stores the value differently
		rrf	temp,W		;Sign of the temperature value
		rrf	lsbstorage,W	;Determine the full degrees
		movwf	float1
		clrf	float2
		rrf	float2,F	;Determine the half degrees
		goto	Success
HighPrecision	swapf	lsbstorage,W	;Get the 1/16 degrees in the high nibble
		andlw	b'11110000'
		movwf	float2		;Store as temperture low byte
		swapf	temp,W		;Get the upper nibble of the degrees
		andlw	b'11110000'
		movwf	temp		;Save for later use
		swapf	lsbstorage,W
		andlw	b'1111'		;Get the lower nibble of the degrees
		iorwf	temp,W		;Combine with the saved upper nibble
		movwf	float1		;Store as temperature high byte
Success		xorlw	85		;Test against powerup value
		skpnz
		goto	Restart		;Not a valid measurement
		clrc			;Temperature reading is valid
ReportResult	lcall	StoreTempValue	;Store the measurement
		clrf	sensorstage	;Start a new round
		retlw	CONTINUE

#ifdef CHECKCRC
ReadCRC		call	ReadByte	;Read the CRC
		tstf	crc		;Check that the CRC ended up at 0
		skpz			;CRC OK
		goto	Restart		;Not a valid measurement
		movfw	msbstorage
		movwf	temp
		goto	CalcTemperature
#endif

Delay480	call	Delay240	;480 = 2 * 240
Delay240	movlw	240 - 8		;Value for 240uS delay (compensated)
		goto	Delay		;Wait for the specified amount of time
Delay60		movlw	60 - 6		;Value for 60uS delay (compensated)
Delay
		banksel	PIE1		;Bank 1
		bcf	PIE1,TMR2IE	;Disable interrupt from timer 2
		banksel	TMR2		;Bank 0
		subwf	PR2,W		;Calculate the start value
		addlw	10		;Overhead for starting and stopping
		movwf	TMR2		;Load the timer
		bsf	T2CON,TMR2ON	;Start timer 2
WaitTimer2	btfss	PIR1,TMR2IF	;Check if timer 2 expired
		goto	WaitTimer2	;Wait some more
		bcf	T2CON,TMR2ON	;Stop timer 2
		bcf	PIR1,TMR2IF	;Prevent unwanted interrupt
		return

Delay15		call	Delay5		;15 = 3 * 5
Delay10		call	Delay5		;10 = 2 * 5
Delay5		nop			;Wait 1 us
		return			;Subroutine Call + Return = 2us + 2us

ReadByte	call	ReadNibble	;Read 4 bits from 1-wire bus
ReadNibble	call	ReadBit		;Read 1 bit from 1-wire bus
		call	ReadBit		;Read 1 bit from 1-wire bus
		call	ReadBit		;Read 1 bit from 1-wire bus
ReadBit		bcf	IOPORT		;Prepare output latch
		banksel	TRISA		;TRIS register is in Bank 1
		bcf	IOPORT		;Drive the 1-wire line low
		clrc			;Prepare carry while waiting
		bsf	IOPORT		;Release the 1-wire bus
		banksel	PORTA		;Back to bank 0
		call	Delay10		;Allow device time to write the bit
		btfsc	IOPORT		;Read the 1-wire bus
		setc			;Device wrote a "1"
		rrf	temp,F		;Shift bit into the result variable
#ifdef CHECKCRC
		rlf	temp,W		;Recover the input bit into the carry
		rlf	CPSCON0,W	;Shift carry into bit 0 & clear carry
		xorwf	crc,F		;Apply XOR of CRC bit 0 and the input
		rrf	crc,W		;Shift the CRC right
		skpnc
		xorlw	0x8c		;Apply the polynomial function
		movwf	crc		;Store the new CRC value
#endif
		movlw	45		;Maximum time a device drives the bus
		call	Delay		;Wait for the device to release the bus
		retlw	CONTINUE

ReadRom		movlw	READROM		;Command to read a device's ROM code
		goto	WriteByte	;Send the command on the 1-wire bus
SkipRom		movlw	SKIPROM		;Address all devices on the 1-wire bus
WriteByte	movwf	temp		;Temporarily store the 1-wire command
		call	WriteNibble	;Write 4 bits to the 1-wire bus
WriteNibble	call	WriteBit	;Write 1 bit to the 1-wire bus
		call	WriteBit	;Write 1 bit to the 1-wire bus
		call	WriteBit	;Write 1 bit to the 1-wire bus
WriteBit	rrf	temp,F		;Set carry to least significant bit
WriteSlot	bcf	IOPORT		;Prepare the output latch
		banksel	TRISA
		bcf	IOPORT		;Drive the 1-wire line low
		skpnc			;Carry contains the bit to send
		bsf	IOPORT		;Release the 1-wire line
		movlw	60 - 10		;Value for 60uS delay (compensated)
		call	Delay		;Bit duration
		banksel	TRISA		;Bank 1
		bsf	IOPORT		;Float the 1-wire line, if necessary
		banksel	PORTA		;Bank 0
		retlw	CONTINUE

		end
