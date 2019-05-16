//########################  YouTube Subscriber Count Display on LED MATRIX  #############################
// Receives and displays YouTube channel statistics
//################# LIBRARIES ################
String version = "v1.0";       // Version of this program

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <DNSServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>    // https://github.com/markruys/arduino-Max72xxPanel
#include <time.h>
//################ VARIABLES ################
// Use your own API key by signing up for a free Youtube developer account at http://www.youtube.com
// LED Matrix Pin -> ESP8266 Pin
// Vcc            -> 3v  (3V on NodeMCU 3V3 on WEMOS)
// Gnd            -> Gnd (G on NodeMCU)
// DIN            -> D7  (Same Pin for WEMOS)
// CS             -> D4  (Same Pin for WEMOS)
// CLK            -> D5  (Same Pin for WEMOS)

const char* channelId = "UC--------------------Tg"; // See your Channel details click on your channel icon at top-right, then Settings, then Advanced
const char* apiKey    = "AI-----------------------------------gM"; // See https://developers.google.com/youtube/v3/getting-started for an API key, you must enable it to make it work!
const char* host      = "https://www.googleapis.com";
unsigned long lastConnectionTime = 0;                 // Last time you connected to the server, in milliseconds
const unsigned long  postingInterval = 10L*60L*1000L; // Delay between updates, in milliseconds, you should limit your YouTube requests per-day maximum

//################ PROGRAM VARIABLES and OBJECTS ################
int pinCS = D4; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays   = 1;
int wait   = 35; // In milliseconds
int spacer = 1;
int width  = 5 + spacer; // The font width is 5 pixels
int port   = 443;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

struct channelStatistics{
  long viewCount;
  long commentCount;
  long subscriberCount;
  bool hiddenSubscriberCount;
  long videoCount;
};

channelStatistics channelStats;

void setup() {
  Serial.begin(115200); // initialize serial communications
  WiFiManager wifiManager;
  // New OOB ESP8266 have no Wi-Fi credentials, so do-not need the next command** to be uncommented and compiled, a used ESP8266 with incorrect credentials does
  // so restart the ESP8266 and connect your PC to the wireless access point called 'ESP8266_AP' or whatever you call it below in ""
  // wifiManager.resetSettings(); // ** Command to be included if needed, then connect to http://192.168.4.1/ and follow instructions to make the WiFi connection
  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);
  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP8266_AP" and waits in a blocking loop for configuration
  if(!wifiManager.autoConnect("ESP8266_AP")) {
    Serial.println(F("failed to connect and timeout occurred"));
    delay(6000);
    ESP.reset(); //reset and try again
    delay(120000);
  }
  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  Serial.println(F("WiFi connected..."));
  //----------------------------------------------------------------------
  configTime(0 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  matrix.setIntensity(0); // Use a value between 0 and 15 for brightness
// Adjust the LED matrix to your own needs
//  matrix.setPosition(0, 0, 0); // The first display is at <0, 0>
//  matrix.setPosition(1, 1, 0); // The second display is at <1, 0>
//  matrix.setPosition(2, 2, 0); // The third display is at <2, 0>
//  matrix.setPosition(3, 3, 0); // And the last display is at <3, 0>
//  ...
  matrix.setRotation(0, 1);    // The first display is position is rotated
  matrix.setRotation(1, 1);    // The first display is position is rotated
  matrix.setRotation(2, 1);    // The first display is position is rotated
  matrix.setRotation(3, 1);    // The first display is position is rotated
  lastConnectionTime = millis();
  obtain_subscriber_stats();
}

void loop() {
  display_message("Subs: " + String(channelStats.subscriberCount));
  time_t now = time(nullptr);
  String time = String(ctime(&now));
  time.trim();
  display_message(" on " + time);
  if (millis() - lastConnectionTime > postingInterval) { // 15-minutes
    obtain_subscriber_stats();
    lastConnectionTime = millis();
  }
}

//################ PROGRAM FUNCTIONS ################

void display_message(String message){ // Scroll the image left
  matrix.fillScreen(LOW);
  for ( int i = 0 ; i < width * message.length() + matrix.width() - 1 - spacer; i++ ) {
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if (letter < message.length()) matrix.drawChar(x, y, message[letter], HIGH, LOW, 1); // HIGH means foreground on, LOW means background off, LOW< HIGH inverts the display
      letter--;
      x -= width;
    }
    matrix.write(); // Send bitmap to display
    delay(wait);
  }
}

void obtain_subscriber_stats() {
  String command = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + String(channelId);
  String response = GetRequestFromYoutube(command);       
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(response); // Parse JSON
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
  }
  else
  {
    JsonObject& current                = root["items"][0]["statistics"];  
    long subscriberCount               = current["subscriberCount"];
    long viewCount                     = current["viewCount"];
    long commentCount                  = current["commentCount"];
    long hiddenSubscriberCount         = current["hiddenSubscriberCount"];
    long videoCount                    = current["videoCount"];
    channelStats.viewCount             = viewCount;
    channelStats.subscriberCount       = subscriberCount;
    channelStats.commentCount          = commentCount;
    channelStats.hiddenSubscriberCount = hiddenSubscriberCount;
    channelStats.videoCount            = videoCount;
  }  
  Serial.println("      View Count = " + String(channelStats.viewCount));
  Serial.println("Subscriber Count = " + String(channelStats.subscriberCount));
}

String GetRequestFromYoutube(String request) {
  String headers, body = "";
  bool Headers    = false;
  bool currentLineIsBlank = true;
  int  MessageLength = 1000;
  // Connect with youtube api over ssl
  WiFiClientSecure client;
  if (client.connect(host, port)) {
    Serial.println(".... connected to server");
    char c;
    int ch_count=0;
    request += "&key=" + String(apiKey);
    request = "GET " + request;
    //Serial.println(request);
    client.println(request);
    int now   = millis();
    while (millis()-now < 1500) {
      while (client.available()) {
        char c = client.read();
        if(!Headers){
          if (currentLineIsBlank && c == '\n') {
            Headers = true;
          }
          else{
            headers = headers + c;
          }
        } else {
          if (ch_count < MessageLength)  {
            body = body+c;
            ch_count++;
          }
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
  }
  return body;
}
