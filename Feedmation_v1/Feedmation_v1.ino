#include <MemoryFree.h>
#include <aJSON.h>
#include <Bridge.h>
#include <HttpClient.h>
#include <Wire.h>
#include <DS1307.h>
#include <FileIO.h>


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

//HTTP Request and data storing
// Enter a api get strings
char urlGetSettings[] = "www.feedmation.com/api/v1/sync_data.php?feederid=12345&function=pull_settings";
char urlGetFeedNow[] = "www.feedmation.com/api/v1/sync_data.php?feederid=12345&function=feed_now";

//Boolean value that gets set when http request data is availble for parsing
boolean httpSettingsDataReady = false;
boolean httpFeedNowDataReady = false;
char httpSettingsFile[] = "/tmp/httpReturnSettings.txt";
char httpFeedNowFile[] = "/tmp/httpReturnFeedNow.txt";
unsigned long settingsLastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long settingsPostingInterval = 10L * 1000L; // delay between updates, in milliseconds
unsigned long feedNowLastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long feedNowPostingInterval = 5L * 1000L; // delay between updates, in milliseconds


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
  
  Bridge.begin();
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
*                                     JSON Parsing and Animal Settings update
***********************************************************************************************************************/


 void jsonParsing( char * filePath )  {
   
   if (FileSystem.exists(filePath)) { //file exists then create object
     
     File file = FileSystem.open(filePath, FILE_READ);
     aJsonStream file_stream(&file);
     aJsonObject* jsonObject = aJson.parse(&file_stream);
     file.close();
     
     if (jsonObject != NULL) { //if JSON opbject was created then parse
       aJsonObject* tagUpdate = aJson.getObjectItem(jsonObject , "tag");
       aJsonObject* feedNow = aJson.getObjectItem(jsonObject , "f");
       aJsonObject* feedNowAmount = aJson.getObjectItem(jsonObject , "fa");
       
       if (feedNow != NULL) { //feed now request if true
         if (feedNow->valueint == 1) {
           //Serial.println(feedNowAmount->valuefloat);
           feedNowRequest(feedNowAmount->valuefloat);
         }
       }
       
       if (tagUpdate != NULL) {
           int i = tagUpdate->valueint;
           Serial.println(i);
           aJsonObject* tagId = aJson.getObjectItem(jsonObject , "tid");
           aJsonObject* amount = aJson.getObjectItem(jsonObject , "a");
           aJsonObject* slot1Start = aJson.getObjectItem(jsonObject , "s1");
           aJsonObject* slot1End = aJson.getObjectItem(jsonObject , "s1e");
           aJsonObject* slot2Start = aJson.getObjectItem(jsonObject , "s2");
           aJsonObject* slot2End = aJson.getObjectItem(jsonObject , "s2e");
           
           //Serial.print(F("Free Memory during JSON processing = "));
           //Serial.println(getFreeMemory());
           
           strncpy ( animal[i].tag, tagId->valuestring, 11 );
           animal[i].amount = amount->valuefloat; //amount of food in cups
           animal[i].slot1Start = (long)(60) * (long)(60) * (long)(slot1Start->valueint);
           animal[i].slot1End = (long)(60) * (long)(60) * (long)(slot1End->valueint);;
           animal[i].slot2Start = (long)(60) * (long)(60) * (long)(slot2Start->valueint);;
           animal[i].slot2End = (long)(60) * (long)(60) * (long)(slot2End->valueint);;
           animal[i].slot1Eaten = 0;
           animal[i].slot2Eaten = 0;
           
           /*
           Serial.print(F("tagid: "));
           Serial.println(animal[i].tag);
           Serial.print(F("amount: "));
           Serial.println(animal[i].amount);
           Serial.print(F("slot1: "));
           Serial.println(animal[i].slot1Start);
           Serial.print(F("slot1: "));
           Serial.println(animal[i].slot1End);
           Serial.print(F("slot2: "));
           Serial.println(animal[i].slot2Start);
           Serial.print(F("slot2: "));
           Serial.println(animal[i].slot2End);
           */
           
           beep(); //beep if update has completed
           
           //httpRequest(urlGetSettings); //call http request and check for more updated tags
       }
       
     } else{
         Serial.print(F("JSON Object is empty"));
     }
     aJson.deleteItem(jsonObject);
   }
 }
 
 
 /**********************************************************************************************************************
*                                                  HTTP Get request
***********************************************************************************************************************/
 
 void httpRequest( char * url) {
  
   HttpClient client;
   client.get(url);

   if (client.available()) {
    
    String httpReturnData =  String("");  //String for http request return
    while (client.available() > 0) { //loop until the whole http request is stored
      char c = client.read();
      httpReturnData.concat(String(c));  //Storing the http request return
    }

    Serial.println(httpReturnData);
    
    if (strcmp(url, urlGetSettings) == 0) {
      
      if (FileSystem.exists(httpSettingsFile)) {
      FileSystem.remove(httpSettingsFile); 
      }
    
      File httpReturnFile = FileSystem.open(httpSettingsFile, FILE_WRITE);
    
      httpReturnFile.print(httpReturnData);
      httpReturnFile.close();
      
      httpSettingsDataReady = true;
    
    }
      
    if (strcmp(url, urlGetFeedNow) == 0) {
      
      if (FileSystem.exists(httpFeedNowFile)) {
      FileSystem.remove(httpFeedNowFile); 
      }
    
      File httpReturnFile = FileSystem.open(httpFeedNowFile, FILE_WRITE);
    
      httpReturnFile.print(httpReturnData);
      httpReturnFile.close();
      
      httpFeedNowDataReady = true;
    }
  
    /*
    File printFile = FileSystem.open("/tmp/httpReturn.txt", FILE_READ);
    
    while (printFile.available() > 0) { 
      char c = printFile.read();
      Serial.print(c); 
    }
    printFile.close();
    */
    
    httpReturnData =  String("");
   
   }
   
   if (strcmp(url, urlGetSettings) == 0) {
     // note the time that the connection was made:
      settingsLastConnectionTime = millis();
   }
   
   if (strcmp(url, urlGetFeedNow) == 0) {
     // note the time that the connection was made:
     feedNowLastConnectionTime = millis();
   }
   
  }
  


/**********************************************************************************************************************
*                                                  Feed Now request function
***********************************************************************************************************************/  

  void feedNowRequest( float feedAmount) {
     
    int steps =  cup * feedAmount;
    
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
  }
  
  

/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/

void loop() { 

  //get current time
  DS1307.getDate(RTCValues);
  
  // if new http settings data is available, then parse settings
  if (httpSettingsDataReady) {
   jsonParsing(httpSettingsFile);
   httpSettingsDataReady = false; 
  }
  
  // if new http feed now data is available, then parse feed now
  if (httpFeedNowDataReady) {
   jsonParsing(httpFeedNowFile);
   httpFeedNowDataReady = false; 
  }
  
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
 
  // if ten seconds have passed since your last connection for Tag Settings,
  // then connect again and get data:
  if (millis() - settingsLastConnectionTime > settingsPostingInterval) {
    httpRequest(urlGetSettings);
    //Serial.print(F("Free Memory = "));
    //Serial.println(getFreeMemory());
  }
  
  // if five seconds have passed since your last connection for Feed Now,
  // then connect again and get data:
  if (millis() - feedNowLastConnectionTime > feedNowPostingInterval) {
    httpRequest(urlGetFeedNow);
  }
  
   //reset time slot variables after time slot passes
   //resetSlots();
   
   //check food tank level
   LDRReading = analogRead(A1);

}
              
