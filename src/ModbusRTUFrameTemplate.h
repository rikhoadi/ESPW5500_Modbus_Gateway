#ifndef MODBUS_RTU_FRAME_TEMPLATE_H
#define MODBUS_RTU_FRAME_TEMPLATE_H

#include <Arduino.h>

class ModbusRTUFrameTemplate {
public:
    // === Struktur RTU ===
    struct Request {
        uint8_t unitID;
        uint8_t pdu[253];   // Max PDU length Modbus RTU
        size_t pduLen;

        Request() : unitID(0), pduLen(0) {}
    };

    struct Response {
        uint8_t unitID;
        uint8_t pdu[253];
        size_t pduLen;

        Response() : unitID(0), pduLen(0) {}
    };

    // === Builder untuk respon ===
    class RTUResponseBuilder {
    public:
        RTUResponseBuilder() : unitID(0), pduLen(0) {}

        void setUnitID(uint8_t id) { unitID = id; }
        void setRawPDU(const uint8_t* data, size_t len) {
            pduLen = len;
            memcpy(pdu, data, len);
        }

        size_t buildFrame(uint8_t* buffer) {
            size_t index = 0;
            buffer[index++] = unitID;
            memcpy(&buffer[index], pdu, pduLen);
            index += pduLen;

            uint16_t crc = calcCRC(buffer, index);
            buffer[index++] = crc & 0xFF;        // CRC Low
            buffer[index++] = (crc >> 8) & 0xFF; // CRC High

            return index;
        }

    private:
        uint8_t unitID;
        uint8_t pdu[253];
        size_t pduLen;

        uint16_t calcCRC(const uint8_t* data, size_t len) {
            uint16_t crc = 0xFFFF;
            for (size_t i = 0; i < len; i++) {
                crc ^= data[i];
                for (uint8_t j = 0; j < 8; j++) {
                    if (crc & 0x0001)
                        crc = (crc >> 1) ^ 0xA001;
                    else
                        crc = crc >> 1;
                }
            }
            return crc;
        }
    };

    // === Parser untuk request ===
    class RTURequestParser {
    public:
        bool parseFrame(const uint8_t* buffer, size_t length, Request& req) {
            if (length < 4) return false; // min frame

            uint16_t receivedCRC = buffer[length - 2] | (buffer[length - 1] << 8);
            uint16_t calc = calcCRC(buffer, length - 2);
            if (receivedCRC != calc) return false;

            req.unitID = buffer[0];
            req.pduLen = length - 3; // tanpa UnitID & CRC
            memcpy(req.pdu, &buffer[1], req.pduLen);

            return true;
        }

    private:
        uint16_t calcCRC(const uint8_t* data, size_t len) {
            uint16_t crc = 0xFFFF;
            for (size_t i = 0; i < len; i++) {
                crc ^= data[i];
                for (uint8_t j = 0; j < 8; j++) {
                    if (crc & 0x0001)
                        crc = (crc >> 1) ^ 0xA001;
                    else
                        crc = crc >> 1;
                }
            }
            return crc;
        }
    };
};

#endif
