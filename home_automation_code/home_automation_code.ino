#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#include <WiFiClientSecure.h>
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
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;

// 🔊 SPEAK FUNCTION
void speak(String text){
  text.replace(" ", "%20");

  String url = "https://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + text;

  file = new AudioFileSourceICYStream(url.c_str());
  out = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();

  mp3->begin(file, out);

  while(mp3->isRunning()){
    mp3->loop();
  }

  delete mp3;
  delete file;
  delete out;
}

// ---------------- HOME PAGE ----------------
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
 });
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
<title>Voice Assistant</title>

<style>
body {
  margin: 0;
  background: #ffffff;
  font-family: Arial;
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
}
.container { text-align:center; }

.mic-btn {
  width: 140px;
  height: 140px;
  border-radius: 50%;
  background: black;
  color: white;
  font-size: 60px;
  display:flex;
  justify-content:center;
  align-items:center;
  cursor:pointer;
}

.mic-btn.active {
  background: red;
  box-shadow: 0 0 25px red;
}

.status { margin-top:20px; color:#666; }
.text { margin-top:15px; }
</style>

</head>

<body>

<div class="container">
<div id="micBtn" class="mic-btn">🎤</div>
<div id="status" class="status">Tap to speak</div>
<div id="text" class="text"></div>
</div>

<script>
let recognition = null;
let listening = false;

const micBtn = document.getElementById("micBtn");

micBtn.onclick = toggleMic;

function initRecognition(){
 const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;

 if(!SpeechRecognition){
  alert("Not supported");
  return null;
 }

 let rec = new SpeechRecognition();
 rec.lang = "en-US";

 rec.onresult = function(e){
   let text = e.results[0][0].transcript;
   document.getElementById("text").innerText = text;

   fetch('/send?text=' + encodeURIComponent(text));
 };

 return rec;
}

function toggleMic(){
 if(!recognition){
  recognition = initRecognition();
  if(!recognition) return;
 }

 if(!listening){
  recognition.start();
  listening = true;
  micBtn.classList.add("active");
  document.getElementById("status").innerText="Listening...";
 }else{
  recognition.stop();
  listening = false;
  micBtn.classList.remove("active");
  document.getElementById("status").innerText="Stopped";
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

 String json = "{\"temp\":" + String(t) + ",\"lpg\":" + String(lpg) + "}";
 server.send(200,"application/json",json);
}

// 🔥 VOICE + SPEAKER
void handleSend(){
 String text = server.arg("text");
 text.toLowerCase();

 float t = dht.readTemperature();

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
   speak("Sorry I am offline");
 }

 server.send(200,"text/plain","OK");
}

// ---------------- SETUP ----------------

void setup(){
 Serial.begin(115200);

 pinMode(FAN_PIN,OUTPUT);
 pinMode(LIGHT_PIN,OUTPUT);

 dht.begin();

 WiFi.begin(ssid,password);
 while(WiFi.status()!=WL_CONNECTED){ delay(500); }

 server.on("/",handleRoot);
 server.on("/mic",handleMic);
 server.on("/fan",handleFan);
 server.on("/light",handleLight);
 server.on("/data",handleData);
 server.on("/send",handleSend);

 server.begin();
}

void loop(){
 server.handleClient();
}
