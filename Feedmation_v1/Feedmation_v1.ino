#include <MemoryFree.h>
#include <Bridge.h>
#include <Wire.h>
#include <DS1307.h>
#include <FileIO.h>
#include <SoftwareSerial.h>


/**********************************************************************************************************************
*                                                   Pin Settings
***********************************************************************************************************************/

// define constants for pins
SoftwareSerial RFID(2, 3); // RX and TX
int speaker = 9;
int tankLED = 8;
int motorPin1 = 4;    // Blue   - 28BYJ48 pin 1
int motorPin2 = 5;    // Pink   - 28BYJ48 pin 2
int motorPin3 = 6;    // Yellow - 28BYJ48 pin 3
int motorPin4 = 7;    // Orange - 28BYJ48 pin 4
int rfidCount;

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

//settings folder path
char* tagSettingsPath[] = {"/feedmation/settings/1/","/feedmation/settings/2/","/feedmation/settings/3/","/feedmation/settings/4/"};

//parse tag settings timer
unsigned long settingsLastCheckTime = 0;            // last time checked for settings update, in milliseconds
const unsigned long settingsPostingInterval = 7L * 1000L; // delay between updates, in milliseconds
boolean systemBoot = true;

/**********************************************************************************************************************
*                                                  Animal Settings
***********************************************************************************************************************/

//animal settings struct
struct animalSettings{
   char* tag;
   float amount;
   long slot1Start;
   long slot1End;
   long slot2Start;
   long slot2End;
   int slot1Eaten;
   int slot2Eaten;
   long lockoutTime;
};

//array of animal setting struct for each tag\animal
animalSettings animal[3];

void initAnimalSettings() {
  //declaring animal settings
  //animal one 
  animal[0].tag = "0000000000";
  animal[0].amount = 0; //amount of food in cups
  animal[0].slot1Start = 0;
  animal[0].slot1End = 0;
  animal[0].slot2Start = 0;
  animal[0].slot2End = 0;
  animal[0].slot1Eaten = 0;
  animal[0].slot2Eaten = 0;
  animal[0].lockoutTime = 0;
  //animal two 
  animal[1].tag = "0000000000";
  animal[1].amount = 0; //amount of food in cups
  animal[1].slot1Start = 0;
  animal[1].slot1End = 0;
  animal[1].slot2Start = 0;
  animal[1].slot2End = 0;
  animal[1].slot1Eaten = 0;
  animal[1].slot2Eaten = 0;
  animal[1].lockoutTime = 0;
  //animal three
  animal[2].tag = "0000000000";
  animal[2].amount = 0; //amount of food in cups
  animal[2].slot1Start = 0;
  animal[2].slot1End = 0;
  animal[2].slot2Start = 0;
  animal[2].slot2End = 0;
  animal[2].slot1Eaten = 0;
  animal[2].slot2Eaten = 0;
  animal[2].lockoutTime = 0;
 //animal four
  animal[3].tag = "0000000000";
  animal[3].amount = 0; //amount of food in cups
  animal[3].slot1Start = 0;
  animal[3].slot1End = 0;
  animal[3].slot2Start = 0;
  animal[3].slot2End = 0;
  animal[3].slot1Eaten = 0;
  animal[3].slot2Eaten = 0;
  animal[3].lockoutTime = 0;
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
  
  // serial setup for brigde, RFID and Serial monitor
  Bridge.begin();
  RFID.begin(9600);
  Serial.begin(9600);

  //pet feeder setup
  initAnimalSettings();
  pinMode(tankLED, OUTPUT);
  digitalWrite(tankLED, HIGH);
  pinMode(speaker, OUTPUT);
 
  //real time clock setup
  DS1307.begin();
  
   // Setup File IO
  FileSystem.begin();
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
    for (int i = 0; i = 3; ++i) 
    {
      int deniedFeeding = 1; 
      // if pet has eaten within five minutes then don't process tag read
      if ( secSinceMidnight > animal[i].lockoutTime ) {
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
          Serial.print(animal[i].tag);
          Serial.println(F(" was fed!"));
          //animal[i].feedAttempts++;
          deniedFeeding = 0;
          animal[i].lockoutTime = secSinceMidnight + (long)(60);
          delay(1000);
      
        }

        if ( (strcmp(animal[i].tag, tagId) == 0) && deniedFeeding == 1 ) { 
          beep();
          //animal[i].feedAttempts++;
          Serial.print(animal[i].tag);
          Serial.println(F("already ate!"));
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
    for (int i = 0; i + 3; ++i) 
    {
      animal[i].slot1Eaten = 0;
      animal[i].slot2Eaten = 0;
      animal[i].lockoutTime = 0;
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
*                                                  Python script function
***********************************************************************************************************************/  

void runPython() {
  //Serial.println(F("Ready to run Python script"));
  const String pyAddr = "/feedmation/feedmation2.py";  // The address of the python script on the Linux side of the Yun board
  Process p;
  p.begin("python");
  p.addParameter(pyAddr);
  p.run();
  //Serial.println(F("Done running Python script."));
}



/**********************************************************************************************************************
*                                                  Parse Tag Settings function
***********************************************************************************************************************/  

  void parseTagSettings() {
    
   for (int i = 0; i < 4; i++)
   {
     // if settings path exists for each tag
     if (FileSystem.exists(tagSettingsPath[i])) {
       
 //******* Update Tag Settings ********
 
       char* updatedFile = (char*)malloc(strlen(tagSettingsPath[i])+1+11);
       strcpy(updatedFile, tagSettingsPath[i]);
       strcat(updatedFile, "updated.txt");
       
       //if updated.txt file exists then update the pet struct
       if (FileSystem.exists(updatedFile) || systemBoot == true) {
         String data = String(""); //data storage for files
         
         // **** Tag ID update ****
         char* tagidPath = (char*)malloc(strlen(tagSettingsPath[i])+1+9);
         strcpy(tagidPath, tagSettingsPath[i]);
         strcat(tagidPath, "tagID.txt");
       
         if (FileSystem.exists(tagidPath)) {
           
           File readFile = FileSystem.open(tagidPath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++;
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           strcpy (animal[i].tag, convertdata);
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(tagidPath);

         // **** Amount update ****
         char* amountPath = (char*)malloc(strlen(tagSettingsPath[i])+1+10);
         strcpy(amountPath, tagSettingsPath[i]);
         strcat(amountPath, "amount.txt");
       
         if (FileSystem.exists(amountPath)) {
           
           File readFile = FileSystem.open(amountPath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++;
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           
           animal[i].amount = atof(convertdata);
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(amountPath);
   
         // **** Slot 1 Start update ****
         char* s1sPath = (char*)malloc(strlen(tagSettingsPath[i])+1+14);
         strcpy(s1sPath, tagSettingsPath[i]);
         strcat(s1sPath, "slot1Start.txt");
         
         if (FileSystem.exists(s1sPath)) {
           
           File readFile = FileSystem.open(s1sPath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++; 
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           
           animal[i].slot1Start = (((long)(60)) * ((long)(60)) * atol(convertdata));
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(s1sPath);
   
         // **** Slot 1 End update ****
         char* s1ePath = (char*)malloc(strlen(tagSettingsPath[i])+1+12);
         strcpy(s1ePath, tagSettingsPath[i]);
         strcat(s1ePath, "slot1End.txt");
         
         if (FileSystem.exists(s1ePath)) {
           
           File readFile = FileSystem.open(s1ePath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++; 
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           
           animal[i].slot1End = (((long)(60)) * ((long)(60)) * atol(convertdata));
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(s1ePath);
   
         // **** Slot 2 Start update ****
         char* s2sPath = (char*)malloc(strlen(tagSettingsPath[i])+1+14);
         strcpy(s2sPath, tagSettingsPath[i]);
         strcat(s2sPath, "slot2Start.txt");
         
         if (FileSystem.exists(s2sPath)) {
           
           File readFile = FileSystem.open(s2sPath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++; 
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           
           animal[i].slot2Start = (((long)(60)) * ((long)(60)) * atol(convertdata));
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(s2sPath);
   
         // **** Slot 2 End update ****
         char* s2ePath = (char*)malloc(strlen(tagSettingsPath[i])+1+12);
         strcpy(s2ePath, tagSettingsPath[i]);
         strcat(s2ePath, "slot2End.txt");
         
         if (FileSystem.exists(s2ePath)) {
           
           File readFile = FileSystem.open(s2ePath, FILE_READ);
           
           int size = 0;
           while (readFile.available() > 0) { 
             char c = readFile.read();
             data.concat(String(c));
             size++; 
           }
           
           char convertdata[size+1];
           data.toCharArray(convertdata, size+1);
           
           animal[i].slot2End = (((long)(60)) * ((long)(60)) * atol(convertdata));
           Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           Serial.println();
         }
         free(s2ePath);
         
         //reset eaten slots
         animal[i].slot1Eaten = 0;
         animal[i].slot2Eaten = 0;
         FileSystem.remove(updatedFile); //remove updated.txt now that tag has been updated
         beep(); //beep if update has completed 
       }
       free(updatedFile);
       
 //******* Check for Deletions ********
      
       char* deleteFile = (char*)malloc(strlen(tagSettingsPath[i])+1+11);
       strcpy(deleteFile, tagSettingsPath[i]);
       strcat(deleteFile, "deleted.txt");
       
       //if updated.txt file exists then update the pet struct
       if (FileSystem.exists(deleteFile)) {
         
         animal[i].tag = "0000000000";
         animal[i].amount = 0;
         animal[i].slot1Start = 0;
         animal[i].slot1End = 0;
         animal[i].slot2Start = 0;
         animal[i].slot2End = 0;
         animal[i].slot1Eaten = 0;
         animal[i].slot2Eaten = 0;
         animal[i].lockoutTime = 0;
         
         Serial.print(i+1);
         Serial.print(F(" deleted"));
         Serial.println();;
         
         FileSystem.remove(deleteFile); //remove delete.txt now that tag has been cleared
         beep(); //beep if update has completed
       
       }
       free(deleteFile);
       
     } 
   }
    
    systemBoot = false;
    settingsLastCheckTime = millis();
    
  }


/**********************************************************************************************************************
*                                                  Feed Now request function
***********************************************************************************************************************/  

  void feedNowRequest() {
    
    char feedNowFilePath[] = "/feedmation/feednow/feednow.txt";
    
    // if feed Now file exists then parse and despense food
    if (FileSystem.exists(feedNowFilePath)) {
      
      beep(); //beep if feed now is received
      String data = String(""); //data storage for files
      File readFile = FileSystem.open(feedNowFilePath, FILE_READ);
           
      int size = 0;
      while (readFile.available() > 0) { 
        char c = readFile.read();
        data.concat(String(c));
        size++;
      }
           
      char convertdata[size+1];
      data.toCharArray(convertdata, size+1);
      
      int steps =  cup * atof(convertdata);
      Serial.print(F("Feed Now in steps: "));
      Serial.println(steps);
      for(int j = 0; j <= steps; j++)
      {
        anticlockwise();
      }
      //turn off all motor pins when food is dispenced
      digitalWrite(motorPin1, LOW);
      digitalWrite(motorPin2, LOW);
      digitalWrite(motorPin3, LOW);
      digitalWrite(motorPin4, LOW);
           
      data = String(""); //clear data
      readFile.close();
      Serial.println();
      FileSystem.remove(feedNowFilePath); //remove feednow.txt now that tag has been cleared
      beep(); //beep if feed now has completed
    }
  }
  
  

/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/

void loop() { 

  //get current time
  DS1307.getDate(RTCValues);
  
  if (RFID.available() > 0)
  {
    rfidCount = RFID.read();
    Serial.print(rfidCount, DEC);
    Serial.print(" ");
  }
  
  /*
    //Looking for a tag
  if (RFID.available() > 0) {
      // read the incoming byte:
      readVal = RFID.read();
    
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
  */
  
  
  // if ten seconds have passed since last Tag Settings check and Feed Now check,
  // then check again and get and parse data:
  if (millis() - settingsLastCheckTime > settingsPostingInterval) {
    //runPython();
    parseTagSettings();
    feedNowRequest();
    Serial.print(F("Free Memory = "));
    Serial.println(getFreeMemory());
  }
 
   //reset time slot variables after time slot passes
   //resetSlots();
   
   //check food tank level
   LDRReading = analogRead(A1);

}          
