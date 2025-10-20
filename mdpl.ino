#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// WiFi credentials - UPDATE THESE!
const char* ssid = "New Order";
const char* password = "oro2dowo";

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BMP280
Adafruit_BMP280 bmp;
float seaLevelPressure_hPa = 1013.25;

// Preferences
Preferences preferences;
float highAltitude = -10000.0;
float lowAltitude = 10000.0;

// Web Server
WebServer server(80);

// Time
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200; // GMT+7 for Indonesia (7 * 3600)
const int daylightOffset_sec = 0;

// Current altitude for web interface
float currentAltitude = 0.0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED error");
    while (1);
  }

  display.clearDisplay();
  display.display();

  // BMP280 Init
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 error");
    while (1);
  }

  // Preferences Init
  preferences.begin("altitude", false);
  highAltitude = preferences.getFloat("high", -10000.0);
  lowAltitude = preferences.getFloat("low", 10000.0);

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  // WiFi Init
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  // Show WiFi connection status on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Connecting WiFi...");
  display.display();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("WiFi connected! IP: ");
  Serial.println(WiFi.localIP());
  
  // Show IP on OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("WiFi Connected!");
  display.setCursor(0, 10);
  display.print("IP: ");
  display.print(WiFi.localIP());
  display.display();
  delay(3000);

  // Time Init
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/data", handleGetData);
  server.on("/reset", HTTP_POST, handleReset);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  
  currentAltitude = bmp.readAltitude(seaLevelPressure_hPa);

  // Update high & low
  if (currentAltitude > highAltitude) {
    highAltitude = currentAltitude;
    preferences.putFloat("high", highAltitude);
  }
  if (currentAltitude < lowAltitude) {
    lowAltitude = currentAltitude;
    preferences.putFloat("low", lowAltitude);
  }

  // Debug
  Serial.print("Altitude: ");
  Serial.println(currentAltitude);

  // OLED Display
  display.clearDisplay();

  // Line 1: H: xxxx.xx L: xxxx.xx (size 1)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("H: ");
  display.print(highAltitude, 2);
  display.print(" L: ");
  display.print(lowAltitude, 2);

  // Line 2: Real-time altitude (size 2) + "Mdpl" (size 1)
  String altStr = String(currentAltitude, 2);
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(2);
  display.getTextBounds(altStr, 0, 0, &x1, &y1, &w, &h);
  int midX = (SCREEN_WIDTH - w - 30) / 2;  // shift a bit to the left to give space for 'Mdpl'

  display.setCursor(midX, 16);
  display.print(altStr);

  display.setTextSize(1);
  display.print(" Mdpl");

  display.display();
  delay(1000);
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Altimeter</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .data-box { background-color: #e8f4fd; padding: 15px; border-radius: 5px; margin: 10px 0; }
        .current-alt { font-size: 24px; font-weight: bold; color: #2c5282; text-align: center; }
        input[type="text"] { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
        button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background-color: #45a049; }
        .reset-btn { background-color: #f44336; }
        .reset-btn:hover { background-color: #da190b; }
        .data-list { max-height: 300px; overflow-y: auto; border: 1px solid #ddd; padding: 10px; border-radius: 5px; margin-top: 10px; }
        .data-item { padding: 5px; border-bottom: 1px solid #eee; }
        .data-item:last-child { border-bottom: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏔️ ESP32 Altimeter</h1>
        
        <div class="data-box">
            <div class="current-alt" id="currentAlt">Loading...</div>
            <div style="text-align: center; margin-top: 10px;">
                <span>High: <span id="highAlt">-</span> mdpl</span> | 
                <span>Low: <span id="lowAlt">-</span> mdpl</span>
            </div>
        </div>
        
        <div style="margin: 20px 0;">
            <h3>💾 Save Current Reading</h3>
            <input type="text" id="location" placeholder="Enter location (e.g., Malang City Center)" />
            <button onclick="saveData()">Save Data</button>
        </div>
        
        <div style="margin: 20px 0;">
            <h3>📋 Saved Data</h3>
            <button onclick="loadData()">Refresh Data</button>
            <button class="reset-btn" onclick="resetData()">Reset All Data</button>
            <div id="dataList" class="data-list">
                <div style="text-align: center; color: #666;">Click "Refresh Data" to load saved readings</div>
            </div>
        </div>
    </div>

    <script>
        function updateAltimeter() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('currentAlt').innerHTML = data.current + ' mdpl';
                    document.getElementById('highAlt').innerHTML = data.high;
                    document.getElementById('lowAlt').innerHTML = data.low;
                });
        }

        function saveData() {
            const location = document.getElementById('location').value.trim();
            if (!location) {
                alert('Please enter a location!');
                return;
            }
            
            fetch('/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'location=' + encodeURIComponent(location)
            })
            .then(response => response.text())
            .then(data => {
                alert('Data saved successfully!');
                document.getElementById('location').value = '';
                loadData();
            })
            .catch(error => {
                alert('Error saving data: ' + error);
            });
        }

        function loadData() {
            // This would normally load from stored data
            // For now, we'll show the format
            document.getElementById('dataList').innerHTML = 
                '<div style="text-align: center; color: #666;">Data is saved to Serial Monitor in format:<br/>"time, location, mdpl"</div>';
        }

        function resetData() {
            if (confirm('Are you sure you want to reset high/low records?')) {
                fetch('/reset', {method: 'POST'})
                    .then(response => response.text())
                    .then(data => {
                        alert('High/Low records reset!');
                        updateAltimeter();
                    });
            }
        }

        // Auto-update every 2 seconds
        setInterval(updateAltimeter, 2000);
        
        // Initial load
        updateAltimeter();
    </script>
</body>
</html>
)";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("location")) {
    String location = server.arg("location");
    
    // Get current time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeString[64];
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
      
      // Format: "time, location, mdpl"
      String dataEntry = String(timeString) + ", " + location + ", " + String(currentAltitude, 2);
      
      // Print to Serial (you can also save to SPIFFS/SD card if needed)
      Serial.println("=== DATA SAVED ===");
      Serial.println(dataEntry);
      Serial.println("==================");
      
      server.send(200, "text/plain", "Data saved: " + dataEntry);
    } else {
      server.send(500, "text/plain", "Error getting time");
    }
  } else {
    server.send(400, "text/plain", "Location parameter missing");
  }
}

void handleGetData() {
  String json = "{";
  json += "\"current\":" + String(currentAltitude, 2) + ",";
  json += "\"high\":" + String(highAltitude, 2) + ",";
  json += "\"low\":" + String(lowAltitude, 2);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleReset() {
  highAltitude = -10000.0;
  lowAltitude = 10000.0;
  preferences.putFloat("high", highAltitude);
  preferences.putFloat("low", lowAltitude);
  
  server.send(200, "text/plain", "High/Low records reset");
}