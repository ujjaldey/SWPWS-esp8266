/*
   swpws-esp8266.ino
*/
#include "Arduino.h"
#include <ESP8266WebServer.h>

// Set the WIFI SSID and password
// Replace with your SSID and password
const char *ssid = "<your wifi ssid>";
const char *password = "<your wifi password>";

// Set Static IP address, Gateway, and Subnet
// Change if necessary
IPAddress staticIP(192, 168, 10, 205);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

void setupWifi()
{
  // Set Wifi mode and IP addresses
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(staticIP, gateway, subnet))
  {
    Serial.println("ERROR: STA could not be configured");
  }

  // Enable reconnect in case of disconnection
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Start the wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid);

  // Wait for the connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void setupServer()
{
  // Set server routing
  restServerRouting();

  // Set 404 (not found) response
  server.onNotFound(handleNotFound);

  // Start server
  server.begin();

  Serial.println("HTTP server started");
  Serial.println("");
}

// Define routing
void restServerRouting()
{
  server.on("/", HTTP_GET, []()
            { server.send(200, F("application/json"), F("{\"success\": true, \"message\": \"Welcome to SWPWS-ESP8266 REST Web Server\"}")); });

  //  server.on(F("/helloWorld"), HTTP_GET, getHelloWord);
  //  server.on(F("/on"), HTTP_GET, led_on);
  //  server.on(F("/off"), HTTP_GET, led_off);
}

void handleNotFound()
{
}

void setup()
{
  Serial.begin(115200);

  setupWifi();
  setupServer();
}

void loop()
{
  server.handleClient();
}
