#include <Arduino.h>

/*************************  General Config ***********************/

const uint bufferSize = 2048;
#define DEBUG

/*************************  UART Config *******************************/
#include "driver/uart.h"
const int UART_BAUD = 115200;
const long SERIAL_PARAM = SERIAL_8N1;

const uart_port_t SERIAL_DE_UART_NUM = UART_NUM_1;
const uint SERIAL_DE_RX_PIN = 16;
const uint SERIAL_DE_TX_PIN = 17;

const uart_port_t SERIAL_BLE_UART_NUM = UART_NUM_2;
const uint SERIAL_BLE_RX_PIN = 13;
const uint SERIAL_BLE_TX_PIN = 27;
const uint SERIAL_BLE_CTS_PIN = 12;
const uint SERIAL_BLE_RTS_PIN = 33;

/****************  WiFi & WebServer Config *************************/
#define WEBSERVER

/***^^^^^^^^^^^^^^^^^^^^^  END OF CONFIGURATIONS ^^^^^^^^^^^^^^^^^^^^^***/

/***** SERIAL INSTANTIATION *****/
HardwareSerial &Serial_USB = Serial;
HardwareSerial Serial_DE(SERIAL_DE_UART_NUM);
HardwareSerial Serial_BLE(SERIAL_BLE_UART_NUM);

/***** WEBSERVER INSTANTIATION *****/
#ifdef WEBSERVER

#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "Update.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");            // access at ws://[esp ip]/ws
AsyncEventSource events("/events");  // event source (Server-Sent events)

const char *ssid = "myssid";
const char *password = "wpa2passwd";
const char *http_username = "admin";
const char *http_password = "admin";

// flag to use from web update to reboot the ESP
bool shouldReboot = false;

void onRequest(AsyncWebServerRequest *request) {
  // Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
            size_t index, size_t total) {
  // Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index,
              uint8_t *data, size_t len, bool final) {
  // Handle upload
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  // Handle WebSocket event
}
#endif  // ifdef WEBSERVER

void setup() {
  delay(100);

  /***** SERIAL INITIALIZATION *****/
  Serial_USB.begin(UART_BAUD);
  Serial_DE.begin(UART_BAUD, SERIAL_PARAM, SERIAL_DE_RX_PIN, SERIAL_DE_TX_PIN);
  Serial_BLE.begin(UART_BAUD, SERIAL_PARAM, SERIAL_BLE_RX_PIN,
                   SERIAL_BLE_TX_PIN);

  // Enabling HW flow control on BLE UART
  // https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/uart.h
  uart_set_hw_flow_ctrl(SERIAL_BLE_UART_NUM, UART_HW_FLOWCTRL_CTS_RTS, 0);
  uart_set_pin(SERIAL_BLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
               SERIAL_BLE_RTS_PIN, SERIAL_BLE_CTS_PIN);

  /***** WEBSERVER INITIALIZATION ***********/
#ifdef WEBSERVER
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // attach AsyncEventSource
  server.addHandler(&events);

  // // respond to GET requests on URL /heap
  // server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
  //   request->send(200, "text/plain", String(ESP.getFreeHeap()));
  // });

  // // upload a file to /upload
  // server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
  //   request->send(200);
  // }, onUpload);

  // // send a file when /index is requested
  // server.on("/index", HTTP_ANY, [](AsyncWebServerRequest *request){
  //   request->send(SPIFFS, "/index.htm");
  // });

  // HTTP basic authentication
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/plain", "Login Success!");
  });
  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
                  "<form method='POST' action='/update' "
                  "enctype='multipart/form-data'><input type='file' "
                  "name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/update", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              shouldReboot = !Update.hasError();
              AsyncWebServerResponse *response = request->beginResponse(
                  200, "text/plain", shouldReboot ? "OK" : "FAIL");
              response->addHeader("Connection", "close");
              request->send(response);
            },
            [](AsyncWebServerRequest *request, String filename, size_t index,
               uint8_t *data, size_t len, bool final) {
              if (!index) {
                Serial.printf("Update Start: %s\n", filename.c_str());
                if (!Update.begin()) {
                }
              }
              if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                  Update.printError(Serial);
                }
              }
              if (final) {
                if (Update.end(true)) {
                  Serial.printf("Update Success: %uB\n", index + len);
                } else {
                  Update.printError(Serial);
                }
              }
            });

  // // attach filesystem root at URL /fs
  // server.serveStatic("/fs", SPIFFS, "/");

  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();
#endif  // WEBSERVER

  delay(10);
  Serial_USB.println("Plucky initialization completed.");
}

void loop() {
  uint8_t buf[bufferSize];
  uint16_t bufIndex = 0;

#ifdef WEBSERVER
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }

  static char temp[128];
  sprintf(temp, "Seconds since boot: %lu", millis() / 1000);
  events.send(temp, "time");  // send event "time"

#endif  // WEBSERVER

  // Broadcast DE1 messages to controllers
  if (Serial_DE.available()) {
    bufIndex = 0;
    while (Serial_DE.available() && (bufIndex < (bufferSize - 1))) {
      buf[bufIndex] = Serial_DE.read();
      bufIndex++;
      if (buf[bufIndex - 1] == '\n') {
        // we don't want to get into a state where buffers misalign with
        // the message frames
        break;
      }
    }
    // Send to serial interfaces
    Serial_BLE.write(buf, bufIndex);
    Serial_USB.write(buf, bufIndex);
  }

  // Bridge BLE messages to DE1
  if (Serial_BLE.available()) {
    bufIndex = 0;
    while (Serial_BLE.available() && (bufIndex < (bufferSize - 1))) {
      buf[bufIndex] = Serial_BLE.read();
      bufIndex++;
      if (buf[bufIndex - 1] == '\n') {
        break;  // we don't want to get into a state where buffers misalign
                // with the message frames
      }
    }
    // Send to DE
    Serial_DE.write(buf, bufIndex);
  }

  // Bridge USB messages to DE1
  if (Serial_USB.available()) {
    bufIndex = 0;
    while (Serial_USB.available() && (bufIndex < (bufferSize - 1))) {
      buf[bufIndex] = Serial_USB.read();
      bufIndex++;
      if (buf[bufIndex - 1] == '\n') {
        break;  // we don't want to get into a state where buffers misalign
                // with the message frames
      }
    }
    // Send to DE
    Serial_DE.write(buf, bufIndex);
    Serial_DE.write(buf, bufIndex);
    // Send to DE
    Serial_DE.write(buf, bufIndex);
    Serial_DE.write(buf, bufIndex);
  }
}