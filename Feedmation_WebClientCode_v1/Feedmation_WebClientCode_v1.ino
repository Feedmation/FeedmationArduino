#include <aJSON.h>
#include <Bridge.h>
#include <HttpClient.h>

// Enter a api get string
char apiGET[] = "www.feedmation.com/api/v1/get_data.php?feederid=12345&function=pull_settings";

void setup() {
  Bridge.begin();
  Serial.begin(9600);
  while (!Serial);
}

void loop() {

  HttpClient client;
  client.get(apiGET);

  if (client.available()) {
    String httpReturn =  String("");  //String for http request return
    while (client.available() > 0) { //loop until the whole http request is stored
      char c = client.read();
      httpReturn.concat(String(c));  //Storing the http request return
    }

    Serial.println(httpReturn); //print return for debugging

    int openingBracket = httpReturn.indexOf('{'); //find start index of json
    int closingBracket = httpReturn.lastIndexOf('}'); //find end index of json
    String jsonString =  String(""); // create blank string
    for (int i = openingBracket; i <= closingBracket; i++) { //Parse out and storage json string
      jsonString.concat(httpReturn.charAt(i));
    }

    //Serial.println(jsonString); //print json serial for debegging

    char jsonArray[(closingBracket - openingBracket) + 2];
    jsonString.toCharArray(jsonArray, (closingBracket - openingBracket) + 2);  //converting string to char array for aJSON

    Serial.println();
    Serial.print("JSON is: ");
    for (int i = 0; i <= (closingBracket - openingBracket); i++) { //print char array debugging only
      Serial.print(jsonArray[i]);
    }

    Serial.println();
    aJsonObject* jsonObject = aJson.parse(jsonArray);
    aJsonObject* name = aJson.getObjectItem(jsonObject , "Name");
    Serial.print("Pets Name: ");
    Serial.println(name->valuestring);
    aJson.deleteItem(jsonObject);

  }
  Serial.flush();

  delay(5000);

}

