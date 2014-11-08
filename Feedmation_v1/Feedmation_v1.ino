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
SoftwareSerial RFID(8, 3); // RX and TX
int speaker = 9;
int tankLED = 12;
int motorPin1 = 4;    // Blue pin 1
int motorPin2 = 5;    // Pink pin 2
int motorPin3 = 6;    // Yellow pin 3
int motorPin4 = 7;    // Orange pin 4



/**********************************************************************************************************************
*                                                   Global Variables
***********************************************************************************************************************/

// variables to keep rfid state
int readVal = 0; // individual character read from serial
unsigned int readData[10]; // data read from serial
int rfidCounter = -1; // counter to keep position in the buffer
char tagId[11]; // final tag ID converted to a string

//Scale Variables
//Food bowl scale
int analogValueAOne;
float FoodLoadA = 0; // grams
int FoodAnalogvalA = 80; // load cell reading taken with loadA on the load cell
float FoodLoadB = 140; // grams
int FoodAnalogvalB = 100; // load cell reading taken with loadB on the load cell


//Pet scale

//declare variables
int motorSpeed = 4800;  //variable to set stepper speed
int cup = 208; // number of steps for one cup of food
int lookup[8] = {B01000, B01100, B00100, B00110, B00010, B00011, B00001, B01001};

//food tank status (see function below for pin setting and more details)
boolean tankEmptyLastVal = false;  //false = has food, true = out of food
boolean tankEmpty = false; //false = has food, true = out of food

//real time clock
int RTCValues[7];
long secSinceMidnight = 0;
char dateTime[20];

//folder and file paths
char tankEmptyFile[] = "/feedmation/tank_status/tank_empty.txt";
char tankFullFile[] = "/feedmation/tank_status/tank_full.txt";
//tag settings
char* tagSettingsPath[] = {"/feedmation/settings/1/","/feedmation/settings/2/","/feedmation/settings/3/","/feedmation/settings/4/"};
//Pet log path
char logPath[] = "/feedmation/log_data/";
//feed now file path
char feedNowFilePath[] = "/feedmation/feednow/feednow.txt";

//parse tag settings timer
unsigned long settingsLastCheckTime = 0;            // last time checked for settings update, in milliseconds
const unsigned long settingsPostingInterval = 7L * 1000L; // delay between updates, in milliseconds

//system boot boolean is used by some function.
boolean systemBoot = true;



/**********************************************************************************************************************
*                                                  Animal Settings
***********************************************************************************************************************/

//animal settings struct
struct animalSettings{
   char tag[11];
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
animalSettings animal[4];

long lockoutTime[4] = {0,0,0,0};

void initAnimalSettings() {
  //declaring animal settings
  //animal one 
  strcpy (animal[0].tag, "0000000000");
  animal[0].amount = 0; //amount of food in cups
  animal[0].slot1Start = 0;
  animal[0].slot1End = 0;
  animal[0].slot2Start = 0;
  animal[0].slot2End = 0;
  animal[0].slot1Eaten = 0;
  animal[0].slot2Eaten = 0;
  //animal[0].lockoutTime = 0;
  //animal two 
  strcpy (animal[1].tag, "0000000000");
  animal[1].amount = 0; //amount of food in cups
  animal[1].slot1Start = 0;
  animal[1].slot1End = 0;
  animal[1].slot2Start = 0;
  animal[1].slot2End = 0;
  animal[1].slot1Eaten = 0;
  animal[1].slot2Eaten = 0;
  //animal[1].lockoutTime = 0;
  //animal three
  strcpy (animal[2].tag, "0000000000");
  animal[2].amount = 0; //amount of food in cups
  animal[2].slot1Start = 0;
  animal[2].slot1End = 0;
  animal[2].slot2Start = 0;
  animal[2].slot2End = 0;
  animal[2].slot1Eaten = 0;
  animal[2].slot2Eaten = 0;
  //animal[2].lockoutTime = 0;
 //animal four
  strcpy (animal[3].tag, "0000000000");
  animal[3].amount = 0; //amount of food in cups
  animal[3].slot1Start = 0;
  animal[3].slot1End = 0;
  animal[3].slot2Start = 0;
  animal[3].slot2End = 0;
  animal[3].slot1Eaten = 0;
  animal[3].slot2Eaten = 0;
  //animal[3].lockoutTime = 0;
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
  //printTag();
  
  //get time in seconds
  secSinceMidnight = ((((long)(60)) * (RTCValues[4]) + (long)(RTCValues[5])) * (long)(60)) + ((long)(RTCValues[6]));
  
  //Serial.println(secSinceMidnight);
  
  if(tankEmpty == false)
  {
     //loop and find animals settings and process animals request for food
    for (int i = 0; i < 4; ++i) 
    {
      int deniedFeeding = 1; 
      // if pet has eaten within one minute then don't process tag read
      if ( secSinceMidnight > lockoutTime[i] ) {
        
        //copy tagID for comparison through out this function
        char tagCompare[11];
        strcpy (tagCompare, tagId);
        
        //if tag is matches register pet tags then process feeding
        if ( (strcmp(animal[i].tag, tagCompare) == 0) ) {
        
          //***Data Logging Starting if tag match found***
          //get current time from clock
          DS1307.getDate(RTCValues);
          sprintf(dateTime, "20%02d-%02d-%02d %02d:%02d:%02d", RTCValues[0], RTCValues[1], RTCValues[2], RTCValues[4], RTCValues[5], RTCValues[6]);//print time to char array
          
          String logData =  String("");  //create string for log file
          logData.concat(tagCompare); //add tag id to log data
          logData.concat(",");
          logData.concat(dateTime); //add date time stamp to log data
          logData.concat(",");

          //if tag parsed matches pet tag and they havent eatten yet, then feed.
          if ( (strcmp(animal[i].tag, tagCompare) == 0) && ((((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) && (animal[i].slot1Eaten == 0)) || (((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) && (animal[i].slot2Eaten == 0))))
          {
            //Dispence animals food allotment
            int amount =  cup * animal[i].amount;
            //print steps 
            //Serial.print(amount);
            //Serial.println(F(" steps"));
            for(int j = 0; j <= amount; j++)
            {
              anticlockwise();
            }
            //turn off all motor pins when food is dispenced
            digitalWrite(motorPin1, LOW);
            digitalWrite(motorPin2, LOW);
            digitalWrite(motorPin3, LOW);
            digitalWrite(motorPin4, LOW);
            
            analogValueAOne = analogRead(A0); //get depensed load cell reading from food bowl
            int depensedWeight = int(analogToLoad(analogValueAOne, FoodAnalogvalA, FoodAnalogvalB, FoodLoadA, FoodLoadB)); //get load in grams
    
            /*
            char floatString[10];
            char amountString[10];
            dtostrf(animal[i].amount,1,2,floatString);
            sprintf(amountString, "%s", floatString);
            */
           
            int amountInt = int((animal[i].amount * 100));
            logData.concat(char(amountInt)); //add amount depensed times 100 to log data
            logData.concat(",");
            
            deniedFeeding = 0;
            
            unsigned long lastTagScanTime = millis();  // last time pets tag was read, in milliseconds
            const unsigned long stopLookingInterval = 10L * 1000L;  // stop looking time is set to 10 seconds, in milliseconds
            
            //While animal is still here feeding, keep looping and looking for a tag until animal has left feeder for more then 10 seconds
            while(millis() - lastTagScanTime < stopLookingInterval){
              
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
                    
                    // if tag scanned the parse tag and convert id to a string
                    parseTag();
                    
                    //if tag still matches pet that started the feed request, then set new scanned time  
                    if ((strcmp(tagCompare, tagId) == 0)) {
                      lastTagScanTime = millis();
                      //Serial.println(F("Pet is still feeding"));
                    }
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
              
            }
            
            
            //Serial.print(animal[i].tag);
            //Serial.println(F("Was fed!"));
            
            analogValueAOne = analogRead(A0); //get after eaten load cell reading from food bowl
            int weightAfter = int(analogToLoad(analogValueAOne, FoodAnalogvalA, FoodAnalogvalB, FoodLoadA, FoodLoadB)); //get load in grams
    
            int weightEaten = depensedWeight - weightAfter; //take depensed weight minus weight after to calculate eaten weight
            logData.concat(char(weightEaten)); //add amount eaten amount in grams to log data
            
            lockoutTime[i] = secSinceMidnight + (long)(60);
            delay(1000);
        
          }
  
          if ( (strcmp(animal[i].tag, tagCompare) == 0) && deniedFeeding == 1 ) { 
            beep();
            //animal[i].feedAttempts++;
            //Serial.print(animal[i].tag);
            //Serial.println(F("already ate!"));
            logData.concat("0"); //add amount depensed to log data
            logData.concat(",");
            
            analogValueAOne = analogRead(A0); //get load cell reading from food bowl
            int weightBefore = int(analogToLoad(analogValueAOne, FoodAnalogvalA, FoodAnalogvalB, FoodLoadA, FoodLoadB)); //get load in grams

            unsigned long lastTagScanTime = millis();  // last time pets tag was read, in milliseconds
            const unsigned long stopLookingInterval = 10L * 1000L;  // stop looking time is set to 10 seconds, in milliseconds
            
            //While animal is still here feeding, keep looping and looking for a tag until animal has left feeder for more then 10 seconds
            while(millis() - lastTagScanTime < stopLookingInterval){
              
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
                    
                    // if tag scanned the parse tag and convert id to a string
                    parseTag();
                    
                    //if tag still matches pet that started the feed request, then set new scanned time  
                    if ((strcmp(tagCompare, tagId) == 0)) {
                      lastTagScanTime = millis();
                      //Serial.println(F("Pet is still feeding"));
                    }
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
              
            }
            
            analogValueAOne = analogRead(A0); //get after eaten load cell reading from food bowl
            int weightAfter = int(analogToLoad(analogValueAOne, FoodAnalogvalA, FoodAnalogvalB, FoodLoadA, FoodLoadB)); //get load in grams
    
            int weightEaten = weightBefore - weightAfter; //take depensed weight minus weight after to calculate eaten weight
            
            if (weightEaten <= 10) {
              logData.concat("0"); //add amount depensed to log data should be zero
            } else { 
              logData.concat(char(weightEaten)); //add amount eaten amount in grams to log data
            }
            
          } 
      
           //if pet ate then mark eaten variable for that time slot
           if ( (strcmp(animal[i].tag, tagCompare) == 0) && ((((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) && (animal[i].slot1Eaten == 0)) || (((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) && (animal[i].slot2Eaten == 0))))
           {
            if ((animal[i].slot1Start <= secSinceMidnight) && ((animal[i].slot1Start + animal[i].slot1End) >= secSinceMidnight)) {
              animal[i].slot1Eaten = 1;
            }
            if ((animal[i].slot2Start <= secSinceMidnight) && ((animal[i].slot2Start + animal[i].slot2End) >= secSinceMidnight)) {
              animal[i].slot2Eaten = 1;
            }
           }
          
          //***End Data Logging*** 
          
          char* logPathFile = (char*)malloc(strlen(logPath)+20); //create path plus date stamp for filename
          strcpy(logPathFile, logPath);
          strcat(logPathFile, dateTime);
          File logFile = FileSystem.open(logPathFile, FILE_WRITE); //open log file with date stamp for filename
          free(logPathFile);
          logFile.print(logData); //print log data to file
          logFile.close();
          logData =  String("");
              
         } //end of tag match
       } //end of lockout
     } //end of for loop
   } else {
     beep();
     //Serial.println(F("Out of Food!"));
   }
}

void resetSlots() {

  //get time in seconds
  secSinceMidnight = ((((long)(60)) * (RTCValues[4]) + (long)(RTCValues[5])) * (long)(60)) + ((long)(RTCValues[6]));

  //reset slots for each pet if time is greater then 11pm 
  
  if ( secSinceMidnight > 82800 ) {
    for (int i = 0; i < 4; ++i) 
    {
      animal[i].slot1Eaten = 0;
      animal[i].slot2Eaten = 0;
      lockoutTime[i] = 0;
    }
  }
  
}

void printTag() {
  Serial.print(F("Tag value: "));
  Serial.println(tagId);
}

// this function clears the rest of data on the serial, to prevent multiple scans
void clearSerial() {
  while (RFID.read() >= 0) {
		; // do nothing
	}
}



/**********************************************************************************************************************
*                                                  Parse Tag Data Files function
***********************************************************************************************************************/  

  void parseTagDataFiles() {
    
   for (int i = 0; i < 4; i++)
   {
     // if settings path exists for each tag
     if (FileSystem.exists(tagSettingsPath[i])) {
       
 //******* Update Tag Settings ********
 
       char* updatedFile = (char*)malloc(strlen(tagSettingsPath[i])+1+11);
       strcpy(updatedFile, tagSettingsPath[i]);
       strcat(updatedFile, "updated.txt");
       
       //if updated.txt file exists of system boot is true, then update the pet struct
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
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
           //Serial.print(convertdata);// print for testing
           
           data = String(""); //clear data
           readFile.close();
           //Serial.println();
         }
         free(s2ePath);
         
         //reset eaten slots
         animal[i].slot1Eaten = 0;
         animal[i].slot2Eaten = 0;
         lockoutTime[i] = 0;
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
         
         strcpy (animal[i].tag, "0000000000");
         animal[i].amount = 0;
         animal[i].slot1Start = 0;
         animal[i].slot1End = 0;
         animal[i].slot2Start = 0;
         animal[i].slot2End = 0;
         animal[i].slot1Eaten = 0;
         animal[i].slot2Eaten = 0;
         lockoutTime[i] = 0;
         
         //Serial.print(i+1);
         //Serial.print(F(" deleted"));
         //Serial.println();
         
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
      //Serial.print(F("Feed Now in steps: "));
      //Serial.println(steps);
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
      //Serial.println();
      FileSystem.remove(feedNowFilePath); //remove feednow.txt now that tag has been cleared
      beep(); //beep if feed now has completed
    }
  }
  
  
  
/**********************************************************************************************************************
*                                                  Food Tank Status function
***********************************************************************************************************************/  

  void tankStatus() {
   
    //check food tank level
    int LDRReading = analogRead(A1);
    
    //set new tank status value
    if ( LDRReading <= 200 ) { 
      tankEmpty = false; //has food
    } else {
      tankEmpty = true; //out of food
    }
    
    //if tankEmpty value does not equal last value then tank status has changed
    if (tankEmpty != tankEmptyLastVal) {
      //if tankEmpty is true then write empty file, else false then write full file
      if ( tankEmpty == true ) { 
        
        //if old status files exist then delete them first
        if (FileSystem.exists(tankEmptyFile)) {
          FileSystem.remove(tankEmptyFile);
        }
        
        if (FileSystem.exists(tankFullFile)) {
          FileSystem.remove(tankFullFile); 
        }
        
        File emptyFile = FileSystem.open(tankEmptyFile, FILE_WRITE);
        emptyFile.close();
        //Serial.println(F("Tank empty file was created"));
        
      } else {
        
        //if old status files exist then delete them first
        if (FileSystem.exists(tankEmptyFile)) {
          FileSystem.remove(tankEmptyFile);
        }
        
        if (FileSystem.exists(tankFullFile)) {
          FileSystem.remove(tankFullFile); 
        }
        
        File fullFile = FileSystem.open(tankFullFile, FILE_WRITE);
        fullFile.close();
        //Serial.println(F("Tank full file was created"));
        
        
      }
      //since value has change, set last val to current tankEmpty val
      tankEmptyLastVal = tankEmpty;  
    } 
    
  }
  
  
  
/**********************************************************************************************************************
*                                                  Scale functions
***********************************************************************************************************************/  

float analogToLoad(int analogval, int analogvalA, int analogvalB, float loadA, float loadB){
  // using a custom map-function, because the standard arduino map function only uses int
  float load = mapfloat(analogval, analogvalA, analogvalB, loadA, loadB);
  //load = load * .002205;
  return load;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

  
  

/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/

void loop() { 
  
  //get current time
  DS1307.getDate(RTCValues);

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
        // save value
        readData[rfidCounter] = readVal;
        // increment counter
        ++rfidCounter;
    } 
  }
  
  // if ten seconds have passed since last Tag Settings check and Feed Now check,
  // then check again and get and parse data:
  if (millis() - settingsLastCheckTime > settingsPostingInterval) {
    //runPython();
    parseTagDataFiles();
    feedNowRequest();
    
    Serial.print(F("Free Memory = "));
    Serial.println(getFreeMemory());
   
    /*
    Serial.println(F("Pet Struct Info:"));
    for (int i = 0; i < 4; ++i) 
    {
        Serial.println(animal[i].tag);
        Serial.println(animal[i].amount);
        Serial.println(animal[i].slot1Start);
        Serial.println(animal[i].slot1End);
        Serial.println(animal[i].slot2Start);
        Serial.println(animal[i].slot2End);
        Serial.println(animal[i].slot1Eaten);
        Serial.println(animal[i].slot2Eaten);
        Serial.println(lockoutTime[i]);
    }
    */
    
    
  }
 
   //reset time slot variables after time slot passes
   //resetSlots();
   tankStatus();

}          
