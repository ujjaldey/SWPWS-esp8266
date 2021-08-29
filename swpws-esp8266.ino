/*
   swpws-esp8266.ino
*/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoJWT.h>
#include <ESP8266WebServer.h>
#include <sha256.h>

// Set the WIFI SSID and password
// Replace with your SSID and password
const char* ssid = "<your wifi ssid>";
const char* password = "<your wifi password>";

const char* piUser = "pi";
const char* piPassword = "raspberry";

// Set Static IP address, Gateway, and Subnet
// Change if necessary
IPAddress staticIP(192, 168, 10, 205);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

ArduinoJWT jwt = ArduinoJWT("secret");

StaticJsonBuffer<200> jsonBuffer;

unsigned int httpStatusCodeSuccess = 200;
unsigned int httpStatusCodeError = 400;
unsigned int httpStatusCodeUnauthorized = 401;

void setupWifi() {
  // Set Wifi mode and IP addresses
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(staticIP, gateway, subnet)) {
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
  while (WiFi.status() != WL_CONNECTED) {
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

void setupServer() {
  // Set server routing
  restServerRouting();

  // Set 404 (not found) response
  server.onNotFound(handleNotFound);

  // Start server
  server.begin();

  Serial.println("HTTP server started");
  Serial.println("");
}

String jwtToJsonStr(String jwtStr) {
  int jwtStrLen = jwtStr.length() + 1;
  char jwtCharArr[jwtStrLen];
  jwtStr.toCharArray(jwtCharArr, jwtStrLen);

  int jsonStrLen = jwt.getJWTPayloadLength(jwtStr);
  char jsonStr[jsonStrLen];
  jwt.decodeJWT(jwtCharArr, jsonStr, jsonStrLen);

  return jsonStr;
}

bool validateCredential(const char* user, const char* password) {
  return strcmp(user, piUser) == 0 && strcmp(password, piPassword) == 0;
}

bool isAuthorized() {
  bool isAuthorized = false;
  String message = "";

  if (server.hasHeader("Authorization")) {
    String authHeader = {server.header("Authorization")};

    if (authHeader.indexOf("Bearer") >= 0) {
      String jwt = authHeader;
      jwt.replace("Bearer ", "");
      String jsonStr = jwtToJsonStr(jwt);

      int jsonStrLen = jsonStr.length() + 1;
      char jsonCharArr[jsonStrLen];
      jsonStr.toCharArray(jsonCharArr, jsonStrLen);

      JsonObject& root = jsonBuffer.parseObject(jsonCharArr);

      if (root.success()) {
        const char* user = root["user"];
        const char* password = root["password"];

        if (validateCredential(user, password)) {
          isAuthorized = true;
        } else {
          message = "Invalid credentials";
          isAuthorized = false;
        }
      } else {
        message = "Token parsing failed";
        isAuthorized = false;
      }
    } else {
      message = "No Bearer token found in the request";
      isAuthorized = false;
    }
  } else {
    message = "No Header found in the request";
  }

  if (isAuthorized) {
    return true;
  } else {
    Serial.println("ERROR: " + message);
    server.send(httpStatusCodeUnauthorized, "application/json", "{\"success\": false, \"message\": \"" + message + "\"}");
    return false;
  }
}

// Define routing
void restServerRouting() {
  server.on("/", HTTP_GET, welcome);
  server.on("/status", HTTP_POST, action_status);
  server.on("/on", HTTP_GET, action_on);
  server.on("/off", HTTP_GET, action_off);
}

void welcome() {
  server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Welcome to SWPWS-ESP8266 REST Web Server\"}");
}

void action_status() {
  bool isValid = false;
  String message = "";
  int returnCode = 0;
  String jwtStr = "";

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));

  if (root.success()) {
    const char* user = root["user"];
    const char* password = root["password"];

    if (validateCredential(user, password)) {
      jwtStr = generateJwt(user, password);
      isValid = true;
      returnCode  = httpStatusCodeSuccess;
    } else {
      message = "Invalid credentials";
      isValid = false;
      returnCode  = httpStatusCodeUnauthorized;
    }
  } else {
    message = "Token parsing failed";
    isValid = false;
    returnCode  = httpStatusCodeError;
  }

  if (isValid) {
    server.send(returnCode, "application/json", "{\"success\": true, \"jwt\": \"" + jwtStr + "\"}");
  } else {
    Serial.println("ERROR: " + message);
    server.send(returnCode, "application/json", "{\"success\": false, \"message\": \"" + message + "\"}");
  }
}

void action_on() {
  String message = "";

  if (isAuthorized()) {
    server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Application is turned on\"}");
  }
}

void action_off() {
  String message = "";

  if (isAuthorized()) {
    server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Application is turned off\"}");
  }
}

String generateJwt(const char* user, const char* password) {
  String userStr(user);
  String passwordStr(password);

  String inputStr = "{\"user\":\"" + userStr + "\",\"password\":\"" + passwordStr + "\"}";
  int inputStrLen = inputStr.length() + 1;
  char inputCharArr[inputStrLen];
  inputStr.toCharArray(inputCharArr, inputStrLen);

  int input1Len = jwt.getJWTLength(inputCharArr);
  char output[input1Len];
  jwt.encodeJWT(inputCharArr, output);

  String outputStr(output);
  return outputStr;
}

void handleNotFound() {

}

void setup() {
  Serial.begin(115200);

  setupWifi();
  setupServer();
}

void loop() {
  server.handleClient();
}