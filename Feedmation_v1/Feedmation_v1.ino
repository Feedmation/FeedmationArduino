#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>
#include <String.h>
#include <Wire.h>
#include <DS1307.h>


/**********************************************************************************************************************
*                                                   Pin Settings
***********************************************************************************************************************/

// define constants for pins
int systemOnLED = 8;
int speaker = 9;
int tankLED = 3;
int motorPin1 = 4;    // Blue   - 28BYJ48 pin 1
int motorPin2 = 5;    // Pink   - 28BYJ48 pin 2
int motorPin3 = 6;    // Yellow - 28BYJ48 pin 3
int motorPin4 = 7;    // Orange - 28BYJ48 pin 4


/**********************************************************************************************************************
*                                                   Global Variables
***********************************************************************************************************************/

// variables to keep rfid state
int readVal = 0; // individual character read from serial
unsigned int readData[10]; // data read from serial
int rfidCounter = -1; // counter to keep position in the buffer
char tagId[11]; // final tag ID converted to a string

//declare variables
int motorSpeed = 4800;  //variable to set stepper speed
int cup = 208; // number of steps for one cup of food
int lookup[8] = {B01000, B01100, B00100, B00110, B00010, B00011, B00001, B01001};

//photocell
int LDRReading;

//real time clock
int RTCValues[7];
long secSinceMidnight = 0;

/**********************************************************************************************************************
*                                                  Animal Settings
***********************************************************************************************************************/

long lockoutTime[3] = {0,0,0};

//animal settings struct
struct animalSettings{
   char* tag;
   char* name;
   int feedAttempts;
   float amount;
   long slot1Start;
   long slot1End;
   long slot2Start;
   long slot2End;
   int slot1Eaten;
   int slot2Eaten;
   //long lockoutTime;
};

//array of animal setting struct for each tag\animal
animalSettings animal[3];

void initAnimalSettings() {
  //declaring animal settings
  //animal one 
  animal[0].tag = "84003515CA";
  animal[0].name = "RoverRover";
  animal[0].feedAttempts = 0;
  animal[0].amount = 0; //amount of food in cups
  animal[0].slot1Start = 0;
  animal[0].slot1End = 0;
  animal[0].slot2Start = 0;
  animal[0].slot2End = 0;
  animal[0].slot1Eaten = 0;
  animal[0].slot2Eaten = 0;
  //animal[0].lockoutTime = 0;
  //animal two 
  animal[1].tag = "8400355406";
  animal[1].name = "RexRexRex";
  animal[1].feedAttempts = 0;
  animal[1].amount = 0; //amount of food in cups
  animal[1].slot1Start = 0;
  animal[1].slot1End = 0;
  animal[1].slot2Start = 0;
  animal[1].slot2End = 0;
  animal[1].slot1Eaten = 0;
  animal[1].slot2Eaten = 0;
 // animal[1].lockoutTime = 0;
  //animal three
  animal[2].tag = "8400351592";
  animal[2].name = "MaxMaxMax";
  animal[2].feedAttempts = 0;
  animal[2].amount = 0; //amount of food in cups
  animal[2].slot1Start = 0;
  animal[2].slot1End = 0;
  animal[2].slot2Start = 0;
  animal[2].slot2End = 0;
  animal[2].slot1Eaten = 0;
  animal[2].slot2Eaten = 0;
 // animal[2].lockoutTime = 0;
}

/**********************************************************************************************************************
*                                                      Speaker Code
***********************************************************************************************************************/
void beep() {
  
  for (int i=0; i<500; i++) {  // generate a 1KHz tone for 1/2 second
  digitalWrite(speaker, HIGH);
  delayMicroseconds(500);
  digitalWrite(speaker, LOW);
  delayMicroseconds(500);
  }
  digitalWrite(speaker, LOW);
    
}


/**********************************************************************************************************************
*                                                      Motor Code
***********************************************************************************************************************/
void anticlockwise()
{
  for(int i = 0; i < 8; i++)
  {
    setOutput(i);
    delayMicroseconds(motorSpeed);
  }
}

void clockwise()
{
  for(int i = 7; i >= 0; i--)
  {
    setOutput(i);
    delayMicroseconds(motorSpeed);
  }
}

void setOutput(int out)
{
  digitalWrite(motorPin1, bitRead(lookup[out], 0));
  digitalWrite(motorPin2, bitRead(lookup[out], 1));
  digitalWrite(motorPin3, bitRead(lookup[out], 2));
  digitalWrite(motorPin4, bitRead(lookup[out], 3));
}



/**********************************************************************************************************************
*                                                         Setup
***********************************************************************************************************************/

void setup() {
  //declare the motor pins as outputs
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(motorPin3, OUTPUT);
  pinMode(motorPin4, OUTPUT);  
  Serial.begin(9600);

  //pet feeder setup
  pinMode(systemOnLED, OUTPUT);
  digitalWrite(systemOnLED, HIGH);
  initAnimalSettings();
  
  //LED light for the tank
  pinMode(tankLED, OUTPUT);
  digitalWrite(tankLED, HIGH);
  
  pinMode(speaker, OUTPUT);
 
  //real time clock setup
  DS1307.begin();

}

/**********************************************************************************************************************
*                                             Tag Parsing and Animal Feeding
***********************************************************************************************************************/

// convert the int values read from serial to ASCII chars
void parseTag() {
  int i;
  for (i = 0; i < 10; ++i) {
    tagId[i] = readData[i];
  }
  tagId[10] = 0;
}

// once a whole tag is read, process it
void processFeedingRequest() {
	// convert id to a string
  parseTag();

	// print tag id
  printTag();
  
  //get time in seconds
  secSinceMidnight = ((((long)(60)) * (RTCValues[4]) + (long)(RTCValues[5])) * (long)(60)) + ((long)(RTCValues[6]));
  
  Serial.println(LDRReading);
  if(LDRReading <= 200)
  {
     //loop and find animals settings and process animals request for food
    for (int i = 0; i < 3; ++i) 
    {
      int deniedFeeding = 1; 
      // if pet has eaten within five minutes then don't process tag read
      if ( secSinceMidnight > lockoutTime[i] ) {
        //if tag parsed matches animal and they havent eatten yet, then feed.
        if ( (strcmp(animal[i].tag, tagId) == 0) && ((((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) && (animal[i].slot1Eaten == 0)) || (((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) && (animal[i].slot2Eaten == 0))))
        {
          
          //Dispence animals food allotment
          int amount =  cup * animal[i].amount;
          //print steps 
          Serial.print(amount);
          Serial.println(F(" steps"));
          for(int j = 0; j <= amount; j++)
          {
            anticlockwise();
          }
          //turn off all motor pins when food is dispenced
          digitalWrite(motorPin1, LOW);
          digitalWrite(motorPin2, LOW);
          digitalWrite(motorPin3, LOW);
          digitalWrite(motorPin4, LOW);
          Serial.print(animal[i].name);
          Serial.println(F(" was fed!"));
          animal[i].feedAttempts++;
          deniedFeeding = 0;
          lockoutTime[i] = secSinceMidnight + (long)(60);
          delay(1000);
      
        }

        if ( (strcmp(animal[i].tag, tagId) == 0) && deniedFeeding == 1 ) { 
          beep();
          animal[i].feedAttempts++;
          Serial.print(animal[i].name);
          Serial.println(F(" already ate!"));
        } 
    
        //if pet ate then mark eaten variable for that time slot
        if ( (strcmp(animal[i].tag, tagId) == 0) && ((((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) && (animal[i].slot1Eaten == 0)) || (((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) && (animal[i].slot2Eaten == 0))))
        {
          if ((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) {
            animal[i].slot1Eaten = 1;
          }
          if ((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) {
            animal[i].slot2Eaten = 1;
          }
      
        }
      }
     }
   } else {
     beep();
     Serial.println(F("Out of Food!"));
   }
}

void resetSlots() {

  //get time in seconds
  secSinceMidnight = ((((long)(60)) * (RTCValues[4]) + (long)(RTCValues[5])) * (long)(60)) + ((long)(RTCValues[6]));

  //reset slots for each pet if time is greater then 11pm 
  
  if ( secSinceMidnight > 82800 ) {
    for (int i = 0; i < 3; ++i) 
    {
      animal[i].slot1Eaten = 0;
      animal[i].slot2Eaten = 0;
      animal[i].feedAttempts = 0;
    }
  }
  
}

void printTag() {
  Serial.print(F("Tag value: "));
  Serial.println(tagId);
}

// this function clears the rest of data on the serial, to prevent multiple scans
void clearSerial() {
  while (Serial.read() >= 0) {
		; // do nothing
	}
}



/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/

void loop() { 
  
  //get current time
  DS1307.getDate(RTCValues);
  
    //Looking for a tag
  if (Serial.available() > 0) {
      // read the incoming byte:
      readVal = Serial.read();
    
      // a "2" signals the beginning of a tag
      if (readVal == 2) {
        rfidCounter = 0; // start reading
    } 
    // a "3" signals the end of a tag
    else if (readVal == 3) {
        // process the tag we just read
        processFeedingRequest();
        // clear serial to prevent multiple reads
        clearSerial();
        // reset reading state
        rfidCounter = -1;
    }
    // if we are in the middle of reading a tag
    else if (rfidCounter >= 0) {
        // save valuee
        readData[rfidCounter] = readVal;
        // increment counter
        ++rfidCounter;
    } 
  }
  
   //reset time slot variables after time slot passes
   //resetSlots();
   
   //check food tank level
   LDRReading = analogRead(A1); 


}

