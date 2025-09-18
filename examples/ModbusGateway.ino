#include <SPI.h>
#include <Ethernet.h>
#include "ModbusGateway.h"

// ===== Konfigurasi Ethernet =====
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip_lan(192, 168, 1, 10);
IPAddress gateway_lan(192, 168, 1, 1);
IPAddress subnet_lan(255, 255, 255, 0);

// ===== RS-485 / Modbus RTU =====
#define RS485_RX 16    
#define RS485_TX 17   
#define RS485_DE 4

uint32_t rtuBaudrate = 9600;
uint8_t rtuParity = SERIAL_8N1;  // opsi: SERIAL_8N1, SERIAL_8E1, SERIAL_8O1
uint16_t rtuTimeout = 100;       // ms

// ===== Modbus TCP port =====
uint16_t tcpPort = 502;

// ===== Instance handler =====
ModbusGateway gateway(tcpPort, Serial2, RS485_DE, rtuBaudrate, rtuParity, rtuTimeout);

// ===== FreeRTOS Task Handle =====
TaskHandle_t taskModbusHandle;

// ===== Task Modbus =====
void taskModbus(void* pvParameters) {
  (void) pvParameters;
  for (;;) {
    gateway.poll();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    SPI.begin(18, 19, 23);  // Pin SPI W5500
    Ethernet.init(5);       // W5500 CS
    Ethernet.begin(mac, ip_lan, gateway_lan, subnet_lan);

    Serial.print("Server IP: ");
    Serial.println(Ethernet.localIP());      

    // Mulai handler ModbusGateway
    gateway.begin();
    Serial.println("Modbus Gateway started");

     // FreeRTOS Task
    xTaskCreatePinnedToCore(taskModbus, "Task Modbus", 4096, NULL, 1, &taskModbusHandle, 1);
}

void loop() {

}
