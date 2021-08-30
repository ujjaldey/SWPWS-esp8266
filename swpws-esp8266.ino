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
const IPAddress staticIP(192, 168, 10, 205);
const IPAddress gateway(192, 168, 10, 1);
const IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

ArduinoJWT jwt = ArduinoJWT("secret");

const unsigned int httpStatusCodeSuccess = 200;
const unsigned int httpStatusCodeNotModified = 304;
const unsigned int httpStatusCodeError = 400;
const unsigned int httpStatusCodeUnauthorized = 401;

const int outputPin = 16;
bool outputPinIsOn = false;

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

      StaticJsonBuffer<200> jsonBuffer;
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
  server.on("/", HTTP_GET, action_welcome);
  server.on("/token", HTTP_POST, action_token);
  server.on("/on", HTTP_POST, action_on);
  server.on("/off", HTTP_POST, action_off);
  server.on("/status", HTTP_POST, action_status);
}

void action_welcome() {
  Serial.println("Endpoint triggered: /");

  server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Welcome to SWPWS-ESP8266 REST Web Server\"}");
}

void action_token() {
  Serial.println("Endpoint triggered: /token");

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
    server.send(returnCode, "application/json", "{\"success\": true, \"jwt\": \"" + jwtStr + "\", \"status\": null}");
  } else {
    Serial.println("ERROR: " + message);
    server.send(returnCode, "application/json", "{\"success\": false, \"message\": \"" + message + "\", \"status\": null}");
  }
}

void action_on() {
  Serial.println("Endpoint triggered: / on");

  String message = "";

  if (isAuthorized()) {
    if (!outputPinIsOn) {
      digitalWrite(outputPin, LOW);
      outputPinIsOn = !outputPinIsOn;
      server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Application is turned on\", \"status\": null}");
    } else {
      server.send(httpStatusCodeNotModified, "application/json", "{\"success\": true, \"message\": \"Application is already turned on\", \"status\": null}");
    }
  }
}

void action_off() {
  Serial.println("Endpoint triggered: /off");

  String message = "";

  if (isAuthorized()) {
    if (outputPinIsOn) {
      digitalWrite(outputPin, HIGH);
      outputPinIsOn = !outputPinIsOn;
      server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Application is turned off\", \"status\": null}");
    } else {
      server.send(httpStatusCodeNotModified, "application/json", "{\"success\": true, \"message\": \"Application is already turned off\", \"status\": null}");
    }
  }
}

void action_status() {
  Serial.println("Endpoint triggered: /status");

  if (isAuthorized()) {
    String onOff = outputPinIsOn ? "on" : "off";
    String status = outputPinIsOn ? "true" : "false";
    server.send(httpStatusCodeSuccess, "application/json", "{\"success\": true, \"message\": \"Application is turned " + onOff + "\", \"status\": " + status + "}");
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

  // Initialize the output variables as outputs
  pinMode(outputPin, OUTPUT);
  // Set outputs to LOW
  digitalWrite(outputPin, HIGH);
}

void loop() {
  server.handleClient();
}