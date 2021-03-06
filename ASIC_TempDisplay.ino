/* Communication application for ASI Controls system bus.
 *
 * This sketch writes a request for analog input readings to the bus
 * and then identifies and decodes the response in order to display
 * temperature readings for ACs 1,2,3,4 and 6.
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

int TXpin = 8;
int RXpin = 9;


//pins reserved for NovaVGA SPI:
//10 = CSN
//11 = MOSI
//13 = SCK

#include "NovaVGAFast.h"
/* -----------------------------------------------------------------------------------
 * library for "NovaVGA" shield by Micro-Nova
 * https://www.micro-nova.com/novavga
 */


//int first = 23;//left most pixel in display area
//int last = 159;//right most pixel in display area
//int maximum = 5;//top most pixel in display area
//int minimum = 101;//bottom most pixel in dsplay area




/*
#include <LiquidCrystal595.h>
/* -----------------------------------------------------------------------------------
 * $Author: Rowan Simms robaby@gmail.com $
 * $Date: 2012-04-08 23:54:07 +0100 (Sun, 08 Apr 2012) $
 *
 * Adaption of the LiquidCrystal library shipped with Arduino 22,
 * now updated for Arduino 1.0.
 * Code originally developed by Steve Hobley - February 2011
 *      updates and maintenance now by haiphamngoc
 * https://github.com/haiphamngoc/LiquidCrystal595
 *
 *

#define DATA_PIN 14 //A0 pin
#define LATCH_PIN 15 //A1 pin
#define CLOCK_PIN 16 //A2 pin

#define LINES 2
#define CHARS 24

// initialize the library with the numbers of the interface pins
LiquidCrystal595 lcd(DATA_PIN,LATCH_PIN,CLOCK_PIN);

*/

int writeState = 0;
/*
 *  0 = Start write cycle.
 *  1 = Wait.
 *  2 = Increment target value.
 *  3 = build message query and calculate checksum
 *  4 = wait for space on bus
 *  5 = send query
 *  9 = read sequence active.
 */

int readState  = 9; //used to step through the read sequence.
/*  0 = Start read cycle complete. Reset message array.
 *  1 = Waiting for data to appear in buffer
 *  2 = Prepare to receive data into array.
 *  3 = Receive data and place in message array
 *  4 = Extract and store temperature values
 *  5 = Display data.  Reset to first step.
 *  9 = Write sequence active
 */

int fault = 0;// fault codes set by RECEIVE function.
/*
 * 0 = no fault
 * 1 = timeout
 * 2 = no start char
 * 3 = message to long
 * 4 = not to expected rx node (this one)
 * 5 = not from expected tx node (ASIC/2)
 * 6 = wrong check sum
 * 7 = wrong object type
 * 8 = wrong instance (not used)
 * 9 = invalid handle - address not found in controller
 */

//software serial is default 8N1.  The ASIC system bus is 8N2.
//should be able to read 8N2 ??
//This is an attempt to send 8N2 by adding tiny delay.

//1000000us / second.
//1 bit at 9600 baud = 1000000/9600 = 104us.
//try adding delayMicroseconds(104) between bytes???

int bitLength = 104;

unsigned long messageInt = 30;
//Minumum interval between messages on the ASIC system bus is 25ms.
//If there is no activity on the bus after this interval, it is safe to transmit.

//unsigned int node_ePAD = 65266;//Holds the node address of the ePAD HMI device = 65266.
unsigned int node_7040 = 32101;//Holds the node address of the ASIC/2 7040 PLC = 32101.(7D65)
unsigned int node_8540 = 32102;//Holds the node address of the ASIC/2 8540 PLC = 32102.(7D66)

unsigned int node_this = 45000;//This node address (AFC8)

byte message[18];            // An array to hold the response message.
int messageSize = sizeof(message);

byte maxIndex = 20;          //Normal ASIC reply message length is 18 bytes.  Back stop to end loop.
byte index = 0;              //Increments as each reply byte is read into the message array.
byte checkSum = 0;           //Holds the check sum value as it is calculated.

//Some Specific byte designations in incoming ASIC message (some used, some not).
//int HIrxNode = 1;
//int LOrxNode = 2;
int HItxNode = 3;
int LOtxNode = 4;
int objectByte = 8;
int instanceByte = 9;
//int attributByte = 10;
//int selectByte = 11;
int acknowledgeByte = 13;     //0x02 if valid, 0x00 if not valid.
int LOdataByte = 15;
int HIdataByte = 16;
int checksumByte = 17;

#include <SoftwareSerial.h>

SoftwareSerial ASICserial(TXpin, RXpin);

int const ACsize = 8;
int ACtemp[ACsize];
// An array to hold the AC temperature values.
// Note that only index 1,2,3,4 and 6 are used generally.
// Index 0 will store the compressor room temperature.
// Index 7 will store the factory temperature.
// IN THIS CASE, only 2,3,4 and 7 are used!!!!
byte ACinst[] = {
  3, 0, 3, 6, 9, 0, 0, 6
};  //instance number for the temperature data of each AC
int ACaddr[] = {
  node_8540, node_7040, node_7040, node_7040, node_7040, 0, node_8540, node_8540
};  //node address of each AC
//position on LCD to display temperature value for each AC 2,3 and 4.
//plus the factory temperature
//int ACrow[] = {0,0,0,0,0,0,0,1};
//int ACcol[] = {0,0,2,10,18,0,0,12};
int average = 0; //holds average of AC2,3and 4 temperatures.

unsigned long updateFresh = 100; //update a value 10 times per second until all value are filled.
unsigned long updateInterval = 15000;  //update a value once per 15 seconds there after.
unsigned long elapsedTime = 0;

int ACindex = 0;
int emptyValue = 0;


void setup()
{
  NovaVGA.init();
  NovaVGA.fillScreen(NovaVGA.Black);
  displayText();
  displayYscale();

  //displayBars();

  for (int x = 0; x < messageSize; x++)  //Clear the message array.
  {
    message[x] = 0;
  }

  for (int x = 0; x < ACsize; x++)
    //Clear the AC array.
  {
    ACtemp[x] = 0;
  }

  // Open serial communications on hardware serial port
  //Serial.begin(57600);
  //serialFlush();// probably not necessary...

  ASICserial.begin(9600);
  ASICserialFlush();// probably not necessary...
}

void loop()
{
  emptyValue = 0;
  for (int x = 0; x < ACsize; x++)
  {
    if (x != 0 && x != 1 && x != 5 & x != 6)
    {
      if (ACtemp[x] == 0)
      {
        emptyValue = 1;
      }
    }
  }

  switch (writeState)
  {
    case 0:
      //Cycle complete or returned from receive procedure in fault mode.
      //Reset message array
      writeState = writeState + 1;
      elapsedTime = millis();
      for (int x = 0; x < messageSize; x++) //Clear the message array.
      {
        message[x] = 0;
      }
      delay(1);
      break;

    case 1:
      if (millis() < elapsedTime)
      {
        elapsedTime = millis();  //deal with millis overflow.
      }
      else if ((emptyValue == 1 && millis() - elapsedTime > updateFresh) || (emptyValue == 0 && millis() - elapsedTime > updateInterval))
      {
        writeState = writeState + 1;
      }
      break;

    case 2:
      //Increment the target value
      // Note that only index 2,3,4 and 7 are used.
      // Index 7 stores the factory temperature.
      writeState = writeState + 1;
      if (fault == 0)
      {
        if (ACindex != 4 && ACindex != 7)
        {
          ACindex = ACindex + 1;
        }
        else if (ACindex == 4)
        {
          ACindex = 7;
        }
        else
        {
          ACindex = 2;
        }
      }
      break;

    case 3:
      //build message query and calculate checksum
      writeState = BUILD_QUERY(writeState, ACindex);
      break;

    case 4:
      //wait for space on bus
      elapsedTime = millis();
      while (ASICserial.available() || millis() - elapsedTime < messageInt)
      {
        if (ASICserial.available())
        {
          elapsedTime = millis();
          ASICserialFlush();
        }
        ASICserialFlush();
      }
      writeState = writeState + 1;
      break;

    case 5:
      //send query
      for (int x = 0; x < 15; x++)
      {
        ASICserial.write(message[x]);
        delayMicroseconds(bitLength);
      }
      writeState = 9;
      readState = 0;
      elapsedTime = millis();
      break;

    case 9:
      break;

    default:
      break;

  }

  switch (readState)
  {
    case 0:
      //Start read cycle.
      //Reset message array
      readState = 1;
      for (int x = 0; x < messageSize; x++)      //Clear the message array.
      {
        message[x] = 0;
      }
      delay(1);
      break;

    case 1:
      //Waiting for data to appear in buffer
      if (ASICserial.available() > 0)
      {
        readState = 2;
      }
      else if (millis() - elapsedTime > updateInterval)
      {
        readState = 9;
        writeState = 0;
        fault = 1;
      }
      break;

    case 2:
      //prepare to receive data into array.
      checkSum = 0; // reset for calculating the incoming check sum
      fault = 0;  //reset fault value for incomming message
      index = 0;
      readState = 3;
      delay(1);  //critical!!!
      break;

    case 3:
      //receive data and place in message array
      readState = RECEIVE(readState, node_this, node_7040, node_8540);
      if (readState == 9)
      {
        writeState = 0;
      }
      delay(1);  //critical!!!
      break;

    case 4: //Extract and srore temperature values
      readState = STORE(readState, node_7040, node_8540);
      break;

    case 5:    //Display data on the LCD.  Return to first step.
      //LCDupdate(ACindex);
      AveUpdate();
      displayBars();
//      if (emptyValue == 1)
//      {
//      NovaVGA.writePixel(ACindex,0,NovaVGA.White);
//      }
      readState = 9;
      writeState = 0;
      break;

    case 9:
      break;

    default:
      break;
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
  int resetCaseValue = 9;       //reset the master sequence.
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.

  while (ASICserial.available() <= 0)
  {
    // do nothing until there is data in the software serial buffer.
  }

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
  checkSum = 0;
  while (index < messageSize - 1)
  {
    delay(1);//critical!

    message[index] = ASICserial.read();  //Add each byte to the array.

    checkSum = checkSum + message[index];
    //The check sum of an ASIC message is the least significant byte of a simple addition of all the message bytes.
    //Since "checkSum" is type byte, this calculation will automatically discard the upper bytes.

    index = index + 1;  //Increment the index
  }

  message[index] = ASICserial.read();//get the last byte from the buffer.  This will be the checksum if this is a valid reply message.

  int x = 0;

  //**** error checking  ****

  if (index >= maxIndex)
    //Check that the size of the message is not greater than expected.
    //Terminate with fault code.
  {
    ASICserialFlush();
    fault = 3;
    return resetCaseValue;
  }

  unsigned int calAddress = (message[1] << 8) + message[2];
  if (calAddress != rxAddress)
    //Check that the destination address of the message received matches the node address of the ePAD HMI.
    //If not, terminate with a fault code.
  {
    ASICserialFlush();
    fault = 4;
    return resetCaseValue;
  }

  calAddress = (message[HItxNode] << 8) + message[LOtxNode];
  if (calAddress != txAddress1 && calAddress != txAddress2)
    //Check that the origin address of the message received matches the node address of either ASIC/2 controller.
    //If not, terminate with a fault code.
  {
    ASICserialFlush();
    fault = 5;
    return resetCaseValue;
  }

  if (checkSum != message[checksumByte])
    //Check that the check sum is correct.
    //If not, terminate with fault code.
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

int STORE (int indexNOW, int TX1, int TX2)
//Uses the instance value to determine which AC temperature message relates to and updates correct temperature value.
{
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.

  if (message[objectByte] == 0x05)
  {
    int address = (message[HItxNode] << 8) + message[LOtxNode];
    /*
     *AC1     = index 1, TX1, instance 0;
     *AC2     = index 2, TX1, instance 3;
     *AC3     = index 3, TX1, instance 6;
     *AC4     = index 4, TX1, instance 9;
     *AC6     = index 6, TX2, instance 0;
     *comp rm = index 0, TX2, instance 3;
     *factory = index 7, TX2, instance 6;
     */

    if (address == TX1)
    {
      switch (message[instanceByte])
      {
        case 0:
          ACtemp[1] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

        case 3:
          ACtemp[2] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

        case 6:
          ACtemp[3] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

        case 9:
          ACtemp[4] = (message[HIdataByte] << 8) + message[LOdataByte];
      }
    }
    else if (address == TX2)
    {
      switch (message[instanceByte])
      {
        case 0:
          ACtemp[6] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

        case 3:
          ACtemp[0] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

        case 6:
          ACtemp[7] = (message[HIdataByte] << 8) + message[LOdataByte];
          break;

      }
    }
  }
  return incrementCaseValue;
}


int BUILD_QUERY (int indexNOW, byte AC)
{
  int incrementCaseValue = indexNOW + 1;   //move on to the next sequence step.

  //**************************** Build the query message  **************************

  //Most of these values are static and are lifted directly from the message structure
  //obtained by sniffing serial packets.  Most of the function codes are not understood.

  message[0] = 0x02;             //byte 0  = Start of message.  Always 0x02
  message[1] = (ACaddr[AC] >> 8); //byte 1  = receiver node address byte 1
  message[2] = (ACaddr[AC]);     //byte 2  = receiver node address byte 2
  message[3] = (node_this >> 8); //byte 3  = transmitter node address byte 1
  message[4] = (node_this);      //byte 4  = transmitter node address byte 2
  message[5] = 0x91;             //byte 5  = function code. Always 0x91
  message[6] = 0x77;             //byte 6  = function code. Always 0x77
  message[7] = 0x02;             //byte 7  = handle start. Always 0x02
  message[8] = 0x05;             //byte 8  = input object. Always 0x05 for an input.  Change as required for other types.
  message[9] = byte (ACinst[AC]); //byte 9  = instance. Note that parsing "0x00" to byte is probably not necessary, but removes abiguity with NULL character.
  message[10] = byte(0x00);      //byte 10 = attribute. Always 0x00 for an input.  Note that parsing "0x00" to byte removes abiguity with NULL character.
  message[11] = 0x01;            //byte 11 = select. Always 0x01 for an input.
  message[12] = 0x01;            //byte 12 = unknown. Always 0x01
  message[13] = 0x02;            //byte 13 = handle end. Usually 0x02. Response returns 0x00 if handle not valid.

  checkSum = 0;
  for (int x = 0; x < 14; x++)
    //message size always 14 bytes
    //calculate the check sum...
  {
    checkSum = checkSum + message[x];
  }

  message[14] = checkSum;             //byte 14 = Checksum.
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



void AveUpdate()
{
  int numRecords = 0;
  average = ACtemp[2] + ACtemp[3] + ACtemp[4];
  if (ACtemp[2] > 0)
  {
    numRecords++;
  }
  if (ACtemp[3] > 0)
  {
    numRecords++;
  }
  if (ACtemp[4] > 0)
  {
    numRecords++;
  }

  average = average / numRecords;

}



void displayBars()
{
  int factBar = ACtemp[7];
  //factBar = constrain(factBar, 800, 4100);
  factBar = map(factBar, 800, 4100, 1, 100);
  factBar = constrain(factBar, 1, 110);
  
  int adminBar = average;
  //adminBar = constrain(adminBar, 800, 4100); 
  adminBar = map(adminBar, 800, 4100, 1, 100);
  adminBar = constrain(adminBar, 1, 110);

  int factBarCol = NovaVGA.Green;
  int adminBarCol = NovaVGA.Green;

  if (average < 2000)
  {
    adminBarCol = NovaVGA.Blue;
  }
  else if  (average > 2400)
  {
    adminBarCol = NovaVGA.Red;
  }

  if (average >= 2000 && average <= 2400)
  {
    if (ACtemp[7] < average - 200)
    {
      factBarCol = NovaVGA.Blue;
    }
    else if  (ACtemp[7] > average + 200)
    {
      factBarCol = NovaVGA.Red;
    }
  }
  else
  {
    if (ACtemp[7] < 2000)
    {
      factBarCol = NovaVGA.Blue;
    }
    else if  (ACtemp[7] > 2400)
    {
      factBarCol = NovaVGA.Red;
    }
  }



  //           x y  w  h

  if (ACindex == 7 && emptyValue == 0)
      {
      NovaVGA.fillScreen(NovaVGA.Black);
      displayText();
      displayYscale();
      }
      else
      {
      NovaVGA.fillRect(23,0,159,106, NovaVGA.Black);
      }

  NovaVGA.fillRect(34, 5 + 100 - factBar, 50, factBar, factBarCol);
  NovaVGA.fillRect(99, 5 + 100 - adminBar, 50, adminBar, adminBarCol);

}

void displayText()
{
  //IMPORTANT:  Relies on modified "font.h" file.
  //Ensure modified file is at \Arduino\libraries\NovaVGAFast\src\include

  //NovaVGA.drawChar(ascii address, x, y, colour);
  //write: 40 degrees
  NovaVGA.drawChar(0x14, 3, 5, NovaVGA.White);
  NovaVGA.drawChar(0x10, 9, 5, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 3, NovaVGA.White);

  //write: 35 degrees
  NovaVGA.drawChar(0x13, 3, 20, NovaVGA.White);
  NovaVGA.drawChar(0x15, 9, 20, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 18, NovaVGA.White);

  //write: 30 degrees
  NovaVGA.drawChar(0x13, 3, 35, NovaVGA.White);
  NovaVGA.drawChar(0x10, 9, 35, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 33, NovaVGA.White);

  //write: 25 degrees
  NovaVGA.drawChar(0x12, 3, 50, NovaVGA.White);
  NovaVGA.drawChar(0x15, 9, 50, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 48, NovaVGA.White);

  //write: 20 degrees
  NovaVGA.drawChar(0x12, 3, 65, NovaVGA.White);
  NovaVGA.drawChar(0x10, 9, 65, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 63, NovaVGA.White);

  //write: 15 degrees
  NovaVGA.drawChar(0x11, 5, 80, NovaVGA.White);
  NovaVGA.drawChar(0x15, 9, 80, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 78, NovaVGA.White);

  //write: 10 degrees
  NovaVGA.drawChar(0x11, 5, 95, NovaVGA.White);
  NovaVGA.drawChar(0x10, 9, 95, NovaVGA.White);
  NovaVGA.drawChar(0x1A, 15, 93, NovaVGA.White);

  String lable_1 = "FACTORY";
  String lable_2 = "ADMIN";

  NovaVGA.drawString(lable_1, 32, 109, NovaVGA.White);
  NovaVGA.drawString(lable_2, 105, 109, NovaVGA.White);

}

void displayYscale()
{
  int startY = 5;
  int firstYlong = 8;
  int longTickInterval = 15;
  int endY = 105;
  int startX = 21;
  //int tickCount = 0;
  for (int y = startY; y < endY + 1; y = y + 3)
  {
    int leng = startX - 2;
    if ( (y - firstYlong) % longTickInterval == 0)
    {
      leng = leng - 2;
    }

    for (int x = startX; x > leng; x--)
    {
      NovaVGA.writePixel(x, y, NovaVGA.White);
    }
  }
}

void displayXscale()
{
  int startX = 23;
  int XtickInterval = 5;
  int midTickInterval = XtickInterval * 6;
  int longTickInterval = XtickInterval * 12;
  int endX = 159;
  int startY = 103;
  //int tickCount = 0;
  for (int x = startX; x < endX + 1; x = x + 5)
  {
    int leng = startY + 2;
    if ( (x - startX) % midTickInterval == 0)
    {
      leng = leng + 1;
    }
    if ( (x - startX) % longTickInterval == 0)
    {
      leng = leng + 1;
    }

    for (int y = startY; y < leng; y++)
    {
      NovaVGA.writePixel(x, y, NovaVGA.White);
    }
  }
}







