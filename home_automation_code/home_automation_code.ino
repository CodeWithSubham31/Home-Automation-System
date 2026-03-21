#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#include <WiFiClientSecure.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#define DHTPIN D2
#define DHTTYPE DHT11
#define FAN_PIN D1
#define LIGHT_PIN D3
#define LPG_PIN A0

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

// AUDIO
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

// -------- HTML HOME --------
String homePage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>HAAAS</title>
<style>
body{margin:0;font-family:Arial;background:white;}
.navbar{background:yellow;padding:15px;display:flex;justify-content:space-between;font-weight:bold;}
.container{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;padding:15px;}
.card{background:#f5f5f5;border-radius:15px;padding:20px;text-align:center;}
.toggle{cursor:pointer;}
.active{background:lightgreen !important;}
.icon{font-size:40px;}
.value{font-size:20px;font-weight:bold;}
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

// -------- MIC PAGE --------
String micPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Voice</title>
<style>
body{display:flex;justify-content:center;align-items:center;height:100vh;font-family:Arial;}
.mic{width:140px;height:140px;border-radius:50%;background:black;color:white;font-size:60px;display:flex;align-items:center;justify-content:center;}
</style>
</head>

<body>

<div class="mic" onclick="start()">🎤</div>

<script>
function start(){
 const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
 let rec = new SpeechRecognition();
 rec.lang="en-US";

 rec.onresult = function(e){
   let text = e.results[0][0].transcript;
   fetch('/send?text='+encodeURIComponent(text));
 };

 rec.start();
}
</script>

</body>
</html>
)rawliteral";

// -------- ROUTES --------

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

// 🔥 VOICE LOGIC + SPEAKER
void handleSend(){
 String text = server.arg("text");
 text.toLowerCase();

 float t = dht.readTemperature();

 if(text.indexOf("temperature")>=0){
   String ans = "Temperature is " + String(t) + " degree";
   speak(ans);
   server.send(200,"text/plain",ans);
 }

 else if(text.indexOf("fan on")>=0){
   digitalWrite(FAN_PIN,HIGH);
   speak("Fan turned on");
   server.send(200,"text/plain","Fan ON");
 }

 else if(text.indexOf("fan off")>=0){
   digitalWrite(FAN_PIN,LOW);
   speak("Fan turned off");
   server.send(200,"text/plain","Fan OFF");
 }

 else{
   speak("Sorry I am offline");
   server.send(200,"text/plain","Offline");
 }
}

// -------- SETUP --------

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
