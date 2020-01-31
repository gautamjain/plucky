#include <Arduino.h>

// config: ////////////////////////////////////////////////////////////

/*************************  General Config *******************************/

#define bufferSize 2048
#define DEBUG

// Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "plucky_0.01"


/*************************  UART Config *******************************/

#define UART_BAUD 115200
#define SERIAL_PARAM SERIAL_8N1

#define SERIAL_DE_UART_NUM UART_NUM_1
#define SERIAL_DE_RX_PIN 16
#define SERIAL_DE_TX_PIN 17

#define SERIAL_BLE_UART_NUM UART_NUM_2
#define SERIAL_BLE_RX_PIN 13
#define SERIAL_BLE_TX_PIN 27
#define SERIAL_BLE_CTS_PIN 12
#define SERIAL_BLE_RTS_PIN 33


/*************************  WiFi & TCP Config *******************************/
#define WIFI
#ifdef WIFI

#define TCP
#ifdef TCP
const uint16_t maxTcpClients = 8;
const int tcpPort = 9090;
#endif // TCP
#endif
// WIFI

// / end config ///////////////////////////////////////////////////////////

// Enumerate the hardware serial devices
#include "driver/uart.h"
HardwareSerial & Serial_USB = Serial;
uint8_t readBuf_USB[bufferSize];
uint16_t readBufIndex_USB = 0;

HardwareSerial Serial_DE(SERIAL_DE_UART_NUM);
uint8_t readBuf_DE[bufferSize];
uint16_t readBufIndex_DE = 0;

HardwareSerial Serial_BLE(SERIAL_BLE_UART_NUM);
uint8_t readBuf_BLE[bufferSize];
uint16_t readBufIndex_BLE = 0;

#ifdef WIFI
#include <WiFi.h> 

#ifdef TCP
// Instantiate the TCP sockets for talking to the machine
WiFiServer TCPServer(tcpPort);
WiFiClient TCPClient[maxTcpClients];
uint8_t readBuf_TCP[maxTcpClients][bufferSize];
uint16_t readBufIndex_TCP[maxTcpClients];
#endif // TCP

#endif // WIFI

void setup() {

    /******* Serial initialization ************/
    Serial_USB.begin(UART_BAUD);
    Serial_DE.begin(UART_BAUD, SERIAL_PARAM, SERIAL_DE_RX_PIN, SERIAL_DE_TX_PIN);
    Serial_BLE.begin(UART_BAUD, SERIAL_PARAM, SERIAL_BLE_RX_PIN, SERIAL_BLE_TX_PIN);
    // HW flow contol for the BLE adaptor.
    // https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/driver/driver/uart.h
    uart_set_hw_flow_ctrl(SERIAL_BLE_UART_NUM, UART_HW_FLOWCTRL_CTS_RTS, 0);
    uart_set_pin(SERIAL_BLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, SERIAL_BLE_RTS_PIN, SERIAL_BLE_CTS_PIN);

    /******* Wifi initialization ***********/
    #ifdef WIFI
    Serial.println();
    Serial.println();
    Serial.print("Connecting to wifi");
    WiFi.begin("myssid", "wpa2passwd");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    #ifdef TCP
    TCPServer.begin(); // start TCP server
    TCPServer.setNoDelay(true);
    Serial.println("TCP server enabled");
    #endif // TCP
    #endif // WIFI

    delay(10);
    Serial_USB.println("Plucky initialization completed.");
}

void loop() {

#ifdef TCP
  // Check for new incoming connections
  if (TCPServer.hasClient()) { // find free/disconnected spot
      for (byte i = 0; i < maxTcpClients; i ++) {
        if (!TCPClient[i]) {
          TCPClient [i] = TCPServer.available();
          readBufIndex_TCP[i] = 0;
          Serial_USB.print("New TCP client in slot: ");
          Serial_USB.println(i);
          break;
        }
        if (i == maxTcpClients - 1) { // no free/disconnected spot so reject it
            WiFiClient TmpserverClient = TCPServer.available();
            TmpserverClient.stop();
            Serial_USB.println("Too many TCP clients; new connection dropped");
        }
      }
  }     
#endif // TCP

  // Receive DE1 messages
  if (Serial_DE.available()) {
    while (Serial_DE.available() && (readBufIndex_DE < bufferSize)) {
      readBuf_DE[readBufIndex_DE] = Serial_DE.read();
      readBufIndex_DE ++;
      if (readBuf_DE[readBufIndex_DE - 1] == '\n') { 
        // let's be super deliberate about resetting the readBufIndex lest we forget to do it below
        uint16_t sendLen = readBufIndex_DE;
        readBufIndex_DE = 0;

        // Broadcast to Serial interfaces
        Serial_BLE.write(readBuf_DE, sendLen);
        Serial_USB.write(readBuf_DE, sendLen);

#ifdef TCP
        // Broadcast to TCP clients
        for (uint16_t i = 0; i < maxTcpClients; i ++) {
          if (TCPClient[i]) {
              TCPClient[i].write(readBuf_DE, sendLen);
          }
        }
#endif // TCP

      }
    }
  } 
  if (readBufIndex_DE >= bufferSize) {
      Serial_USB.printf("WARNING: DE Read Buffer Overrun, purging.  Buffer Contents: ");
      Serial_USB.write(readBuf_DE, bufferSize);
      Serial_USB.printf("\n");
      readBufIndex_DE = 0;
  }

  // Bridge BLE messages to DE1
  if (Serial_BLE.available()) {
    while (Serial_BLE.available() && (readBufIndex_BLE < bufferSize)) {
      readBuf_BLE[readBufIndex_BLE] = Serial_BLE.read();
      readBufIndex_BLE ++;
      if (readBuf_BLE[readBufIndex_BLE - 1] == '\n') { 
        // let's be super deliberate about resetting the readBufIndex lest we forget to do it below
        uint16_t sendLen = readBufIndex_BLE;
        readBufIndex_BLE = 0;

        // Send to DE
        Serial_DE.write(readBuf_BLE, sendLen);
      }
    }
  }
  if (readBufIndex_BLE >= bufferSize) {
    Serial_USB.printf("WARNING: BLE Read Buffer Overrun.  Buffer Contents: ");
    Serial_USB.write(readBuf_BLE, bufferSize);
    Serial_USB.printf("\n");
    readBufIndex_BLE = 0;
  }

  // Bridge USB messages to DE1
  if (Serial_USB.available()) {
    while (Serial_USB.available() && (readBufIndex_USB < bufferSize)) {
      readBuf_USB[readBufIndex_USB] = Serial_USB.read();
      readBufIndex_USB ++;
      if (readBuf_USB[readBufIndex_USB - 1] == '\n') { 
        // let's be super deliberate about resetting the readBufIndex lest we forget to do it below
        uint16_t sendLen = readBufIndex_USB;
        readBufIndex_USB = 0;

        // Send to DE
        Serial_DE.write(readBuf_USB, sendLen);
      }
    }
  }
  if (readBufIndex_USB >= bufferSize) {
    Serial_USB.printf("WARNING: BLE Read Buffer Overrun.  Buffer Contents: ");
    Serial_USB.write(readBuf_USB, bufferSize);
    Serial_USB.printf("\n");
    readBufIndex_USB = 0;
  }


#ifdef TCP
  // Bridge TCP messages to DE1
  for (byte i = 0; i < maxTcpClients; i ++) {
    if (TCPClient[i] && TCPClient[i].available()) {
      while (TCPClient[i].available() && (readBufIndex_TCP[i] < bufferSize)) {
        readBuf_TCP[i][readBufIndex_TCP[i]] = TCPClient[i].read();
        readBufIndex_TCP[i]++;
        if (readBuf_TCP[i][readBufIndex_TCP[i] - 1] == '\n') { 
          // let's be super deliberate about resetting the readBufIndex lest we forget to do it below
          uint16_t sendLen = readBufIndex_TCP[i];
          readBufIndex_TCP[i] = 0;

          // Send to DE
          Serial_DE.write(readBuf_USB, sendLen);
        }
      }
    }
    if (readBufIndex_USB >= bufferSize) {
      Serial_USB.printf("WARNING: BLE Read Buffer Overrun.  Buffer Contents: ");
      Serial_USB.write(readBuf_USB, bufferSize);
      Serial_USB.printf("\n");
      readBufIndex_TCP[i] = 0;
    } 
  } 
#endif // WIFI}}}#endif // WIFI#endif // 0
}