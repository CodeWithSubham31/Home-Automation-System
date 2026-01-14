#include<SoftwareSerial.h>
#include<LiquidCrystal_I2C.h>
#include<Wire.h>
#include<DHT.h>

SoftwareSerial gsm(2,3);
SoftwareSerial bluetooth(4,5);

DHT dht(6,DHT11);
LiquidCrystal_I2C lcd(0x27,16,2);

#define lpg A0
#define ir A1
#define light 7
#define buzzer 8

void sendSMS(String message) {
  gsm.println("AT+CMGF=1"); 
  delay(1000);
  gsm.println("AT+CMGS=\"+919851807533\""); 
  delay(1000);
  gsm.print(message);
  delay(500);
  gsm.write(26); // CTRL+Z
  delay(3000);
}

void setup(){
  gsm.begin(9600);
  bluetooth.begin(9600);
  Serial.begin(9600);
  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("DHT11 Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing");
  delay(2000);
  lcd.clear();

  pinMode(ir,INPUT);
  pinMode(lpg,INPUT);
  pinMode(light,OUTPUT);
  pinMode(buzzer,OUTPUT);
}
void loop(){
  float t = dht.readTemperature(); 
  float h = dht.readHumidity();

  if (isnan(h) || isnan(t)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    Serial.println("DHT Error");
    delay(2000);
    return;
  }

  // LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(t);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(h);
  lcd.print("%");

  //lpg gas detection

  int lpg_value = analogRead(lpg);
  if(lpg_value > 500){
    sendSMS("LPG got detected plese do some action");
    //send message to number
    digitalWrite(buzzer,HIGH);
  }
  else{
    digitalWrite(buzzer,LOW);
  }

  //fire detection

  int ir_value = analogRead(ir);
  if(ir_value > 500){
    sendSMS("Fire got detected plese do some action");
    //send message to number
    digitalWrite(buzzer,HIGH);
  }
  else{
    digitalWrite(buzzer,LOW);
  }
  

  //bluetooth module

  if(bluetooth.available() == 1){
    String cmd = Serial.readString();
    cmd.trim();

    if(cmd == "light on" || cmd == "turn on the light" || cmd == "turn on led" || cmd == "led on"){
      digitalWrite(light,HIGH);
    }

    if(cmd == "light off" || cmd == "turn off the light" || cmd == "turn off the led" || cmd == "led off"){
      digitalWrite(light,LOW);
    }
  }

}
