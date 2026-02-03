#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_MPRLS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Hardware pins
#define VALVE_PIN 14
#define PUMP_PIN  13
#define RST_PIN   16
#define EOC_PIN  -1

// Sensor and network objects
Adafruit_MPRLS mpr = Adafruit_MPRLS(RST_PIN, EOC_PIN);
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Forward declarations - tell compiler these functions exist
void handleRoot();
void handleInputDay();
void handleSubmitDay();
void handleComfort();
void handleComfortManual();
void handleRigid();
void handleLoosen();
void handleStop();
void handleStatus();
int calculateDaysBetween(String startDate, String endDate);
String getBaseStyles();
String getSvgDefs(float cycleProgress);
String getGradientBackground();
String getHardwareStatus();
String getPhaseText();

// Cycle tracking variables
struct CycleData {
  int dayNumber;
  float targetPressure;
  String date;
  bool hasData;
};

CycleData cycleArray[32];
int currentCycleDay = 0;
int cycleLength = 28;
bool irregularCycle = false;
String cycleStartDate = "";
int cycleDayOfStart = 1;

// Control state
bool pumpRunning = false;
bool valveActivated = false;
bool autoAdjusting = false;
float currentPressure = 0.0;
float targetPressure = 0.0;

// Auto-adjustment parameters
const float PRESSURE_TOLERANCE = 5.0;
unsigned long lastPressureCheck = 0;
const unsigned long PRESSURE_CHECK_INTERVAL = 2000;

const bool TESTING_MODE = false;

// ============================================
// SHARED CSS STYLES
// ============================================
String getBaseStyles() {
  String css = "<style>";
  css += "@import url('https://fonts.googleapis.com/css2?family=Bebas+Neue&family=Oswald:wght@400;700&display=swap');";
  css += "*{margin:0;padding:0;box-sizing:border-box;}";
  css += "body{font-family:'Oswald',sans-serif;letter-spacing:1px;background:#fff;min-height:100vh;overflow-x:hidden;}";
  
  // Gradient background
  css += ".gradient-bg{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;z-index:0;}";
  css += ".orb-container{width:600px;height:600px;border-radius:50%;position:absolute;overflow:hidden;filter:blur(0);}";
  
  // Gradient shapes
  css += ".grad-shape{position:absolute;filter:blur(25px);top:50%;left:50%;border-radius:50%;}";
  css += ".blue-1{width:200px;height:200px;background:radial-gradient(circle,rgba(65,75,140,0.95),rgba(30,40,100,1));animation:drift1 16s ease-in-out infinite;}";
  css += ".blue-2{width:150px;height:150px;background:radial-gradient(circle,rgba(55,65,130,0.9),rgba(35,40,90,1));animation:drift2 14s ease-in-out infinite;}";
  css += ".yellow-1{width:180px;height:180px;background:radial-gradient(circle,rgba(220,200,120,0.85),rgba(120,115,100,0.6));animation:drift3 18s ease-in-out infinite;}";
  css += ".yellow-2{width:120px;height:120px;background:radial-gradient(circle,rgba(215,195,115,0.8),rgba(140,135,110,0.6));animation:drift4 12s ease-in-out infinite;}";
  css += ".red-1{width:170px;height:170px;background:radial-gradient(circle,rgba(180,90,90,0.85),rgba(100,90,90,0.6));animation:drift5 17s ease-in-out infinite;}";
  css += ".red-2{width:130px;height:130px;background:radial-gradient(circle,rgba(170,85,85,0.8),rgba(115,95,95,0.6));animation:drift6 13s ease-in-out infinite;}";
  
  // Drift animations
  css += "@keyframes drift1{0%,100%{transform:translate(-50%,-50%) scale(1);}35%{transform:translate(-100%,-55%) scale(1.3,0.75);}75%{transform:translate(-60%,-48%) scale(1.08,0.95);}}";
  css += "@keyframes drift2{0%,100%{transform:translate(-50%,-50%) scale(1);}40%{transform:translate(-55%,-110%) scale(0.7,1.4);}80%{transform:translate(-48%,-65%) scale(0.92,1.1);}}";
  css += "@keyframes drift3{0%,100%{transform:translate(-50%,-50%) scale(1);}28%{transform:translate(0%,-45%) scale(1.35,0.72);}68%{transform:translate(-40%,-50%) scale(1.1,0.92);}}";
  css += "@keyframes drift4{0%,100%{transform:translate(-50%,-50%) scale(1);}44%{transform:translate(-95%,-90%) scale(1.3,0.75);}85%{transform:translate(-58%,-58%) scale(1.05,0.96);}}";
  css += "@keyframes drift5{0%,100%{transform:translate(-50%,-50%) scale(1);}32%{transform:translate(-45%,-5%) scale(0.72,1.38);}72%{transform:translate(-50%,-42%) scale(0.92,1.1);}}";
  css += "@keyframes drift6{0%,100%{transform:translate(-50%,-50%) scale(1);}42%{transform:translate(-110%,-52%) scale(1.4,0.7);}82%{transform:translate(-62%,-50%) scale(1.08,0.94);}}";
  
  // Center stage
  css += ".center-stage{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;z-index:2;}";
  
  // Progress ring
  css += ".progress-ring{position:absolute;width:486px;height:486px;pointer-events:none;z-index:5;}";
  css += ".progress-ring svg{width:100%;height:100%;transform:rotate(-90deg);}";
  css += ".progress-ring-bg{fill:none;stroke:rgba(255,255,255,0.15);stroke-width:8;}";
  css += ".progress-ring-fill{fill:none;stroke:url(#progressGradient);stroke-width:8;stroke-linecap:round;filter:drop-shadow(0 0 6px rgba(255,230,0,0.6));}";
  
  // Glass orb
  css += ".circle-container{width:480px;height:480px;border-radius:50%;background:linear-gradient(135deg,rgba(255,255,255,0.25) 0%,rgba(255,255,255,0.05) 50%,rgba(255,255,255,0.2) 100%);";
  css += "backdrop-filter:blur(40px) saturate(180%);position:relative;";
  css += "box-shadow:0 40px 100px rgba(0,0,0,0.2),0 15px 40px rgba(0,0,0,0.15),inset 0 0 0 1px rgba(255,255,255,0.4),inset 0 3px 12px rgba(255,255,255,0.6),inset 0 -3px 12px rgba(0,0,0,0.12);";
  css += "display:flex;flex-direction:column;align-items:center;justify-content:center;padding:40px;overflow:visible;z-index:10;}";
  
  // Adjustment hub label
  css += ".hub-label{position:absolute;top:50px;left:50%;transform:translateX(-50%);font-family:'Oswald',sans-serif;font-size:16px;letter-spacing:3px;color:#FFE500;text-transform:uppercase;text-shadow:0 1px 2px rgba(0,0,0,0.1),0 0 10px rgba(255,230,0,0.3);z-index:26;}";
  
  // Status blob
  css += ".status{position:absolute;top:85px;left:50%;transform:translateX(-50%);font-size:11px;color:rgba(0,0,0,0.7);text-align:center;padding:10px 16px;background:rgba(255,255,255,0.5);border-radius:18px;font-weight:700;z-index:25;}";
  
  // Glass circular buttons
  css += ".btn{width:130px;height:130px;border-radius:50%;border:1px solid rgba(255,255,255,0.5);cursor:pointer;";
  css += "background:linear-gradient(135deg,rgba(255,255,255,0.4) 0%,rgba(255,255,255,0.15) 50%,rgba(255,255,255,0.3) 100%);";
  css += "backdrop-filter:blur(10px);color:rgba(0,0,0,0.8);font-family:'Bebas Neue',sans-serif;font-size:14px;letter-spacing:1px;line-height:1.3;";
  css += "transition:all 0.3s ease;box-shadow:0 4px 15px rgba(0,0,0,0.1),inset 0 1px 0 rgba(255,255,255,0.6),inset 0 -1px 0 rgba(0,0,0,0.05);";
  css += "position:absolute;z-index:20;display:flex;align-items:center;justify-content:center;text-align:center;padding:20px;text-decoration:none;}";
  css += ".btn:hover{transform:scale(1.08);background:linear-gradient(135deg,rgba(255,255,255,0.55) 0%,rgba(255,255,255,0.25) 50%,rgba(255,255,255,0.45) 100%);}";
  
  // Button positions
  css += ".btn-left{left:55px;top:50%;transform:translateY(-50%);}.btn-left:hover{transform:translateY(-50%) scale(1.08);}";
  css += ".btn-bottom{bottom:15px;left:50%;transform:translateX(-50%);}.btn-bottom:hover{transform:translateX(-50%) scale(1.08);}";
  css += ".btn-right{right:55px;top:50%;transform:translateY(-50%);}.btn-right:hover{transform:translateY(-50%) scale(1.08);}";
  
  // Yellow glass button
  css += ".btn-yellow{background:linear-gradient(135deg,rgba(255,229,0,0.6) 0%,rgba(255,229,0,0.3) 50%,rgba(255,229,0,0.5) 100%);border:1px solid rgba(255,229,0,0.5);}";
  css += ".btn-yellow:hover{background:linear-gradient(135deg,rgba(255,229,0,0.75) 0%,rgba(255,229,0,0.45) 50%,rgba(255,229,0,0.65) 100%);}";
  
  // Stop button (red glass)
  css += ".btn-stop{width:130px;height:130px;border-radius:50%;border:1px solid rgba(255,100,100,0.4);cursor:pointer;";
  css += "background:linear-gradient(135deg,rgba(255,100,100,0.5) 0%,rgba(255,100,100,0.2) 50%,rgba(255,100,100,0.4) 100%);";
  css += "backdrop-filter:blur(10px);color:rgba(0,0,0,0.8);font-family:'Bebas Neue',sans-serif;font-size:14px;letter-spacing:1px;";
  css += "transition:all 0.3s ease;box-shadow:0 4px 15px rgba(255,100,100,0.2),inset 0 1px 0 rgba(255,255,255,0.6);";
  css += "position:absolute;bottom:15px;left:50%;transform:translateX(-50%);z-index:20;display:flex;align-items:center;justify-content:center;text-decoration:none;}";
  css += ".btn-stop:hover{transform:translateX(-50%) scale(1.08);background:linear-gradient(135deg,rgba(255,100,100,0.65) 0%,rgba(255,100,100,0.35) 50%,rgba(255,100,100,0.55) 100%);}";
  
  // Inline button
  css += ".btn-inline{position:absolute;bottom:15px;left:50%;transform:translateX(-50%);width:130px;height:130px;}.btn-inline:hover{transform:translateX(-50%) scale(1.08);}";
  
  // Hardware status
  css += ".hardware-status{position:fixed;top:30px;right:30px;background:rgba(255,255,255,0.8);backdrop-filter:blur(10px);padding:20px;border-radius:15px;font-size:10px;z-index:100;text-transform:uppercase;}";
  css += ".hw-item{display:flex;justify-content:space-between;margin:8px 0;align-items:center;font-family:'Bebas Neue',sans-serif;color:rgba(0,0,0,0.7);}";
  css += ".hw-indicator{width:14px;height:14px;border-radius:50%;margin-left:12px;}";
  css += ".hw-on{background:#4CAF50;box-shadow:0 0 10px #4CAF50;animation:pulse-hw 2s infinite;}";
  css += ".hw-off{background:#ddd;}";
  css += "@keyframes pulse-hw{0%,100%{box-shadow:0 0 10px #4CAF50;}50%{box-shadow:0 0 20px #4CAF50;}}";
  
  // Running state pulse
  css += ".running .orb-container{animation:orb-pulse 2s ease-in-out infinite;}";
  css += "@keyframes orb-pulse{0%,100%{filter:brightness(1);}50%{filter:brightness(1.15);}}";
  
  // Status large text
  css += ".status-large{font-size:32px;color:rgba(0,0,0,0.8);margin:20px 0;font-family:'Bebas Neue',sans-serif;letter-spacing:4px;z-index:20;}";
  css += ".pressure-display{font-size:16px;color:rgba(0,0,0,0.8);margin:20px 0;font-family:'Bebas Neue',sans-serif;z-index:20;text-align:center;}";
  
  // Info text
  css += ".info-text{font-size:11px;color:rgba(0,0,0,0.7);margin-bottom:15px;font-family:'Bebas Neue',sans-serif;z-index:20;}";
  css += ".info-title{font-size:20px;color:rgba(0,0,0,0.8);margin-bottom:10px;font-family:'Bebas Neue',sans-serif;}";
  
  // Input field
  css += "input[type=number]{width:100%;max-width:200px;padding:15px;font-size:16px;margin:20px 0;background:rgba(255,255,255,0.6);border:1px solid rgba(255,255,255,0.8);border-radius:10px;color:rgba(0,0,0,0.8);font-family:'Bebas Neue',sans-serif;text-align:center;z-index:20;}";
  css += "input::placeholder{color:rgba(0,0,0,0.4);}";
  
  // Manifesto footer
  css += ".manifesto{position:fixed;bottom:30px;left:50%;transform:translateX(-50%);font-family:'Oswald',sans-serif;font-size:9px;color:rgba(120,120,120,0.6);text-align:center;max-width:600px;line-height:1.4;z-index:50;}";
  
  css += "</style>";
  return css;
}

// ============================================
// SVG FILTERS AND PROGRESS RING
// ============================================
String getSvgDefs(float cycleProgress) {
  // Calculate stroke-dashoffset for progress (circumference = 2 * PI * 240 ≈ 1508)
  float circumference = 1508.0;
  float dashOffset = circumference - (circumference * cycleProgress / 100.0);
  
  String svg = "<svg style='position:absolute;width:0;height:0;'><defs>";
  svg += "<filter id='goo'><feGaussianBlur in='SourceGraphic' stdDeviation='10' result='blur'/>";
  svg += "<feColorMatrix in='blur' mode='matrix' values='1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 20 -8' result='goo'/>";
  svg += "<feComposite in='SourceGraphic' in2='goo' operator='atop'/></filter></defs></svg>";
  
  svg += "<div class='progress-ring'><svg viewBox='0 0 486 486'><defs>";
  svg += "<linearGradient id='progressGradient' x1='0%' y1='0%' x2='100%' y2='0%'>";
  svg += "<stop offset='0%' style='stop-color:#FFFF00'/>";
  svg += "<stop offset='50%' style='stop-color:#FFE500'/>";
  svg += "<stop offset='100%' style='stop-color:#FFD000'/></linearGradient></defs>";
  svg += "<circle class='progress-ring-bg' cx='243' cy='243' r='240'/>";
  svg += "<circle class='progress-ring-fill' cx='243' cy='243' r='240' style='stroke-dasharray:1508;stroke-dashoffset:" + String(dashOffset, 0) + ";'/>";
  svg += "</svg></div>";
  
  return svg;
}

// ============================================
// GRADIENT BACKGROUND
// ============================================
String getGradientBackground() {
  String bg = "<div class='gradient-bg'><div class='orb-container'>";
  bg += "<div class='grad-shape blue-1'></div>";
  bg += "<div class='grad-shape blue-2'></div>";
  bg += "<div class='grad-shape yellow-1'></div>";
  bg += "<div class='grad-shape yellow-2'></div>";
  bg += "<div class='grad-shape red-1'></div>";
  bg += "<div class='grad-shape red-2'></div>";
  bg += "</div></div>";
  return bg;
}

// ============================================
// HARDWARE STATUS INDICATOR
// ============================================
String getHardwareStatus() {
  String hw = "<div class='hardware-status'>";
  hw += "<div class='hw-item'><span>Pump</span><div class='hw-indicator " + String(pumpRunning ? "hw-on" : "hw-off") + "'></div></div>";
  hw += "<div class='hw-item'><span>Valve</span><div class='hw-indicator " + String(valveActivated ? "hw-on" : "hw-off") + "'></div></div>";
  hw += "</div>";
  return hw;
}

// ============================================
// GET PHASE INFO
// ============================================
String getPhaseText() {
  if (currentCycleDay >= 1 && currentCycleDay <= 5) return "Menstrual";
  if (currentCycleDay >= 6 && currentCycleDay <= 12) return "Follicular";
  if (currentCycleDay >= 13 && currentCycleDay <= 15) return "Ovulation";
  if (currentCycleDay >= 16 && currentCycleDay <= cycleLength) return "Luteal";
  return "Unknown";
}

// ============================================
// SETUP
// ============================================
void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE_PIN, LOW);
  
  Serial.begin(115200);
  Wire.begin(4, 5);
  
  Serial.println("\n========================================");
  Serial.println("    CYCLE TRACKER INITIALIZING");
  Serial.println("========================================\n");
  
  for (int i = 0; i < 32; i++) {
    cycleArray[i].dayNumber = i + 1;
    cycleArray[i].targetPressure = 0.0;
    cycleArray[i].date = "";
    cycleArray[i].hasData = false;
  }
  
  if (!mpr.begin()) {
    Serial.println("[SENSOR] Failed to find MPRLS sensor!");
  } else {
    Serial.println("[SENSOR] MPRLS initialized successfully!");
  }
  
  WiFiManager wm;
  Serial.println("[WiFi] Starting WiFiManager...");
  wm.setConfigPortalTimeout(180);
  
  bool res = wm.autoConnect("CycleTrackerAP", "password123");
  
  if(!res) {
    Serial.println("[WiFi] Failed to connect - AP mode only");
    Serial.print("[WiFi] AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    
    // Start mDNS after WiFi connects
    if (MDNS.begin("cycletracker")) {
      Serial.println("[mDNS] http://cycletracker.local");
      MDNS.addService("http", "tcp", 80);
    }
  }
  
  timeClient.begin();
  timeClient.update();
  
  // Setup routes
  server.on("/", handleRoot);
  server.on("/inputday", handleInputDay);
  server.on("/submitday", handleSubmitDay);
  server.on("/comfort", handleComfort);
  server.on("/comfort-manual", handleComfortManual);
  server.on("/rigid", handleRigid);
  server.on("/loosen", handleLoosen);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);
  
  server.begin();
  Serial.println("[SERVER] HTTP server started\n");
}

// ============================================
// LOOP
// ============================================
void loop() {
  server.handleClient();
  timeClient.update();
  MDNS.update();
  currentPressure = mpr.readPressure();
}

// ============================================
// CALCULATE DAYS BETWEEN DATES
// ============================================
int calculateDaysBetween(String startDate, String endDate) {
  int startYear = startDate.substring(0, 4).toInt();
  int startMonth = startDate.substring(5, 7).toInt();
  int startDay = startDate.substring(8, 10).toInt();
  
  int endYear = endDate.substring(0, 4).toInt();
  int endMonth = endDate.substring(5, 7).toInt();
  int endDay = endDate.substring(8, 10).toInt();
  
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  int startDays = startDay;
  for (int i = 0; i < startMonth - 1; i++) startDays += daysInMonth[i];
  
  int endDays = endDay;
  for (int i = 0; i < endMonth - 1; i++) endDays += daysInMonth[i];
  
  int daysDiff = endDays - startDays + (endYear - startYear) * 365;
  return daysDiff + 1;
}

// ============================================
// MAIN PAGE
// ============================================
void handleRoot() {
  timeClient.update();
  
  float cycleProgress = 0;
  if (currentCycleDay > 0) {
    cycleProgress = (float)currentCycleDay / (float)cycleLength * 100.0;
  }
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Cycle Tracker</title>";
  html += getBaseStyles();
  html += "</head><body>";
  
  html += getSvgDefs(cycleProgress);
  html += getGradientBackground();
  html += getHardwareStatus();
  
  html += "<div class='center-stage'>";
  html += getSvgDefs(cycleProgress);
  
  html += "<div class='circle-container'>";
  html += "<div class='hub-label'>Adjustment Hub</div>";
  
  // Status blob
  html += "<div class='status'>Pressure: " + String(currentPressure, 1) + " hPa<br>";
  if (currentCycleDay > 0) {
    html += "Day: " + String(currentCycleDay) + "/" + String(cycleLength) + " · " + getPhaseText();
  } else {
    html += "No cycle data";
  }
  html += "</div>";
  
  // Circular buttons
  html += "<a href='/inputday' class='btn btn-left'>Input day</a>";
  html += "<a href='/comfort' class='btn btn-bottom'>Auto adjust</a>";
  html += "<a href='/comfort-manual' class='btn btn-right'>Manual</a>";
  
  html += "</div></div>";
  
  html += "<div class='manifesto'>This is a manifesto project of Katarzyna Dlugosz, MA Fashion Futures New Fashion Perspectives unit. It aims to address women's biological needs in bra design.</div>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// INPUT DAY PAGE
// ============================================
void handleInputDay() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Input Day</title>";
  html += getBaseStyles();
  html += "</head><body>";
  
  html += getGradientBackground();
  html += getHardwareStatus();
  
  html += "<div class='center-stage'>";
  html += "<div class='circle-container'>";
  html += "<div class='hub-label'>Adjustment Hub</div>";
  
  html += "<div class='info-title'>Input cycle day</div>";
  html += "<div class='info-text'>Current pressure: " + String(currentPressure, 1) + " hPa</div>";
  
  html += "<form action='/submitday' method='GET'>";
  html += "<input type='number' name='day' min='1' max='35' placeholder='Enter day (1-35)' required>";
  html += "</form>";
  
  html += "<a href='/submitday' class='btn btn-inline btn-yellow' onclick='this.href=\"/submitday?day=\"+document.querySelector(\"input\").value'>Submit</a>";
  
  html += "</div></div>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// SUBMIT DAY HANDLER
// ============================================
void handleSubmitDay() {
  if (server.hasArg("day")) {
    int day = server.arg("day").toInt();
    String currentDate = timeClient.getFormattedTime().substring(0, 10);
    
    if (day > 32) {
      irregularCycle = true;
      String html = "<!DOCTYPE html><html><head>";
      html += "<meta charset='UTF-8'>";
      html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
      html += "<title>Irregular Cycle</title>";
      html += getBaseStyles();
      html += "</head><body>";
      html += getGradientBackground();
      html += "<div class='center-stage'><div class='circle-container'>";
      html += "<div class='hub-label'>Adjustment Hub</div>";
      html += "<div class='status-large' style='color:#ff5722;'>Irregular cycle</div>";
      html += "<div class='info-text'>Switch to manual settings</div>";
      html += "<a href='/' class='btn btn-inline'>Back</a>";
      html += "</div></div></body></html>";
      server.send(200, "text/html", html);
      return;
    }
    
    currentCycleDay = day;
    currentPressure = mpr.readPressure();
    
    cycleArray[day - 1].dayNumber = day;
    cycleArray[day - 1].targetPressure = currentPressure;
    cycleArray[day - 1].date = currentDate;
    cycleArray[day - 1].hasData = true;
    
    if (day == 1) {
      cycleStartDate = currentDate;
      cycleDayOfStart = 1;
    }
    
    if (day > cycleLength && day <= 32) {
      cycleLength = day;
    }
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Day Updated</title>";
    html += getBaseStyles();
    html += "</head><body>";
    
    html += getGradientBackground();
    
    html += "<div class='center-stage'><div class='circle-container'>";
    html += "<div class='hub-label'>Adjustment Hub</div>";
    html += "<div class='status-large' style='color:#4CAF50;'>Day updated</div>";
    html += "<div class='info-text'>Day " + String(day) + " saved at " + String(currentPressure, 1) + " hPa</div>";
    html += "<div class='info-text'>Change comfort level?</div>";
    html += "<a href='/comfort' class='btn btn-left btn-yellow'>Yes</a>";
    html += "<a href='/' class='btn btn-right'>No</a>";
    html += "</div></div></body></html>";
    
    server.send(200, "text/html", html);
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// ============================================
// COMFORT PAGE (AUTO)
// ============================================
void handleComfort() {
  String todayDate = timeClient.getFormattedTime().substring(0, 10);
  int estimatedDay = currentCycleDay;
  
  if (cycleStartDate != "" && currentCycleDay > 0) {
    int daysSinceStart = calculateDaysBetween(cycleStartDate, todayDate);
    estimatedDay = ((daysSinceStart - 1) % cycleLength) + 1;
  }
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Comfort Settings</title>";
  html += getBaseStyles();
  html += "</head><body>";
  
  html += getGradientBackground();
  html += getHardwareStatus();
  
  html += "<div class='center-stage'><div class='circle-container'>";
  html += "<div class='hub-label'>Comfort Settings</div>";
  
  html += "<div class='status'>Estimated day: " + String(estimatedDay) + "<br>Pressure: " + String(currentPressure, 1) + " hPa</div>";
  
  html += "<a href='/rigid' class='btn btn-left' style='background:linear-gradient(135deg,rgba(255,87,34,0.6),rgba(255,87,34,0.3));border-color:rgba(255,87,34,0.5);'>Rigid</a>";
  html += "<a href='/' class='btn btn-bottom'>Back</a>";
  html += "<a href='/loosen' class='btn btn-right' style='background:linear-gradient(135deg,rgba(33,150,243,0.6),rgba(33,150,243,0.3));border-color:rgba(33,150,243,0.5);'>Loosen</a>";
  
  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// COMFORT PAGE (MANUAL)
// ============================================
void handleComfortManual() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Manual Comfort</title>";
  html += getBaseStyles();
  html += "</head><body>";
  
  html += getGradientBackground();
  delay (1000);
  html += getHardwareStatus();
  
  html += "<div class='center-stage'><div class='circle-container'>";
  html += "<div class='hub-label'>Manual Mode</div>";
  
  html += "<div class='status'>Pressure: " + String(currentPressure, 1) + " hPa</div>";
  
  html += "<a href='/rigid' class='btn btn-left' style='background:linear-gradient(135deg,rgba(255,87,34,0.6),rgba(255,87,34,0.3));border-color:rgba(255,87,34,0.5);'>Rigid</a>";
  html += "<a href='/' class='btn btn-bottom'>Back</a>";
  html += "<a href='/loosen' class='btn btn-right' style='background:linear-gradient(135deg,rgba(33,150,243,0.6),rgba(33,150,243,0.3));border-color:rgba(33,150,243,0.5);'>Loosen</a>";
  
  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// RIGID MODE - PUMP ON
// ============================================
void handleRigid() {
  autoAdjusting = false;
  pumpRunning = true;
  valveActivated = false;
  
  Serial.println("[MANUAL] Rigid mode - PUMP ON");
  digitalWrite(VALVE_PIN, HIGH);
  delay(500);
  digitalWrite(PUMP_PIN, HIGH);
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<title>Pump Active</title>";
  html += getBaseStyles();
  html += "</head><body class='running'>";
  
  html += getGradientBackground();
  html += getHardwareStatus();
  
  html += "<div class='center-stage'><div class='circle-container'>";
  html += "<div class='hub-label'>Adjustment Hub</div>";
  html += "<div class='status-large'>Pump activated</div>";
  html += "<div class='pressure-display'>Pressure: " + String(currentPressure, 1) + " hPa</div>";
  html += "<a href='/stop' class='btn-stop'>Stop</a>";
  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// LOOSEN MODE - VALVE PULSE
// ============================================
void handleLoosen() {
  autoAdjusting = false;
  pumpRunning = false;
  valveActivated = true;
  
  Serial.println("[MANUAL] Loosen mode - VALVE ON");
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE_PIN, HIGH);
  delay(500);
  digitalWrite(VALVE_PIN, LOW);
  valveActivated = false;
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Loosening Complete</title>";
  html += getBaseStyles();
  html += "</head><body>";
  
  html += getGradientBackground();
  html += getHardwareStatus();
  
  html += "<div class='center-stage'><div class='circle-container'>";
  html += "<div class='hub-label'>Adjustment Hub</div>";
  html += "<div class='status-large' style='color:#2196F3;'>Loosening complete</div>";
  html += "<div class='pressure-display'>Pressure: " + String(currentPressure, 1) + " hPa</div>";
  html += "<a href='/' class='btn btn-inline'>Back</a>";
  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

// ============================================
// STOP - ALL OFF
// ============================================
void handleStop() {
  Serial.println("[MANUAL] STOP - All OFF");
  
  pumpRunning = false;
  valveActivated = false;
  autoAdjusting = false;
  
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE_PIN, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================
// STATUS JSON ENDPOINT
// ============================================
void handleStatus() {
  String json = "{";
  json += "\"currentPressure\":" + String(currentPressure) + ",";
  json += "\"targetPressure\":" + String(targetPressure) + ",";
  json += "\"cycleDay\":" + String(currentCycleDay) + ",";
  json += "\"cycleLength\":" + String(cycleLength) + ",";
  json += "\"phase\":\"" + getPhaseText() + "\",";
  json += "\"autoAdjusting\":" + String(autoAdjusting) + ",";
  json += "\"pumpRunning\":" + String(pumpRunning);
  json += "}";
  
  server.send(200, "application/json", json);
}
