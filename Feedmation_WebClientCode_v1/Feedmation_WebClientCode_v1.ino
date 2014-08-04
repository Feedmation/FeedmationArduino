#include <aJSON.h>
#include <Bridge.h>
#include <HttpClient.h>
#include <MemoryFree.h>

// Enter a api get string
char apiGET[] = "www.feedmation.com/api/v1/get_data.php?feederid=12345&function=pull_settings";

void setup() {
  Bridge.begin();
  Serial.begin(9600);
  while (!Serial);
}


/**********************************************************************************************************************
*                                                      JSON Parsing
***********************************************************************************************************************/


 void jsonParsing(char *jsonString)  {
 
   aJsonObject* jsonObject = aJson.parse(jsonString);
   
   Serial.print("Free Memory in JSON Functions = ");
   Serial.println(getFreeMemory());
    
   if (jsonObject != NULL) {
     
     aJsonObject* updates = aJson.getObjectItem(jsonObject , "updates");
     if (updates->valueint == 1) {
       Serial.println(F("There are config updates"));
       aJsonObject* tag1 = aJson.getObjectItem(jsonObject , "one");
       aJsonObject* tag1Id = aJson.getObjectItem(tag1 , "tagId");
       Serial.print(F("tag1Id: "));
       Serial.println(tag1Id->valuestring);
       aJsonObject* amount = aJson.getObjectItem(tag1, "amountSlot1");
       Serial.print(F("amount in cups: "));
       Serial.println(amount->valuestring);
     } else {
       Serial.println(F("There are no updates")); 
     }
   } else{
       Serial.print(F("JSON Object is empty"));
   }
   aJson.deleteItem(jsonObject); 
  
 }


void loop() {

  Serial.print("Free Memory = ");
  Serial.println(getFreeMemory()); 
  
  HttpClient client;
  client.get(apiGET);

  if (client.available()) {
    
    String httpReturn =  String("");  //String for http request return
    while (client.available() > 0) { //loop until the whole http request is stored
      char c = client.read();
      httpReturn.concat(String(c));  //Storing the http request return
    }
    
    int openingBracket = httpReturn.indexOf('{'); //find start index of json
    int closingBracket = httpReturn.lastIndexOf('}'); //find end index of json
    char jsonString[(closingBracket - openingBracket) + 2];
    httpReturn.toCharArray(jsonString, (closingBracket - openingBracket) + 2);  //converting string to char array for aJSON
    
    httpReturn =  String("");
    
    Serial.print("Free Memory after http client copy = ");
    Serial.println(getFreeMemory());
    
    /*
    Serial.println();
    Serial.print("JSON is: ");
    for (int i = 0; i <= (closingBracket - openingBracket); i++) { //print char array debugging only
      Serial.print(jsonString[i]);
    }
    */
    
    Serial.println(); 
    Serial.println(); 
    
    //jsonParsing(jsonString);
    
  }
  Serial.flush();

  delay(5000);

}

