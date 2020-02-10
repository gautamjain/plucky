#ifndef _PLUCKY_INTERFACE_SERIAL_HPP_
#define _PLUCKY_INTERFACE_SERIAL_HPP_

#include <HardwareSerial.h>
#include <ArduinoSimpleLogging.h>

#include "PluckyInterface.hpp"
#include "config.hpp"

#define UART_BAUD 115200
#define SERIAL_PARAM SERIAL_8N1

#define  SERIAL_USB_UART_NUM UART_NUM_0

#define SERIAL_DE_UART_NUM UART_NUM_1
#define SERIAL_DE_RX_PIN 16
#define SERIAL_DE_TX_PIN 17

#define SERIAL_BLE_UART_NUM UART_NUM_2
#define SERIAL_BLE_RX_PIN 13  // bummer, mk3b uses the LED pin (13) for RX.  This is because of Feather M0 compatibility.
#define SERIAL_BLE_TX_PIN 27
// CTS/RTS pins are not used (at all) if BLE flow control is off
#define SERIAL_BLE_CTS_PIN 12
#define SERIAL_BLE_RTS_PIN 33

class PluckyInterfaceSerial : public PluckyInterface {
public:
  PluckyInterfaceSerial(int uart_nr);
  void doInit();
  void doLoop();

  void begin() = 0;
  void end() = 0;
  bool available() = 0;
  bool readAll() = 0;
  bool availableForWrite(size_t len=0) = 0;
  bool writeAll(const uint8_t *buf, size_t size) = 0;

private:
  HardwareSerial *_serial;
  uint8_t _readBuf[READ_BUFFER_SIZE];
  uint16_t _readBufIndex;
  int _uart_nr;
  char _interfaceName[32];

};


#endif // _PLUCKY_INTERFACE_HPP_