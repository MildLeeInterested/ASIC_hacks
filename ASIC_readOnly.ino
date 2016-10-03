/* Communication application for ASI Controls system bus.
 *
 * This sketch intercepts analog input readings from the bus
 * and displays as temperature readings for ACs 1,2,3,4 and 6.
 *
 * Based on "SoftwareSerialExample" by Tom Igoe
 * 
 * This example code is in the public domain.
 *
 * The circuit utilises the SeeedStudio RS232 shield.
 * http://www.seeedstudio.com/wiki/RS232_Shield
 * 
 * Set RX jumper to digita1 pin 11.
 * Set TX jumper to digital pin 10.
 * Use a null modem cable for connection to the ASIC/2 bus.
 */

int TXpin = 10;
int RXpin = 11;

#include <icrmacros.h>

int state  = 0; //used to step through the read sequence. 
/*  0 = Cycle complete or returned from receive procedure in fault mode. Reset message array
 *  1 = Waiting for data to appear in buffer
 *  2 = Prepare to receive data into array.
 *  3 = Receive data and place in message array
 *  4 = Extract and store temperature values
 *  5 = Display data.  Reset to first step.
 */

int fault = 0;// fault codes set by RECEIVE function.
/*
 * 0 = no fault
 * 1 = not defined
 * 2 = no start char
 * 3 = message to long
 * 4 = wrong check sum
 * 5 = not to expected rx node (ePAD)
 * 6 = not from expected tx node (ASIC/2)
 * 7 = wrong object type
 * 8 = wrong instance (not used)
 * 9 = invalid handle - address not found in controller
 */

unsigned int node_ePAD = 65266;//Holds the node address of the ePAD HMI device = 65266.
unsigned int node_7040 = 32101;//Holds the node address of the ASIC/2 7040 PLC = 32101.
unsigned int node_8540 = 32102;//Holds the node address of the ASIC/2 8540 PLC = 32102.

byte message[18];            // An array to hold the response message.
int messageSize = sizeof(message);

byte maxIndex = 20;          //Normal ASIC reply message length is 18 bytes.  Back stop to end loop.
byte index = 0;              //Increments as each reply byte is read into the message array.
byte checkSum = 0;           //Holds the check sum value as it is calculated.

//Some Specific byte designations in incomming ASIC message.
int HIrxNode = 1;
int LOrxNode = 2;
int HItxNode = 3;
int LOtxNode = 4;
int objectByte = 8;          
int instanceByte = 9;
int attributByte = 10;
int selectByte = 11;
int acknowledgeByte = 13;     //0x02 if valid, 0x00 if not valid.
int LOdataByte = 15;
int HIdataByte = 16;
int checksumByte = 17;

#include <SoftwareSerial.h>

SoftwareSerial ASICserial(TXpin, RXpin);

int const ACsize = 7;
int AC[ACsize];                    // An array to hold the AC temperature values.  Note that only index 1,2,3,4 and 6 are used.  Index 0 will store the factory temperature.

void setup()
{
  for (int x = 0; x < messageSize; x++)
    //Clear the message array.
  {
    message[x] = 0;
  }

  for (int x = 0; x < ACsize; x++)
    //Clear the AC array.
  {
    AC[x] = 0;
  }



  // Open serial communications on hardware serial port
  Serial.begin(57600);
  serialFlush();// probably not necessary...

  ASICserial.begin(9600);
  ASICserialFlush();// probably not necessary...
}

void loop()
{


  switch (state)
  {
  case 0: 
  //Cycle complete or returned from receive procedure in fault mode.
  //Reset message array
    state = 1;
    for (int x = 0; x < messageSize; x++)
      //Clear the message array.
    {
      message[x] = 0;
    }
    delay(1);
    break;

  case 1: 
  //Waiting for data to appear in buffer
    if (ASICserial.available() > 0)
      state = 2;
    break;

  case 2: 
  //prepare to receive data into array.
    checkSum = 0; // reset for calculating the incoming check sum
    fault = 0;//reset fault value for incomming message
    index = 0;
    state = 3;
    delay(1);//critical!!!
    break;

  case 3: 
  //receive data and place in message array
    state = RECEIVE(state, node_ePAD, node_7040, node_8540);
    delay(1);//critical!!!
    //state = 4;
    break;

  case 4: //Extract and srore temperature values
    state = STORE(state, node_7040, node_8540);
    //state = DISPLAY1(state);
    //state = 5;
    break;

  case 5:    //Display data.  Return to first step.
    for (int x = 0; x < ACsize; x++)
    {
      if (x != 0 && x != 5)
      {  
        Serial.print("A/C");
        Serial.print(x);
        Serial.print(": ");
        Serial.print(AC[x] / 100);
        Serial.print(".");
        Serial.print(AC[x] % 100);
        Serial.println(" degrees ");
      } 
    }
    Serial.println(" ");

    state = 0;
    break;

  default:
    break;
  }


  while (ASICserial.available() <= 0)
  {
    // do nothing until there is data in the software serial buffer.
  }

}





int RECEIVE (int indexNOW, int rxAddress, int txAddress1, int txAddress2)
// This procedure reads bytes from the ASIC bus and peforms error checking to ensure a
// valid message has been received.

//STRUCTURE:
//byte 0  = Start of message.  Always 0x02
//byte 1  = reciever node address byte 1.  Always = query byte 3
//byte 2  = receiver node address byte 2.  Always = query byte 4
//byte 3  = transmitter node address byte 1. Always = query byte 1
//byte 4  = transmitter node address byte 2. Always = query byte 2
//byte 5  = function code. Always 0x06
//byte 6  = function code. Always = query byte 5 (0x91)
//byte 7  = handle start. Always 0x02
//byte 8  = input object. Always = query byte 8. Always 0x05 for an input.  Change as required for other types.
//byte 9  = instance. Always = query byte 9.
//byte 10 = attribute. Always = query byte 10. Always 0x00.  Note that parsing to byte removes abiguity with NULL character.
//byte 11 = select. Always = query byte 11. Always 0x01
//byte 12 = unknown. Always = query byte 12. Always 0x01
//byte 13 = handle end (acknowledge?). Usually 0x02.  Response returns 0x00 if handle not valid.
//byte 14 = function code?  Always = query byte 6. (0x77).
//byte 15 = LOW DATA BYTE
//byte 16 = HIGH DATA BYTE
//byte 17 = checksum.  Lowest byte of simple addition of all previous bytes.

{
  int resetCaseValue = 0;       //reset the master sequence.
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.
  //int incrementCaseValue = 4;   //move on to the next sequence step.


  if (byte(ASICserial.peek()) != 0x02)
    //Check that first character is a valid message start character.
    //if not, terminate with fault code.
  {
    ASICserialFlush();
    fault = 2;
    return resetCaseValue;
  }

  /*
    //Note:
   //ASICserial.peek() must be parsed to a byte type, as it seems to return a 32bit unsigned integer and
   //sometimes the leading bytes are FFFFFF rather than 000000.  Parsing to byte is a simple way to make
   //sure only the least significant byte is compared with the value in "checkSum".
   */

  //Read data from the software serial buffer into the message array.
  while (index < messageSize-1)
  {
    delay(1);//critical!

    message[index] = ASICserial.read();  //Add each byte to the array.

    checkSum = checkSum + message[index];
    //The check sum of an ASIC message is the least significant byte of a simple addition of all the message bytes.
    //Since "checkSum" is type byte, this calculation will automatically discard the upper bytes.

    index = index + 1;  //Increment the index
  }

  message[index] = ASICserial.read();//get the last byte from the buffer.  This will be the checksum if this is a valid reply message.


  //**** error checking  ****

  if (index >= maxIndex)
    //Check that the size of the message is not greater than expected.
    //Terminate with fault code.
  {
    ASICserialFlush();
    fault = 3;
    return resetCaseValue;
  }

  if (checkSum != message[checksumByte])
    //Check that the check sum is correct.
    //If not, terminate with fault code.
  {
    ASICserialFlush();
    fault = 4;
    return resetCaseValue;
  }

  unsigned int calAddress = (message[1] << 8) + message[2];
  if (calAddress != rxAddress)
    //Check that the destination address of the message received matches the node address of the ePAD HMI.
    //If not, terminate with a fault code.
  {
    ASICserialFlush();
    fault = 5;
    return resetCaseValue;
  }

  calAddress = (message[HItxNode] << 8) + message[LOtxNode];
  if (calAddress != txAddress1 && calAddress != txAddress2)
    //Check that the origin address of the message received matches the node address of either ASIC/2 controller.
    //If not, terminate with a fault code.
  {
    ASICserialFlush();
    fault = 6;
    return resetCaseValue;
  }


  if (message[objectByte] != 0x05 && message[objectByte] != 0x21)
    //Check that the object byte of the message received is the correct type 
    //(i.e. 0x05 for an input or 0x21 for a monitor object).
    //If not, terminate with a fault code.
  {
    ASICserialFlush();
    fault = 7;
    return resetCaseValue;
  }

  if (message[acknowledgeByte] != 0x02)
    //Check that the ASCI controller has found a valid handle and has returned data.
    //If not, terminate with fault code.
  {
    ASICserialFlush();
    fault = 9;
    return resetCaseValue;
  }

  //If the code reaches this point, these's a good chance that this is a valid message.

  ASICserialFlush();
  fault = 0;
  delay(1);
  return incrementCaseValue;
}


int DISPLAY1(int indexNOW)//shows raw data stream.  Not used.
{
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.

  if (message[objectByte] == 0x05)
  {
    Serial.println(" ");
    Serial.print("input: ");
    Serial.print(message[instanceByte],HEX);
    int temperature = (message[HIdataByte] << 8) + message[LOdataByte];
    if (temperature == byte(0x0000))
    {
      Serial.println(" digital: OFF ");
    }
    else if (temperature == 0xFFFF)
    {
      Serial.println(" digital: ON ");
    }
    else
    {
      Serial.print(" data: ");
      Serial.print(temperature / 100);
      Serial.print(".");
      Serial.print(temperature % 100);
      Serial.println(" degrees ");
    }
  }
  else if (message[objectByte] == 0x21)
  {
    Serial.println(" ");
    Serial.println(" monitor block ");
  }
  return incrementCaseValue;
}

int STORE (int indexNOW, int TX1, int TX2)
//Uses the instance value to determine which AC temperature message relates to and updates correct temperature value.
{
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.
  //int incrementCaseValue = 5;   //move on to the next sequence step.

  if (message[objectByte] == 0x05)
  {
    int address = (message[HItxNode] << 8) + message[LOtxNode];

    if (address == TX1)
    {
      switch (message[instanceByte])
      {
      case 0:
        AC[1] = (message[HIdataByte] << 8) + message[LOdataByte];
        break;

      case 3:
        AC[2] = (message[HIdataByte] << 8) + message[LOdataByte];
        break;

      case 6:
        AC[3] = (message[HIdataByte] << 8) + message[LOdataByte];
        break;

      case 9:
        AC[4] = (message[HIdataByte] << 8) + message[LOdataByte];
      }
    }
    else if (address == TX2)
    {
      switch (message[instanceByte])
      {
      case 0:
        AC[6] = (message[HIdataByte] << 8) + message[LOdataByte];
        break;

      case 3:
        AC[0] = (message[HIdataByte] << 8) + message[LOdataByte];
        break;
      }
    }
  } 
  return incrementCaseValue;
}







void serialFlush()
//Procedure to clear all characters from the hardware serial buffer.
{
  while (Serial.available() > 0)
  {
    char t = Serial.read();
  }
}


void ASICserialFlush()
//Procedure to clear all characters from the Software Serial buffer.
{
  while (ASICserial.available() > 0)
  {
    char t = ASICserial.read();
  }
}


//int calcInteger(int HIbyte, int LObyte)
//{
//      int integer = message[HIbyte] << 8;
//      integer = integer + message[LObyte];
//      return integer
//}




