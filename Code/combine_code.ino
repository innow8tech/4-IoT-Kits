/***************************************************
 *       AGRITECH IoT with ESPAsyncWebServer
 *       Includes the Dashboard HTML + Live Data
 ***************************************************/

#include <Wire.h>
#include <Adafruit_SHT31.h>    
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>   
#include <Arduino.h>
#include <WiFi.h>              
#include <AsyncTCP.h>          // Required for ESPAsyncWebServer on ESP32
#include <ESPAsyncWebServer.h> // Async WebServer library

/****************** I2C Addresses *****************/
#define RAINFALL_SENSOR_ADDRESS 0x5B
#define HEIGHT_SENSOR_ADDRESS   0x5A
#define WATER_PUMP_ADDRESS      0x5D
#define FAN_MOTOR_ADDRESS       0x5C
#define COMMAND_REQUEST_DATA    2

#define OLED_I2C_ADDRESS 0x3C
#define RAINFALL_COMMAND 0x01

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire);
Adafruit_SHT31   sht31   = Adafruit_SHT31(); 

bool waterPumpState = false; 
bool fanState       = false; 

#define BTN_UP     18
#define BTN_DOWN   16
#define BTN_LEFT   17
#define BTN_RIGHT  4
#define BTN_CENTER 5

int currentScreen     = 0; 
const int totalScreens = 8;  

#define LDR_PIN 26 
int   ldrValue             = 0;      
int   ldrDayNightThreshold = 3000;   

#define TOUCH_PIN1  32
#define TOUCH_PIN2  33
#define TOUCH_PIN3  14
#define TOUCH_PIN4  12
#define BUZZER_PIN  19

int threshold = 30;  
int touch1, touch2, touch3, touch4;

/****************** IoT Mode **********************/
#define AP_SSID       "AGRITECH"
#define AP_PASSWORD   "12345678"
bool iotModeActive = false;

/** Use AsyncWebServer  **/
AsyncWebServer server(80);

/****************** Function Prototypes ***********/
void configureBuzzer();
void playTone(int frequency);
void stopTone();
void handleNavigation();
void updateDisplay();       
void updateDetailView();   
void updateControlView();  
void updateLDRView();      
void updateMusicModeView();
String getRainfallStatus();
uint16_t readHeightSensor();
void controlFan(bool state);
void controlWaterPump(bool state);
void setupWiFiAP();
void stopWiFiAP();

/** Replaced old handleClientRequests() with Async routes **/
String buildJSONData();

/*************************************************
 *        New HTML Dashboard (from your code)
 *
 * We'll serve this string when the user requests "/"
 *************************************************/
const char dashboard_html[] PROGMEM = R"====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta
        name="viewport"
        content="width=device-width, initial-scale=1.0"
    />
    <title>AGRITECH IoT Dashboard</title>
    <style>
        :root {
            --wet-color: #16C47F;
            --dry-color: #FF9D23;
            --distance-color: #F93827;
        }

        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f4f4f4;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: flex-start;
            height: 100vh;
        }

        header {
            background-color: #16C47F;
            color: white;
            padding: 20px;
            width: 100%;
            text-align: center;
            box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.2);
            font-size: 1.5em;
            font-weight: bold;
        }

        .dashboard {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }

        .controls {
            margin: 20px 0;
        }

        .round-btn {
            padding: 15px 25px;
            border: none;
            border-radius: 50px;
            background-color: #FF9D23;
            color: white;
            font-size: 1em;
            margin: 10px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .round-btn:hover {
            background-color: #F93827;
        }

        .gauges {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 20px;
        }

        .gauge {
            width: 200px;
            height: 200px;
            position: relative;
        }

        .gauge svg {
            transform: rotate(-90deg);
        }

        .gauge circle {
            fill: none;
            stroke-width: 15;
        }

        .gauge .background {
            stroke: #ccc;
        }

        .gauge .progress {
            stroke-linecap: round;
            stroke-dasharray: 440;
            transition: stroke-dashoffset 0.4s ease;
        }

        .gauge .text {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            text-align: center;
            font-size: 1.2em;
            font-weight: bold;
        }

        .gauge .text .value {
            font-size: 2em;
            color: #FF9D23;
        }

        .label {
            color: #555;
        }

        /* Rainfall-specific styling */
        .rainfall .progress {
            stroke: var(--dry-color);
        }

        .rainfall[data-status="wet"] .progress {
            stroke: var(--wet-color);
        }

        .rainfall .text .value {
            color: var(--dry-color);
        }

        .rainfall[data-status="wet"] .text .value {
            color: var(--wet-color);
        }

        /* Distance-specific styling */
        .distance .progress {
            stroke: var(--distance-color);
        }

        .distance .text .value {
            color: var(--distance-color);
        }
    </style>
</head>
<body>
    <header>
        AGRITECH IoT Dashboard
    </header>
    <div class="dashboard">
        <div class="controls">
            <button class="round-btn" id="pumpBtn" onclick="togglePump()">Water Pump: OFF</button>
            <button class="round-btn" id="fanBtn"  onclick="toggleFan()">Fan: OFF</button>
        </div>
        <div class="gauges">
            <!-- Temperature -->
            <div class="gauge" id="tempGauge" style="--value: 0;">
                <svg width="200" height="200">
                    <circle class="background" cx="100" cy="100" r="70"></circle>
                    <circle
                      class="progress"
                      cx="100"
                      cy="100"
                      r="70"
                      style="stroke-dashoffset: calc(440 - (440 * var(--value)) / 100); stroke: #FF9D23;"
                    ></circle>
                </svg>
                <div class="text">
                    <span class="value" id="tempValue">0°C</span><br />
                    <span class="label">Temperature</span>
                </div>
            </div>

            <!-- Humidity -->
            <div class="gauge" id="humGauge" style="--value: 0;">
                <svg width="200" height="200">
                    <circle class="background" cx="100" cy="100" r="70"></circle>
                    <circle
                      class="progress"
                      cx="100"
                      cy="100"
                      r="70"
                      style="stroke-dashoffset: calc(440 - (440 * var(--value)) / 100); stroke: #16C47F;"
                    ></circle>
                </svg>
                <div class="text">
                    <span class="value" id="humValue">0%</span><br />
                    <span class="label">Humidity</span>
                </div>
            </div>

            <!-- Rainfall -->
            <div class="gauge rainfall" id="rainGauge" data-status="dry">
                <svg width="200" height="200">
                    <circle class="background" cx="100" cy="100" r="70"></circle>
                    <circle
                      class="progress"
                      cx="100"
                      cy="100"
                      r="70"
                      style="stroke-dashoffset: calc(440 - (440 * 100) / 100);"
                    ></circle>
                </svg>
                <div class="text">
                    <span class="value" id="rainValue">Dry</span><br />
                    <span class="label">Rainfall</span>
                </div>
            </div>

            <!-- Distance (Height) -->
            <div class="gauge distance" id="distGauge" style="--value: 0;">
                <svg width="200" height="200">
                    <circle class="background" cx="100" cy="100" r="70"></circle>
                    <circle
                      class="progress"
                      cx="100"
                      cy="100"
                      r="70"
                      style="stroke-dashoffset: calc(440 - (440 * var(--value)) / 100);"
                    ></circle>
                </svg>
                <div class="text">
                    <span class="value" id="distValue">0cm</span><br />
                    <span class="label">Distance</span>
                </div>
            </div>
        </div>
    </div>

    <script>
      // Toggle pump
      function togglePump() {
        fetch('/togglePump')
          .then(response => response.json())
          .then(data => {
            // Update button text
            const pumpBtn = document.getElementById("pumpBtn");
            pumpBtn.textContent = data.pump ? "Water Pump: ON" : "Water Pump: OFF";
          });
      }

      // Toggle fan
      function toggleFan() {
        fetch('/toggleFan')
          .then(response => response.json())
          .then(data => {
            // Update button text
            const fanBtn = document.getElementById("fanBtn");
            fanBtn.textContent = data.fan ? "Fan: ON" : "Fan: OFF";
          });
      }

      // Periodically fetch sensor data
      setInterval(() => {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            // data => { temperature, humidity, rainfall, distance, fan, pump }
            // Update Temperature
            const tempElem = document.getElementById("tempGauge");
            tempElem.style.setProperty('--value', data.temperature);
            document.getElementById("tempValue").textContent = data.temperature + "°C";

            // Update Humidity
            const humElem = document.getElementById("humGauge");
            humElem.style.setProperty('--value', data.humidity);
            document.getElementById("humValue").textContent = data.humidity + "%";

            // Update Rainfall
            const rainGauge = document.getElementById("rainGauge");
            const rainValue = document.getElementById("rainValue");
            if (data.rainfall === "Wet") {
              rainGauge.setAttribute("data-status", "wet");
              rainValue.textContent = "Wet";
            } else {
              rainGauge.setAttribute("data-status", "dry");
              rainValue.textContent = "Dry";
            }

            // Update Distance
            const distElem = document.getElementById("distGauge");
            distElem.style.setProperty('--value', data.distance);
            document.getElementById("distValue").textContent = data.distance + "cm";

            // Update Pump/Fan button text
            document.getElementById("pumpBtn").textContent = data.pump
              ? "Water Pump: ON"
              : "Water Pump: OFF";
            document.getElementById("fanBtn").textContent = data.fan
              ? "Fan: ON"
              : "Fan: OFF";
          });
      }, 3000);
    </script>
</body>
</html>
)====";


/*************************************************
 *   Setup & Loop
 *************************************************/
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(OLED_I2C_ADDRESS)) {
    Serial.println("Failed to initialize SH110X display!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  if (!sht31.begin(0x45)) {
    Serial.println("SHT31 not found. Check wiring!");
  } else {
    Serial.println("SHT31 Sensor initialized successfully.");
  }

  pinMode(BTN_UP,     INPUT_PULLUP);
  pinMode(BTN_DOWN,   INPUT_PULLUP);
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);
  pinMode(BTN_CENTER, INPUT_PULLUP);

  touchAttachInterrupt(TOUCH_PIN1, NULL, threshold);
  touchAttachInterrupt(TOUCH_PIN2, NULL, threshold);
  touchAttachInterrupt(TOUCH_PIN3, NULL, threshold);
  touchAttachInterrupt(TOUCH_PIN4, NULL, threshold);

  configureBuzzer();
  updateDisplay();

  // Start WiFi AP + Async server
  setupWiFiAP();

  // Define how to serve the main dashboard page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", dashboard_html);
  });

  // Define how to serve JSON sensor data
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = buildJSONData();
    request->send(200, "application/json", json);
  });

  // Toggle Pump route
  server.on("/togglePump", HTTP_GET, [](AsyncWebServerRequest *request){
    waterPumpState = !waterPumpState;
    controlWaterPump(waterPumpState);

    // Return updated states in JSON
    String resp = "{";
    resp += "\"pump\":" + String(waterPumpState ? 1 : 0) + ",";
    resp += "\"fan\":"  + String(fanState       ? 1 : 0);
    resp += "}";
    request->send(200, "application/json", resp);
  });

  // Toggle Fan route
  server.on("/toggleFan", HTTP_GET, [](AsyncWebServerRequest *request){
    fanState = !fanState;
    controlFan(fanState);

    // Return updated states in JSON
    String resp = "{";
    resp += "\"pump\":" + String(waterPumpState ? 1 : 0) + ",";
    resp += "\"fan\":"  + String(fanState       ? 1 : 0);
    resp += "}";
    request->send(200, "application/json", resp);
  });

  server.begin();
  Serial.println("Async Web Server started.");
}

void loop() {
  handleNavigation();

  // Because we moved to AsyncWebServer, we do not handle clients here anymore
  // The rest of your screen logic remains intact
  switch (currentScreen) {
    case 0:  
      updateDisplay();
      break;
    case 1: case 2: case 3: case 4:
      updateDetailView();
      break;
    case 5: case 6:
      updateControlView();
      break;
    case 7:
      updateLDRView();
      break;
    case 8:
      updateMusicModeView();
      break;
    case 9:
      // IoT Mode screen
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor((128 - 6 * 8) / 2, 0);
      display.println("IoT MODE");
      display.setCursor(10, 20);
      display.println("SSID: AGRITECH");
      display.setCursor(10, 30);
      display.println("Password: 12345678");
      {
        IPAddress ip = WiFi.softAPIP();
        String ipStr = ip.toString();
        display.setCursor(10, 40);
        display.print("IP: ");
        display.println(ipStr);
      }
      display.setCursor(0, 55);
      display.println("   <- Left Exit");
      display.display();
      break;
  }
}

/*************************************************
 *             Handle Navigation
 *************************************************/
void handleNavigation() {
  if (currentScreen != 8 && currentScreen != 9) {
    if (digitalRead(BTN_UP) == LOW) {
      currentScreen = (currentScreen - 1 + totalScreens) % totalScreens; 
      delay(200); 
    }
    if (digitalRead(BTN_DOWN) == LOW) {
      currentScreen = (currentScreen + 1) % totalScreens;
      delay(200); 
    }
  }

  // Music Mode
  if (currentScreen == 0 && digitalRead(BTN_LEFT) == LOW) {
    currentScreen = 8;
    delay(200);
  }
  if (currentScreen == 8 && digitalRead(BTN_RIGHT) == LOW) {
    currentScreen = 0;
    delay(200);
  }

  // IoT Mode
  if (currentScreen == 0 && digitalRead(BTN_RIGHT) == LOW && !iotModeActive) {
    // Already started AP in setup, so just switch screen
    iotModeActive = true;
    currentScreen = 9;
    delay(200);
  }
  if (currentScreen == 9 && digitalRead(BTN_LEFT) == LOW) {
    stopWiFiAP();
    iotModeActive = false;
    currentScreen = 0;
    delay(200);
  }

  // Fan / Pump toggle
  if (digitalRead(BTN_CENTER) == LOW) {
    if (currentScreen == 5) {
      fanState = !fanState;
      controlFan(fanState);
      delay(300);
    } else if (currentScreen == 6) {
      waterPumpState = !waterPumpState;
      controlWaterPump(waterPumpState);
      delay(300);
    }
  }
}

/*************************************************
 *           Screen 0: Main Display
 *************************************************/
void updateDisplay() {
  String rainfallStatus = getRainfallStatus();
  float  temperature    = sht31.readTemperature();
  float  humidity       = sht31.readHumidity();
  uint16_t height       = readHeightSensor();

  display.clearDisplay();

  if (iotModeActive) {
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("IoT Mode");
    display.drawLine(0, 10, 128, 10, SH110X_WHITE);
    display.setCursor(0, 12);
  } else {
    display.setCursor(0, 0);
  }

  display.setTextSize(1);
  display.println("     MAIN SCREEN");
  display.println("Rain: " + rainfallStatus);
  display.println("Temp: " + (isnan(temperature) ? "Error" : String(temperature, 1) + " C")+"  Right->");
  display.println("Hum:  " + (isnan(humidity) ? "Error" : String(humidity, 1) + " %")+"   IoT");
  display.println("Hgt:  " + ((height == 0xFFFF) ? "Error" : String(height) + " cm"));
  display.println(waterPumpState ? "Pump: ON" : "Pump: OFF");
  display.println(fanState      ? "Fan:  ON" : "Fan:  OFF");

  // Nav arrows
  display.fillTriangle(123,  5, 128, 10, 118, 10, SH110X_WHITE); 
  display.fillTriangle(123, 59, 128, 54, 118, 54, SH110X_WHITE); 

  display.setCursor(0, 55);
  display.print("             <-Left");
  display.display();
}

/*************************************************
 *     Screens 1..4: Detailed Sensor Views
 *************************************************/
void updateDetailView() {
  display.clearDisplay();

  switch (currentScreen) {
    case 1: { 
      String rainfallStatus = getRainfallStatus();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.println("Rainfall");
      display.setTextSize(3);
      display.setCursor(10, 20);
      display.println(rainfallStatus);
      break;
    }
    case 2: {
      float temperature = sht31.readTemperature();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.println("Temperature");
      display.setTextSize(3);
      display.setCursor(10, 20);
      display.println(isnan(temperature) ? "Err" : String(temperature, 1) + "C");
      break;
    }
    case 3: {
      float humidity = sht31.readHumidity();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.println("Humidity");
      display.setTextSize(3);
      display.setCursor(10, 20);
      display.println(isnan(humidity) ? "Err" : String(humidity, 1) + "%");
      break;
    }
    case 4: {
      uint16_t height = readHeightSensor();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.println("Height");
      display.setTextSize(3);
      display.setCursor(10, 20);
      display.println((height == 0xFFFF) ? "Err" : String(height) + "cm");
      break;
    }
  }

  display.fillTriangle(123,  5, 128, 10, 118, 10, SH110X_WHITE);
  display.fillTriangle(123, 59, 128, 54, 118, 54, SH110X_WHITE);

  display.display();
}

/*************************************************
 *    Screens 5..6: Fan & Water Pump Control
 *************************************************/
void updateControlView() {
  display.clearDisplay();

  if (currentScreen == 5) {
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Fan Control");
    display.setTextSize(3);
    display.setCursor(10, 20);
    display.println(fanState ? "ON" : "OFF");
  } 
  else if (currentScreen == 6) {
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Pump Control");
    display.setTextSize(3);
    display.setCursor(10, 20);
    display.println(waterPumpState ? "ON" : "OFF");
  }

  display.fillTriangle(123,  5, 128, 10, 118, 10, SH110X_WHITE);
  display.fillTriangle(123, 59, 128, 54, 118, 54, SH110X_WHITE);

  display.display();
}

/*************************************************
 *    Screen 7: LDR Day/Night
 *************************************************/
void updateLDRView() {
  ldrValue = analogRead(LDR_PIN);
  String ldrState = (ldrValue > ldrDayNightThreshold) ? "NIGHT" : "DAY";

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("LDR Screen");
  display.setCursor(0, 15);
  display.setTextSize(1);
  display.println("Raw Value: " + String(ldrValue));
  display.setCursor(0, 30);
  display.println("State: " + ldrState);

  display.fillTriangle(123,  5, 128, 10, 118, 10, SH110X_WHITE);
  display.fillTriangle(123, 59, 128, 54, 118, 54, SH110X_WHITE);

  display.display();
}

/*************************************************
 *    Screen 8: Music Mode
 *************************************************/
void updateMusicModeView() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("     MUSIC MODE");
  display.setCursor(0, 15);
  display.println("Touch pins -> Notes");

  touch1 = touchRead(TOUCH_PIN1);
  touch2 = touchRead(TOUCH_PIN2);
  touch3 = touchRead(TOUCH_PIN3);
  touch4 = touchRead(TOUCH_PIN4);

  if (touch1 < threshold) {
    playTone(262);  
  } 
  else if (touch2 < threshold) {
    playTone(294);  
  } 
  else if (touch3 < threshold) {
    playTone(330);  
  } 
  else if (touch4 < threshold) {
    playTone(349);  
  } 
  else {
    stopTone();
  }

  display.setCursor(0, 55);
  display.println("  Right-> Exit");
  display.display();
}

/*************************************************
 *             Fan & Pump
 *************************************************/
void controlFan(bool state) {
  Wire.beginTransmission(FAN_MOTOR_ADDRESS);
  Wire.write(state ? 1 : 0); 
  Wire.endTransmission();
}

void controlWaterPump(bool state) {
  Wire.beginTransmission(WATER_PUMP_ADDRESS);
  Wire.write(state ? 1 : 0); 
  Wire.endTransmission();
}

/*************************************************
 *            Rainfall Sensor
 *************************************************/
String getRainfallStatus() {
  Wire.beginTransmission(RAINFALL_SENSOR_ADDRESS);
  Wire.write(RAINFALL_COMMAND);
  if (Wire.endTransmission() != 0) {
    return "Error";
  }
  Wire.requestFrom(RAINFALL_SENSOR_ADDRESS, 1);
  if (Wire.available() == 1) {
    uint8_t status = Wire.read();
    return (status == 0) ? "Wet" : "Dry";
  }
  return "Error";
}

/*************************************************
 *             Height Sensor
 *************************************************/
uint16_t readHeightSensor() {
  Wire.beginTransmission(HEIGHT_SENSOR_ADDRESS);
  Wire.write(COMMAND_REQUEST_DATA);
  if (Wire.endTransmission() != 0) {
    return 0xFFFF; 
  }
  delay(10);
  Wire.requestFrom(HEIGHT_SENSOR_ADDRESS, 2);
  if (Wire.available() == 2) {
    uint8_t highByte = Wire.read();
    uint8_t lowByte  = Wire.read();
    return (highByte << 8) | lowByte;
  }
  return 0xFFFF; 
}

/*************************************************
 *          Buzzer Helpers
 *************************************************/
void configureBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT); 
  digitalWrite(BUZZER_PIN, LOW);
}

void playTone(int frequency) {
  tone(BUZZER_PIN, frequency, 500);
}

void stopTone() {
  noTone(BUZZER_PIN);
}

/*************************************************
 *        WiFi AP Setup / Teardown
 *************************************************/
void setupWiFiAP() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void stopWiFiAP() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

/*************************************************
 *   Build JSON Data (for /data endpoint)
 *************************************************/
String buildJSONData() {
  String rainfallStatus = getRainfallStatus();
  float  temperature    = sht31.readTemperature();
  float  humidity       = sht31.readHumidity();
  uint16_t heightVal    = readHeightSensor();
  ldrValue              = analogRead(LDR_PIN);

  // Convert sensors to gauge-friendly values
  // Temperature range: 0..100 => for gauge
  // (But realistically let's just clamp to 100 max)
  int tempGauge  = 0;
  if (!isnan(temperature)) {
    tempGauge = (int)temperature;
    if (tempGauge < 0) tempGauge = 0;
    if (tempGauge > 100) tempGauge = 100;
  }

  // Humidity range 0..100
  int humGauge = 0;
  if (!isnan(humidity)) {
    humGauge = (int)humidity;
    if (humGauge < 0) humGauge = 0;
    if (humGauge > 100) humGauge = 100;
  }

  // "Wet"/"Dry" from rainfall sensor
  // Distance from "height sensor" 0..100?
  int distGauge = (heightVal == 0xFFFF) ? 0 : (int)min((float)heightVal, 500.0f);

  // Construct JSON
  String json = "{";
  json += "\"temperature\":" + String(tempGauge) + ",";
  json += "\"humidity\":"    + String(humGauge)  + ",";
  json += "\"rainfall\":\""  + rainfallStatus   + "\",";
  json += "\"distance\":"    + String(distGauge) + ",";
  json += "\"pump\":"        + String(waterPumpState ? 1 : 0) + ",";
  json += "\"fan\":"         + String(fanState       ? 1 : 0);
  json += "}";

  return json;
}