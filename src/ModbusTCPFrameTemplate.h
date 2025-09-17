/*
    ModbusTCPFrameTemplate - Handler for collecting, parsing, building Modbus TCP frames
    Standalone: works with raw TCP frames from ModbusTCPServer
    Inspired by Alexander Emelianov's style
*/

#pragma once
#include <Arduino.h>

class FrameCollector {
public:
    FrameCollector() : len(0) {}

    // Tambah byte baru ke buffer
    void appendBytes(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n && len < sizeof(buf); i++) {
            buf[len++] = data[i];
        }
    }

    // Cek apakah frame sudah lengkap (menggunakan MBAP Length field)
    bool isFrameComplete() {
        if (len < 7) return false; // MBAP header minimal 7 byte
        uint16_t mbapLen = (buf[4] << 8) | buf[5];
        return len >= (mbapLen + 6); // MBAP Length = PDU+UnitID, total frame = 6 + mbapLen
    }

    uint8_t* getFrame() { return buf; }
    size_t getLength() { return len; }
    void reset() { len = 0; }

private:
    uint8_t buf[260]; // MBAP(7) + PDU(max 253)
    size_t len;
};

class MBAPParser {
public:
    MBAPParser() : transactionID(0), protocolID(0), length(0), unitID(0) {}

    bool parseHeader(const uint8_t* frame, size_t len) {
        if (len < 7) return false;
        transactionID = (frame[0] << 8) | frame[1];
        protocolID    = (frame[2] << 8) | frame[3];
        length        = (frame[4] << 8) | frame[5];
        unitID        = frame[6];
        return protocolID == 0;
    }

    uint16_t getTransactionID() { return transactionID; }
    uint8_t  getUnitID() { return unitID; }
    uint16_t getLength() { return length; }

private:
    uint16_t transactionID;
    uint16_t protocolID;
    uint16_t length;
    uint8_t  unitID;
};

class PDUParser {
public:
    PDUParser() : funcCode(0), dataLen(0) {}

    void parsePDU(const uint8_t* frame, size_t len) {
        if (len < 2) return; // minimal FunctionCode(1)+data(>=1)
        funcCode = frame[0];
        dataLen = len - 1;
        memcpy(data, &frame[1], dataLen);
    }

    uint8_t getFunctionCode() { return funcCode; }
    const uint8_t* getData() { return data; }
    size_t getDataLength() { return dataLen; }

    // Helper untuk dapat UnitID+PDU (TCP -> RTU bridge)
    size_t buildUnitID_PDU(uint8_t unitID, uint8_t* outBuf) {
        outBuf[0] = unitID;
        outBuf[1] = funcCode;
        memcpy(&outBuf[2], data, dataLen);
        return dataLen + 2; // UnitID + FuncCode + Data
    }

private:
    uint8_t funcCode;
    uint8_t data[253]; // max PDU
    size_t dataLen;
};

class TCPResponseBuilder {
public:
    TCPResponseBuilder() : transactionID(0), unitID(0), pduLen(0) {}

    void setTransactionID(uint16_t id) { transactionID = id; }
    void setUnitID(uint8_t id) { unitID = id; }

    // ========== READ RESPONSES ==========
    void setReadCoilsResponse(const uint8_t* coilStatus, size_t byteCount) {
        pdu[0] = 0x01;  // Function code
        pdu[1] = byteCount;
        memcpy(&pdu[2], coilStatus, byteCount);
        pduLen = byteCount + 2;
    }

    void setReadDiscreteInputsResponse(const uint8_t* inputStatus, size_t byteCount) {
        pdu[0] = 0x02;
        pdu[1] = byteCount;
        memcpy(&pdu[2], inputStatus, byteCount);
        pduLen = byteCount + 2;
    }

    void setReadHoldingRegistersResponse(const uint16_t* regs, size_t qty) {
        pdu[0] = 0x03;
        pdu[1] = qty * 2;
        for (size_t i = 0; i < qty; i++) {
            pdu[2 + i*2]     = regs[i] >> 8;
            pdu[2 + i*2 + 1] = regs[i] & 0xFF;
        }
        pduLen = qty * 2 + 2;
    }

    void setReadInputRegistersResponse(const uint16_t* regs, size_t qty) {
        pdu[0] = 0x04;
        pdu[1] = qty * 2;
        for (size_t i = 0; i < qty; i++) {
            pdu[2 + i*2]     = regs[i] >> 8;
            pdu[2 + i*2 + 1] = regs[i] & 0xFF;
        }
        pduLen = qty * 2 + 2;
    }

    // ========== WRITE RESPONSES ==========
    void setWriteSingleCoilResponse(uint16_t address, bool value) {
        pdu[0] = 0x05;
        pdu[1] = address >> 8;
        pdu[2] = address & 0xFF;
        pdu[3] = value ? 0xFF : 0x00;
        pdu[4] = 0x00;
        pduLen = 5;
    }

    void setWriteSingleRegisterResponse(uint16_t address, uint16_t value) {
        pdu[0] = 0x06;
        pdu[1] = address >> 8;
        pdu[2] = address & 0xFF;
        pdu[3] = value >> 8;
        pdu[4] = value & 0xFF;
        pduLen = 5;
    }

    void setWriteMultipleCoilsResponse(uint16_t startAddr, uint16_t qty) {
        pdu[0] = 0x0F;
        pdu[1] = startAddr >> 8;
        pdu[2] = startAddr & 0xFF;
        pdu[3] = qty >> 8;
        pdu[4] = qty & 0xFF;
        pduLen = 5;
    }

    void setWriteMultipleRegistersResponse(uint16_t startAddr, uint16_t qty) {
        pdu[0] = 0x10;
        pdu[1] = startAddr >> 8;
        pdu[2] = startAddr & 0xFF;
        pdu[3] = qty >> 8;
        pdu[4] = qty & 0xFF;
        pduLen = 5;
    }

    // ========== EXCEPTION RESPONSE ==========
    void setExceptionResponse(uint8_t functionCode, uint8_t exceptionCode) {
        pdu[0] = functionCode | 0x80; // set MSB
        pdu[1] = exceptionCode;
        pduLen = 2;
    }

    // Build final TCP frame
    size_t buildFrame(uint8_t* outBuf) {
        outBuf[0] = transactionID >> 8;
        outBuf[1] = transactionID & 0xFF;
        outBuf[2] = 0; // Protocol ID high
        outBuf[3] = 0; // Protocol ID low
        outBuf[4] = (pduLen + 1) >> 8; // Length high
        outBuf[5] = (pduLen + 1) & 0xFF; // Length low
        outBuf[6] = unitID;
        memcpy(&outBuf[7], pdu, pduLen);
        return pduLen + 7;
    }

    void setRawPDU(const uint8_t* pduData, size_t len) {
        memcpy(pdu, pduData, len);
        pduLen = len;
    }

    // Helper untuk isi dari RTU (UnitID+PDU) -> TCP
    void setUnitID_PDU(const uint8_t* data, size_t len) {
        if (len < 2) return; // minimal UnitID+FuncCode
        unitID = data[0];
        memcpy(pdu, &data[1], len - 1);
        pduLen = len - 1;
    }

private:
    uint16_t transactionID;
    uint8_t unitID;
    uint8_t pdu[253];
    size_t pduLen;
};
