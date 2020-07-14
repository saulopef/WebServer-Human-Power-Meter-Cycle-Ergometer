/*
  Saulo Senoski - OneCorpore®
  for Help - suporte@onecorpore.com
*/

#include <SPI.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h> // Biblioteca SPI

const int dacChipSelectPin = D8;

const char *ssid = "OneCorpore";
const char *password = "41142207";
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

bool startTimer;
unsigned long stTime;
unsigned long actTime;
unsigned int actStage = 0;
unsigned long previousMillis = 0;
unsigned int chargePower = 0;
unsigned int stepPower = 100;
unsigned int interval = 1;
unsigned int k = 1000; //segundos - 60000; milisegundos

const uint8_t interruptPin = D1; //pino de interrupção, hall rpm
const uint8_t beepPin = D2;      //buzzerPin
void ICACHE_RAM_ATTR contador(); //Função contagem de pulsos hall (RAM)
int rpm;
volatile byte pulsos;
unsigned long timeold;
const unsigned int pulsos_por_volta = 31; //Altere o numero abaixo de acordo com o seu disco encoder

const char *PARAM_INPUT_STAGE = "stage";
const char *PARAM_INPUT_INCREMENTO = "incr";
const char *PARAM_INPUT_INITPW = "initPW";

void contador()
{
    //Incrementa contador
    pulsos++;
}

void beep(int beep)
{
    int oldTime = 0;
    for (int i = 0; i < beep * 2;)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - oldTime >= 100)
        {
            oldTime = currentMillis;
            digitalWrite(beepPin, !digitalRead(beepPin));
            i++;
        }
    }
    digitalWrite(beepPin, LOW);
}

void buzz(long frequency, long length) {
  long delayValue = 1000000/frequency/2; // calculate the delay value between transitions
  // 1 second's worth of microseconds, divided by the frequency, then split in half since
  // there are two phases to each cycle
  long numCycles = frequency * length/ 1000; // calculate the number of cycles for proper timing
  // multiply frequency, which is really cycles per second, by the number of seconds to 
  // get the total number of cycles to produce
 for (long i=0; i < numCycles; i++){ // for the calculated length of time...
    digitalWrite(beepPin,HIGH); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
    digitalWrite(beepPin,LOW); // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue); // wait again for the calculated delay value
  }
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request)
    {
        //request->addInterestingHeader("ANY");
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request)
    {

        request->send(SPIFFS, "/index.html", String());
    }
};

void setup()
{
    Serial.begin(115200);
    //Pino do sensor como entrada
    pinMode(interruptPin, INPUT_PULLUP);
    pinMode(beepPin, OUTPUT);
    digitalWrite(beepPin, LOW);
    pinMode(dacChipSelectPin, OUTPUT);    // Chip Select Pino como saída
    digitalWrite(dacChipSelectPin, HIGH); // Desativa a comunicação SPI com o MCP4922

    SPI.begin();                // Inicialização da comunicação SPI
    SPI.setBitOrder(MSBFIRST);  // Configura o envio dos bytes mais significativos primeiro
    SPI.setDataMode(SPI_MODE0); // Configura o modo SPI - mode0

    // Initialize SPIFFS
    if (!SPIFFS.begin())
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    // Connect to Wi-Fi
    Serial.println(WiFi.softAP(ssid) ? "Ready" : "Failed!");
    // WiFi.softAP(ssid, password);
    // Print ESP32 Local IP Address
    //  myIP = WiFi.softAPIP();

    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/index.html", String());
        interrupts();
        beep(2);
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/index.html", String());
        interrupts();
        beep(2);
    });
    server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/about.html", String());
        interrupts();
    });
    // Route to load style.css file
    server.on("/sb-admin-2.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.min.js", "text/js");
        interrupts();
    });
    server.on("/sb-admin-2.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.min.css", "text/css");
        interrupts();
    });
    server.on("/sb-admin-2.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.css", "text/css");
        interrupts();
    });
    server.on("/bootstrap.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/bootstrap.js", "text/js");
        interrupts();
    });
    server.on("/jquery-1.11.3.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(SPIFFS, "/jquery-1.11.3.min.js", "text/js");
        interrupts();
    });
    // Route for root / web page
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        startTimer = !startTimer;
        digitalWrite(2, !digitalRead(2));
        request->send(200, "text/plain", "ok");
        interrupts();
        beep(4);
    });
    server.on("/ResetTest", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        resetTest();
        request->send(200, "text/plain", String(actTime));
        interrupts();
    });
    server.on("/RpmCheck", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        request->send(200, "text/plain", String(rpm));
        interrupts();
    });
    server.on("/getIp", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
        response->print("<p>This is out captive portal front page.</p>");
        response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
        response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
        response->print("</body></html>");
        request->send(response);
        interrupts();
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        if (request->hasParam(PARAM_INPUT_STAGE))
        {
            interval = atoi(request->getParam(PARAM_INPUT_STAGE)->value().c_str());
            Serial.print("interval: ");
            Serial.println(interval);
            beep(1);
        }
        // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
        if (request->hasParam(PARAM_INPUT_INCREMENTO))
        {
            stepPower = atoi(request->getParam(PARAM_INPUT_INCREMENTO)->value().c_str());
            Serial.print("stepPower: ");
            Serial.println(stepPower);
            beep(2);
        }
        // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
        if (request->hasParam(PARAM_INPUT_INITPW))
        {
            chargePower = atoi(request->getParam(PARAM_INPUT_INITPW)->value().c_str());
            Serial.print("chargePower: ");
            Serial.println(chargePower);
            setDac(chargePower);
            beep(3);
        }
        request->send(200, "text/plain", String("ok"));
        interrupts();
    });

        server.on("/stageUP", HTTP_GET, [](AsyncWebServerRequest *request) {
        noInterrupts();
        StageUp();
        request->send(200, "text/plain", String(map(chargePower, 0, 4096, 0,100)));
        interrupts();
    });

    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    // Start server
    server.begin();

    Serial.println(WiFi.softAPIP());
    attachInterrupt(digitalPinToInterrupt(interruptPin), contador, FALLING);
    pulsos = 0;
    rpm = 0;
    timeold = 0;

    beep(1);
    delay(200);
    beep(3);
    delay(200);
    beep(1);
    delay(400);
    beep(1);
    delay(200);
    beep(1);
    
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(SPIFFS, "/404.html", String());
}

void resetTest()
{
    actStage = 0;
    startTimer = false;
    previousMillis = 0;
    chargePower = 0;
}

void StageUp()
{
    
    if(chargePower <= 4096 ){
      actStage++;
        chargePower = chargePower + stepPower;
    }else{
      actStage--;
        chargePower = chargePower + stepPower;
    }
    Serial.print("pw: ");
    Serial.println(chargePower);
    setDac(chargePower);
    beep(1);
}

void loop()
{
    dnsServer.processNextRequest();
    //  Serial.println(WiFi.softAPIP());

    //Atualiza contador a cada segundo
    if (millis() - timeold >= 1000)
    {
        //Desabilita interrupcao durante o calculo
        noInterrupts();
        rpm = (60 * 1000 / pulsos_por_volta) / (millis() - timeold) * pulsos;
        timeold = millis();
        pulsos = 0;

        //Mostra o valor de RPM no serial monitor
        Serial.print("RPM = ");
        Serial.println(rpm, DEC);
        //Habilita interrupcao
        interrupts();
    }
}

void setDac(int valor)
{
    byte dacRegister = 0b00110000;                        // Byte default de configuração (veja tabela do registrador do MCP4922) 8 bits da esquerda
    int dacSecondaryByteMask = 0b0000000011111111;        // Máscara para separar os 8 bits menos significativos do valor do DAC (12 bits)
    byte dacPrimaryByte = (valor >> 8) | dacRegister;     // Desloca para a direita 8 bits do valor  para obter os outros 4 bits do DAC e acrescenta os bits de configuração
    byte dacSecondaryByte = valor & dacSecondaryByteMask; // Compara o valor (12 bits) para isolar os 8 bits menos significativos

    noInterrupts();                       // Desabilita interrupções para não interferir na interface SPI
    digitalWrite(dacChipSelectPin, LOW);  // Pino Chip Select setado para 0 para selecionar a comunicação com o MCP4922
    SPI.transfer(dacPrimaryByte);         // Envio do primeiro Byte (mais significativo)
    SPI.transfer(dacSecondaryByte);       // Envio do segundo Byte (menos significativo)
    digitalWrite(dacChipSelectPin, HIGH); // Pino Chip Select setado para 1 para terminar a comunicação com o MCP4922
    interrupts();                         // Habilita interrupções novamente
}
