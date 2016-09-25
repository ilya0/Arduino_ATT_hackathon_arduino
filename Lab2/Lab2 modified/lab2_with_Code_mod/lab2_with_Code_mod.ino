/* 
   lab2.ino

   AT&T DevLab pushbutton demo
*/
#include <aJSON.h>
#include "SPI.h"
#include "WiFi.h"

#include "M2XStreamClient.h"

char ssid[] = "LAUNCHPAD1"; //  your network SSID (name)
char pass[] = "launchpad"; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

char deviceId[] = "ff25f1c74cd3b7a725278f5eb862bd43"; // Feed you want to post to
char m2xKey[] = "accdb7558e6296bd9877e70ae4d6f66b"; // Your M2X access key
char streamName[] = "Press_time"; // Stream you want to post to

WiFiClient client;
M2XStreamClient m2xClient(&client, m2xKey);

int btn1 = 1;  // The button state
boolean pushed = false; // True when the button is pushed
int whenPushed = 0;  // The time in milliseconds from when the button is pushed.

void setup() {

    Serial.begin(9600);
    pinMode(PUSH1, INPUT_PULLUP);
    
    // attempt to connect to Wifi network:
    Serial.print("Attempting to connect to Network named: ");
    // print the network name (SSID);
    Serial.println(ssid); 
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);
    while ( WiFi.status() != WL_CONNECTED) {
      // print dots while we wait to connect
      Serial.print(".");
      delay(300);
    }
  
    Serial.println("\nYou're connected to the network");
    Serial.println("Waiting for an ip address");
  
    while (WiFi.localIP() == INADDR_NONE) {
      // print dots while we wait for an ip addresss
      Serial.print(".");
      delay(300);
    }

    Serial.println("\nIP Address obtained");
  
    // you're connected now, so print out the status  
    printWifiStatus();
}

void loop() {
  // Read the pushbutton state. 1 for not pushed, 0 for pushed.
  btn1 = digitalRead(PUSH1);
  
  if (btn1 == 0 && !pushed) {
    // If pushed, then store the time
    pushed = true;
    whenPushed = millis();
  } else if (btn1 == 1 && pushed) {
    // Reset pushed
    pushed = false;
    
    // Released. Calculate the time elapsed
    double seconds = (millis() - whenPushed) / 1000.0;
    Serial.print("Seconds pushed: ");
    Serial.println(seconds);
    
    // Send to M2X
    int response = m2xClient.updateStreamValue(deviceId, streamName, seconds);
    Serial.print("M2X client response code: ");
    Serial.println(response);

    // If the response is an error, then send into infinite loop to stop loop
    if (response == -1)
      while (1)
        ;
  }

}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}
