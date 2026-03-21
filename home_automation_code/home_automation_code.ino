#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#include <WiFiClient.h>
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

ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

// ---------------- AUDIO ----------------
AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceICYStream *file = NULL;
AudioOutputI2S *out = NULL;

// ---------------- SAFE SPEAK FUNCTION ----------------
void speak(String text){

  if(text.length() == 0) return;

  text.replace(" ", "%20");

  String url = "http://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + text;

  // cleanup previous (extra safety)
  if(mp3){ delete mp3; mp3 = NULL; }
  if(file){ delete file; file = NULL; }
  if(out){ delete out; out = NULL; }

  file = new AudioFileSourceICYStream(url.c_str());
  out = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();

  if(mp3 && file && out && mp3->begin(file, out)){
    unsigned long start = millis();

    while(mp3->isRunning()){
      if(!mp3->loop()){
        mp3->stop();
        break;
      }

      yield(); // watchdog safe

      // 🔥 timeout safety (max 10 sec)
      if(millis() - start > 10000){
        mp3->stop();
        break;
      }
    }
  }

  // cleanup
  if(mp3){ delete mp3; mp3 = NULL; }
  if(file){ delete file; file = NULL; }
  if(out){ delete out; out = NULL; }
}

// ---------------- HTML ----------------

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

// ---------------- MIC PAGE ----------------

String micPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AI Assistant</title>
<style>
body { margin:0; background:white; display:flex; justify-content:center; align-items:center; height:100vh; font-family:Arial;}
.mic { width:140px;height:140px;border-radius:50%;background:black;color:white;font-size:60px;display:flex;justify-content:center;align-items:center;cursor:pointer;}
.active{background:red;}
</style>
</head>
<body>

<div class="mic" id="mic">🎤</div>

<script>
let rec;
let listening=false;

document.getElementById("mic").onclick=()=>{

 const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
 if(!SpeechRecognition){ alert("Not supported"); return; }

 if(!rec){
   rec = new SpeechRecognition();
   rec.lang="en-US";
   rec.onresult=(e)=>{
     let text=e.results[0][0].transcript;
     fetch('/send?text='+encodeURIComponent(text));
   };
   rec.onend=()=>{ listening=false; document.getElementById("mic").classList.remove("active"); };
 }

 if(!listening){
   rec.start();
   listening=true;
   document.getElementById("mic").classList.add("active");
 }else{
   rec.stop();
 }
}
</script>

</body>
</html>
)rawliteral";

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

void handleData(){
 float t = dht.readTemperature();
 int lpg = analogRead(LPG_PIN);

 if(isnan(t)) t = 0;

 String json = "{\"temp\":" + String(t) + ",\"lpg\":" + String(lpg) + "}";
 server.send(200,"application/json",json);
}

void handleSend(){
 String text = server.arg("text");
 text.toLowerCase();

 float t = dht.readTemperature();
 if(isnan(t)) t = 0;

 if(text.indexOf("temperature")>=0){
   speak("Temperature is " + String(t) + " degree");
 }

 else if(text.indexOf("fan on")>=0){
   digitalWrite(FAN_PIN,HIGH);
   speak("Fan turned on");
 }

 else if(text.indexOf("fan off")>=0){
   digitalWrite(FAN_PIN,LOW);
   speak("Fan turned off");
 }

 else{
   speak("Command not recognized");
 }

 server.send(200,"text/plain","OK");
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

 unsigned long start = millis();
 while(WiFi.status()!=WL_CONNECTED){
   delay(500);
   yield();

   // 🔥 timeout (20 sec)
   if(millis() - start > 20000){
     ESP.restart();
   }
 }

 server.on("/",handleRoot);
 server.on("/mic",handleMic);
 server.on("/fan",handleFan);
 server.on("/light",handleLight);
 server.on("/data",handleData);
 server.on("/send",handleSend);

 server.begin();
}

// ---------------- LOOP ----------------

void loop(){
 server.handleClient();
 yield();
}
