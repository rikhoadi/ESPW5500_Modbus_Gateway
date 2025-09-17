#include "ModbusGateway.h"

// Inisialisasi pointer static
ModbusGateway* ModbusGateway::_instance = nullptr;

// Konstruktor
ModbusGateway::ModbusGateway(uint16_t tcpPort,
                             HardwareSerial& rs485Serial,
                             uint8_t dePin,
                             uint32_t baudrate,
                             uint8_t parity,
                             uint16_t rtuTimeout)
    : mbServer(tcpPort),
      rtuServer(rs485Serial, dePin),
      _rtuTimeout(rtuTimeout)
{
    _instance = this; // Simpan pointer instance saat ini

    // Inisialisasi RS-485
    rtuServer.begin(baudrate, parity);

    // Pasang callback static
    rtuServer.onFrameReceived(ModbusGateway::rtuFrameCallback);
}

// Callback RTU static
void ModbusGateway::rtuFrameCallback(uint8_t* buf, size_t len) {
    if (_instance && len >= 4) {
        memcpy(_instance->rtuRespBuf, buf, len);
        _instance->rtuRespLen = len;
        _instance->rtuFrameReady = true;
    }
}

// Mulai TCP server
void ModbusGateway::begin() {
    mbServer.begin();
}

// Poll loop utama
void ModbusGateway::poll() {
    yield();

    // Poll RTU server
    rtuServer.poll();

    // Terima TCP request
    EthernetClient client = mbServer.accept();
    if (!client) return;

    size_t n = mbServer.readFrame(client, frameBuf, sizeof(frameBuf));
    if (n == 0) return;

    collector.appendBytes(frameBuf, n);
    if (!collector.isFrameComplete()) return;

    uint8_t* tcpFrame = collector.getFrame();
    size_t tcpFrameLen = collector.getLength();

    if (!mbap.parseHeader(tcpFrame, tcpFrameLen)) {
        //Serial.println("Invalid MBAP header, discard frame");
        collector.reset();
        return;
    }

    uint8_t* pduStart = tcpFrame + 7;
    size_t pduLen = tcpFrameLen - 7;
    pduParser.parsePDU(pduStart, pduLen);

    // Build RTU request
    rtuResponse.setUnitID(mbap.getUnitID());
    rtuResponse.setRawPDU(pduStart, pduLen);
    size_t rtuReqLen = rtuResponse.buildFrame(rtuBuf);

    // Kirim RTU request via RS-485
    rtuFrameReady = false;
    rtuRespLen = 0;
    rtuServer.sendFrame(rtuBuf, rtuReqLen);

    // Tunggu RTU response (timeout _rtuTimeout)
    unsigned long start = millis();
    while (millis() - start < _rtuTimeout) {
        rtuServer.poll();
        if (rtuFrameReady) break;
    }

    if (!rtuFrameReady) {
        //Serial.println("Timeout / no RTU response");
        collector.reset();
        return;
    }

    // Parsing RTU response
    ModbusRTUFrameTemplate::Request req;
    if (!rtuParser.parseFrame(rtuRespBuf, rtuRespLen, req)) {
        //Serial.println("RTU Response CRC invalid!");
        collector.reset();
        return;
    }

    // Build TCP response
    TCPResponseBuilder tcpResp;
    tcpResp.setTransactionID(mbap.getTransactionID());
    tcpResp.setUnitID(req.unitID);
    tcpResp.setUnitID_PDU(rtuRespBuf, rtuRespLen);

    size_t tcpRespLen = tcpResp.buildFrame(frameBuf);
    mbServer.sendFrame(client, frameBuf, tcpRespLen);

    collector.reset();
}

// Dummy readRegister, bisa di-override atau dimodifikasi di handler jika perlu
uint16_t ModbusGateway::readRegister(uint8_t unitID, uint16_t address) {
    return address;
}
