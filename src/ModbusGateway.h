#ifndef MODBUSGATEWAY_H
#define MODBUSGATEWAY_H

#include <Arduino.h>
#include <Ethernet.h>
#include "ModbusTCPServer.h"
#include "ModbusTCPFrameTemplate.h"
#include "ModbusRTUFrameTemplate.h"
#include "ModbusRTUServer.h"

class ModbusGateway {
public:
    // Konfigurasi semua di main.ino
    ModbusGateway(uint16_t tcpPort,
                  HardwareSerial& rs485Serial,
                  uint8_t dePin,
                  uint32_t baudrate,
                  uint8_t parity,
                  uint16_t rtuTimeout);

    void begin();
    void poll();

private:
    ModbusTCPServer mbServer;
    FrameCollector collector;
    MBAPParser mbap;
    PDUParser pduParser;
    ModbusRTUFrameTemplate::RTURequestParser rtuParser;
    ModbusRTUFrameTemplate::RTUResponseBuilder rtuResponse;
    ModbusRTUServer rtuServer;

    uint8_t frameBuf[260];
    uint8_t rtuBuf[260];
    uint8_t rtuRespBuf[260];
    size_t rtuRespLen = 0;
    volatile bool rtuFrameReady = false;
    uint16_t _rtuTimeout;

    // Static instance untuk callback RTU
    static ModbusGateway* _instance;
    static void rtuFrameCallback(uint8_t* buf, size_t len);

    uint16_t readRegister(uint8_t unitID, uint16_t address);
};

#endif
