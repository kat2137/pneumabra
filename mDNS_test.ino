#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nCycle Tracker Starting...");
  delay(500);

   Serial.println();
   Serial.print("MAC: ");
   Serial.println(WiFi.macAddress());


  // Connect to WiFi
  WiFiManager wm;
  wm.autoConnect("CycleTrackerAP", "password123");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Start mDNS
  if (MDNS.begin("esp-2996d8.arts")) {
    Serial.println("mDNS: http://esp-2996d8.arts.local");
  }

  // Routes
  server.on("/", []() {
    server.send(200, "text/html", 
      "<h1>Hello World!</h1>"
      "<a href='/test'>Test Page</a>"
    );
  });

  server.on("/test", []() {
    server.send(200, "text/html", "<h1>Test Page</h1>");
  });

  server.begin();
  Serial.println("Server ready");
}

void loop() {
  server.handleClient();
  MDNS.update();
}
