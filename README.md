# ASIC_hacks
ASIC HVAC comms bus experiments

The purpose of this collection is to document my exploration of the packet structure of an RS-485 serial communication bus utilised to transfer information between HVAC equipment manufactured by ASI Controls. (http://www.asicontrols.com/) 
The code is for an Arduino based device that can successfully send and receive data on the bus.

The HVAC system under investigation consists of two “ASIC/2” controllers (a 7040 and an 8540) for I/O and control and a “UnioOP - ePAD04” Human Machine Interface (HMI) panel. Programming and configuration of the ASIC/2 controllers and ePAD HMI is performed using proprietary software packages and is not discussed here.

In this particular system, the RS485 bus is mirrored on an RS232 bus.  All investigations were performed via the RS232 bus.

# HARDWARE
Very little hardware was required for the initial proof of concept:
- An Arduino Uno.
- An RS232 “shield” from Seeed Studios http://wiki.seeedstudio.com/wiki/RS232_Shield
- A D9 null modem cable and gender changer.

In addition to this, a 2x24 character LCD display was used to display the data.

The RS232shield utilises a MAX232 IC to convert between the TTL Arduino signals and the significantly higher and lower RS232 voltages. I do not intend to discuss the technical aspects of RS232 here but suffice to say, it is a really bad idea to try to connect an Arduino to an RS232 bus without this type of voltage level conversion.

The shield also allows the TX and RX signals to be connected to any of the Arduino digital pins, thus making it possible to use the “Software Serial” library.  This leaves the Arduino UART pins free for standard USB/Serial programming uploads and communication.
The shield is supplied with a female D9 connector and configured as a DCE (data carrier equipment) device. 
Since the ASIC controller is also a DCE device, the two are not directly pin compatible. Thus, a null modem cable and gender changers are required, so that the Arduino acts as a DTE (data terminal equipment) device like a laptop running a terminal emulator (e.g. puTTY.)

The LCD dispaly is a standard HD44780 16 pin display driven via a shift register to reduce the number of control pins.  The "LiquidCrystal595" Arduino library by Rowan Simms was used. ( https://github.com/haiphamngoc/LiquidCrystal595 )  

# ASIC/2 data structures.
The following is paraphrased from the ASIC/2 documentation:

The software control elements of the ASIC/2 are referred to as “Objects”.  Objects perform a specific function and have a specific data structure associated with them.  Objects include “Inputs” and “Outputs” for dealing with digital and analogue signals, “Clock” objects for time keeping. “Logic” objects for boolean operations and a host of other more specialised structures.

Each object type can be used multiple times, with each use called an “Instance”.  For an Input or Output object, the Instance will correspond to a physical input or output.

Each Object Instance consists of many bytes of data. This data is partitioned into “Attributes”.  For example, the first two bytes of an Input Object are the Attribute “Present Value”.

An Attribute may be further divided, so to find the state of a single status bit (for example), you must “Select” the required bit.
Together Object/Attribute/Instance/Select provides a “Handle” that is a unique address for every data point in the system.

# Data telegraph structure: QUERY.
A typical QUERY telegraph on the data bus is 15 bytes long.  This is an example of a request from the ePAD HMI screen for the present value of an analogue input on the 7040 controller.  I have started the numbering from zero to simplify reference to the code:

02 7D 65 FE F2 91 77 02 05 03 00 01 01 02 EA 

Byte 0: Start of message signal. Always 0x02

Byte 1: Destination node (the 7040 controller) address HI byte

Byte 2: Destination node address LO byte
  Destination node address is always a 16 bit word.  If the system uses “token passing”, the node address will be between 32001 and 32255. Our system does not, however the address is still in that range. 0x7D65  = 32101.  Depending on the installation configuration, any addresses in the range 00001 to 65535 “not divisible by 256” seem to be possible.  The value 0x5AFF  (23295) indicates a “global” message. 

Byte 3: Origin node (the ePAD display) address HI byte

Byte 4: Origin node address LO byte
	In this case the origin node address is the ePAD panel. 0xFEF2 = 65266.

Byte 5: Function code. 
  I have not been able to locate a function code list in any of the documentation I’ve found.  I assume 0x91 is something like “read value”.

Byte 6: Unknown.  Always 0x77 in this sort of data query telegraph.
  The value 0x77 always appears after the function code in a query and before data in response.  Probably an “end of header” or “start of data” signal?

Byte 7: Delimiter?  “Start of handle” signal?  Always 0x02.

Byte 8: Handle - “Object”
  Values consistent with the type values as defined in the Visual Expert configuration software.  E.g. 0x05 is an Input object, 0x03 is an Output object, etc.

Byte 9: Handle - “Instance” 
  For inputs and Outputs, values consistent with the physical hardware address as defined in the Visual Expert configuration software.

Byte 10: Handle – “Attribute” 

Byte 11: Handle – “Select” 

Byte 12: Unknown. 

Byte 13: Delimiter?  “End of handle” signal?  Always 0x02.

Byte 14: Check sum.  
  The check sum is simply the lowest byte of a simple addition of all the other query message bytes.

# Data telegraph structure: RESPONSE.
A RESPONSE telegraph on the data bus has a length that depends on what the query was.  The response for a query on a single input or output value is always 18 bytes long.  It is this type of response that is examined below.  
This is an example of a response from the request documented above.  It is the present value of an analogue input on the 7040 controller addressed to the ePAD HMI screen.  I have started the numbering from zero to simplify reference to the code included later:

02 FE F2 7D 65 06 91 02 05 03 00 01 01 02 77 AB 08 A3

Byte 0: Start of message signal. Always 0x02

Byte 1: Destination node (the ePAD display) address HI byte

Byte 2: Destination node address LO byte

Byte 4: Origin node (the 7040 controller) address HI byte

Byte 5: Origin node address LO byte
  Note that since this is a response, the destination and origin values are simply the inverse of the request message.

Byte 6: Unknown. Seems to be always 0x06 for this type of response telegraph.  
  Perhaps an acknowledge code indicating that the message is a response?  

Byte 6: Echo of Function code in byte 6 of the query. 

Byte 7: Delimiter?  “Start of handle” signal?  Always 0x02.

Byte 8: Handle - “Object”. Echo of byte 9 of query.

Byte 9: Handle - “Instance”. Echo of byte 10 of query. 

Byte 10: Handle – “Attribute”. Echo of byte 11 of query. 

Byte 11: Handle – “Select”. Echo of byte 12 of query. 

Byte 12: Unknown. Echo of byte 13 of query.

Byte 13: Delimiter?  “End of handle” signal?  
0x02 if data is returned. 0x00 if the handle is not defined in the controller..

Byte 15: Unknown.  Always 0x77 in this sort of data response telegraph.
  The value 0x77 always appears after the function code in a query and before data in response.  Probably an “end of header” or “start of data” signal?

Byte 16: First data byte (“LO” byte).

Byte 17: Second data byte (“HI” byte).
  The Visual Expert manual states that Attribute 0 of an INPUT object is “Present Value” and has the type “signed word”, but this type is a little misleading and depends on the data being represented.  The data being represented is configured in the Visual Expert configuration software.

- For a binary type Input, 0xFFFF is ON and 0x0000 is OFF. 
- For an analogue type Input, the 10 bit raw value from the physical input is stored, presumably with the MSB of the HI byte being the sign bit and the remaining 5 bits of the HI byte set to 0.
- If a “conversion type” is set for the analogue Input in the Visual Expert software to correct the raw value according to the type of sensor used, MSB of the HI data byte is again the sign bit (not confirmed) and presumably all the remaining HI and LO bits encode the data, giving a possible range of +/-32767.  I have confirmed that on our system the value encoded in these bytes can be simply divided by 100 to give the present temperature value in Centigrade to 2 decimal places. Presumably temperature values up to a ridiculous  327.67oC ( LO = 0xFF, HI = 0x7F) and below absolute zero (- 327.67oC : LO = 0xFF, HI = 0xFF) can be encoded by this method.

  The Visual Expert manual states that Attribute 0 of a DIGITAL OUTPUT object is “Present Value” of type “word”, but this is again a little misleading. The data is encoded in only the “LO” byte: 0x0001 is ON and 0x0000 is OFF. 

  The Visual Expert manual states that Attribute 0 of an ANALOGUE OUTPUT object is also “Present Value” of type “word”, but again this is a little misleading. The data is encoded in only the first “LO” byte as a value between 0 (0x0000) and 255 (0x00FF).  This drives an actual voltage value at the output between 0 and 10V.

Byte 18: Check sum.  
  Again, this is the lowest byte of a simple addition of all the previous message bytes.

