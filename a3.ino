// This code takes reading from the barometric sensor and dht22 sensor that provides
// temperature, pressure and humidity data. The code also provides button state input data. The 
// 4 data types (temperature, pressure, humidity and button state) are then sent to a MQTT server
// in the form of JSON data in their respective topics. use Treasure/+ to see all the data being sent out at once.
// The code then subscribes to the topics and prints the data to the serial monitor and OLED display.

#include "config.h" // edit the config.h tab and enter your credentials

// Required libraries for code to work
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>   
#include <ArduinoJson.h>    
#include <ESP8266WiFi.h> 
#include <Adafruit_MPL115A2.h>
#include <Adafruit_Sensor.h>

#define BUTTON_PIN 13 // digital pin 13
// button state
bool buttonCurrent = false;
bool buttonLast = false;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1  // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 
Adafruit_MPL115A2 mpl115a2; 

#define DATA_PIN 12 // pin connected to DH22 data line

DHT_Unified dht(DATA_PIN, DHT22); // create DHT22 instance

WiFiClient espClient;            
PubSubClient mqtt(espClient);     

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!

char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array

void setup() {

  Serial.begin(115200); // start the serial connection
  
  // Prints the results to the serial monitor
  Serial.print("This board is running: ");  //Prints that the board is running
  Serial.println(F(__FILE__));
  Serial.print("Compiled: "); //Prints that the program was compiled on this date and time
  Serial.println(F(__DATE__ " " __TIME__));

  while(! Serial); // wait for serial monitor to open

  setup_wifi();
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function

  dht.begin(); // initialize dht22

  mpl115a2.begin(); // initialize barometric sensor

  pinMode(BUTTON_PIN, INPUT_PULLUP); // set button pin as an input

  //setup for the display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initailize with the i2c addre 0x3C
  display.clearDisplay();                    //Clears any existing images or text
  display.setTextSize(1);                    //Set text size
  display.setTextColor(WHITE);               //Set text color to white
  display.setCursor(0,0);                    //Puts cursor on top left corner
  display.println("Starting up...");         //Test and write up
  display.display();                         //Displaying the display
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // wait 5 ms
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}                                     

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("Treasure/+"); //we are subscribing to 'Treasure' and all subtopics below that topic
    } else {                       
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // Wait 5 seconds before retrying
    }
  }
}

void loop() {

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'

  if(digitalRead(BUTTON_PIN) == LOW)
    buttonCurrent = true;
  else
    buttonCurrent = false;

  // send message to topic containing button state data, 1 = pressed button and 0 = no button press
  sprintf(message, "{\"Button State\":\"%d\"}", buttonCurrent); 
  mqtt.publish("Treasure/button", message);

  sensors_event_t event;
  dht.humidity().getEvent(&event); // get humidity data from dht22
  dht.temperature().getEvent(&event); // get temperature data from dht22

  float celcius = event.temperature; // temperature data from dht22 and stored as numbers with decimal points
  float fahrenheit = (event.temperature * 1.8) + 32; // temperature data conversion to fahrenheit
  float pressure = mpl115a2.getPressure(); // pressure data from mpl115a2 and stored as numbers with decimal points
  float humidity = event.relative_humidity; // hum data from dht22 and stored as numbers with decimal points

  char fah[6]; //a temporary array of size 6 to hold "XX.XX" + the terminating character
  char cel[6]; //a temporary array of size 6 to hold "XX.XX" + the terminating character
  char pres[7]; //a temp array of size 7 to hold "XX.XX" + the terminating character
  char hum[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character

  //take pres, format it into 5 or 6 char array with a decimal precision of 2
  dtostrf(pressure, 6, 2, pres);
  dtostrf(humidity, 5, 2, hum);
  dtostrf(fahrenheit, 5, 2, fah);
  dtostrf(celcius, 5, 2, cel);

  // send message to topics containing temperature/humidity/pressure data
  sprintf(message, "{\"Temperature (F)\":\"%s\", \"Temperature (C)\":\"%s\"}", fah, cel);
  mqtt.publish("Treasure/temperature", message);

  sprintf(message, "{\"Pressure\":\"%s\"}", pres);
  mqtt.publish("Treasure/pressure", message);

  sprintf(message, "{\"Humidity\":\"%s\"}", hum);
  mqtt.publish("Treasure/humidity", message);

  delay(1000); // wait for a second
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; 
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }

  display.clearDisplay(); //Clears any existing images or text
  display.setCursor(0,0); //Puts cursor on top left corner

  // Searches for messages on the following topics and prints them out on serial monitor and display
  if (strcmp(topic, "Treasure/temperature") == 0) {
    Serial.println("Temperature . . .");
  }

  else if (strcmp(topic, "Treasure/humidity") == 0) {
    Serial.println("Humidity . . .");
  }

  else if (strcmp(topic, "Treasure/pressure") == 0) {
    Serial.println("Pressure . . .");
  }

  else if (strcmp(topic, "Treasure/button") == 0) {
    Serial.println("Button State . . .");
  }

  root.printTo(Serial); //print out the parsed message to serial
  Serial.println(); //give us some space on the serial monitor read out
  root.printTo(display); // print out the parsed message to display
  display.display(); //Displaying the display
}