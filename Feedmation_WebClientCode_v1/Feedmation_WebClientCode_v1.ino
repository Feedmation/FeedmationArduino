#include <aJSON.h>
#include <SPI.h>
#include <Ethernet.h>


// Enter a MAC address for your controller below.
byte mac[] = {  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
char server[] = "www.feedmation.com";
char apiGET[] = "GET /api/v1/get_data.php?feederid=12345&function=pull_settings HTTP/1.0";

// Initialize the Ethernet client library
EthernetClient client;

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 10*1000;  // delay between updates, in milliseconds


void setup() {
  // start serial port:
  Serial.begin(9600);
  // give the ethernet module time to boot up:
  delay(1000);
 // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    while(true);
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  
  if (client.available()) {
    String httpReturn =  String("");  //String for http request return
    while (client.available() > 0) { //loop until the whole http request is stored
      char c = client.read();
      httpReturn.concat(String(c));  //Storing the htto request return
    }
    
    Serial.println(httpReturn); //print return for debugging
    
    int openingBracket = httpReturn.indexOf('{'); //find start index of json 
    int closingBracket = httpReturn.lastIndexOf('}'); //find end index of json 
    String jsonString =  String(""); // create blank string
    for (int i=openingBracket; i <= closingBracket; i++){ //Parse out and storage json string
      jsonString.concat(httpReturn.charAt(i));   
    }
    
    //Serial.println(jsonString); //print json serial for debegging
    
    char jsonArray[(closingBracket - openingBracket) + 2];
    jsonString.toCharArray(jsonArray, (closingBracket - openingBracket) + 2);  //converting string to char array for aJSON   
    
    Serial.println();
    Serial.print("JSON is: ");
    for (int i=0; i <= (closingBracket - openingBracket); i++){ //print char array debugging only
      Serial.print(jsonArray[i]);   
    }
    
    //Serial.println(); 
    //aJsonObject* jsonObject = aJson.parse(jsonArray);
    //aJsonObject* name = aJson.getObjectItem(jsonObject , "Name");
    //Serial.print("Pets Name: ");
    //Serial.println(name->valuestring);

  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    httpRequest();
  }
  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}

// this method makes a HTTP connection to the server:
void httpRequest() {
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println(apiGET);
    client.println("Host: www.feedmation.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
}




