#pragma once
#include <Arduino.h>

class ModbusRTUServer {
public:
    // Constructor minimal: serial + DE pin
    ModbusRTUServer(HardwareSerial &serial, uint8_t dePin)
        : _serial(&serial), _dePin(dePin) {}

    // Begin dengan baudrate dan config (paritas + 8 data + 1 stop)
    void begin(uint32_t baud, uint16_t config = SERIAL_8N1) {
        pinMode(_dePin, OUTPUT);
        _enableDriver(false);   // mode receive
        _baud = baud;
        _serial->begin(_baud, config);
    }

    // Set timeout antar-byte (ms)
    void setByteTimeout(unsigned long timeoutMs) { _byteTimeout = timeoutMs; }

    // Set callback frame
    void onFrameReceived(void (*callback)(uint8_t* buffer, size_t length)) {
        _frameCallback = callback;
    }

    // Polling harus dipanggil di loop()
    void poll() {
        while (_serial->available()) {
            if (_bufferIndex < BUFFER_SIZE) {
                _buffer[_bufferIndex++] = _serial->read();
                _lastByteTime = millis();
            } else {
                // Buffer overflow, reset
                _bufferIndex = 0;
                _lastByteTime = millis();
            }
        }

        // Timeout antar-byte untuk deteksi akhir frame
        if (_bufferIndex > 0 && (millis() - _lastByteTime > _byteTimeout)) {
            // Kirim callback dengan panjang frame yang benar
            if (_frameCallback) _frameCallback(_buffer, _bufferIndex);
            _bufferIndex = 0; // reset buffer
        }
    }

    // Kirim frame
    void sendFrame(const uint8_t* buf, size_t len) {
        if (len == 0 || buf == nullptr) return;
        _enableDriver(true);
        _serial->write(buf, len);
        _serial->flush();
        _enableDriver(false);
    }

private:
    HardwareSerial* _serial;
    uint8_t _dePin;
    uint32_t _baud;

    static const size_t BUFFER_SIZE = 256;
    uint8_t _buffer[BUFFER_SIZE];
    size_t _bufferIndex = 0;
    unsigned long _lastByteTime = 0;
    unsigned long _byteTimeout = 5; // default ms

    void (*_frameCallback)(uint8_t* buffer, size_t length) = nullptr;

    void _enableDriver(bool enable) { digitalWrite(_dePin, enable ? HIGH : LOW); }
};
