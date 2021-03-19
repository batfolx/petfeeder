#include <ArduinoWebsockets.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "ESPServo.h"


#define SERVO_HIGH 20000
#define SERVO_LOW 1000

using namespace websockets;


uint8_t servo_pin = 26;
uint8_t pwm_channel = 0;
uint8_t led_pin = 27;
uint8_t led_pwm_channel = 2;


// meant for rebooting ESP after failed internet connections
uint8_t max_tries = 25;



// server address to connect to
int16_t PORT = 0;
char HOST[] = "xx.xx.x.x";
char PROTO[] = "http";

// wireless access point credentials
char ssid[] = "";
char network_password[] = "";

// this device credentials
char username[] = "";
char password[] = "";


// this device name supported operations and name
char DEVICE_NAME[] = "Pet Feeder";
WebsocketsClient client;

// session key to make authenticated requests to the server
String SESSION_KEY;

// servo pin, pwmchannel 0, 50hz, 16bit timer width, -1 and -1 default to COUNT_LOW and COUNT_HIGH
ESPServo servo(servo_pin, pwm_channel, 50, 16, -1, -1);

// authenticates to the server
bool authenticate() {

  HTTPClient http;
  char endpoint[256];
  Serial.println("Authenticating!");

  // get correct URL in `endpoint` variable
  snprintf(endpoint, 256, "%s://%s:%d", PROTO, HOST, PORT);
  if (WiFi.status() == WL_CONNECTED) {
     http.begin(endpoint);
     
      
     // add header for JSON in HTTP post
     http.addHeader("Content-Type", "application/x-www-form-urlencoded");
     char creds[128];
     snprintf(creds, 128, "%s%s", username, password);
     int code = http.POST(creds);

     // good request
     if (code == 200) {
      // I modified my server to send the cookie in the response body, go figure I guess

      SESSION_KEY = http.getString();
      http.end();
      return true;
      
     } else {
      Serial.println("Could not send POST to host");
      return false;
     }

  } else {
      return false;
  }
  
}


void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
        ESP.restart();
    } 
}




void setup()
{
    ledcSetup(led_pwm_channel, 2000, 8);
    ledcAttachPin(led_pin, led_pwm_channel);
    

    Serial.begin(115200);
    while ( !Serial ) ;

    int tries = 0;
    Serial.println("Beginning to connect to wifi!");
    WiFi.begin(ssid, network_password);
    while ( WiFi.status() != WL_CONNECTED ) {
      delay(1000);
      Serial.println("Failed to connect... retrying");
      ledcWrite(led_pwm_channel, 200);
      delay(500);
      ledcWrite(led_pwm_channel, 0);
      delay(500);

      if ( ++tries > max_tries ) ESP.restart();
    }

    Serial.println("Successfully connected to WiFi!");
    // keep trying to authenticate
    while ( !authenticate() ) delay(1000);

    for (int i = 0; i < 10; i++) {
        ledcWrite(led_pwm_channel, 200);
        delay(75);
        ledcWrite(led_pwm_channel, 0);
        delay(75);
    }
    ledcWrite(led_pwm_channel, 200);
    Serial.println("Successfully authenticated to server!");
    Serial.println("Setting extra headers and upgrading to websocket");

    // remove quotes from the string, idk...
    SESSION_KEY.remove(0, 1);
    SESSION_KEY.remove(SESSION_KEY.length() - 2, 5);
    
    client.addHeader("cookie", SESSION_KEY);
    client.addHeader("supported_operations", "[\"RESET\", \"GIVE FOOD\"]");
    client.addHeader("device_name", DEVICE_NAME);
    client.addHeader("Connection", "upgrade");
    client.addHeader("Upgrade", "websocket");
    client.addHeader("Origin", "The Hub");
    bool connected = client.connect(HOST, PORT, "/api/device/register");
    if(connected) {
        Serial.println("Connected!");
    } else {
        Serial.println("Not Connected!");
        ESP.restart();
    }

    // used to detect when connection is terminated
    client.onEvent(onEventsCallback);
    // run callback when messages are received
    client.onMessage([&](WebsocketsMessage message){

        String command = message.data();
        Serial.println("Got command: " + command);
        if ( command == "RESET" ) {

            servo.write( servo.get_high() );
            delay(2000);
        
        } else if ( command == "GIVE FOOD" ) {
            // rotate the dropper 60 degrees
            int32_t sixth = ( servo.get_high() - servo.get_low() ) / 5;
            servo.write( ( servo.read() - sixth ) );
            delay(2000);

        } else {
          
        }
        
    });
 

}


void feed() {

    servo.write( servo.get_high() );
    delay(2000);
    for ( int i = 0; i < 6; i++ ) {
        int32_t sixth = ( servo.get_high() - servo.get_low() ) / 6;
        servo.write( ( servo.read() - sixth ) );
        delay(1000);
    }

}


void loop()
{
       // let the websockets client check for incoming messages

    if(client.available()) {
        client.poll();
    } 
    delay(150);
}
