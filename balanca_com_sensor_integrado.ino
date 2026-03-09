#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "HX711.h"
#include <ThingSpeak.h>
#include <WiFi.h>
#include "sd_read_write.h"
#include "HardwareSerial.h"

// =====================
// Dados da rede Wi-Fi
// =====================
const char ssid[] = "TP-Link_2536";
const char password[] = "V58VNaZ7";

// =====================
// Configurações ThingSpeak
// =====================
WiFiClient client;
const long CHANNEL = 3042773;
const char *WRITE_API = "TINUHYUAB8DTO9Z5";

// =====================
// Display OLED
// =====================
static SSD1306Wire display(0x3C, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// =====================
// SPI (SD Card)
// =====================
SPIClass sd_spi(HSPI);

// =====================
// UART do sensor RS485
// =====================
#define RX_SENSOR 33
#define TX_SENSOR 34
#define RE 48   // pino ligado ao DE e RE do módulo RS485
#define bussControl 45
#define sensorControl 47

HardwareSerial mod(1);  // UART1 para o sensor

// =====================
// HX711
// =====================
#define HX_DT  6  // pino DT (DOUT)
#define HX_SCK 7  // pino SCK (CLK)
HX711 balanca;

// =====================
// Variáveis globais
// =====================
float fatorCalibracao = -200792.187;
long prevMillisThingSpeak = 0;
int intervalThingSpeak = 15000;
volatile uint32_t contadorLeituras = 0;

// Protótipos
void sdInit(void);
void gravaDados(float peso, float temp, float umid, float cond);
void readSensor(float *temperatura, float *umidade, float *condutividade);

//=====================
// Energia externa
// =====================
void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

// =====================
// SETUP
// =====================
void setup() {
  VextON();
  delay(200);

  Serial.begin(115200);
  Serial.println("\nIniciando sistema com balança + sensor RS485");

  // Inicializa UART do sensor
  mod.begin(4800, SERIAL_8N1, RX_SENSOR, TX_SENSOR);
  pinMode(RE, OUTPUT);
  digitalWrite(RE, LOW); // modo recepção

  pinMode(bussControl, OUTPUT);
  pinMode(sensorControl, OUTPUT);

  digitalWrite(bussControl, LOW);
  digitalWrite(sensorControl, LOW);


  // Inicializa SD
  sdInit();

  File file = SD.open("/data.txt");
  if (!file) {
    Serial.println("Criando arquivo no SD...");
    writeFile(SD, "/data.txt", "Contador, Peso(kg), Temp(C), Umid(%), Cond(uS/cm), Timestamp(ms)\r\n");
  } else {
    Serial.println("Arquivo já existe.");
  }
  file.close();

  // Conecta Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(1000);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());
 


  // Inicializa ThingSpeak
  ThingSpeak.begin(client);

  // Inicializa display
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 24, "Inicializando...");
  display.display();

  // Inicializa balança
  balanca.begin(HX_DT, HX_SCK);
  balanca.set_scale(fatorCalibracao);
  balanca.tare();

  display.clear();
  display.drawString(64, 24, "Balança pronta!");
  display.display();
  delay(1000);
}

// =====================
// LOOP PRINCIPAL
// =====================
void loop() {
  float peso = 0, temperatura = 0, umidade = 0, condutividade = 0;

  if (balanca.is_ready()) {
    peso = balanca.get_units(10);
    contadorLeituras++;

    // Lê o sensor RS-485
    readSensor(&temperatura, &umidade, &condutividade);

    // Envia dados ao ThingSpeak
    if (millis() - prevMillisThingSpeak > intervalThingSpeak) {
      ThingSpeak.setField(1, peso);
      ThingSpeak.setField(2, temperatura);
      ThingSpeak.setField(3, umidade);
      ThingSpeak.setField(4, condutividade);
      int x = ThingSpeak.writeFields(CHANNEL, WRITE_API);
      if (x == 200) Serial.println("Update no ThingSpeak OK");
      else Serial.println("Erro HTTP: " + String(x));
      prevMillisThingSpeak = millis();
    }

    // Serial monitor
    Serial.printf("Leitura %lu | Peso: %.3f kg | T: %.1f°C | U: %.1f%% | C: %.0f uS/cm\n",
                  contadorLeituras, peso, temperatura, umidade, condutividade);

    // Display
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, "Peso: " + String(peso, 3) + " kg");
    display.drawString(64, 20, "T: " + String(temperatura, 1) + "C");
    display.drawString(64, 40, "U: " + String(umidade, 1) + "%");
    display.display();

    // Grava no SD
    gravaDados(peso, temperatura, umidade, condutividade);
  }

  delay(500);
}

// =====================
// Lê sensor RS485
// =====================
void readSensor(float *temperatura, float *umidade, float *condutividade) {

  const uint8_t CMD[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB};
  uint8_t buffer[11];
  uint8_t i = 0;

  //  LIGA SENSOR 
  digitalWrite(bussControl, HIGH);
  delay(1000);

  digitalWrite(sensorControl, HIGH);
  delay(2000);   // tempo de estabilização real

  //  Limpa buffer
  while (mod.available()) mod.read();

  // Envia comando
  digitalWrite(RE, HIGH);
  delay(5);

  mod.write(CMD, sizeof(CMD));
  mod.flush();

  digitalWrite(RE, LOW);

  // 🔹 Aguarda resposta
  uint32_t timeout = millis();
  while (i < 11 && (millis() - timeout) < 1000) {
    if (mod.available()) {
      buffer[i++] = mod.read();
    }
  }

  
  if (i >= 9) {
    uint16_t rawHum  = (buffer[3] << 8) | buffer[4];
    uint16_t rawTemp = (buffer[5] << 8) | buffer[6];
    uint16_t rawCond = (buffer[7] << 8) | buffer[8];

    *umidade = rawHum / 10.0;
    *temperatura = rawTemp / 10.0;
    *condutividade = rawCond;
  } 
  else {
    Serial.println("Falha na leitura do sensor RS485");
    *temperatura = *umidade = *condutividade = 0;
  }

  //  DESLIGA SENSOR
  digitalWrite(sensorControl, LOW);
  delay(200);

  digitalWrite(bussControl, LOW);
}


// =====================
// Inicialização do SD
// =====================
void sdInit(void) {
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sd_spi)) {
    Serial.println("Falha ao montar SD");
    return;
  }
  Serial.println("Cartão SD pronto.");
}

// =====================
// Grava dados no SD
// =====================
void gravaDados(float peso, float temp, float umid, float cond) {
  String linha = String(contadorLeituras) + "," +
                 String(peso, 3) + "," +
                 String(temp, 1) + "," +
                 String(umid, 1) + "," +
                 String(cond, 0) + "," +
                 String(millis()) + "\r\n";
  appendFile(SD, "/data.txt", linha.c_str());
}

