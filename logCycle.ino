#include <SoftwareSerial.h>
#include <Wire.h>

#define TX_PIN 10
#define RX_PIN 11
#define RESET_PIN 4

#define trigPin 9
#define echoPin 8

#define ledPin 12

float duration, distance;
SoftwareSerial sim800lSerial(TX_PIN, RX_PIN);  // RX, TX Pins
String IMEI;                                   //IMEI
String apn = "";                               //APN
String apn_u = "";                             //APN-Username
String apn_p = "";                             //APN-Password
String url = "";                               //URL of Server
String key = "";
int interval_minuts_send_http = 1;

String send_command_sim800l(String command, int time = 1000, int error = 0) {
  Serial.print("Command => ");
  Serial.println(command);
  sim800lSerial.println(command);
  String response = "";
  unsigned long previous;
  for (previous = millis(); (millis() - previous) < time;) {
    while (sim800lSerial.available()) {
      response = sim800lSerial.readString();
      if (response.indexOf("ERROR") < 0) {
        goto OUTSIDE;
      } else if (error < 5) {
        Serial.print("<= ERROR =>");
        Serial.println(error);
        send_command_sim800l(command, time, error + 1);
      } else {
        Serial.print("<= RESEEEEEEET =>");
        Serial.println(response);
        digitalWrite(RESET_PIN, LOW);
        delay(100);
        digitalWrite(RESET_PIN, HIGH);
        delay(30000);  
      }
    }
  }
OUTSIDE:
  if (response != "") {
    Serial.print("Response => ");
    Serial.println(response);
  }
  return response;
}


void getIMEI() {
  char incomingByte;
  String tempIMEI;
  sim800lSerial.println("AT+GSN");
  unsigned long previous;
  for (previous = millis(); (millis() - previous) < 1000;) {
    if (!sim800lSerial.available()) {
      delay(5);
      continue;
    }
    incomingByte = sim800lSerial.read();
    if (incomingByte == 0) {
      continue;
    }

    if (incomingByte == '\n' && tempIMEI.length() != 17) {
      tempIMEI = "";
    }
    if (incomingByte == '\n' && tempIMEI.length() == 17) {
      tempIMEI.trim();
      IMEI = tempIMEI;
      return;
    }
    tempIMEI += incomingByte;
  }
  IMEI = "";
  sim800lSerial.flush();
}
void gprs_config() {
  Serial.println("---------------------------------------------- gprs_config -----------------------------------------------");
  send_command_sim800l("AT+CMEE=2");
  send_command_sim800l("AT+SAPBR=3,1,CONTYPE,GPRS", 10000);         //response ok
  send_command_sim800l("AT+SAPBR=3,1,\"APN\",\"" + apn + "\"");     //response ok
  send_command_sim800l("AT+SAPBR=3,1,\"USER\",\"" + apn_u + "\"");  //response ok
  send_command_sim800l("AT+SAPBR=3,1,\"PWD\",\"" + apn_p + "\"");   //response ok
  send_command_sim800l("AT+CREG?", 10000);                          // Check the registration status
  send_command_sim800l("AT+SAPBR=1,1", 10000);                      // Enable bearer 1
  delay(2000);
  send_command_sim800l("AT+SAPBR=2,1");  // response +SAPBR: 1,3,"0.0.0.0" Check whether bearer 1 is open.
}

void setup() {

  Serial.begin(9600);
  Serial.println(">> setup <<");
  sim800lSerial.begin(9600);

  pinMode(ledPin, OUTPUT);

  pinMode(RESET_PIN, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);
  delay(30000);  //Wait 30 seconds for start configs sim800l
  while (!send_command_sim800l("AT", 1000)) {
    Serial.println(">> Wait Sim800l Response <<");
  }
  getIMEI();
  delay(30000);
  gprs_config();
  delay(1000);
}

void piscaLed() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);  
    delay(200);                  
    digitalWrite(ledPin, LOW);   
    delay(200);                 
  }
}

float utrasonic_level() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration * .0343) / 2;
  Serial.print("Distance: ");
  Serial.println(distance);
  return distance;
}

void gprs_send(String payload) {
  Serial.println("-------------------------------------- Send HTTP Get Request -----------------------------------------");

  //--------------------------------------------------------
  send_command_sim800l("AT+HTTPINIT", 1000);
  send_command_sim800l("AT+HTTPPARA=\"CID\",1", 10000);
  send_command_sim800l("AT+HTTPPARA=URL," + url + payload, 2000);
  send_command_sim800l("AT+HTTPACTION=0", 2000);

  String responseHTTPACTION = send_command_sim800l("AT+HTTPREAD", 100000);
  Serial.print("responseHTTPACTION:");
  Serial.println(responseHTTPACTION);

  //--------------------------------------------------------
  if (responseHTTPACTION.indexOf("+HTTPACTION: 0,200,") != -1) {
    Serial.println("Resposta 200 recebida!");  
    piscaLed();                               
  }
  delay(10000);
  send_command_sim800l("AT+HTTPTERM", 2000);
  delay(6000);
  //---------------------------------------------------------
}
float check_baterry() {
  String response = send_command_sim800l("AT+CBC", 3000);  //tension battery
  delay(1000);
  response = response.substring(response.indexOf(",") + 1, response.length());
  int bat_percentage = response.toInt();
  response = response.substring(response.indexOf(",") + 1, response.length());
  float bat_voltge = response.toFloat();
  Serial.print("bat_percentage => ");
  Serial.println(bat_percentage);
  Serial.print("bat_voltge => ");
  Serial.print(bat_voltge / 1000.0, 3);
  Serial.println(" V");
  return bat_voltge;
}
void loop() {
  //-------------------------------------------------------------------
  String payload = IMEI + "/" + utrasonic_level() + "/" + check_baterry();
  Serial.print("payload=> ");
  Serial.println(payload);
  gprs_send(payload);
  //-------------------------------------------------------------------
  Serial.print("Wait some minute for next http request: ");
  Serial.println(interval_minuts_send_http);
  delay(interval_minuts_send_http * 60000); 
}
