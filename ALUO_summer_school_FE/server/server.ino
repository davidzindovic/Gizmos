// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid     = "PoletnaSolaFE";
const char* password = "grasak1234";

// current temperature, humidity, and pressure
float t = 0.0;
float h = 0.0;
float p = 0.0;
float a = 0.0;
float g = 0.0;
float b = 0.0;
float l = 0.0;
uint8_t people = 0;


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// HTML and CSS for the web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; margin: 20px 0; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align: middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Summer school Measuring station</h2>
  

    <p>
      <span class="dht-labels">Temperature</span> 
      <span id="temperature">%TEMPERATURE%</span>
      <sup class="units">&deg;C</sup>
    </p>

    <p>
      <span class="dht-labels">Humidity</span>
      <span id="humidity">%HUMIDITY%</span>
      <sup class="units">%%</sup>
    </p>

    <p>
      <span class="dht-labels">Pressure</span>
      <span id="pressure">%PRESSURE%</span>
      <sup class="units">hPa</sup>
    </p>

        <p>
      <span class="dht-labels">Altitude</span>
      <span id="altitude">%ALTITUDE%</span>
      <sup class="units">m</sup>
    </p>

        <p>
      <span class="dht-labels">Gas</span>
      <span id="gas">%GAS%</span>
      <sup class="units">kOhm</sup>
    </p>

        <p>
      <span class="dht-labels">Brightness</span>
      <span id="brightness">%BRIGHTNESS%</span>
      <sup class="units">lux</sup>
    </p>

        <p>
      <span class="dht-labels">Loudness</span>
      <span id="loudness">%LOUDNESS%</span>
      <sup class="units">Db</sup>
    </p>
       <p>
      <span class="dht-labels">People</span>
      <span id="people">%PEOPLE%</span>
      <sup class="units"></sup>
    </p>

</body>
<script>
function updateData(id, endpoint) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById(id).innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", endpoint, true);
  xhttp.send();
}

setInterval(function () {
  updateData("temperature", "/temperature");
  updateData("humidity", "/humidity");
  updateData("pressure", "/pressure");
  updateData("altitude", "/altitude");
  updateData("gas", "/gas");
  updateData("brightness", "/brightness");
  updateData("loudness", "/loudness");
  updateData("people", "/people");
}, 10000);
</script>
</html>)rawliteral";

// Replaces placeholder with sensor values
String processor(const String& var) {
  if (var == "TEMPERATURE") {
    return String(t);
  } else if (var == "HUMIDITY") {
    return String(h);
  } else if (var == "PRESSURE") {
    return String(p);
  }else if (var == "ALTITUDE") {
    return String(a);
  }else if (var == "GAS") {
    return String(g);
  }
  else if (var == "BRIGHTNESS") {
    return String(b);
  }
  else if (var == "LOUDNESS") {
    return String(l);
  }
  else if (var == "PEOPLE") {
    return String(people);
  }
  return String();
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Route to get temperature
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(t).c_str());
  });

  // Route to get humidity
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(h).c_str());
  });

  // Route to get pressure
  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(p).c_str());
  });

    server.on("/altitude", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(a).c_str());
  });

    server.on("/gas", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(g).c_str());
  });

    server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(b).c_str());
  });

      server.on("/loudness", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(l).c_str());
  });

      server.on("/people", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(people).c_str());
  });

  // Start server
  server.begin();
}

void loop() {
  // Your sensor reading and update logic goes here
l++;
g+=2;
delay(500);
  
}
