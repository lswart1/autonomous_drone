#include <WiFi.h>
#include <WebServer.h>

#define RC_CHANNEL_MIN 990
#define RC_CHANNEL_MAX 2010

#define SBUS_MIN_OFFSET 173
#define SBUS_MID_OFFSET 992
#define SBUS_MAX_OFFSET 1811
#define SBUS_CHANNEL_NUMBER 16
#define SBUS_PACKET_LENGTH 25
#define SBUS_FRAME_HEADER 0x0f
#define SBUS_FRAME_FOOTER 0x00
#define SBUS_FRAME_FOOTER_V2 0x04
#define SBUS_STATE_FAILSAFE 0x08
#define SBUS_STATE_SIGNALLOSS 0x04
#define SBUS_UPDATE_RATE 15 //ms
#define SBUS_BAUD_RATE 100000

const char* ssid = "Drone_Server";
const char* password = "Drone123";

WebServer server(80);

int rcChannels[SBUS_CHANNEL_NUMBER] = {1500}; // Initialize all channels to mid-value
uint8_t sbusPacket[SBUS_PACKET_LENGTH];
uint32_t sbusTime = 0;


const int trigPin = 2; // Example pin number for the trigger
const int echoPin = 15; // Example pin number for the echo

unsigned long previousTriggerTime = 0;
unsigned long previousEchoReadTime = 0;
unsigned long delayBeforeNextMeasurement = 500;
bool measuring = false;

float distance;

void sbusPreparePacket(uint8_t packet[], int channels[], bool isSignalLoss, bool isFailsafe) {
    static int output[SBUS_CHANNEL_NUMBER] = {0};

    for (uint8_t i = 0; i < SBUS_CHANNEL_NUMBER; i++) {
        output[i] = map(channels[i], RC_CHANNEL_MIN, RC_CHANNEL_MAX, SBUS_MIN_OFFSET, SBUS_MAX_OFFSET);
    }

    uint8_t stateByte = 0x00;
    if (isSignalLoss) {
        stateByte |= SBUS_STATE_SIGNALLOSS;
    }
    if (isFailsafe) {
        stateByte |= SBUS_STATE_FAILSAFE;
    }

    packet[0] = SBUS_FRAME_HEADER; // Header
    packet[1] = (uint8_t) (output[0] & 0x07FF);
    packet[2] = (uint8_t) ((output[0] & 0x07FF) >> 8 | (output[1] & 0x07FF) << 3);
    packet[3] = (uint8_t) ((output[1] & 0x07FF) >> 5 | (output[2] & 0x07FF) << 6);
    packet[4] = (uint8_t) ((output[2] & 0x07FF) >> 2);
    packet[5] = (uint8_t) ((output[2] & 0x07FF) >> 10 | (output[3] & 0x07FF) << 1);
    packet[6] = (uint8_t) ((output[3] & 0x07FF) >> 7 | (output[4] & 0x07FF) << 4);
    packet[7] = (uint8_t) ((output[4] & 0x07FF) >> 4 | (output[5] & 0x07FF) << 7);
    packet[8] = (uint8_t) ((output[5] & 0x07FF) >> 1);
    packet[9] = (uint8_t) ((output[5] & 0x07FF) >> 9 | (output[6] & 0x07FF) << 2);
    packet[10] = (uint8_t) ((output[6] & 0x07FF) >> 6 | (output[7] & 0x07FF) << 5);
    packet[11] = (uint8_t) ((output[7] & 0x07FF) >> 3);
    packet[12] = (uint8_t) ((output[8] & 0x07FF));
    packet[13] = (uint8_t) ((output[8] & 0x07FF) >> 8 | (output[9] & 0x07FF) << 3);
    packet[14] = (uint8_t) ((output[9] & 0x07FF) >> 5 | (output[10] & 0x07FF) << 6);  
    packet[15] = (uint8_t) ((output[10] & 0x07FF) >> 2);
    packet[16] = (uint8_t) ((output[10] & 0x07FF) >> 10 | (output[11] & 0x07FF) << 1);
    packet[17] = (uint8_t) ((output[11] & 0x07FF) >> 7 | (output[12] & 0x07FF) << 4);
    packet[18] = (uint8_t) ((output[12] & 0x07FF) >> 4 | (output[13] & 0x07FF) << 7);
    packet[19] = (uint8_t) ((output[13] & 0x07FF) >> 1);
    packet[20] = (uint8_t) ((output[13] & 0x07FF) >> 9 | (output[14] & 0x07FF) << 2);
    packet[21] = (uint8_t) ((output[14] & 0x07FF) >> 6 | (output[15] & 0x07FF) << 5);
    packet[22] = (uint8_t) ((output[15] & 0x07FF) >> 3);
    packet[23] = stateByte; // Flags byte
    packet[24] = SBUS_FRAME_FOOTER; // Footer
}

void handleRoot() {
    String html = R"rawliteral(
        <html>
        <body>
            <h1>ESP32 Joypad Controller</h1>
            <div>
                Joypad 1 (Channels 0 & 1): <br>
                Roll: <input type="range" id="joypad1X" min="413" max="613" style="width: 300px;" oninput="sendData()"> <br>
                Pitch: <input type="range" id="joypad1Y" min="413" max="613" style="width: 300px;" oninput="sendData()"> <br>
            </div>
            <div>
                Joypad 2 (Channels 2 & 3): <br>
                Throttle: <input type="range" id="joypad2X" min="0" max="1023" style="width: 300px;" oninput="sendData()"> <br>
                Yaw: <input type="range" id="joypad2Y" min="413" max="613" style="width: 300px;" oninput="sendData()"> <br>
            </div>
            <div>
                Switch (Channel 4): <br>
                <button id="switch" onclick="toggleSwitch()">Toggle Switch</button>
                <span id="switchStateText">Off</span>
            </div>
            <div>
                <button onclick="setJoypadsTo1500()">Set Joypads to 1500</button>
            </div>
            <script>
                var switchState = 0;
                function sendData() {
                    var joypad1X = document.getElementById('joypad1X').value;
                    var joypad1Y = document.getElementById('joypad1Y').value;
                    var joypad2X = document.getElementById('joypad2X').value;
                    var joypad2Y = document.getElementById('joypad2Y').value;

                    var url = `/control?joy1x=${joypad1X}&joy1y=${joypad1Y}&joy2x=${joypad2X}&joy2y=${joypad2Y}&switch=${switchState}`;
                    fetch(url);
                }
                
                function toggleSwitch() {
                    switchState = switchState == 0 ? 1 : 0;
                    document.getElementById('switchStateText').textContent = switchState == 1 ? "On" : "Off";
                    sendData();
                }

                function setJoypadsTo1500() {
                    document.getElementById('joypad1X').value = 513;
                    document.getElementById('joypad1Y').value = 513;
                    document.getElementById('joypad2Y').value = 513;
                    sendData();  // Send updated values after setting to 1500
                }
            </script>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", html);
}

void handleControl() {
    int joy1X = server.arg("joy1x").toInt();
    int joy1Y = server.arg("joy1y").toInt();
    int joy2X = server.arg("joy2x").toInt();
    int joy2Y = server.arg("joy2y").toInt();
    int switchState = server.arg("switch").toInt();

    rcChannels[0] = map(joy1X, 0, 1023, RC_CHANNEL_MIN, RC_CHANNEL_MAX);
    rcChannels[1] = map(joy1Y, 0, 1023, RC_CHANNEL_MIN, RC_CHANNEL_MAX);
    rcChannels[2] = map(joy2X, 0, 1023, RC_CHANNEL_MIN, RC_CHANNEL_MAX);
    rcChannels[3] = map(joy2Y, 0, 1023, RC_CHANNEL_MIN, RC_CHANNEL_MAX);
    rcChannels[4] = (switchState == 1) ? 1700 : 1000;

    server.send(200, "text/plain", "OK");
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(SBUS_BAUD_RATE, SERIAL_8E2, -1, 1);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    server.on("/", handleRoot);
    server.on("/control", handleControl);

    server.begin();
    Serial.println("HTTP server started");

    Serial1.begin(SBUS_BAUD_RATE, SERIAL_8E2, -1, 1); // Initialize Serial1 with 100000 baud rate, TX = GPIO 9
    
    for (uint8_t i = 0; i < SBUS_CHANNEL_NUMBER; i++) {
        rcChannels[i] = 1500; // Initialize channels to mid-value
        
    }
}

void loop() {
    server.handleClient(); // Handle web server requests

    uint32_t currentMillis = millis();

    // Send SBUS packet at regular intervals
    if (currentMillis > sbusTime) {
        sbusPreparePacket(sbusPacket, rcChannels, false, false);
        Serial1.write(sbusPacket, SBUS_PACKET_LENGTH); // Send SBUS packet through Serial1
        sbusTime = currentMillis + SBUS_UPDATE_RATE;
    }
}


