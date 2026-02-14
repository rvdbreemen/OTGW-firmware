# OpenTherm Protocol Specification v4.2 (Markdown-conversie)

Bronbestand: `OpenTherm-Protocol-Specification-v4.2.pdf`

> Let op: dit is een automatische tekstextractie uit de PDF en kan kleine opmaakverschillen bevatten t.o.v. het origineel.

Totaal pagina's: 52

## Pagina 1

OpenTherm™ Protocol Specification v4.2  10 November 2020
©1996-2020 The OpenTherm Association  Page 1

The OpenTherm™
Communications Protocol

A Point-to-Point Communications System
for HVAC Controls

Protocol Specification

The OpenTherm Association is an independent European organisation, constituted under Dutch law, whose object is to
promote the introduction and adoption of the OpenTherm technical standard for HVAC system control communication,  laid
down in this Protocol Specification. The OpenTherm Association controls the application by, and the granting of licenc es
for use of, the OpenTherm trademark and logo.

OpenTherm, OpenTherm/Plus, OpenTherm/Lite and the OpenTherm logo are registered trademarks of The OpenTherm
Association.

## Pagina 2

OpenTherm™ Protocol Specification v4.2  10 November 2020
©1996-2020 The OpenTherm Association  Page 2

Change Control History

Version Date Description of Changes
1.0  First official release
1.1 1 Feb 1998 2.3 OT/+ & OT/- configuration section expanded.
4.2 Bit-order specified in frame format.
5.1 Definition of data format expanded and clarified.
5.3 Message directions changed to indicate read/write instead.
5.3.1 Special status exchange message mechanism defined.
6.1 Clarified duty cycle period definition for OT/- signalling.
1.2A DRAFT
13 Mar 98
This change control history added.
Page numbering corrected.
References to Oem/Customer ID Codes changed to Member ID Code
3.2,High-voltage idle-line state is allowed subject to certain restrictions.
3.2.1.1,3.2.2.1 Current, voltage signal level conditions stated.
3.2.1.2,3.2.2.2 Inductance and capacitance test conditions defined.
3.3 New section added on device impedance characteristics.
3.5 New section added on short-circuit feature.
5.1 Definition of data format further clarified.
5.2 Correction to message type references.
1.2B DRAFT
15 June 98
2.3 State diagram added for OT/+ & OT/- detection.
3.2 Corrected reference to section 3.6
      Idle line-high removed - not acceptable to all members.
3.2.1.1 Allowed spec to be different for OT/- and OT/+
3.2.1.2 Changed “across” to “in series with” (twice)
3.2.2.1 corrected reference to section 3.6
3.3 Changed “capacitance across” to “serial inductance at”
3.6 State machine for short-circuit feature
      Idle line-high removed - not acceptable to all members.
4.4.1 removed references to 0x00 to allow more general case.
4.4.2 removed references to 0x00 to allow for slave changing data-value.
4.4.3 removed references to 0x00 to allow more general case.
4.5 Added that master & slave should notify communication error.
5.2 changed references to invalid-data-id to Unknown_DataID
1.2C DRAFT
19 June 98
2.3 Automatic OT plus-lite configuration made mandatory for masters.
3.2 Clarified allowable line signal states.
3.6 Short-circuit detection defined between 5 .. 15 secs.
4.3.1 Inter-message communications period defined as 1.15 sec max.
4.5 Error notification text removed as irrelevant and not always practical.
5.1 Added u16 and s16 data formats for completeness.
5.3 Unused bits/bytes defined as “reserved” (don’t care values) instead of 00.
1.2D DRAFT
25 June 98
5.1 Reserved/unused bits/bytes should be set to zero, but not checked by receiver.
1.2 RELEASE
25 June 1998
Approved by members of the OpenTherm Association.
1.3A DRAFT
5 May 1999
3.4.2.4 Galvanic isolation specified according to EN60730-1
5.2 CH-enable and DHW-enable are now declared mandatory for the master.
5.1 & 5.4 “Member” or “OEM” data id area (128..255) redefined as “Test & Diagnostics” area.
1.3B DRAFT
17 August 1999
3.2.1.2 Signal dynamic characteristics further clarified in line with test requirements
3.2.2.2 Signal dynamic characteristics further clarified in line with test requirements
3.3 Device Impedance Characteristics removed
4.3.1 Message latency time increased to 100ms
New Data ID’s added according to
 NDI 110399-1 : Product Version Number
 NDI 110399-2 : Capacity Control for Sequencer Applications
 NDI 110599-3 : Day & Time
 NDI 110399-4 : Date & Year
 NDI 110599-5 : Cooling Control
 NDI 200599-1 ; Solar System Applications
 NDI 140599-1 : CH Water Filling
5.3.1 Status bits added for cooling control
5.3.2 Configuration bits added for cooling control
5.3.2 Product version number and type added to Configuration Data class
5.3.3 CH Water Fill command added to Remote Command class

## Pagina 3

OpenTherm™ Protocol Specification v4.2  10 November 2020
©1996-2020 The OpenTherm Association  Page 3

5.3.4 Day, Time, Date, Year , Solar storage & collector temp. added to Information Data class
5.3.8 New class added for new applications
5.3.8.1 Cooling control signal added
5.3.8.2 Boiler sequence control signal added
1.3C DRAFT
24 October 1999
5.3.1 Correction made to error in definition of “Remote Reset enable” bit.
1.3 RELEASE
31 Oct. 1999
Approved by members of the OpenTherm Association.
1.4A DRAFT
30 May 2000
Document renamed “Protocol Specification” in place of “Technical Specification”
4.3.1 Correction made in text concerning wait time of messages (diagram was correct)
5.3.1 Added status bit (bit3 id0-master status) for OTC systems NDI-071299
5.3.2 Added configuration bit (bit3 id3-slave configuration) for DHW storage systems NDI-
1303000
5.3.2 Added configuration bit (bit4 id3-slave configuration ) for off-low/pump control systems
NDI-210400
5.2 !!! New list of mandatory items specified.
5.3.4 Added new data ids for DHW2 and exhaust temperatures NDI-220500-1
5.3.4 Added new data id for Flow temp 2 NDI-220500-2
5.3.1 Added new data id for Tset2 NDI-220500-2
5.3.1 Added new master status bit (bit 4, id0) and slave status bit (bit5, id0) for second loop NDI-
220500-2
5.3.3 Added new slave configuration bit (bit5, id3) for second loop NDI-220500-2
2.0A DRAFT
22 June 2000
1.3 Added comment to help explain important change in version2.0 for mandatory Ids.
4.2 remove MSB-LSB note in the centre of the data-value definition.
5.1 correct description of message classes (six to seven).
5.2 insert table to define all mandatory data-ids and their use.
5.3.2 redefine “low-off&pump control” flag as “Master low-off&pump control function” with states
“allowed” and “not-allowed”.
5.3.2 redefine “DHWconfig” flag default state as “not specified” instead of “instantaneous”
5.3.2 remove statement referring to configuration data as non-mandatory
5.3.8.2 redefine “Capacity-level setting” as “Maximum relative modulation level setting”
5.4 Add new data ids 31,32,33 and engineering units to overview table and updated some
terminology.
2.0B DRAFT
21 September 2000
5.3.1 Description id 8 corrected
5.4 Overview table completed with id8, message and data type.
2.0 RELEASE
15 Dec. 2000
Approved by members of the OpenTherm Association.
2.1A DRAFT
12 February 2002
1.3 Note mandatory id’s and backward compatibility updated.
2.3 Mandatory OT/- for OpenTherm logo marked masters deleted.
3.2.1.2. Slope of current signal edge deleted.
3.2.2.2. Slope of voltage signal edge deleted.
5.3.2. OpenTherm version master and slave added (ID 124, 125).
5.3.2. Explanation ID3 bit 4 corrected.
5.3.4. Operation hours boilers, CH pump and DHW pump/valve added (ID120, 121, 122).
5.3.5. OTC not, flags and ID’s removed
5.3.7.3. Remote override room Setpoint added
5.4. Operation hours boilers, CH pump and DHW pump/valve added (ID120, 121, 122)..
5.4. OpenTherm version master and slave added (ID 124, 125).
6 Remark that OT/- is mandatory for masters deleted.

2.1B DRAFT
27 March 2002
5.3.4. Room Setpoint CH2 (ID23) added.
5.4. Room Setpoint CH2 (ID23) added.
2.1 RELEASE
9 April 2002
Approved by members of the OpenTherm Association.
2.2A 11 October 3.2.1.1. Max. open-circuit voltage slave added.
3.2.2.3. Receive threshold voltage range extended.
5.3.1. Diagnostic flag (ID0:LB6) and code (ID115) added.
5.3.4. ID’s related to operation hours and number of starts added.
5.3.7.3. Remote override function (ID100) added
5.4. Data-Id overview map updated.
2.2 RELEASE
7 February
2003
Approved by members of the OpenTherm Association.
2.3A DRAFT
22 March 2004
5.3.1 Added ID0:HB5 “Summer/winter mode”. Description ID0:LB6 “diagnostic indication”
changed to “diagnostic/service indication”.
5.3.2 Added ID3:HB6 “Remote water filling function.
5.3.3 Description ID4 command and response changed.

## Pagina 4

OpenTherm™ Protocol Specification v4.2  10 November 2020
©1996-2020 The OpenTherm Association  Page 4

2.3B DRAFT
 5 May 2004

5.3.2 Description ID3:HB6 “Remote water filling function changed.
5.3.3 Description ID4 command changed to request.
5.3 Ventilation and heat-recovery ID’s added
5.4 Description ID4 command changed to request
2.3C DRAFT
23 July 2004

5.3.1. DHW block ID0:HB6 added
5.3.1. ID 70 bit numbering corrected
5.3.1. ID 72 bit numbering corrected
5.3.1. ID 74 bit numbering corrected
5.4 Overview corrected

2.3D

DRAFT
17 August 2005
5.3.1. Clarified description ID70HB bi1 1and 2 (bypass mode and position)
5.3.1. Type specification ID71 improved
5.3.1. Updated new ID70LB bit 3 and 4 Automatic bypass and free ventilation status
5.3.1. New ID’s 101 and 102 added
5.3.2. New ID’s 103 and 104 added
5.3.3. ID4 extended with new request code 3..9
5.3.4. Changed description ID116
5.3.4. Type specification ID77 and ID78 improved
5.3.4. New ID’s 34, 35, 113 and 114 added
5.3.5. Type specification ID87 improved
5.3.6. New ID’s 105 and 106 added
5.3.7. New ID’s 107 and 108 added
5.4. Changed description ID116
5.4. Type specification ID71, ID77, ID78, ID87 improved
5.4. New ID’s 34, 35, 101-108, 113, 114 added
5.4. ID 50 and 58 removed (OTC heat curve items)

2.3E DRAFT
11 November 2005
ID103 page 27. Type should be flag8 instead of u8.
ID4 page 28. Msg LB should be the same as for HB.
ID4 page 28. request code 10 added
ID35 page 29. Name should Actual boiler fan speed.
ID116 page 30. Extend description with successful.
ID87 page 31. Indicate in Name that value is in HB.
ID87 page 36. Change type to u8 /- (meaning HB)
ID116 page 37.  Extend description with successful.

2.3F DRAFT
28 May 2006
ID4 page 29: Extension with request code:
- 0: Normal operation mode
- 11: Service test 1 (OEM specific)
- 12: Automatic hydronic air purge
ID4 page 29: Remark “Chimney sweep function” added to request code 3.

2.3 RELEASE
1 October 2006
Approved by members of the OpenTherm Association.
3.0A DRAFT
26 May 2008
§3.2,  §3.4, §5.2 and §5.3.2 Smart Power Mode added
§4.3.1 and §4.3.2.  Multi Point to Point,Gateway  added
Page 35: ID27 Outdoor Temperature Type changed from R to RW
Page 35: ID36 Flame current added
Page 35: ID37 Room temperature 2nd CH circuit added
Page 35: ID98 RF sensor status information added
Page 40: ID99 Remote Override of Operating Modes for Heating and DHW
Page 42: Data-ID Overview Map updated
3.0 RELEASE
16 June 2008
Approved by members of the OpenTherm Association.
4.0A DRAFT 22
December 2010
§3.4.2.1 Clarifiication and correction of available power in different power modes
§5.3.1. New ID’s 0LB7 added
§5.3.2  New ID 3HB7 added
§5.3.4 New ID’s 38,109,110,111, 112 added
§5.4 Functions and Data-ID mapping added
4.0B DRAFT
12 April 2011
History 4.0A completed
§5.3 Requirement short-circuit feature limited to devices who support central heating
§5.2 revised and extended with functions and data-ID mapping
4.0 RELEASE
12 May 2011
Approved by members of the OpenTherm Association.
4.1A DRAFT
13 February 2018
§5.3.7.3 New ID39 Remote Override Room Setpoint 2
§5.3.3 New ID96 Cooling Operation Hours
           New ID97 Power Cycles
§5.3.2 New ID93 Brand Index
           New ID94 Brand Version
           New ID95 Brand Serial Number
§5.2.1 New mandatory IDs (ID93, ID94, ID95, ID125, ID127)

## Pagina 5

OpenTherm™ Protocol Specification v4.2  10 November 2020
©1996-2020 The OpenTherm Association  Page 5

4.1 RELEASE
08 Oct 2018
Approved by members of the OpenTherm Association.
4.1B 22 Oct 2020

§5.4    Updated Data-Id overview
§5.2.1 Updated ID2 Slave description (more explicit): smart power support is mandatory
§1.3    Added related documents
§1.2, §6 OpenTherm Lite – OT/- demoted to legacy status.
§3.4.1 Clarification on Smart Power support
Updated Logo
§3.2.1, §3.3.2 Updated signal level and bit timing notation style

4.2 RELEASE
10-11-2020
Approved by members of the OpenTherm Association.

## Pagina 6

OpenTherm™ Protocol Specification v4.2  Table of Contents
©1996-2020 The OpenTherm Association  Page 6

Table of Contents

Description of Changes 2
1. INTRODUCTION 8
1.1 Background 8
1.2 Key OpenTherm Characteristics 8
1.3 Document Overview 9
1.3.1 Related documents 9
1.4 Nomenclature & Abbreviations 10
2. SYSTEM OVERVIEW 11
2.1 System Architecture and Application Overview 11
2.2 Provision for Future Architectures/Expansion 11
2.3 Product Compliance and Marking 12
2.4 Protocol Reference Model 13
3. PHYSICAL LAYER 15
3.1 Medium Definition- Characteristics of the Transmission Line 15
3.2 Signal Transmission Definition 15
3.2.1 Transmitted Signal - Boiler Unit to Room Unit 16
3.2.2 Transmitted Signal - Room Unit to Boiler Unit 17
3.3 OpenTherm/plus Bit-Level Signalling 19
3.3.1 Bit Encoding Method 19
3.3.2 Bit Rate and Timing 19
3.3.3 Bit-level Error Checking 20
3.4 Power Feeding 21
3.4.1 Power Feed Options 21
3.4.2 Smart Power Mode mechanism 21
3.4.3 Connection polarity 24
3.4.4 Galvanic isolation 24
3.5 Special Installation Short-Circuit Feature 24
4. OPENTHERM/PLUS DATALINK LAYER PROTOCOL 25
4.1 Overview 25
4.2 Frame Format 25
4.2.1 Parity Bit - P 25
4.2.2 Message Type - MSG-TYPE 25
4.2.3 Spare Data - SPARE 26
4.2.4 Data Item Identifier - DATA-ID 26
4.2.5 Data Item Value - DATA-VALUE 26

## Pagina 7

OpenTherm™ Protocol Specification v4.2  Table of Contents
©1996-2020 The OpenTherm Association  Page 7

4.3 Conversation Format 26
4.3.1 Overview 26
4.3.2 Multi Point to Point, Gateways 27
4.3.3 Message Notation 28
4.3.4 Default Data-Values 28
4.4 Conversation Details 29
4.4.1 Read-Data Request 29
4.4.2 Write-Data Request 29
4.4.3 Writing Invalid Data 29
4.5 Frame Error Handling 30
5. OPENTHERM/PLUS APPLICATION LAYER PROTOCOL 31
5.1 Overview 31
5.2 Mandatory OT/+ Application-Layer Support 32
5.2.1 Mandatory ID’s for OpenTherm devices who support central heating 32
5.2.2 OpenTherm Functions and Data-ID mapping 33
5.3 Data Classes 34
5.3.1 Class 1 : Control and Status Information 34
5.3.2 Class 2 : Configuration Information 36
5.3.3 Class 3 : Remote Request 38
5.3.4 Class 4 : Sensor and Informational Data 39
5.3.5 Class 5 : Pre-Defined Remote Boiler Parameters 42
5.3.6 Class 6 : Transparent Slave Parameters 43
5.3.7 Class 7 : Fault History Data 44
5.3.8 Class 8 : Control of Special Applications 45
5.4 Data-Id Overview Map 47
6. OPENTHERM/LITE DATA ENCODING AND APPLICATION SUPPORT 50
6.1 Room Unit to Boiler Signalling 50
6.2 Boiler to Room Unit Signalling 50
6.3 OT/- Application Data Equivalence to OT/+ 51

## Pagina 8

OpenTherm™ Protocol Specification v4.2  Introduction
©1996, 2011 The OpenTherm Association  Page 8

1. Introduction
1.1 Background
The trend in boiler technologies towards high-efficiency appliances with gas/air modulation and increased
sophistication in control electronics has created a requirement for system communication between boilers
and room controllers. At the higher end, home-systems buses provide extensive communications capability
and several such systems are available, although no single standard has emerged. Generally, they all
require hardware/software solutions whose cost is significant at the lower-end of the market, especially for
point-to-point systems. Several proprietary solutions at this low-end have been developed, but offer no cross-
compatibility with products from different manufacturers.

There is an increasing demand for a new standard to be established to connect room controllers and boilers
in a simple point-to-point fashion with very low entry-level costs. OpenTherm was developed to meet this
requirement. Since then it has been extended with support for various HVAC applications and is not limited
to boiler applications, the protocol is being extended further as required by new technologies and
applications.
1.2 Key OpenTherm Characteristics
• Compatibility with so-called “dumb” or non-intelligent HVAC systems.
• Compatibility with low-cost entry-level room thermostats.
• Compatible with electrical supplies typically normally available within HVAC systems.
• Two-wire, polarity-free connection for concurrent power supply and data transmission.
• Provides a suitable power supply for a room controller so that it can operate without the need for an
additional power source such as batteries.
• Implemental in low-cost microcontrollers with small ROM / RAM / CPU-speed requirements.
• Installer friendly feature for boiler testing. Shorting the wires at the boiler provides a simulated maximum
heat demand (similar to current on/off systems).
• Allows for the transfer of sensor, fault and configuration data between the devices.
• Provides a mandatory minimum set of data objects, which allows for transmission of a modulating control
signal from the room controller to the HVAC system.

One of the key characteristics of the OpenTherm standard is the two-level approach which allows analogue-
type solutions at the low-end.

OT/+ The OpenTherm/plus protocol provides a digital communications system for data -exchange between
two microprocessor-based devices.
OT/- The OpenTherm/Lite* protocol uses a PWM signal and simple signalling capabilities to allow
implementation on analogue-only products.
IMPORTANT: As of OpenTherm version 4.1 OpenTherm/Lite – OT/- is no longer being tested
nor certified and has been demoted to legacy functionality, it should not be incorporated in
new designs. The information should only to be used as reference for existing (legacy)
products

## Pagina 9

OpenTherm™ Protocol Specification v4.2  Introduction
©1996, 2011 The OpenTherm Association  Page 9

Both protocols use the same physical layer for data transmission and power -feeding ensuring that the two
levels of communications are physically compatible.
1.3 Document Overview
This document specifies a communication system for use with boilers, heat pumps, ventilation systems and
room controls, which can also be applied to similar devices in the same or related applications. The
characteristics and communications features of both the infrastructure and the attached devices are specified
in detail. This document does not provide a prescriptive solution for OpenTherm-compatible controllers, but
rather specifies the requirements for such a solution.

This document is divided into the following sections :

1. Introduction provides an overview of the background and key features of the
OpenTherm communications system and defines some terms used in
the document.
2. System Overview gives a top-level architectural view of the target application system and
explains OpenTherm in relation to the OSI reference model and outlines
the process for ensuring product compliance.
3. Physical Layer describes the characteristics of the physical medium and the method for
bit-level signalling.
4. OT/+ DataLink Layer describes the composition of OpenTherm/plus frames and allowable
conversation formats.
5. OT/+ Application Layer defines data objects and the mechanisms for transfer of application
data between the boiler and room controllers.
6. OT/- Encoding/Application data describes the special mechanisms for transferring data in the
OpenTherm/Lite system.
1.3.1 Related documents
Please use this document in conjunction with the following related documents:
Function Matrix (Excel sheet)
- This document contains information on the mandatory ID’s based on device functions
Application Functional Specification (pdf)
- This documentation contains information on how to implement specific features
Data-Id Overview Map (pdf)
- The OpenTherm ID map, containing all available OpenTherm IDs
Test Specification (pdf)
- The test specification for manually testing OpenTherm implementations
These documents are located in the on the OTA website at the: Members area

## Pagina 10

OpenTherm™ Protocol Specification v4.2  Introduction
©1996, 2011 The OpenTherm Association  Page 10

1.4 Nomenclature & Abbreviations
OT/+ OpenTherm/plus
OT/- OpenTherm/Lite
OSI/RM The OSI 7-layer protocol reference model.
PWM Pulse-width modulation
Room Unit The device which calculates the “demand” in the system, which is communicated to the
Boiler Unit. The use of the term room is not intended to be literally restrictive but is used for
convenience.
Boiler Unit The device which receives the “demand” from the room unit and typically is responsible for
providing energy to satisfy that demand. The use of the term boiler is not intended to be
literally restrictive but is used for convenience.
AL Application Layer
DLL Data-Link Layer
PL Physical Layer
TSP Transparent Slave Parameter
RBP Remote Boiler Parameter
CH Central Heating
DHW Domestic Hot Water
OEM Original Equipment Manufacturer
OTC Outside Temperature Compensation
FHB Fault History Buffer

## Pagina 11

OpenTherm™ Protocol Specification v4.2  System Overview
©1996, 2011 The OpenTherm Association  Page 11

2. System Overview
2.1 System Architecture and Application Overview
OpenTherm is a point-to-point communication system and connects boilers with room controllers, therefore it
is not possible to connect several boilers or room controllers in the manner of bus -based systems.
OpenTherm assumes that the room controller is calculating a heating demand signal in the form of a water
temperature Control Set point based on room temperature error (or other control form, e.g. OTC) which it
needs to transmit to the boiler so that it can control the output of the boiler. The boiler in turn can transmit
fault and system information to the room controller for display or diagnostics. A large number of data items
are defined in the OT/+ Application Layer Protocol, covering these and many other pieces of system data.

OpenTherm
Room ControllerBoiler

2.2 Provision for Future Architectures/Expansion
The OpenTherm communication system is designed to allow for future expansion at the application layer by
provision of reserved data-ids and at the data-link layer by the use of reserved (spare) bits within the frame.

In order to address applications which would normally require a bus-based communications system, it is
conceived that intermediate gateways / interface devices would manage multiple OpenTherm
communications lines. In the example below, the interface device acts as a “virtual boiler” to the room
controller and acts as a “virtual room unit” to the boiler. In this way, other devices can be addressed while
maintaining the basic point-to-point approach. See also § 4.3.2

OpenTherm
Room ControllerBoiler
OpenTherm
Interface Device
Other interfaces

All future revisions of the OpenTherm Protocol Specification must be approved by the Members of The
OpenTherm Association.

## Pagina 12

OpenTherm™ Protocol Specification v4.2  System Overview
©1996, 2011 The OpenTherm Association  Page 12

2.3 Product Compliance and Marking
All products marked with the OpenTherm logo must comply with the requirements of this document. The
OpenTherm logo, trademark and the protocol can only be used with the permission of The OpenTherm
Association. The OpenTherm Association is responsible for compliance testing procedures and licensing.

• A boiler or room controller can be marked with the OpenTherm logo if it conforms to the specification
contained herein for OT/+.
• A boiler or room controller can not be marked with the OpenTherm logo if it only conforms to the
specification contained herein for OT/-.

When a room unit which can operate both in OT/+ and OT/- modes is connected to a boiler controller, some
configuration needs to take place to determine which protocol to use. This configuration should be achieved
automatically as follows :

On power-up or after the physical connection is made, the room unit tries to communicate using OT/+
messages. If the boiler controller does not respond to one of these messages after 20 seconds, then the
room unit switches to using OT/- signalling.

An OT/+ boiler controller must start communications within this 20 second period or future OT/+
communications will not be possible unless the room unit is reset or re-connected.

The state diagram below illustrates the OT/+ to OT/- detection in the master.

Room stat
OT+/OT-
OT- Room stat
OT-
OT+/OT-
OT+ Room stat
OT+
Not OT-marked Not OT-marked

## Pagina 13

OpenTherm™ Protocol Specification v4.2  System Overview
©1996, 2011 The OpenTherm Association  Page 13

 1.
TRYING OT/+
COMMUNICATION
4.
COMMUNICATIONS
FAULT
3.
OT/+
MODE
2.
OT/-
MODE
Valid response
received.
No valid response
for 1 min.
Valid response
received.
No valid response
for 20 sec.

2.4 Protocol Reference Model
In order to describe the OpenTherm system, it is split up into a layered architecture based on the OSI
Reference Model. The OSI/RM is an abstract description  of inter-process data communication. It provides a
standard architecture model that constitutes the framework for the development of standard protocols.  The
OSI/RM defines the functions of each of 7 defined layers and the services each layer provides to the layers
above and below it. OpenTherm is only described in terms of the functions of the layers . Inter-layer
communication is considered an implementation issue. The diagram below shows the  OpenTherm
Reference Model.
Physical Layer
OT/+
Data-Link Layer
OT/+
Application Layer
OT/- EncodingOT/+ Encoding
OT/-
Application Data
Support

## Pagina 14

OpenTherm™ Protocol Specification v4.2  System Overview
©1996, 2011 The OpenTherm Association  Page 14

The Application Layer is responsible for transfer of application data between the application software in the
boiler and room controllers. It defines data-classes, data-id numbers and format of data-values for
transmission. It also specifies the minimum AL support for all OpenTherm-compatible devices.

The Data-Link Layer is responsible for building the complete frame incorporating the AL data-id and value
and calculating the error-check code. It defines message types and conversation formats and performs error -
checking on a received frame. It regulates the flow of information on the line.

The Physical Layer defines the electrical  and mechanical characteristics of the medium and the
mechanism for transmission of a bit, including bit-level encoding. It also performs bit-level error checking on
an incoming frame(OT/+)

## Pagina 15

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 15

3. Physical Layer
3.1 Medium Definition- Characteristics of the Transmission Line
Number of Wires  : 2
Wiring type  : untwisted pair *
Maximum line length  : 50 metres
Maximum cable resistance  : 2 * 5 Ohms
Polarity of connections  : Polarity-free, i.e. interchangeable.

* In electrically noisy environments it may be necessary to use twisted pair or screened cable.
3.2 Signal Transmission Definition
The system operates by sending current signals from the boiler unit to the room unit and voltage signals in
the reverse direction. The signals are sent by switching between two defined levels, the idle and active state.
The idle and active levels are dependant of the Power Mode the system is working in.

Three Power Modes are defined:

1. Low Power: Idle current is low and idle voltage is low
2. Medium Power: Idle current is high and idle voltage is low
3. High Power: Idle current and idle voltage is both high.

Note that all specifications should be fulfilled within the complete temperature range in which the device is in
use.

Summary of allowable line signal conditions:

OpenTherm Plus System
5
18 15 8
9
17
23
Line Voltage
Line
Current

OpenTherm Lite System
5
18 15 8
9
17
23
Line Voltage
Line
Current

## Pagina 16

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 16

3.2.1 Transmitted Signal - Boiler Unit to Room Unit
3.2.1.1 Static Characteristics - Amplitude

Ilow
Ihigh
Time
Current

Description Symbol Min Typ Max value
Current signal High level Ihigh 17 - 23 mA
Current signal Low level Ilow 5 - 9 mA
Maximum Open circuit voltage - - - 42 Vdc

In low power mode the idle state equals the current signal low level. In medium and in high power mode the
idle state equals the current signal high level

Current signal specifications to be maintained when voltage is Vlow or Vhigh
3.2.1.2 Dynamic Characteristics
90%
10%
Time
Current
tr tf

Requirement for Room Unit / Master
Description Symbol Min Typ Max value
Current signal rise time tr - 20 50 µS
Current signal fall time tf - 20 50 µS

## Pagina 17

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 17

3.2.1.3 Receiver Thresholds
In order to set satisfactory signal-to-noise ratios, the receiver (room unit) should recognise a level change as
significant at a threshold point within the following limits :

Description Symbol Min Typ Max value
Current receive threshold Ircv 11.5 13 14.5 mA
3.2.2 Transmitted Signal - Room Unit to Boiler Unit
3.2.2.1 Static Characteristics - Amplitude
Vhigh
Vlow
Time
Voltage

Description Symbol Min Typ Max value
Voltage signal High level Vhigh 15 - 18 V
Voltage signal Low level Vlow - - 8 V

In low power mode the idle state equals the Voltage signal Low Level. In medium and in high power mode
the idle state equals the Voltage signal High Level

Voltage signal specifications to be maintained when current is Ilow.or Ihigh

## Pagina 18

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 18

3.2.2.2 Dynamic Characteristics
90%
10%
Time
Voltage
tr tf

Requirement for Boiler Unit / Slave
Description Symbol Min Typ Max value
Voltage signal rise time tr - 20 50 µS
Voltage signal fall time tf - 20 50 µS
3.2.2.3 Receiver Thresholds
In order to set satisfactory signal-to-noise ratios, the receiver (boiler unit) should recognise a level change as
significant at a threshold point within the following limits :

Description Symbol Min Typ Max value
Voltage receive threshold Vrcv 9.5 11 12.5 V

## Pagina 19

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 19

3.3 OpenTherm/plus Bit-Level Signalling
3.3.1 Bit Encoding Method
Bit encoding method  : Manchester / Bi-phase-L
Bit value ‘1’  : active-to-idle transition
Bit value ‘0’  : idle-to-active transition

 Active
Idle
Bit value = ‘0’ Bit value = ‘1’

Manchester encoding is a self-clocking code giving the advantage of bit-synchronisation since there is
always at least one transition in the middle of the bit-interval. It also has a fixed average d.c. component over
the frame of half the idle and active levels which allows greater predictability of power supply requirements,
and additionally the absence of an expected transition can be used to detect errors.

Example
    1        0        0         1        1         1         0        1

3.3.2 Bit Rate and Timing
Description Min Typ Max value
Bit rate  1000  bits/sec
Period between mid-bit transitions 900 1000 1150 µS

Timing should be reset on each transition so that any timing errors do not accumulate

## Pagina 20

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 20

Previous Bit Data Bit Next Bit
1ms -10%+15%
next
bit transition
bit transitionprevious
bit transition
acceptable window for transition
100µs 150µs
Bit period = 1ms nominal
500µs nominal

3.3.3 Bit-level Error Checking
The primary error-checking method in OpenTherm is provided through the Manchester encoding.
Manchester validity should be checked by the receiver and the data frame rejected if an error is detected.

## Pagina 21

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 21

3.4 Power Feeding
It is the intention that OpenTherm provides suitable power from the boiler unit to the room  unit such that no
additional power connection or use of batteries is required for the room unit. With the Smart Power
mechanism the power delivered via OpenTherm can be changed fast and easy.
3.4.1 Power Feed Options
The options for supplying power to an OpenTherm room unit are :

i)  Local supply (mains, batteries or other independent power source)
ii)  Line (OpenTherm supplied) Low Power.
iii)  Line (OpenTherm supplied) Medium Power
iv)  Line (OpenTherm supplied) High Power

Any OpenTherm room unit is permitted to exercise option (i) above, or use line-power within one of the
defined power modes

If the OpenTherm room unit is using line power, it always has to start-up in Low Power mode,
Normal (basic) operation of the OpenTherm room-unit must be guaranteed in Low Power mode.
*It is mandatory for a slave device to implement Smart Power support since OpenTherm version 3.0

3.4.2 Smart Power Mode mechanism
3.4.2.1 Definition of Power Modes
Three Power Modes are defined:
Low Power:
• Idle current low
• idle voltage is low
• Available power 40 mW (5mA at 8V) *

Medium Power:
• Idle current high
• idle voltage is low
• Available power 136 mW (17mA at 8V) *

High Power:
• Idle current high
• idle voltage is high
• Available power 306mW (17mA at 18V) *

*) Available power at lowest allowed current provided by slave and highest allowed voltage created by
master.
3.4.2.2 Master start-up requirements
The Master must be able to start-up in Low Power Mode (idle voltage is low).
Basic operation should be guaranteed in Low Power Mode.

Procedure to check if High or Medium power is allowed:
• Communication must been established in Low Power mode.
• Write ID2 to slave and receive a acknowledge. If invalid data or unknown ID received then
medium/high power is not allowed.

## Pagina 22

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 22

• If acknowledge received on ID2 then Initiate High Power:
• If Slave doesn’t go to high current after 5 msec then Medium or High power is not allowed. Master
switches back to Low idle voltage.
3.4.2.3 Slave start-up requirements
The slave must start-up in Low Power Mode (idle current is low).
Procedure to check if High or Medium power is allowed:
• Communication must been established in Low Power mode and ID2 must be received before Slave
reacts on High level idle voltage (i.e. going to High level idle current after 5 msec)
• If Master switches to High idle voltage (i.e. initiates High Power) then switch to High idle current after
5msec

Note: When a master without high power is disconnected for a short time, there’s a change that the slave
goes to high idle current. To prevent this, the slave must have received ID2 with Smart Powe r bit set before
it’s allowed to switch to high idle current.

3.4.2.4 Normal and invert receive mode
• If the idle voltage switches from low to high level the slave has to switch to invert receive mode after
5 msec.
• If the idle voltage switches from high to low level the slave has to switch to normal receive mode
after 5 msec.
• If the idle current switches from low to high level the master has to switch to invert receive mode
after 5 msec.
• If the idle current switches from high to low level the master has to switch to normal receive mode
after 5 msec.
• Switching receive mode should always be done, even if no communication was established in low
power mode at start-up or if Medium or High power mode is not allowed.
3.4.2.5 Initiate High Idle Current (Start for High or Medium Power mode)

Low to High Power mode

Low to Medium Power Mode

Master
Slave
min 20 ms
5ms
Initiate High Power
 High Power
min 20ms
Master
Slave
Medium Power
10ms
 min 20ms
5ms
min 20ms

## Pagina 23

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 23

• Master switches to High Idle Voltage.
• The slave switches to High Idle Current after 5 msec.
• Master stays at High level Voltage if High Power mode needed.
• Master switches to Low level Voltage 10 msec after it switched to High level if  Medium Power mode
is needed.
• Restrictions:
o Only allowed if communication was established in low power mode and Smart Power
configuration (ID2:bit0) sent and answered with acknowledge.
o Master must wait at least 20 msec after a message sent or received before initiati ng High
Idle Current.
o If Slave doesn’t go to High Idle Current within 3-7 msec after Master switched to high idle
voltage, the Master must switch back to Low Idle Voltage.

3.4.2.6 Initiate Low Idle Current (Start for Low Power mode)

Medium to Low Power mode

High to Low Power mode

• Master switches to High Idle Voltage.
• The slave switches to High Idle Current after 5 msec.
• Restrictions:
o Only allowed if communication was established in low power mode.
o Master must wait at least 20 msec after a message sent or received before initiating High
Idle Current.
3.4.2.7 Exception handling Slave
If the slave receives for one minute or more the same Message it must switch back to low idle current
(could be caused by the fact that an old master is connected, because it will repeat messages if no
correct answer is received)
3.4.2.8 Message repetition Master
The master should not repeat the same message for more than one minute, because the slave will fall
back to low idle current.
Master
Slave
min 20ms
Low Power
min 15ms
 min 20ms
5ms
Master
Slave
min 20ms
Low Power
min 20ms
5ms

## Pagina 24

OpenTherm™ Protocol Specification v4.2  Physical Layer
©1996, 2011 The OpenTherm Association  Page 24

3.4.2.9 Timing tolerance
• Timing  specifications of transition changes: +/- 1 ms.
• Timing  specifications of detecting transition changes: +/- 2 ms.
3.4.3 Connection polarity
The room unit shall provide the functionality to operate regardless of polarity of the line signal.
3.4.4 Galvanic isolation
The boiler interface shall provide safety isolation from the mains power line (ref. EN60730-1).
3.5 Special Installation Short-Circuit Feature
This feature is only mandatory for slave units who actually can create a heat demand. For instance a slave
with cooling only or ventilation do not have this feature.

A slave unit with heat demand control must support an important installation feature which allows the
terminals at the boiler to be short-circuited to simulate a heat demand such as can be done with existing
on/off boilers. The boiler unit should interpret the short-circuit as a heat demand within15 secs of the short-
circuit being applied. This must be supported by both OT/+ and OT/- boilers.

It is allowable that this can implemented by a software-detection method. The software short-circuit condition
is defined as a low-voltage state (Vlow) with no valid communications frame for at least 5 seconds.

The state diagram below illustrates this.

2.
SHORT-CIRCUIT
STATE
1.
COMMUNICATIONS
ACTIVE
No valid frame
received for 5..15
secs.
Valid communications
re-started.

## Pagina 25

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 25

4. OpenTherm/plus DataLink Layer Protocol
4.1 Overview
The data-link layer is responsible for the following functions :

• builds the complete frame from the information passed to it from the application layer.
• performs error-checking on an incoming frame.
• defines message types and allowable conversation exchanges.
4.2 Frame Format
Data is transmitted in 32 bit frames, with an added start bit (‘1’) at the beginning and stop bit (‘1’) at the  end.

The frame format is identical for both directions, being laid out as follows:

 Start bit (1)    Stop bit (1)
 |   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<           32-BIT FRAME           >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> |
 |   <------------BYTE 1 --------------->  <------------BYTE 2 ---------------> <------------BYTE 3 ---------------> <------------BYTE 4 ---------------> |
 | Parity bit (1)    |
 | | Message Type (3)   |
 | | |    |

1 P MSG-TYPE SPARE(4) DATA-ID(8) DATA-VALUE(16) 1
   MSB             LSB  MSB                       LSB  MSB                                                              LSB  MSB                                                                                                                                            LSB

Elements of the frame structure are described  in the following sections.
4.2.1 Parity Bit - P
The parity bit should be set or cleared such the total number of ‘1’ bits in the entire 32 bits of the message is
even.
4.2.2 Message Type - MSG-TYPE
The message type determines the contents and meaning of the frame. Seven of the eight possible values for
the message type are defined.

## Pagina 26

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 26

Master-to-Slave Messages  Slave-to-Master Messages
Value Message type  Value Message type
000 READ-DATA  100 READ-ACK
001 WRITE-DATA  101 WRITE-ACK
010 INVALID-DATA  110 DATA-INVALID
011 -reserved-  111 UNKNOWN-DATAID
4.2.3 Spare Data - SPARE
These bits are unused in this release of the protocol. They should always be ‘0’.
4.2.4 Data Item Identifier - DATA-ID
The DATA-ID is an 8 bit value which uniquely identifies the data item or items being transmitted. A full list of
data ID’s and corresponding data items are listed in the OT/+ Application Layer section.
4.2.5 Data Item Value - DATA-VALUE
This contains the 16 bit value of the data item corresponding to the frame’s data identifier. In some
messages, the data-value is composed of two separate items, each of 8-bits in length. These will be denoted
as DATA-BYTE1 and DATA-BYTE2.
4.3 Conversation Format
4.3.1 Overview
OpenTherm data transfer consists of a series of ‘conversations’ between the devices controlled by a strict
master/slave relationship. OpenTherm requires that the control device, e.g. a room unit, is always the master
and the control plant, e.g. a boiler, is always the slave.

In all cases the master initiates a conversation by sending a single frame. The slave is expected to respond
with a single frame reply within a defined period of 20ms to 400ms from the end of the master transmission.
The typical answering time of a slave should be 100ms or less.

In case the slave is functioning as a gateway (see also § 4.3.2) it must wait for an answer from the next slave
in line, so the maximum waiting time does not apply.

The master unit must wait 100ms (MWT) from the end of a previous conversation before initiating a new
conversation. The master must communicate at least every 1 sec (+15% tolerance)  (MCI).

The timing for a single master-slave system is shown below

## Pagina 27

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 27

A conversation is limited to a single exchange of frames. Three types of conversation are possible, listed in §
4.4..

Description Min Typ Max value
Slave answering time 20 <100 400 ms
Master wait time (MWT) 100   ms
Master communication interval (MCI) MWT <1 1.15 s

4.3.2 Multi Point to Point, Gateways
A gateway is an intermediate device that acts as a slave to the connected master, and acts as a master to
the connected slave. The gateway is transparent for all messages sent by the master, unless the message is
mend for the gateway itself. The gateway should try not to disturb the normal operation between Room-unit
and Boiler when it needs to sent own commands to the Boiler. If the Boiler responds fast it may be possible
to sent an additional command to the boiler before handling the command from the Room-unit.

In order to maintain the normal timing between the first master and last slave in the line , the following
applies:
• A message received from the master side is checked. If it is addressed to the gateway, it will be
answered by the gateway, otherwise it is sent to the next slave in line.
• Within 7 ms a message has to be sent to the next slave in the line. In most cases the message
received will be sent. In case the received message is meant for the gateway itself it can sent
another message to the next slave.
• Wait for a answer from the slave (no maximum time of 400ms to answer the master)
• After the answer from the next slave in line is received, a answer is sent to the master. If the original
message was meant for the gateway, then the gateway will create the answer, otherwise the
received answer is forwarded to the master.
• Within 7ms an answer has to be sent to the master.

With the applied restrictions, a total of 4 intermediate devices can be connected in line, and still the slave
response times for the master are met.

slave
master
min: 20ms
max: 400ms
min: 100ms
Maximum: 1.15s

## Pagina 28

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 28

Master Gateway 2
34ms
34ms
34ms
34ms
34ms
34ms
7ms
7ms
7ms
Max
400 ms
Max 796 ms
Gateway 1 SlaveGateway 4Gateway 3
7ms
7ms
7ms
7ms
7ms
34ms
34ms
34ms
34ms

4.3.3 Message Notation
In the rest of this section and in the OT/+ Application Layer Protocol section, a function -style notation is used
to describe the various messages in order to aide explanation.

<msg-type> ( id=<data-id>, <data-value>)
represents a message with msg-type=<msg-type>, data-id =<data-id> and data-value = <data-value>.

<msg-type> ( id=<data-id>, <data-byte1>, <data-byte2>)
represents a message with msg-type=<msg-type>, data-id =<data-id> and data-value = <data-
byte1>&<data-byte2>.

e.g. READ-DATA (id=1, ControlSetpoint-value)
READ-DATA (id=11, TSP-index, 00)

4.3.4 Default Data-Values
For some messages, no “real” data is being sent. e.g. in a normal Read-Data request. The data-value will be
set to a default value of zero. i.e. two bytes of 0x00 and 0x00.

## Pagina 29

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 29

4.4 Conversation Details
4.4.1 Read-Data Request
READ-DATA (DATA-ID, DATA-VALUE)

The master is requesting a data value, specified by the data identifier, from the slave. The message type
sent by the master is ‘Read-Data’, as shown above. Typically no data-value is sent and a value of 0x0000
will be used, but in some circumstances the master may, although it is requesting a value from the sl ave,
also send a value to the slave with this message e.g. for data-verification. This is defined by the OT/+
Application Layer protocol.

The slave should make one of the three possible responses listed below:

• READ-ACK (DATA-ID, DATA-VALUE,)
If the data ID is recognised by the slave and the data requested is available and valid. The value of the data
item is returned.

• DATA-INVALID (DATA-ID, DATA-VALUE)
If the data ID is recognised by the slave but the data requested is not available or invalid.  DATA-VALUE can be
0x0000 in this case.

• UNKNOWN-DATAID (DATA-ID, DATA-VALUE)
If the slave does not recognise the data identifier. DATA-VALUE can be 0x0000 in this case.
4.4.2 Write-Data Request
WRITE-DATA (DATA-ID, DATA-VALUE)

The master is writing a data value, specified by the data identifier, to the slave. The message type sent by
the master is ‘Write-Data’, as shown above.

The slave should make one of the three possible responses listed below: (Note that DATA-VALUE may be
modified by the slave in some circumstances.)

• WRITE-ACK (DATA-ID, DATA-VALUE)
If the data ID is recognised by the slave and the data sent is valid.

• DATA-INVALID (DATA-ID, DATA-VALUE)
If the data ID is recognised by the slave but the data sent is invalid.

• UNKNOWN-DATAID (DATA-ID, DATA-VALUE)
If the slave does not recognise or does not support the data identifier.
4.4.3 Writing Invalid Data

INVALID-DATA (DATA-ID, DATA-VALUE)

A data item to be sent by the master may be invalid in a particular application, but may still require to be
sent. In this case the master may use the message type ‘data invalid’.

## Pagina 30

OpenTherm™ Protocol Specification v4.2  OT/+ DataLink Layer
©1996, 2011 The OpenTherm Association  Page 30

The slave should make one of the two possible responses listed below: (Note that DATA-VALUE may be
modified by the slave in some circumstances.)

• DATA-INVALID (DATA-ID, DATA-VALUE)
If the data ID is recognised by the slave.

• UNKNOWN-DATAID (DATA-ID, DATA-VALUE)
If the slave does not recognise or does not support the data identifier.
4.5 Frame Error Handling
In all cases of errors being detected in the incoming frame, the partial frame is rejected and the conversation
should be terminated. No errors are treated as recoverable.

If the slave does not respond, then the master should note that the conversation is incomplete.

If a conversation is terminated the master should re-attempt the same conversation at the next appropriate
scheduled time for communications.

## Pagina 31

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 31

5. OpenTherm/plus Application Layer Protocol
5.1 Overview
The Application Layer of the OpenTherm protocol is divided into an Open-Area of data-item id’s and a Test &
Diagnostic area for Member use. Id’s 0 .. 127 are reserved for OpenTherm pre -defined information, while id’s
from 128 .. 255 can be used by manufacturers (members of the association) for test & diagnostic purposes
only. The MemberID codes of the master and slave can be used to handshake between two compatible
devices and enable the use of the Test & Diagnostic-Area data-items. MemberID codes are assigned and
managed by The OpenTherm Association.

There are seven classes of information defined in the Application Layer :

Class 1. Control and Status Information
This class contains basic control information from the master and status information exchange
(including fault status) and incorporates all the mandatory OpenTherm data.
Class 2. Configuration Information
Information relating to the configuration of the master and slave and Member identification.
Class 3. Remote Commands
This class allows for commands to be passed from the master to the slave.
Class 4. Sensor and Informational Data
This class covers typically sensor temperatures, pressures etc.
Class 5. Remote Boiler Parameters
These are parameters of the slave device which may be read or set by the master and are
specific to boiler applications.
Class 6. Transparent Slave Parameters
This class allows slave parameters to be read or set by the master without knowledge of their
physical or application-specific meaning.
Class 7. Fault History Information
This data allows historical fault information to be passed from the slave to the master.
Class 8. Control of Special Applications
This class defines data id’s to be exchanged between the master and a application specific slave.

Special abbreviations and data-types are used in the Application Layer Protocol section and are defined
below :
LB low-byte of the 16-bit data field.
HB high-byte of the 16-bit data field.
S>M information flow from slave to master
M>S information flow from master to slave
flag8 byte composed of 8 single-bit flags
u8 unsigned 8-bit integer 0 .. 255
s8 signed 8-bit integer -128 .. 127 (two’s compliment)
f8.8 signed fixed point value : 1 sign bit, 7 integer bit, 8 fractional bits (two’s compliment i.e. the
LSB of the 16bit binary number represents 1/256th of a unit).
u16 unsigned 16-bit integer 0..65535
s16 signed 16-bit integer -32768..32767
Example :  A temperature of 21.5C in f8.8 format is represented by the 2-byte value 1580 hex
(1580hex = 5504dec, dividing by 256 gives 21.5)
A temperature of -5.25C in f8.8 format is represented by the 2-byte value FAC0 hex
(FAC0hex = - (10000hex-FACOhex) = - 0540hex = - 1344dec, dividing by 256 gives -5.25)

## Pagina 32

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 32

• All data-item ID’s are in decimal unless denoted otherwise.
• “00” is used to represent the dummy data byte as defined in the Data-Link Layer and is transmitted when
valid data-values are not available or appropriate.
• For future compatibility, a bit or byte is marked as unused or reserved, should be set to 0 (zero) by the
transmitter; however the receiver should ignore these bits/bytes, since they may be set in future versions
of the  protocol.
• ‘R’ and ‘W’ under the column labelled ‘Msg’, indicate whether the data object is supported with a Read or
a Write command.

5.2 Mandatory OT/+ Application-Layer Support
5.2.1 Mandatory ID’s for OpenTherm devices who support central heating
It is required that OpenTherm-compliant devices support the following data items in case they support central
heating or in case no OpenTherm functions (see 5.2.2) are defined for the device. Please consult the
OpenTherm Function matrix document which contains all mandatory ID’s per function for the full mandatory
ID list.

ID Description Master Slave
0 Master and slave status flags • Must sent message with
READ_DATA
• Must support all bits in master status
• Must respond with READ_ACK
• Must support all bits in slave status
1 Control Setpoint
i.e. CH water temp. Setpoint
• Must sent message with
WRITE_DATA or INVALID_DATA
(not recommended)
• Must respond with WRITE_ACK
2 Master Configuration • Not mandatory (only if Smart Power is
needed by Master
• Must respond with WRITE_ACK
• Must support bit 0: Smart Power
3 Slave configuration flags • Must sent message with
READ_DATA (at least at start up)
• Must respond with READ_ACK
• Must support all slave configuration
flags
14 Maximum relative modulation
level setting
• Not mandatory
• Recommended for use in on-off
control mode.
• Must respond with WRITE_ACK
17 Relative modulation level • Not mandatory • Must respond with READ_ACK or
DATA_INVALID
25 Boiler temperature • Not mandatory • Must respond with READ_ACK or
DATA_INVALID (for example if
sensor fault)
93 Brand Index • Not mandatory • Must respond with READ_ACK or
DATA_INVALID (if out of range)
94 Brand Version • Not mandatory • Must respond with READ_ACK or
DATA_INVALID (if out of range)
95 Brand Serial Number • Not mandatory • Must respond with READ_ACK or
DATA_INVALID (if out of range)
125 OpenTherm Version Slave • Not mandatory • Must respond with READ_ACK
127 Slave product version
number and type
HB : product type
LB : product version
• Not mandatory • Must respond with READ_ACK

The slave can respond to all other read/write requests, if not supported, with an UNKNOWN-DATAID reply.
Master units (typically room controllers) should be designed to act in a manner consistent with this rule.

## Pagina 33

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 33

5.2.2 OpenTherm Functions and Data-ID mapping
In OpenTherm are basic functions defined related to central heating, domestic hotwater, information,
diagnostics/service etcetera. For each function is defined which data-id’s are related and are mandatory to
implemented if the function is used. With the definition of functions the interoperability between devices with
the same function is improved. Also the function list can help to select the right product or product
combination.
If a manufacturer states that specific functions are supported then the related data-id’s must be implemented,
unless the data is related to sensors or signals which are not always available in the appliance.

The OpenTherm Function Matrix (excel sheet) gives an overview of all functions with the related data-id’s.
Consult the members area on www.opentherm.eu for the latest published version.

Data-ID’s marked with “B” for a specific function are mandatory for both master and slave.
Data-ID’s marked with “b” for a specific function are mandatory for a master, but only mandatory for the slave
if the data is available (i.e. sensor or input is available in appliance). Also called conditional mandatory Data-
ID’s for a slave.
Data-ID’s marked with “M” for a specific function are mandatory for Master devices
Data-ID’s marked with “m” for a specific function are conditional mandatory for Master devices
Data-ID’s marked with “S” for a specific function are mandatory for Slave devices
Data-ID’s marked with “s” for a specific function are conditional mandatory for Slave devices  (i.e. only if data
or sensor is available)

## Pagina 34

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 34

5.3 Data Classes
5.3.1 Class 1 : Control and Status Information
This group of data-items contains important control and status information relating to the slave and master.
The slave status contains a mandatory fault-indication flag and there is an optional application-specific set of
fault flags which relate to specific faults in boiler-related applications, and an OEM fault code whose meaning
is unknown to the master but can be used for display purposes.

ID Msg Name Type Range Description
0 R   - HB: Master status flag8  bit: description [ clear/0, set/1]
0: CH enable [ CH is disabled, CH is enabled]
1: DHW enable [ DHW is disabled, DHW is enabled]
2: Cooling enable [ Cooling is disabled, Cooling is
enabled]
3: OTC active [OTC not active, OTC is active]
4: CH2 enable [CH2 is disabled, CH2 is enabled]
5: Summer/winter mode [winter mode active, summer
mode active]
6: DHW blocking [DHW unblocked, DHW blocked]
7: reserved
  LB: Slave status flag8  bit: description [ clear/0, set/1]
0: fault indication [ no fault, fault ]
1: CH mode [CH not active, CH active]
2: DHW mode [ DHW not active, DHW active]
3: Flame status [ flame off, flame on ]
4: Cooling status [ cooling mode not active, cooling
mode active ]
5: CH2 mode [CH2 not active, CH2 active]
6: diagnostic/service indication [no diagnostic/service,
  diagnostic/service event]
7: Electricity production [off, on]
70 R   - HB: Master status for
ventilation/heat-recovery
flag8  bit: description   [ clear/0, set/1]
0: Ventilation enable [ disabled, enabled]
1: Bypass position (only bypass manual mode)
                  [ close bypass,  open bypass]
2: Bypass mode       [ manual, automatic]
3: Free ventilation mode [not active, active]
4..7: Reserved
  LB: Status ventilation /
heat-recovery
flag8  bit: description [ clear/0, set/1]
0: Fault indication  [ no fault, fault ]
1: Ventilation  mode [ not active, active]
2: Bypass status  [ closed, open]
3: Bypass automatic status [ manual, automatic]
4: Free ventilation status [not active, active]
5: Reserved
6: Diagnostic indication [no diagnostics, diagnostic event]
7: Reserved

101 R   - HB: Master Solar Storage
status
flag8  bit: description [ clear/0, set/1]
Bit 2,1 and 0 = Solar mode
  000 = off  (solar completely switched off)
  001 = DHW eco (solar heating enabled)
  010 = DHW comfort (boiler keeps small part of storage
            tank loaded)
  011 = DHW single boost (boiler does single loading of
            storage tank )
  100 = DHW continuous boost (boiler keeps whole tank
            loaded)

## Pagina 35

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 35

ID Msg Name Type Range Description
101 R  - LB: Solar Storage mode
and status
flag8  bit: description [ clear/0, set/1]
Bit 0: fault indication
Bit 3,2 and 1 = Solar mode
  000 = off  (solar completely switched off)
  001 = DHW eco (solar heating enabled)
  010 = DHW comfort (boiler keeps small part of storage
            tank loaded)
  011 = DHW single boost (boiler does single loading of
            storage tank )
  100 = DHW continuous boost (boiler keeps whole tank
            loaded)
Bit 5,4 = Solar status
  00= standby
  01= loading of solar storage tank by the sun
  10= loading of solar storage tank by the boiler
  11= anti-legionella mode active

1  -  W Control Setpoint f8.8 0..100 degrees C
(see notes below)
71  -  W LB: Control Setpoint
ventilation / heat-recovery
u8 0..100 Relative ventilation position (0-100%). 0% is
the minimum set ventilation and 100% is the
maximum set ventilation.

5 R   - HB: Application-specific
fault flags
flag8  bit: description [ clear/0, set/1]
0: Service request [service not req’d, service required]
1: Lockout-reset [ remote reset disabled, rr enabled]
2: Low water press [no WP fault, water pressure fault]
3: Gas/flame fault [ no G/F fault, gas/flame fault ]
4: Air press fault [ no AP fault, air pressure fault ]
5: Water over-temp [ no OvT fault, over-temperat. Fault]
6: reserved
7: reserved
  LB: OEM fault code u8 0..255 An OEM-specific fault/error code
72 R   - HB: Application-specific
fault flags ventilation /
heat-recovery
flag8  bit: description [ clear/0, set/1]
0: Service request [service not req’d, service required]
1: Exhaust fan fault [ no fault, fault]
2: Inlet fan fault [ no fault, fault]
3: Frost protection [ not active, active ]
4..7: Reserved

  LB: OEM fault code
ventilation / heat-recovery

u8 0..255 An OEM-specific fault/error code for ventilation
/ heat-recovery system

102 R   - HB: Solar Storage
specific fault flags
flag8  bit: description [ clear/0, set/1]
reserved
  LB: OEM fault code Solar
Storage
u8 0..255 An OEM-specific fault/error code for Solar
Storage
8  -  W Control Setpoint 2
(TsetCH2)
f8.8 0..100 degrees C

115 R - OEM diagnostic code  u16 0..65535 An OEM-specific diagnostic/service code
73 R - OEM diagnostic code
ventilation / heat-recovery

u16 0..65535 An OEM-specific diagnostic/service code
for ventilation / heat-recovery system

Note : The master decides the actual range over which the control Setpoint is defined. The default range is
assumed to be 0 to 100.

There is only one control value defined - data-id=01, the control Setpoint. The control Setpoint ranges
between a minimum of 0 and maximum of 100. It represents directly a temperature Setpoint for the supply
from the boiler. The slave does not need to know how the master has calculated the control Setpoint, e.g.
whether it used room control or OTC, it only needs to control to the value. Likewise, the master does not
need to know how the slave is controlling the supply.

## Pagina 36

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 36

The CHenable bit has priority over the Control Setpoint. The master can indicate that no CH de mand is
required by putting the CHenable bit  = 0 (i.e. CH is disabled), even if the Control Setpoint is non-zero.

The status exchange is a special form of conversation which should be initiated by the master by sending a
READ-DATA(id=0,MasterStatus,00) message. The slave must respond with READ-ACK(id=0,MasterStatus,
SlaveStatus) to send back the Slave Status information in the same single conversation. Since it is
mandatory to support this data object, the slave cannot respond with DATA-INVALID or UNKNOWN-DATAID. A
WRITE-DATA(id=0,…) from the master should not be used.
5.3.2 Class 2 : Configuration Information
This group of data-items defines configuration information on both the slave and master sides. Each has a
group of configuration flags (8 bits) and an MemberID code (1 byte). A valid Read Slave Configuration and
Write Master Configuration message exchange is recommended before control and status information is
transmitted.

ID Msg Name Type Range Description
2 -  W HB: Master configuration flag8  bit: description [ clear/0, set/1]
0: Smart Power [not implemented, implemented]
1-7: reserved

  LB: Master MemberID
code
u8 0..255 MemberID code of the master
3 R   - HB: Slave configuration flag8  bit: description [ clear/0, set/1]
0: DHW present [ dhw not present, dhw is present ]
1: Control type [ modulating, on/off ]
2: Cooling config [ cooling not supported,
  cooling supported]
3: DHW config [instantaneous or not-specified,
  storage tank]
4: Master low-off&pump control function [allowed,
 not allowed]
5: CH2 present [CH2 not present, CH2 present]
6: Remote water filling function [available or unknown,
  not available]. Unknown for
  applications with protocol version 2.2
  or older.
7: Heat/cool mode control [Heat/cool mode switching can
be
 done by  master, Heat/cool mode
switching is done by slave]
  LB: Slave MemberID
code
u8 0..255 MemberID code of the slave
74 R   - HB: Configuration
ventilation / heat-recovery
flag8  bit: description [ clear/0, set/1]
0: System type [ 0= central exhaust ventilation]
                                [ 1= heat-recovery ventilation]
1: Bypass  [ not present, present ]
2: Speed control [ 3-speed, variable]
3..7: Reserved

  LB: MemberID code
ventilation / heat-recovery

u8 0..255 MemberID code of the ventilation / heat-
recovery device

103 R   - HB: Solar Storage
configuration
flag8 0..255 bit: description [ clear/0, set/1]
LB: bit 0: system type
  0 = DHW preheat system
  1 = DHW parallel system
  LB: Solar Storage
member ID
u8  MemberID code of the Solar Storage
124 -  W OpenTherm version
Master
f8.8 0..127 The implemented version of the OpenTherm
Protocol Specification in the master.
125 R - OpenTherm version
Slave
f8.8 0..127 The implemented version of the OpenTherm
Protocol Specification in the slave.

## Pagina 37

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 37

ID Msg Name Type Range Description
75 R - OpenTherm version
ventilation / heat-recovery
f8.8 0..127 The implemented version of the OpenTherm
Protocol Specification in the ventilation / heat-
recovery system.

126 -  W Master product version
number and type
HB : product type
LB : product version

u8
u8

0..255
0..255

The master device product version number
and type as defined by the manufacturer.
127 R  -   Slave product version
number and type
HB : product type
LB : product version

u8
u8

0..255
0..255

The slave device product version number and
type as defined by the manufacturer.
76 R  -   Ventilation / heat-
recovery product version
number and type
HB : product type
LB : product version

u8
u8

0..255
0..255

The ventilation / heat-recovery device product
version number and type as defined by the
manufacturer.

104 R  -   Solar Storage product
version number and type
HB : product type
LB : product version

u8
u8

0..255
0..255

The Solar Storage product version number
and type as defined by the manufacturer.

Note 1 ID2 is related to the Room unit and the slave interface of the Gateway It's advised to set bit 0 to the
value according the Gateway (in practice gateways will not have implemented Smart Power at the
Master interface) since protocol version 3.0 it is mandatory to support Smart Power on a slave
interface. A gateway therefore must support Smart Power on the slave interface and handle ID2
and Smart Power accordingly.

Note 2 An MemberID code of zero signifies a customer non-specific device.

Note 3 The product version number/type should be used in conjunction with the “Member ID code”, which
identifies the manufacturer of the device.

ID Msg Name Type Range Description
93  R  -   HB: Brand index
LB: Brand ASCII
character
u8
u8
0..49
0..255
Index number of the character in the text string
ASCII character referenced by the above index
number
94 R - HB: Brand version index
LB: Brand version ASCII
character
u8
u8
0..49
0..255
Index number of the character in the text string
ASCII character referenced by the above index
number
95 R - HB: Brand serial number
index
LB: Brand serial number
ASCII character
u8
u8
0..49
0..255
Index number of the character in the text string
ASCII character referenced by the above index
number

ID93, ID94 and ID95 are mandatory for the slave. Every slave has to be readable concerning the brand
information- characters.

Example reading the brand index (ID93), ID94 and ID95 work similarly

To read the string of ASCII characters, the master uses the following command:

## Pagina 38

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 38

READ-DATA (id=93,brand-index-number,00).
The slave response will be either :
1. READ-ACK (id=93,max-brand-index-number, index-character)     Everything OK, the requested character
                                                                                                             is returned.
2. DATA-INVALID (id=93,max-brand-index-number,00)                  The brand-index-number is out-of-range
                                                                                                             00 is returned.
3. UNKNOWN-DATAID (id=93, brand-index-number,00)                   The slave does not yet support ID93

Example reading the word “boiler”:
Master:  READ-DATA   ID93   0X00   0X00          Slave answers:  READ-ACK    ID93  0x06  0x62
                 HB = 0X00 → read first character                                                                HB = 0X06 → 6 characters can be read
                 LB = 0x00                                                                                      LB = 0x62→  first character is “b”
The first character is read, the remaining 5 characters can be read in the same way.
5.3.3 Class 3 : Remote Request
This class of data represents commands sent by the master to the slave. There is a single data -id for a
request “packet”, with the Request-Code embedded in the high-byte of the data-value field.

ID Msg Name Type Range Description
4  -  W HB: Request-Code u8 0..255 Request code
0: Back to Normal oparation mode
1: “BLOR”= Boiler Lock-out  Reset request
2: “CHWF”=CH water filling request
3: Service mode maximum power request
   (for instance for CO2 measurement during
Chimney Sweep Function )
4: Service mode minimum power request
   (CO2 measurement)
5: Service mode spark test request (no gas)
6: Service mode fan maximum speed request
   (no flame)
7: Service mode fan to minimum speed
   request (no flame)
8: Service mode 3-way valve to CH request
   (no pump, no flame)
9: Service mode 3-way valve to DHW request
   (no pump, no flame)
10:Request to reset service request flag
11: Service test 1. This is a OEM specific test.
12: Automatic hydronic air purge.
13..255 : -Reserved - for future use
LB: Req-Response-Code u8 0..255 Response to the request
0..127 : Request refused.
128..255 : Request accepted.

Example

The master will send a WRITE-DATA(id=4,Cmd=BLOR,00) message.
The slave response will be either :
1. WRITE-ACK (id=4,Cmd=BLOR, Req-Resp..) The request was accepted; Req-Response-Code indicates
completion status.
2. DATA-INVALID (id=4,BLOR,00) The request was not recognised, Req-Response-Code=00;
3. UNKNOWN-DATAID (id=4, BLOR,00) Remote Request not supported, Req-Response-Code=00;.

## Pagina 39

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 39

5.3.4 Class 4 : Sensor and Informational Data
This group of data-items contains sensor data (temperatures, pressures etc.) and other informational data
from one unit to the other.

ID Msg Name Type Range Description
16  -  W Room Setpoint f8.8 -40..127 Current room temperature Setpoint (°C)
17 R   - Relative Modulation Level f8.8 0..100 Percent modulation between min and max
modulation levels. i.e.
0% = Minimum modulation level
100% = Maximum modulation level
18 R   - CH water pressure f8.8 0..5 Water pressure of the boiler CH circuit (bar)
19 R   - DHW flow rate f8.8 0..16 Water flow rate through the DHW circuit (l/min)
20 R W Day of Week & Time of Day
HB : bits 7,6,5 : day of week
        bits 4,3,2,1,0 : hours
LB : minutes

special

u8

1..7
0..23
0..59

1=Monday, etc…. (0=no DoW info available)

21 R W Date
HB : Month
LB : Day of Month

u8
u8

1..12
1..31

1=January, etc
22 R W Year u16 0..65535 note : 1999-2099 will normally be sufficient
23  -  W Room Setpoint CH2 f8.8 -40..127 Current room Setpoint for 2nd CH circuit (°C)
24  -  W Room temperature f8.8 -40..127 Current sensed room temperature (°C)
25 R   - Boiler water temp. f8.8 -40..127 Flow water temperature from boiler (°C)
26 R   - DHW temperature f8.8 -40..127 Domestic hot water temperature (°C)
27 R   W Outside temperature f8.8 -40..127 Outside air temperature (°C)
28 R   - Return water temperature f8.8 -40..127 Return water temperature to boiler (°C)
29 R   - Solar storage temperature f8.8 -40..127 Solar storage temperature (°C)
30 R   - Solar collector temperature s16 -40..250 Solar collector temperature (°C)
31 R   - Flow temperature CH2 f8.8 -40..127 Flow water temperature of the second central
heating circuit.
32 R   - DHW2 temperature f8.8 -40..127 Domestic hot water temperature 2 (°C)
33 R   - Exhaust temperature s16 -40..500 Exhaust temperature (°C)
34 R  - Boiler heat exchanger
temperature
f8.8 -40..127 Boiler heat exchanger temperature (°C)
35 R  -

HB: Boiler fan speed
Setpoint
u8 0..255 Actual boiler fan speed Setpoint in Hz
(RPM/60)
LB: Boiler fan speed  u8 0..255 Actual boiler fan speed in Hz (RPM/60)
36 R  - Flame current f8.8 0..127 Electrical current through burner flame [µA]
37 - W TrCH2 f8.8 -40..127 Room temperature for 2nd CH circuit (°C)
38 R W Relative Humidity f8.8 0..100 Actual relative humidity as a percentage
77 R   - LB: Relative ventilation u8 0..100 Relative ventilation (0-100%). 0% means that
ventilation is at minimum set value and 100%
means that ventilation is at maximum set
value.
78 R   W LB: Relative humidity u8 0-100 Relative humidity exhaust air (0-100%)

## Pagina 40

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 40

ID Msg Name Type Range Description
79 R   W CO2 level u16 0-2000 CO2 level exhaust air (0-2000 ppm)
80 R   - Supply inlet temperature f8.8 -40..127 Supply inlet temperature (°C)
81 R   - Supply outlet temperature f8.8 -40..127 Supply outlet temperature (°C)
82 R   - Exhaust inlet temperature f8.8 -40..127 Exhaust inlet temperature (°C)
83 R   - Exhaust outlet temperature f8.8 -40..127 Exhaust outlet temperature (°C)
84 R - Actual exhaust fan speed u16 0-6000 Exhaust fan speed in rpm
85 R  - Actual inlet fan speed u16 0-6000 Inlet fan speed in rpm
96 R W Cooling Operation hours u16 0..65535 Number of hours that the slave is in Cooling
Mode. Reset by zero is optional for slave
97 R W Power Cycles u16 0..65535 Number of Power Cycles of a slave (wake-up
after Reset), Reset by zero is optional for slave
98 - W HB: Type of sensor special  bits 3,2,1,0:
 index of sensor

bits 7,6,5,4:
 0000 = Room temp. controllers
 0001 = Room temp. sensors
 0010 = Outside temperature sensors
 1111 = Not defined type
Others are reserved (example: Radiator
valves, humidity, CO2 sensors, wind
velocity)
- W LB : RF and battery
indication
special  bits 1,0
 00 = No battery indication
01 = Low battery (possible loss of
        functionality)
 10 = Nearly low battery (advice to replace
             battery)
 11 = No low battery

bits 4,3,2
 000 = No signal strength indication
 001 = strength 1: Weak or lost signal str.
 010 = strength 2
 011 = strength 3
 100 = strength 4
 101 = strength 5: Perfect signal strength

bits 7,6,5
     reserved
109 R W Electricity producer starts u16 0..65535 Number of start of the electricity producer.
Reset by writing zero. Writing is optional for
slave.
110 R W Electricity producer hours u16 0..65535 Number of hours the electricity produces is in
operation. Reset by writing zero. Writing is
optional for slave.
111 R Electricity production u16 0..65535 Current electricity production in Watt.
112 R W Cumulative Electricity
production
u16 0..65535 Cumulative electricity production in KWh.
Reset by writing zero. Writing is optional for
slave.
113 R W Number of un-successful
burner starts
u16 0..65535

## Pagina 41

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 41

ID Msg Name Type Range Description
114 R W Number of times flame
signal was too low
u16 0..65535
116 R W Successful Burner starts u16 0..65535 Number of successful starts burner. Reset by
writing zero is optional for slave.
117 R W CH pump starts u16 0..65535 Number of starts CH pump. Reset by writing
zero is optional for slave.
118 R W DHW pump/valve starts u16 0..65535 Number of starts DHW pump/valve. Reset by
writing zero is optional for slave.
119 R W DHW burner starts u16 0..65535 Number of starts burner in DHW mode. Reset
by writing zero is optional for slave.
120 R W Burner operation hours u16 0..65535 Number of hours that burner is in operation
(i.e. flame on). Reset by writing zero is optional
for slave.
121 R W CH pump operation hours u16 0..65535 Number of hours that CH pump has been
running. Reset by writing zero is optional for
slave.
122 R W DHW pump/valve operation
hours
u16 0..65535 Number of hours that DHW pump has been
running or DHW valve has been opened. Reset
by writing zero is optional for slave.
123  R W DHW burner operation
hours
u16 0..65535 Number of hours that burner is in operation
during DHW mode. Reset by writing zero is
optional for slave.

## Pagina 42

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 42

5.3.5 Class 5 : Pre-Defined Remote Boiler Parameters
This group of data-items defines specific parameters of the slave device (Setpoints, etc.) which may be
available to the master device and may, or may not,  be adjusted remotely. These parameters are pre -
specified in the protocol and are specifically related to boiler/room controller applications. There is a
maximum of 8 remote boiler parameters. Each remote-boiler-parameter has a upper- and lower-bound (max
and min values) which the master should read from the slave in order to make sure they are not set outside
the valid range. If the slave does not support sending the upper- and lower-bounds, the master can apply
default bounds as it chooses.

The remote-parameter transfer-enable flags indicate which remote parameters are supported by the slave.
The remote-parameter read/write flags indicate whether the master can only read the parameter from the
slave, or whether it can also modify the parameter and write it back to the slave. An Unknown Data-Id
response to a Read Remote-Parameter-Flags message indicates no support for remote-parameters
(equivalent to all transfer-enable flags equal to zero). In these flag bytes bit 0 corresponds to remote-boiler-
parameter 1 and bit 7 to remote-boiler-parameter 8.

ID Msg Name Type Range Description
6 R   - HB: Remote-parameter
transfer-enable flags
flag8 n/a bit: description [ clear/0, set/1]
0: DHW Setpoint [ transfer disabled, transfer enabled ]
1: max CHsetpoint [ transfer disabled, transfer enabled ]
2..7: reserved

 R   - LB: Remote-parameter
read/write flags
flag8 n/a bit: description [ clear/0, set/1]
0: DHW Setpoint [ read-only, read/write ]
1: max CHsetpoint [ read-only, read/write ]
2..7: reserved

86 R   - HB: Remote-parameter
transfer-enable flags
ventilation / heat-recovery

flag8 n/a bit: description                [ clear/0, set/1]
0: Nominal ventilation value [ transfer disabled, transfer
enabled ]
1..7: reserved
 R   - LB: Remote-parameter
read/write flags ventilation /
heat-recovery
flag8 n/a bit: description                 [ clear/0, set/1]
0: Nominal ventilation value [ read-only, read/write ]
1..7: reserved
48 R   - HB: DHWsetp upp-bound s8 0..127 Upper bound for adjustment of DHW setp (°C)
  LB: DHWsetp low-bound s8 0..127 Lower bound for adjustment of DHW setp (°C)
49 R   - HB: max CHsetp upp-bnd s8 0..127 Upper bound for adjustment of maxCHsetp (°C)
  LB: max CHsetp low-bnd s8 0..127 Lower bound for adjustment of maxCHsetp (°C)
56 R  W DHW Setpoint f8.8 0..127 Domestic hot water temperature Setpoint (°C)
57 R  W max CH water Setpoint f8.8 0..127 Maximum allowable CH water Setpoint (°C)
87 R  W HB: Nominal ventilation
value
u8 0-100 Nominal relative value for ventilation (0-
100%), i.e. the value for the mid position in
case of a 3-speed ventilation system. 0% is
the minimum set ventilation and 100% is the
maximum set ventilation.

## Pagina 43

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 43

5.3.6 Class 6 : Transparent Slave Parameters
This group of data-items defines parameters of the slave device which may (or may not) be remotely set by
the master device. These parameters are not pre-specified in the protocol and are “transparent” to the
master in the sense that it has no knowledge about their application meaning.

ID Msg Name Type Range Description
10 R   - HB: Number of TSP's u8 0..255 Number of transparent-slave-parameter
supported by the slave device.
  LB: Not used u8  -Reserved-
11 R  W HB: TSP index no. u8 0..255 Index number of following TSP
  LB: TSP value u8 0..255 Value of above referenced TSP
88 R   - HB: Number of TSP’s
ventilation / heat-recovery

u8 0..255 Number of transparent parameters supported
by the ventilation / heat-recovery system.
  LB: Not used u8  -Reserved-
89 R  W HB: TSP index no.
ventilation / heat-recovery

u8 0..255 Index number of following TSP for ventilation /
heat-recovery system.
  LB: TSP value Ventilation /
heat-recovery
u8 0..255 Value of above referenced TSP for ventilation
/ heat-recovery system.
105 R   - HB: Number of TSP’s Solar
Storage

u8 0..255 Number of transparent parameters supported
by the Solar Storage.
  LB: Not used u8  -Reserved-
106 R  W HB: TSP index no. Solar
Storage

u8 0..255 Index number of following TSP for Solar
Storage.
  LB: TSP value Solar
Storage
u8 0..255 Value of above referenced TSP for Solar
Storage.

The first data-item (id=10) allows the master to read the number of transparent-slave-parameters supported
by the slave. The second data-item (ID=11) allows the master to read and write individual transparent-slave-
parameters from/to the slave.

Example

To read a TSP, the master uses the following command: READ-DATA (id=11,TSP-index,00).
The slave response will be either :
1. READ-ACK (id=11,TSP-index,TSP-value) Everything OK, the requested data-value is returned.
2. DATA-INVALID (id=11,TSP-index,00) The TSP-index is out-of-range or undefined, 00 is returned.
3. UNKNOWN-DATAID (id=11, TSP-index,00) The slave does not support transparent-slave-parameters.

To write a TSP, the master uses the following command: WRITE-DATA(id=11,TSP-index, TSP-value)
The slave response will be either :
1. WRITE-ACK (id=11,TSP-index,TSP-value) Everything OK, the value is echoed back. Note however,
that the TSP-value may be changed by the slave if it is out-
of-range.
2. DATA-INVALID (id=11,TSP-index,00) The TSP-index is out-of-range or undefined, 00 is returned.
3. UNKNOWN-DATAID (id=11,TSP-index,00) The slave does not support transparent-slave-parameters.

## Pagina 44

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 44

5.3.7 Class 7 : Fault History Data
This group of data-items contains information relating to the past fault condition of the slave device.

ID Msg Name Type Range Description
12 R   - HB: Size of Fault Buffer u8 0..255 The size of the fault history buffer..
  LB: Not used u8  -Reserved-
13 R   - HB: FHB-entry index no. u8 0..255 Index number of following Fault Buffer entry.
  LB: FHB-entry value u8 0..255 Value of above referenced Fault Buffer entry.
90 R   - HB: Size of Fault Buffer
ventilation / heat-recovery

u8 0..255 The size of the fault history buffer for
ventilation / heat-recovery system.
  LB: Not used u8  -Reserved-
91 R   - HB: FHB-entry index no.
ventilation / heat-recovery

u8 0..255 Index number of following Fault Buffer entry
for ventilation / heat-recovery system.
  LB: FHB-entry value
ventilation / heat-recovery

u8 0..255 Value of above referenced Fault Buffer entry
for ventilation / heat-recovery system.
107 R   - HB: Size of Fault Buffer
Solar Storage

u8 0..255 The size of the fault history buffer for Solar
Storage.
  LB: Not used u8  -Reserved-
108 R   - HB: FHB-entry index no.
Solar Storage

u8 0..255 Index number of following Fault Buffer entry
for Solar Storage.
  LB: FHB-entry value Solar
Storage

u8 0..255 Value of above referenced Fault Buffer entry
Solar Storage.

The first data-item (id=12) allows the master to read the size of the fault history buffer supported by the
slave. The second data-item (ID=13) allows the master to read individual entries from the buffer.

Example

To read an entry from the fault history buffer, the master uses the following command:
READ-DATA (id=13,FHB-index,00).
The slave response will be either :
1. READ-ACK (id=13,FHB-index,FHB-value) Everything OK, the requested value is returned.
2. DATA-INVALID (id=13,FHB-index,00) The FHB-index is out-of-range or undefined, 00 is returned.
3. UNKNOWN-DATAID (id=13, FHB-index,00) The slave does not support a fault history buffer.

## Pagina 45

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 45

5.3.8 Class 8 : Control of Special Applications
5.3.8.1 Cooling Control

ID Msg Name Type Range Description
7 -  W Cooling control signal f8.8 0..100% Signal for cooling plant.

The cooling control signal is to be used for cooling applications. First the master should determine if the
slave supports cooling by reading the slave configuration. Then the master can use the cooling control signal
and the cooling-enable flag (status) to control the cooling plant. The status of the cooling plant can be read
from the slave cooling status bit.
5.3.8.2 Boiler Sequencer Control

ID Msg Name Type Range Description
14 -  W Maximum relative
modulation level setting
f8.8 0..100% Maximum relative boiler modulation level
setting for sequencer and off-low&pump
control applications.
15 R  - Maximum boiler capacity &
Minimum modulation level
HB : max. boiler capacity
LB : min. modulation level

u8
u8

0..255kW
0..100%

expressed as a percentage of the maximum
capacity.

The boiler capacity level setting is to be used for boiler sequencer applications. The control Setpoint should
be set to maximum, and then the capacity level setting throttled back to the required value. The default value
in the slave device should be 100% (i.e. no throttling back of the capacity). The master can read the
maximum boiler capacity and minimum modulation levels from the slave if it supports these.
5.3.8.3 Remote override room Setpoint

ID* Msg.
Type NAME Format Range DESCRIPTION
9 R - Remote Override Room
Setpoint
f8.8 0..30 0= No override
1..30= Remote override room Setpoint
39 R - Remote Override Room
Setpoint 2
f8.8 0..30 0= No override
1..30= Remote override room Setpoint 2
99 R W LB: Remote Override
Operating Mode Heating
special  0..255 Bit0..3: Operating Mode HC1 (0..15)
 0 = No override
 1 = Auto (time switch program)
 2 = Comfort
 3 = Precomfort
 4 = Reduced
 5 = Protection (e.g. frost)
 6 = Off
 7…15 = reserved
Bit4..7: Operating Mode HC2 (0..15)
 0 = No override
 1 = Auto (time switch program)
 2 = Comfort
 3 = Precomfort
 4 = Reduced
 5 = Protection (e.g. frost)
 6 = Off
 7…15 = reserved

## Pagina 46

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 46

  HB: Remote Override
Operating Mode DHW
special 0..255 Bit0..3: Operating Mode DHW (0..15)
 0 = No override
 1 = Auto (time switch program)
 2 = Anti-Legionella
 3 = Comfort
 4 = Reduced
 5 = Protection (e.g. frost)
 6 = Off
 7…15 = reserved
Bit4..7: Process bits DHW (Bitset)
[clear/0;set/1]
 Bit4 = Manual DHW push2     [no push;
push]
 Bit5..7 = reserved (set to 0)

100 R - LB: Remote Override
Room Setpoint function
flag8 0..255 bit: description [ clear/0, set/1]
0: Manual change priority [disable overruling
remote Setpoint by manual Setpoint
change, enable overruling remote Setpoint
by manual Setpoint change ]
1: Program change priority [disable
overruling remote Setpoint by program
Setpoint change, enable overruling remote
Setpoint by program Setpoint change ]
2: reserved
3: reserved
4: reserved
5: reserved
6: reserved
7: reserved
HB: reserved u8 0 reserved

Note’s to Remote Override Room Setpoint (ID9, ID39 and ID100):
There are applications where it’s necessary to override  the room Setpoint of the master (room-unit).
For instance in situations where room controls are connected to home or building controls or room controls in
holiday houses which are activated/controlled remotely.

The master can read on Data ID 9 the remote override room Setpoint. A value unequal to zero is a valid
remote override room Setpoint. A value of zero means no remote override room Setpoint. ID100 defines how
the master should react while remote room Setpoint is active and there is a manual Setpoint change an d/or a
program Setpoint change.

On ID39, the master can read the Remote Override Room Setpoint of a second zone (comparable to ID9).

The master can read on Data ID 99 (proposal) the remote override Operating Modes. A value unequal to zero is a
valid remote override Operating Mode. A value of zero means no remote override Operating Mode.

Note’s to Remote Override of Operating Modes for CH and DHW:
With the 'No Override' feature for Heating and DHW you are able to change Heating or DHW only.

'Manual DHW-push' means: rise the DHW temperature once to Comfort level and return to previous
Operating Mode (for DHW storage tanks)

The Operating Modes are choosen according to prEN 15'500 with some extensions.

## Pagina 47

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 47

5.4 Data-Id Overview Map
Nr. Msg Data Object Type   Description
0 R - Status flag8 / flag8 Master and Slave Status flags.
1 - W Tset f8.8 Control Setpoint  i.e. CH  water temperature Setpoint (°C)
2 - W M-Config / M-MemberIDcode flag8 / u8 Master Configuration Flags /  Master MemberID Code
3 R - S-Config / S-MemberIDcode flag8 / u8 Slave Configuration Flags /  Slave MemberID Code
4 - W Remote Request u8 / u8 Remote Request
5 R - ASF-flags / OEM-fault-code flag8 / u8 Application-specific fault flags and OEM fault code
6 R - RBP-flags flag8 / flag8 Remote boiler parameter transfer-enable & read/write flags
7 - W Cooling-control f8.8 Cooling control signal (%)
8 - W  TsetCH2 f8.8 Control Setpoint for 2e CH circuit (°C)
9 R - TrOverride f8.8 Remote override room Setpoint
10 R - TSP u8 / u8 Number of Transparent-Slave-Parameters supported by slave
11 R W TSP-index / TSP-value u8 / u8 Index number / Value of referred-to transparent slave parameter.
12 R - FHB-size u8 / u8 Size of Fault-History-Buffer supported by slave
13 R - FHB-index / FHB-value u8 / u8 Index number / Value of referred-to fault-history buffer entry.
14 - W Max-rel-mod-level-setting f8.8 Maximum relative modulation level setting (%)
15 R - Max-Capacity / Min-Mod-Level u8 / u8 Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
16 - W TrSet f8.8 Room Setpoint (°C)
17 R - Rel.-mod-level f8.8 Relative Modulation Level (%)
18 R - CH-pressure f8.8 Water pressure in CH circuit  (bar)
19 R - DHW-flow-rate f8.8 Water flow rate in DHW circuit. (litres/minute)
20 R W Day-Time special / u8 Day of Week and Time of Day
21 R W Date u8 / u8 Calendar date
22 R W Year u16 Calendar year
23 - W TrSetCH2 f8.8 Room Setpoint for 2nd CH circuit (°C)
24 - W Tr f8.8 Room temperature (°C)
25 R - Tboiler f8.8 Boiler flow water temperature (°C)
26 R - Tdhw f8.8 DHW temperature (°C)
27 R W Toutside f8.8 Outside temperature (°C)
28 R - Tret f8.8 Return water temperature (°C)
29 R - Tstorage f8.8 Solar storage temperature (°C)
30 R - Tcollector f8.8 Solar collector temperature (°C)
31 R - TflowCH2 f8.8 Flow water temperature CH2 circuit (°C)
32 R - Tdhw2 f8.8 Domestic hot water temperature 2 (°C)
33 R - Texhaust s16 Boiler exhaust temperature (°C)
34 R  - Tboiler-heat-exchanger f8.8 Boiler heat exchanger temperature (°C)
35 R  - Boiler fan speed Setpoint and actual u8 / u8 Boiler fan speed Setpoint and actual value
36 R - Flame current f8.8 Electrical current through burner flame [μA]
37 - W TrCH2 f8.8 Room temperature for 2nd CH circuit (°C)
38 R W Relative Humidity f8.8 Actual relative humidity as a percentage
39 R - TrOverride 2 f8.8 Remote Override Room Setpoint 2
48 R - TdhwSet-UB / TdhwSet-LB s8 / s8 DHW Setpoint upper & lower bounds for adjustment  (°C)
49 R - MaxTSet-UB / MaxTSet-LB s8 / s8 Max CH water Setpoint upper & lower bounds for adjustment  (°C)
56 R W TdhwSet f8.8 DHW Setpoint (°C)  (Remote parameter 1)
57 R W MaxTSet f8.8 Max CH water Setpoint (°C) (Remote parameters 2)
70 R - Status ventilation / heat-recovery flag8 / flag8 Master and Slave Status flags ventilation / heat-recovery

## Pagina 48

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 48

Nr. Msg Data Object Type   Description
71  -  W Vset - / u8 Relative ventilation position (0-100%). 0% is the minimum set
ventilation and 100% is the maximum set ventilation.

72 R - ASF-flags / OEM-fault-code
ventilation / heat-recovery
flag8 / u8 Application-specific fault flags and OEM fault code ventilation / heat-
recovery
73 R - OEM diagnostic code
ventilation / heat-recovery

u16 An OEM-specific diagnostic/service code
for ventilation / heat-recovery system
74 R - S-Config / S-MemberIDcode
ventilation / heat-recovery
flag8 / u8 Slave Configuration Flags /  Slave MemberID Code ventilation /
heat-recovery
75 R - OpenTherm version
ventilation / heat-recovery
f8.8 The implemented version of the OpenTherm Protocol Specification in
the ventilation / heat-recovery system.

76 R - Ventilation / heat-recovery version u8 / u8 Ventilation / heat-recovery product version number and type
77 R   - Rel-vent-level - / u8 Relative ventilation (0-100%)
78 R   W RH-exhaust - / u8 Relative humidity exhaust air (0-100%)
79 R   W CO2-exhaust u16 CO2 level exhaust air (0-2000 ppm)
80 R   - Tsi f8.8 Supply inlet temperature (°C)
81 R   - Tso f8.8 Supply outlet temperature (°C)
82 R   - Tei f8.8 Exhaust inlet temperature (°C)
83 R   - Teo f8.8 Exhaust outlet temperature (°C)
84 R - RPM-exhaust u16 Exhaust fan speed in rpm
85 R - RPM-supply u16 Supply fan speed in rpm
86 R - RBP-flags ventilation / heat-recovery flag8 / flag8 Remote ventilation / heat-recovery parameter transfer-enable &
read/write flags
87 R  W Nominal ventilation value u8 / - Nominal relative value for ventilation (0-100 %)
88 R - TSP ventilation / heat-recovery

u8 / u8 Number of Transparent-Slave-Parameters supported by TSP’s
ventilation / heat-recovery

89 R W TSP-index / TSP-value ventilation /
heat-recovery

u8 / u8 Index number / Value of referred-to transparent TSP’s ventilation /
heat-recovery parameter.
90 R - FHB-size ventilation / heat-recovery

u8 / u8 Size of Fault-History-Buffer supported by ventilation / heat-recovery

91 R - FHB-index / FHB-value ventilation /
heat-recovery
u8 / u8 Index number / Value of referred-to fault-history buffer entry
ventilation / heat-recovery
93 R - Brand u8 / u8 Index number of the character in the text string
ASCII character referenced by the above index number
94 R - Brand Version u8 / u8 Index number of the character in the text string
ASCII character referenced by the above index number
95 R - Brand Serial Number u8 / u8 Index number of the character in the text string
ASCII character referenced by the above index number
96 R W Cooling Operation Hours u16 Number of hours that the slave is in Cooling Mode. Reset by zero is
optional for slave
97 R W Power Cycles u16 Number of Power Cycles of a slave (wake-up after Reset), Reset by
zero is optional for slave
98 - W RF sensor status information special /
special
For a specific RF sensor the RF strength and battery level is written
99 R W Remote Override Operating Mode
Heating/DHW
special /
special
Operating Mode HC1, HC2/ Operating Mode DHW
100 R - Remote override function flag8 / - Function of manual and program changes in master and  remote
room Setpoint
101 R - Status Solar Storage flag8 / flag8 Master and Slave Status flags Solar Storage
102 R - ASF-flags / OEM-fault-code Solar
Storage
flag8 / u8 Application-specific fault flags and OEM fault code Solar Storage
103 R - S-Config / S-MemberIDcode Solar
Storage
flag8 / u8 Slave Configuration Flags /  Slave MemberID Code Solar Storage

## Pagina 49

OpenTherm™ Protocol Specification v4.2  OT/+ Application Layer
©1996, 2011 The OpenTherm Association  Page 49

Nr. Msg Data Object Type   Description
104 R - Solar Storage version u8 / u8 Solar Storage product version number and type
105 R - TSP Solar Storage

u8 / u8 Number of Transparent-Slave-Parameters supported by TSP’s Solar
Storage

106 R W TSP-index / TSP-value Solar
Storage

u8 / u8 Index number / Value of referred-to transparent TSP’s Solar Storage
parameter.
107 R - FHB-size Solar Storage

u8 / u8 Size of Fault-History-Buffer supported by Solar Storage

108 R - FHB-index / FHB-value Solar
Storage
u8 / u8 Index number / Value of referred-to fault-history buffer entry Solar
Storage
109 R W Electricity producer starts U16 Number of start of the electricity producer.
110 R W Electricity producer hours U16 Number of hours the electricity produces
is in operation
111 R Electricity production U16 Current electricity production in Watt.
112 R W Cumulativ Electricity production U16 Cumulative electricity production in KWh.
113 R W Un-successful burner starts u16 Number of un-successful burner starts
114 R W Flame signal too low number u16 Number of times flame signal was too low
115 R - OEM diagnostic code u16 OEM-specific diagnostic/service code
116 R W Successful Burner starts u16 Number of succesful starts burner
117 R W CH pump starts u16 Number of starts CH pump
118 R W DHW pump/valve starts u16 Number of starts DHW pump/valve
119 R W DHW burner starts u16 Number of starts burner during DHW mode
120 R W Burner operation hours u16 Number of hours that burner is in operation (i.e. flame on)
121 R W CH pump operation hours u16 Number of hours that CH pump has been running
122 R W DHW pump/valve operation hours u16 Number of hours that DHW pump has been running or DHW valve
has been opened
123 R W DHW burner operation hours u16 Number of hours that burner is in operation during DHW mode

124 -  W OpenTherm version Master f8.8 The implemented version of the OpenTherm Protocol Specification
in the master.
125 R - OpenTherm version Slave f8.8 The implemented version of the OpenTherm Protocol Specification
in the slave.
126 - W Master-version u8 / u8 Master product version number and type
127 R - Slave-version u8 / u8 Slave product version number and type

All data id’s not defined above are reserved for future use.

## Pagina 50

OpenTherm™ Protocol Specification v4.2  OT/- Encoding & Application Support
©1996, 2011 The OpenTherm Association  Page 50

6. OpenTherm/Lite Data Encoding and Application Support
IMPORTANT: As of OpenTherm version 4.1 OpenTherm/Lite – OT/- is no longer being tested or certified and
has been demoted to legacy functionality, it should not be incorporated in new designs

OpenTherm / Lite uses the same medium and physical signalling levels as OpenTherm/plus as described in
section 3. It can be implemented using the same hardware as for OT/+.
6.1 Room Unit to Boiler Signalling
The room unit transmits a PWM signal to the boiler.

Vlow
Vhigh
Time
Voltage
tperiod
tlowthigh

Duty Cycle (%) = tlow / tperiod i.e. the % time over the period that the line is low.

The Duty Cycle Period (tperiod) does not require to be constant. The frequency of the PWM signal must lie
between 100Hz and 500Hz (2ms < tperiod < 10ms). The duty cycle can vary between 0% and 100%.
6.2 Boiler to Room Unit Signalling
The boiler signals only by changing the current between the Ilow and Ihigh states. The high current state
represents the presence of a boiler lock-out fault. Different from OpenTherm/plus, it can permanently keep
the line in the high current state. It is mandatory for the room and boiler controller to support the transmission
and detection of this feature.

## Pagina 51

OpenTherm™ Protocol Specification v4.2  OT/- Encoding & Application Support
©1996, 2011 The OpenTherm Association  Page 51

Ihigh
Ilow
Time
Current
Boiler Lock-Out Fault Indication
No Fault (normal condition)

6.3 OT/- Application Data Equivalence to OT/+
The PWM voltage signal sent from the room unit to the boiler unit, principally represents the water
temperature Control Setpoint It is mandatory for both the room and boiler units to support the transmission
and detection of this signal.

OpenTherm/Lite supports the following application data items, which are shown with their equivalent OT/+
Application Data-IDs.

OpenTherm/Lite Data Item Equivalent OT/+ Data Item and Data-ID
Tset Control Setpoint (mandatory) id=1  Tset Control Setpoint (mandatory)
CH-Enable (mandatory) id=0, bit 0.0 Master Status : CH-Enable flag
(mandatory)
Boiler Lock-Out Fault (mandatory) id=0, bit 1.0 Slave Status : Fault Indication (mandatory)

CH is disabled
CH is enabled
CH-Enable Tset
10°
90°
Duty-Cycle
90%
5%
10%
0% 100%

The tolerance of both generation and measurement of the Duty-Cycle signal should be less than ±2%.

## Pagina 52

OpenTherm™ Protocol Specification v4.2  OT/- Encoding & Application Support
©1996, 2011 The OpenTherm Association  Page 52

A Duty-Cycle less than 5% is used to indicate a CH-disabled state (i.e. “positive-off” or no CH-demand
condition). It is not mandatory for the boiler controller to support this feature, or for the room unit to use it.

