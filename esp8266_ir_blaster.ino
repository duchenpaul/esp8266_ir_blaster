/*
* This programm runs a webserver on the chip providing to main routes:
* POST /play
*   This will trigger a 38khz PWM signal on the IR LED with the given timings
*   Parameters: expects form encoded parameter 'timings' as comma separated list of integers
*
* GET /record
*   This will probe the IR sensor and return the timings in the response body.
*   The probing call is not blocking apparently, so it may have to be triggered again, to
*   actually show a result.
*   If nothing was receive the response will contain the string 'empty'.
*
* All neccessary configuration parameters are defined in the included config.h
* If you copied/cloned this project for the first time you may have to copy
* the config.h.example -> config.h and update the parameters with the matching values.
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include "config.h"


IRrecv irrecv(RECV_PIN);
IRsend irsend(IR_LED); // Set the GPIO to be used to sending the message.
ESP8266WebServer server(80);

void configModeCallback(WiFiManager * myWiFiManager);

void configModeCallback(WiFiManager * myWiFiManager)
{
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    // if you used auto generated SSID, print it
    Serial.println(myWiFiManager -> getConfigPortalSSID());
}

void handleRoot()
{
    server.send(200, "text/plain", "NodeMCU infrared remote");
}

#define MAX_DATA_LENGTH 100
uint16_t signal_data[MAX_DATA_LENGTH];
int current_data_length = -1;
void readCSV(String s)
{
    int numStart = 0;
    current_data_length = 0;
    for (int i = 0; i <= s.length(); ++i)
    {
        if ((s.charAt(i) == ',') || (i == (s.length())))
        {
            signal_data[current_data_length++] = s.substring(numStart, i).toInt();
            numStart = i + 1;
        }
    }
}

void handlePlay()
{
    String response = "POSTED";
    response += server.arg("timings");
    readCSV(server.arg("timings"));
    for (int i = 0; i < current_data_length; ++i)
    {
        Serial.println(signal_data[i]);
    }
    digitalWrite(LED_BUILTIN, LOW);
    irsend.sendRaw(signal_data, current_data_length, 38); // Send a raw data capture at 38kHz.
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Signal sent!");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", response);
}

#define CAPTURE_BUFFER_SIZE 1024

#if DECODE_AC
#define TIMEOUT 50U // Some A/C units have gaps in their protocols of ~40ms.
// e.g. Kelvinator
// A value this large may swallow repeats of some protocols
#else // DECODE_AC
#define TIMEOUT 15U // Suits most messages, while not swallowing many repeats.
#endif // DECODE_AC

decode_results results;
void handleRecord()
{
    String output = "";
    if (irrecv.decode(& results))
    {
        output += resultToSourceCode(& results);
        irrecv.resume(); // Receive the next value
    }
    else
    {
        output = "empty";
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", output);
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)? "GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

void setup(void)
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.begin(115200);
    irrecv.enableIRIn(); // Start the receiver
    irsend.begin(); // Start IR sender
    while (!Serial)
        // Wait for the serial connection to be establised.
    delay(50);
    Serial.println();
    Serial.print("IRrecvDemo is now running and waiting for IR message on Pin ");
    Serial.println(RECV_PIN);
    // WiFiManager
    // Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    // Uncomment for testing wifi manager
    // wifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);
    // or use this for auto generated name ESP + ChipID
    wifiManager.autoConnect();
    // Manual Wifi
    // WiFi.begin(WIFI_SSID, WIFI_PWD);
    String hostname(HOSTNAME);
    hostname += String(ESP.getChipId(), HEX);
    // WiFi.hostname(hostname);
    WiFi.hostname("esp8266_ir_blaster");
    // Connecting to WiFi network
    Serial.println();
    Serial.print("Connecting to ");

    // WiFi.mode(WIFI_STA);
    // WiFi.begin(SSID, PASSWORD);
    Serial.println("");
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    if (MDNS.begin("esp8266"))
    {
        Serial.println("MDNS responder started");
    }
    server.on("/", handleRoot);
    server.on("/record", handleRecord);
    server.on("/play", HTTP_POST, handlePlay);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");
}

void loop(void)
{
    server.handleClient();
}
