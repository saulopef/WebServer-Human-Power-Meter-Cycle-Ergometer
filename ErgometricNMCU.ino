/*
  Saulo Senoski - OneCorpore®
  for Help - suporte@onecorpore.com
*/

#include <SPI.h> //to connect with load control
#include <DNSServer.h> // to make captive portal
#include <ESP8266WiFi.h> // to make ap network
#include <ESPAsyncTCP.h> // to send async data to webserver
#include <ESPAsyncWebServer.h> // to make bootstrap webserver
#include <FS.h> // to store bootstrap pages

const int dacChipSelectPin = D8; //SPI pin MOSI to DAC load controler

const char *ssid = "OneCorpore"; // network SSID
const char *password = "41142207"; // network passworld
const byte DNS_PORT = 53; // captive portal port
DNSServer dnsServer; // server for captive portal

// Create AsyncWebServer object on port 80
AsyncWebServer server(80); 

bool startTimer; // flag to know with timer was running
unsigned long stTime; // initial time
unsigned long actTime; // actual time
unsigned int actStage = 0; // actual stage of increment
unsigned long previousMillis = 0; 
unsigned int chargePower = 0; // load value from 0 to 4095
unsigned int stepPower = 100; // increment value of load 
unsigned int interval = 1; // intervar between steps
unsigned int k = 1000; //segundos - 60000; milisegundos
unsigned long Vi = 0; //velocidade inicial
unsigned long Vf = 0; //velocidade Final


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

void beep(int beep) // buzzer beps
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

class CaptiveRequestHandler : public AsyncWebHandler // Captive portal Hendler
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

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { // initial page
        noInterrupts();
        request->send(SPIFFS, "/index.html", String());
        interrupts();
        beep(2);
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) { // initial page
        noInterrupts();
        request->send(SPIFFS, "/index.html", String());
        interrupts();
        beep(2);
    });
    server.on("/about.html", HTTP_GET, [](AsyncWebServerRequest *request) { // about page
        noInterrupts();
        request->send(SPIFFS, "/about.html", String());
        interrupts();
    });
    // Route to load style.css file
    server.on("/sb-admin-2.min.js", HTTP_GET, [](AsyncWebServerRequest *request) { // bootstrap theme js
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.min.js", "text/js");
        interrupts();
    });
    server.on("/sb-admin-2.min.css", HTTP_GET, [](AsyncWebServerRequest *request) { // bootstrap theme css
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.min.css", "text/css");
        interrupts();
    });
    server.on("/sb-admin-2.css", HTTP_GET, [](AsyncWebServerRequest *request) { // bootstrap theme css
        noInterrupts();
        request->send(SPIFFS, "/sb-admin-2.css", "text/css");
        interrupts();
    });
    server.on("/bootstrap.js", HTTP_GET, [](AsyncWebServerRequest *request) { // bootstrap js
        noInterrupts();
        request->send(SPIFFS, "/bootstrap.js", "text/js");
        interrupts();
    });
    server.on("/jquery-1.11.3.min.js", HTTP_GET, [](AsyncWebServerRequest *request) { // jaquery js
        noInterrupts();
        request->send(SPIFFS, "/jquery-1.11.3.min.js", "text/js");
        interrupts();
    });
    // Route for root / web page
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) { // Start/stop timer and test
        noInterrupts();
        startTimer = !startTimer;
        digitalWrite(2, !digitalRead(2));
        request->send(200, "text/plain", "ok");
        interrupts();
        beep(4);
    });
    server.on("/ResetTest", HTTP_GET, [](AsyncWebServerRequest *request) { //Reset Test
        noInterrupts();
        resetTest();
        request->send(200, "text/plain", String(actTime));
        interrupts();
    });
    server.on("/RpmCheck", HTTP_GET, [](AsyncWebServerRequest *request) { //return rpm to server
        noInterrupts();
        request->send(200, "text/plain", String(rpm));
        interrupts();
    });
    server.on("/getIp", HTTP_GET, [](AsyncWebServerRequest *request) { // get board ip on server
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

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) { //setup test data with parameters
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

        server.on("/stageUP", HTTP_GET, [](AsyncWebServerRequest *request) { // test step
        noInterrupts();
        StageUp();
        request->send(200, "text/plain", String(map(chargePower, 0, 4096, 0,100)));
        interrupts();
    });

    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // setup captive portal only in ap mode
    // Start server
    server.begin();

    Serial.println(WiFi.softAPIP());
    attachInterrupt(digitalPinToInterrupt(interruptPin), contador, FALLING); // attach interrutions on rpm sensor pin
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

void notFound(AsyncWebServerRequest *request) // on not found a page in webserver
{
    request->send(SPIFFS, "/404.html", String());
}

void resetTest() // reinitialize test
{
    actStage = 0;
    startTimer = false;
    previousMillis = 0;
    chargePower = 0;
}

void StageUp() // test step
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

void setDac(int valor) // send load value in bytes to DAC
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
