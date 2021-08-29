/*
   swpws-esp8266.ino
*/
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJWT.h>
#include <sha256.h>
#include <ArduinoJson.h>

// Set the WIFI SSID and password
// Replace with your SSID and password
const char *ssid = "<your wifi ssid>";
const char *password = "<your wifi password>";

const char *piUser = "pi";
const char *piPassword = "raspberry";

// Set Static IP address, Gateway, and Subnet
// Change if necessary
IPAddress staticIP(192, 168, 10, 205);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

ArduinoJWT jwt = ArduinoJWT("secret");

StaticJsonBuffer<200> jsonBuffer;

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

String jwtToJsonStr(String jwtStr)
{
  int jwtStrLen = jwtStr.length() + 1;
  char jwtCharArr[jwtStrLen];
  jwtStr.toCharArray(jwtCharArr, jwtStrLen);

  int jsonStrLen = jwt.getJWTPayloadLength(jwtStr);
  char jsonStr[jsonStrLen];
  jwt.decodeJWT(jwtCharArr, jsonStr, jsonStrLen);
  Serial.println("\nDecoded Payload:");
  Serial.println(jsonStr);

  return jsonStr;
}

bool isAuthorized()
{
  bool isAuthorized = false;
  String message = "";

  if (server.hasHeader("Authorization"))
  {
    String authHeader = {server.header("Authorization")};

    if (authHeader.indexOf("Bearer") >= 0)
    {
      String jwt = authHeader;
      jwt.replace("Bearer ", "");
      String jsonStr = jwtToJsonStr(jwt);

      int jsonStrLen = jsonStr.length() + 1;
      char jsonCharArr[jsonStrLen];
      jsonStr.toCharArray(jsonCharArr, jsonStrLen);

      JsonObject &root = jsonBuffer.parseObject(jsonCharArr);

      if (root.success())
      {
        const char *user = root["user"];
        const char *password = root["password"];

        Serial.println("*****************");
        Serial.println(user);
        Serial.println(password);

        if (strcmp(user, piUser) == 0 && strcmp(password, piPassword) == 0)
        {
          isAuthorized = true;
        }
        else
        {
          message = "Invalid credentials";
          isAuthorized = false;
        }
      }
      else
      {
        message = "Token parsing failed";
        isAuthorized = false;
      }
    }
    else
    {
      message = "No Bearer token found in the request";
      isAuthorized = false;
    }
  }
  else
  {
    message = "No Header found in the request";
  }

  if (isAuthorized)
  {
    return true;
  }
  else
  {
    Serial.println("ERROR: " + message);
    server.send(401,
                "application/json",
                "{\"success\": false, \"message\": \"" + message + "\"}");
    return false;
  }
}

// Define routing
void restServerRouting()
{
  server.on("/", HTTP_GET, welcome);
  server.on(F("/ping"), HTTP_GET, ping);
  //  server.on(F("/on"), HTTP_GET, led_on);
  //  server.on(F("/off"), HTTP_GET, led_off);
}

void welcome()
{
  server.send(200,
              "application/json",
              "{\"success\": true, \"message\": \"Welcome to SWPWS-ESP8266 REST Web Server\"}");
}

void ping()
{
  if (isAuthorized())
  {
    server.send(200, "text/json", "{\"ping\": \"pong\"}");
  }
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
