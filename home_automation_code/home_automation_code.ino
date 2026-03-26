#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>   // 🔥 NEW
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// ---------------- PIN ----------------
#define DHTPIN D2
#define DHTTYPE DHT11
#define FAN_PIN D1
#define LIGHT_PIN D3
#define LPG_PIN A0

// ---------------- WIFI ----------------
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

// ---------------- TELEGRAM ----------------
const char* botToken = "YOUR_BOT_TOKEN";
const char* chatID   = "YOUR_CHAT_ID";

WiFiClientSecure client;

// LPG threshold
#define LPG_THRESHOLD 400

// cooldown (avoid spam)
unsigned long lastAlertTime = 0;
const long alertInterval = 30000; // 30 sec

ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

// ---------------- AUDIO ----------------
AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceICYStream *file = NULL;
AudioOutputI2S *out = NULL;

// ---------------- TELEGRAM FUNCTION ----------------
void sendTelegram(String message){

  client.setInsecure(); // skip SSL certificate

  String url = "/bot" + String(botToken) + "/sendMessage?chat_id=" + chatID + "&text=" + message;

  if(client.connect("api.telegram.org", 443)){
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");
  }
}

// ---------------- SAFE SPEAK ----------------
void speak(String text){

  if(text.length()==0) return;
  if(WiFi.status()!=WL_CONNECTED) return;

  text.replace(" ", "%20");

  String url = "http://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + text;

  if(mp3){ mp3->stop(); delete mp3; mp3=NULL; }
  if(file){ delete file; file=NULL; }
  if(out){ delete out; out=NULL; }

  file = new AudioFileSourceICYStream(url.c_str());
  out = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();

  if(!mp3 || !file || !out) return;

  if(mp3->begin(file,out)){
    unsigned long start = millis();

    while(mp3->isRunning()){
      if(!mp3->loop()){
        mp3->stop();
        break;
      }

      yield();

      if(millis()-start > 8000){
        mp3->stop();
        break;
      }
    }
  }

  if(mp3){ delete mp3; mp3=NULL; }
  if(file){ delete file; file=NULL; }
  if(out){ delete out; out=NULL; }
}

// ---------------- HOME PAGE ----------------
// ❌ CHANGE KORA HOYNI (same as yours)
String homePage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>HAAAS</title>
<style>
body { margin:0; font-family:Arial; background:white; }
.navbar { background:yellow; padding:15px; display:flex; justify-content:space-between; font-weight:bold; }
.container { display:grid; grid-template-columns:repeat(2,1fr); gap:15px; padding:15px; }
.card { background:#f5f5f5; border-radius:15px; padding:20px; text-align:center; }
.toggle { cursor:pointer; }
.active { background:lightgreen !important; }
.icon { font-size:40px; }
.value { font-size:20px; font-weight:bold; }
</style>
</head>

<body>

<div class="navbar">
<div>HAAAS</div>
<div>ESP8266 Smart Home</div>
</div>

<div class="container">

<div class="card toggle" onclick="toggleFan(this)">
<div class="icon">🌀</div>Fan
</div>

<div class="card toggle" onclick="toggleLight(this)">
<div class="icon">💡</div>Light
</div>

<div class="card">
<div class="icon">🌡️</div>
Temperature
<div class="value" id="temp">--</div>
</div>

<div class="card">
<div class="icon">🔥</div>
LPG
<div class="value" id="lpg">--</div>
</div>

<div class="card toggle" onclick="location.href='/mic'">
<div class="icon">🎤</div>Assistant
</div>

</div>

<script>
function toggleFan(el){
 fetch('/fan');
 el.classList.toggle('active');
}

function toggleLight(el){
 fetch('/light');
 el.classList.toggle('active');
}

function updateData(){
 fetch('/data')
 .then(r=>r.json())
 .then(d=>{
  document.getElementById("temp").innerText=d.temp+" °C";
  document.getElementById("lpg").innerText=d.lpg;
 })
 .catch(()=>{});
}
setInterval(updateData,2000);
</script>

</body>
</html>
)rawliteral";

// ---------------- SAME MIC PAGE ----------------
// ❌ unchanged (same as yours)
String micPage = R"rawliteral( ... SAME AS YOUR CODE ... )rawliteral";

// ---------------- ROUTES ----------------

void handleRoot(){ server.send(200,"text/html",homePage); }
void handleMic(){ server.send(200,"text/html",micPage); }

void handleFan(){
 digitalWrite(FAN_PIN,!digitalRead(FAN_PIN));
 server.send(200,"text/plain","OK");
}

void handleLight(){
 digitalWrite(LIGHT_PIN,!digitalRead(LIGHT_PIN));
 server.send(200,"text/plain","OK");
}

// 🔥 UPDATED DATA ROUTE (LPG ALERT)
void handleData(){
 float t = dht.readTemperature();
 int lpg = analogRead(LPG_PIN);

 if(isnan(t)) t = 0;

 // 🔔 LPG ALERT LOGIC
 if(lpg > LPG_THRESHOLD){
   if(millis() - lastAlertTime > alertInterval){
     sendTelegram("🚨 LPG GAS LEAK DETECTED! Value: " + String(lpg));
     lastAlertTime = millis();
   }
 }

 String json = "{";
 json += "\"temp\":" + String(t,1) + ",";
 json += "\"lpg\":" + String(lpg);
 json += "}";

 server.send(200,"application/json",json);
}

void handleSend(){
 if(server.hasArg("text")){
   String text = server.arg("text");
   Serial.println("User: " + text);
 }
 server.send(200,"text/plain","OK");
}

void handleSpeak(){
 if(!server.hasArg("text")){
   server.send(400,"text/plain","No Text");
   return;
 }

 String text = server.arg("text");
 speak(text);

 server.send(200,"text/plain","OK");
}

void handleStatus(){
 if(WiFi.status() == WL_CONNECTED){
   server.send(200,"text/plain","OK");
 }else{
   server.send(200,"text/plain","FAIL");
 }
}

// ---------------- SETUP ----------------

void setup(){
 Serial.begin(115200);

 pinMode(FAN_PIN,OUTPUT);
 pinMode(LIGHT_PIN,OUTPUT);

 digitalWrite(FAN_PIN,LOW);
 digitalWrite(LIGHT_PIN,LOW);

 dht.begin();

 WiFi.begin(ssid,password);

 while(WiFi.status()!=WL_CONNECTED){
   delay(500);
   yield();
 }

 server.on("/",handleRoot);
 server.on("/mic",handleMic);
 server.on("/fan",handleFan);
 server.on("/light",handleLight);
 server.on("/data",handleData);
 server.on("/send",handleSend);
 server.on("/speak",handleSpeak);
 server.on("/status",handleStatus);

 server.begin();
}

// ---------------- LOOP ----------------

void loop(){

 if(WiFi.status() != WL_CONNECTED){
   WiFi.begin(ssid,password);
 }

 server.handleClient();
 yield();
}
