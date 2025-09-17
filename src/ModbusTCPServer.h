/*
    ModbusTCPServer - Raw Modbus TCP Server handler
    Inspired by ModbusEthernet (by Alexander Emelianov)
    Standalone: no ModbusAPI / ModbusTCPTemplate binding
*/

#pragma once

#if defined(ARDUINO_PORTENTA_H7_M4) || defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_PORTENTA_X8)
#define TCP_WRAP_ACCEPT
#undef TCP_WRAP_BEGIN
#elif defined(ESP32)
#undef TCP_WRAP_ACCEPT
#define TCP_WRAP_BEGIN
#else
#undef TCP_WRAP_ACCEPT
#undef TCP_WRAP_BEGIN
#endif

// EthernetServer wrapper mirip aslinya
class EthernetServerWrapper : public EthernetServer {
  public:
    EthernetServerWrapper(uint16_t port) : EthernetServer(port) {}

#if defined(TCP_WRAP_BEGIN)
    void begin(uint16_t port=0) {
        EthernetServer::begin();
    }
#endif
#if defined(TCP_WRAP_ACCEPT)
    inline EthernetClient accept() {
        return available();
    }
#endif
};

// Handler baru untuk raw Modbus TCP
class ModbusTCPServer {
  public:
    ModbusTCPServer(uint16_t port = 502) : server(port) {}

    void begin() {
        server.begin();
    }

    // Cek client baru
    EthernetClient accept() {
        return server.available();
    }

    // Baca frame TCP (MBAP + PDU)
    size_t readFrame(EthernetClient& client, uint8_t* buf, size_t maxlen) {
        if (!client || !client.available()) return 0;
        size_t len = 0;
        while (client.available() && len < maxlen) {
            buf[len++] = client.read();
        }
        return len;
    }

    // Kirim frame TCP (MBAP + PDU)
    void sendFrame(EthernetClient& client, const uint8_t* buf, size_t len) {
        if (!client) return;
        client.write(buf, len);
    }

  private:
    EthernetServerWrapper server;
};
