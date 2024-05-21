#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "ESP32_MailClient.h"
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>


#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

Adafruit_BMP085 bmp;

const char* ssid = "Ali";
const char* password = "12345678";

// To send Emails using Gmail on port 465 (SSL)
#define emailSenderAccount "alysameh2002@gmail.com"
#define emailSenderPassword "hwyqnfltdivrdqfd"
#define smtpServer "smtp.gmail.com"
#define smtpServerPort 465
#define emailSubject "[ALERT] ESP32 Temperature, Humidity, Pressure, and Altitude"

// Default Recipient Email Address
String inputMessage = "aliosameh2002@gmail.com";
String enableEmailChecked = "checked";
String inputMessage2 = "true";
String inputMessage3 = "5.0";  // Temperature Threshold
String inputMessage4 = "50.0";  // Humidity Threshold
String lastTemperature;
String lastHumidity;
String lastPressure;
String lastAltitude;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Email Notification with Temperature, Humidity, Pressure, and Altitude</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      text-align: center;
      background-color: #282c35; /* Dark background color */
      color: #fff; /* Light text color */
      margin: 0;
      padding: 0;
    }

    h2 {
      color: #61dafb; /* Bright blue header color */
    }

    .reading-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-top: 20px;
    }

    .reading {
      background-color: #3a3e4c; /* Darker background for readings */
      border: 1px solid #61dafb; /* Bright blue border */
      border-radius: 8px;
      padding: 15px;
      margin-bottom: 15px;
      width: 300px;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); /* Light shadow */
    }

    input[type="submit"] {
      background-color: #61dafb; /* Bright blue submit button */
      color: #fff; /* Light text color */
      padding: 10px 15px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
      transition: background-color 0.3s ease; /* Smooth transition */
    }

    input[type="submit"]:hover {
      background-color: #45a049; /* Darker green on hover */
    }
  </style>
  <script>
    function updateReadings() {
      // Make an AJAX request to get sensor readings
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          var sensorData = JSON.parse(xhr.responseText);
          document.getElementById("temperature").innerHTML = "Temperature: " + sensorData.temperature + " &deg;C";
          document.getElementById("humidity").innerHTML = "Humidity: " + sensorData.humidity + " %";
          document.getElementById("pressure").innerHTML = "Pressure: " + sensorData.pressure + " hPa";
          document.getElementById("altitude").innerHTML = "Altitude: " + sensorData.altitude + " meters";
        }
      };
      xhr.open("GET", "/getReadings", true);
      xhr.send();
    }

    // Update readings every 5 seconds
    setInterval(updateReadings, 5000);
  </script>
</head>
<body>
  <h2>ESP32 Sensor Readings</h2>
  <div class="reading-container">
    <div id="temperature" class="reading">Temperature: --</div>
    <div id="humidity" class="reading">Humidity: --</div>
    <div id="pressure" class="reading">Pressure: --</div>
    <div id="altitude" class="reading">Altitude: --</div>
  </div>
  <form action="/sendMail" method="get">
    <input type="submit" value="Send Mail">
  </form>
</body>
</html>)rawliteral";



void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

File sensorDataFile;

bool hasPrintedReadings = false;

String readDHTTemperature() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  } else {
    Serial.print("Temperature: ");
    Serial.println(t);
    return String(t);
  }
}

String readDHTHumidity() {
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  } else {
    Serial.print("Humidity: ");
    Serial.println(h);
    return String(h);
  }
}

String readBMP180() {
  String pressure = String(bmp.readPressure() / 100.0F);
  String altitude = String(bmp.readAltitude());
  Serial.print("Pressure: ");
  Serial.println(pressure);
  Serial.print("Altitude: ");
  Serial.println(altitude);
  return pressure + " hPa, Altitude: " + altitude + " meters";
}

String processor(const String& var) {
  Serial.println("Processing: " + var);
  if (var == "TEMPERATURE") {
    Serial.print("Temperature: ");
    Serial.println(lastTemperature);
    return lastTemperature;
  } else if (var == "HUMIDITY") {
    Serial.print("Humidity: ");
    Serial.println(lastHumidity);
    return lastHumidity;
  } else if (var == "PRESSURE") {
    Serial.print("Pressure: ");
    Serial.println(lastPressure);
    return lastPressure;
  } else if (var == "ALTITUDE") {
    Serial.print("Altitude: ");
    Serial.println(lastAltitude);
    return lastAltitude;
  } else if (var == "EMAIL_INPUT") {
    return inputMessage;
  } else if (var == "ENABLE_EMAIL") {
    return enableEmailChecked;
  } else if (var == "TEMPERATURE_THRESHOLD") {
    return inputMessage3;
  } else if (var == "HUMIDITY_THRESHOLD") {
    return inputMessage4;
  }
  return String();
}


bool emailSent = false;

const char* PARAM_INPUT_1 = "email_input";
const char* PARAM_INPUT_2 = "enable_email_input";
const char* PARAM_INPUT_3 = "temperature_threshold_input";
const char* PARAM_INPUT_4 = "humidity_threshold_input";

void setup() {
  Serial.begin(115200);

  if (!bmp.begin()) {
    Serial.println("Not connected with BMP180/BMP085 sensor, check connections");
    while (1)
      ;
  }

  dht.begin();

  if (!SD.begin(SS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  Serial.println("SD Card Mounted");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  Serial.println("Connected to WiFi");
  Serial.println("IP address of ESP32 is : ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      if (request->hasParam(PARAM_INPUT_2)) {
        inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
        enableEmailChecked = "checked";
      } else {
        inputMessage2 = "false";
        enableEmailChecked = "";
      }
      if (request->hasParam(PARAM_INPUT_3)) {
        inputMessage3 = request->getParam(PARAM_INPUT_3)->value();
      }
      if (request->hasParam(PARAM_INPUT_4)) {
        inputMessage4 = request->getParam(PARAM_INPUT_4)->value();
      }
    } else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    Serial.println(inputMessage2);
    Serial.println(inputMessage3);
    Serial.println(inputMessage4);
    request->send(200, "text/html", "HTTP GET request sent to your ESP.<br><a href=\"/\">Return to Home Page</a>");
  });

  server.on("/sendMail", HTTP_GET, [](AsyncWebServerRequest* request) {
    String emailMessage = String("Temperature: ") + lastTemperature + String(" °C\n") + String("Humidity: ") + lastHumidity + String(" %\n") + String("Pressure: ") + lastPressure + String(" Pa\n") + String("Altitude: ") + lastAltitude + String(" meters");
    if (sendEmailNotification(emailMessage)) {
      Serial.println(emailMessage);
      emailSent = true;
    } else {
      Serial.println("Email failed to send");
    }
    request->send(200, "text/html", "Email sent.<br><a href=\"/\">Return to Home Page</a>");
  });

  server.on("/getReadings", HTTP_GET, [](AsyncWebServerRequest* request) {
  DynamicJsonDocument jsonDoc(200);
  jsonDoc["temperature"] = lastTemperature.toFloat();
  jsonDoc["humidity"] = lastHumidity.toFloat();
  jsonDoc["pressure"] = lastPressure.toFloat();
  jsonDoc["altitude"] = lastAltitude.toFloat();

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  request->send(200, "application/json", jsonString);
  });

  server.onNotFound(notFound);

  server.begin();
}

void loop() {
  if (!hasPrintedReadings) {
    lastTemperature = readDHTTemperature();
    lastHumidity = readDHTHumidity();
    lastPressure = String(bmp.readPressure() / 100.0F);
    lastAltitude = String(bmp.readAltitude());

    // Serial.println("BMP180 Sensor Readings:");
    readBMP180();

    hasPrintedReadings = true;
  }

  float temperature = lastTemperature.toFloat();
  float humidity = lastHumidity.toFloat();

  if (temperature > inputMessage3.toFloat() && humidity > inputMessage4.toFloat() && inputMessage2 == "true" && !emailSent) {
    String emailMessage = String("Temperature above threshold. Current temperature: ") + String(temperature) + String(" °C\n") + String("Humidity: ") + String(humidity) + String(" %");
    if (sendEmailNotification(emailMessage)) {
      Serial.println(emailMessage);
      emailSent = true;
    } else {.
      Serial.println("Email failed to send");
    }
  } else if ((temperature < inputMessage3.toFloat() || humidity < inputMessage4.toFloat()) && inputMessage2 == "true" && emailSent) {
    String emailMessage = String("Temperature below threshold. Current temperature: ") + String(temperature) + String(" °C\n") + String("Humidity: ") + String(humidity) + String(" %");
    if (sendEmailNotification(emailMessage)) {
      Serial.println(emailMessage);
      emailSent = false;
    } else {
      Serial.println("Email failed to send");
    }
  }

  // Append the readings to a file named "sensor_readings.txt" on the SD card
  appendSensorReadingsToFile("/sensor_readings.txt", temperature, humidity, lastPressure, lastAltitude);

  delay(5000);
}

bool sendEmailNotification(String emailMessage) {
  SMTPData smtpData;
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  smtpData.setSender("ESP32", emailSenderAccount);
  smtpData.setPriority("High");
  smtpData.setSubject(emailSubject);
  smtpData.setMessage(emailMessage + String("Pressure: ") + lastPressure + String(" hPa\n") + String("Altitude: ") + lastAltitude + String(" meters"), true);
  smtpData.addRecipient(inputMessage);

  if (!MailClient.sendMail(smtpData)) {
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    return false;
  }
  smtpData.empty();
  return true;
}

void appendSensorReadingsToFile(const String& fileName, float temperature, float humidity, String pressure, String altitude) {
  sensorDataFile = SD.open(fileName, FILE_APPEND);
  if (sensorDataFile) {
    sensorDataFile.print("Timestamp: ");
    sensorDataFile.print(millis());  // Add a timestamp
    sensorDataFile.print(", Temperature: ");
    sensorDataFile.print(temperature);
    sensorDataFile.print(" °C, Humidity: ");
    sensorDataFile.print(humidity);
    sensorDataFile.print(" %, Pressure: ");
    sensorDataFile.print(pressure);
    sensorDataFile.print(" hPa, Altitude: ");
    sensorDataFile.print(altitude);
    sensorDataFile.println(" meters");
    sensorDataFile.close();
  } else {
    Serial.println("Failed to open file for appending");
  }
}