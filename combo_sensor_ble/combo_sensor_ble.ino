/*
  Description: Transmits Arduino Nano 33 BLE Sense sensor readings over BLE,
               including temperature, humidity, barometric pressure, and color,
               using the Bluetooth Generic Attribute Profile (GATT) Specification
  Author: Gary Stafford
  Reference: Source code adapted from `Nano 33 BLE Sense Getting Started`
  Adapted from Arduino BatteryMonitor example by Peter Milne
*/

/*
Generic Attribute Profile (GATT) Specifications
GATT Service: Environmental Sensing Service (ESS) Characteristics

Temperature
sint16 (decimalexponent -2)
Unit is in degrees Celsius with a resolution of 0.01 degrees Celsius
https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.temperature.xml

Humidity
uint16 (decimalexponent -2)
Unit is in percent with a resolution of 0.01 percent
https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.humidity.xml

Barometric Pressure
uint32 (decimalexponent -1)
Unit is in pascals with a resolution of 0.1 Pa
https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.pressure.xml
*/

#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>
#include <Arduino_LPS22HB.h>
#include <Arduino_APDS9960.h>

const int UPDATE_FREQUENCY = 2000;     // Update frequency in ms
const float CALIBRATION_FACTOR = -4.0; // Temperature calibration factor (Celsius)

int previousTemperature = 0;
unsigned int previousHumidity = 0;
unsigned int previousPressure = 0;
String previousColor = "";
int r, g, b, a;
long previousMillis = 0; // last time readings were checked, in ms

BLEService environmentService("181A"); // Standard Environmental Sensing service

BLEIntCharacteristic tempCharacteristic("2A6E",               // Standard 16-bit Temperature characteristic
                                        BLERead | BLENotify); // Remote clients can read and get updates

BLEUnsignedIntCharacteristic humidCharacteristic("2A6F", // Unsigned 16-bit Humidity characteristic
                                                 BLERead | BLENotify);

BLEUnsignedIntCharacteristic pressureCharacteristic("2A6D",               // Unsigned 32-bit Pressure characteristic
                                                    BLERead | BLENotify); // Remote clients can read and get updates

BLECharacteristic colorCharacteristic("936b6a25-e503-4f7c-9349-bcc76c22b8c3", // Custom Characteristics
                                      BLERead | BLENotify, 24);               // 1234,5678,

BLEDescriptor colorLabelDescriptor("2901", "16-bit ints: r, g, b, a");

void setup() {
    Serial.begin(9600); // Initialize serial communication
    // while (!Serial); // only when connected to laptop

    if (!HTS.begin()) { // Initialize HTS221 sensor
        Serial.println("Failed to initialize humidity temperature sensor!");
        while (1);
    }

    if (!BARO.begin()) { // Initialize LPS22HB sensor
        Serial.println("Failed to initialize pressure sensor!");
        while (1);
    }

    // Avoid bad readings to start bug
    // https://forum.arduino.cc/index.php?topic=660360.0
    BARO.readPressure();
    delay(1000);

    if (!APDS.begin()) { // Initialize APDS9960 sensor
        Serial.println("Failed to initialize color sensor!");
        while (1);
    }

    pinMode(LED_BUILTIN, OUTPUT); // Initialize the built-in LED pin

    if (!BLE.begin()) { // Initialize NINA B306 BLE
        Serial.println("starting BLE failed!");
        while (1);
    }

    BLE.setLocalName("ArduinoNano33BLESense");    // Set name for connection
    BLE.setAdvertisedService(environmentService); // Advertise environment service

    environmentService.addCharacteristic(tempCharacteristic);     // Add temperature characteristic
    environmentService.addCharacteristic(humidCharacteristic);    // Add humidity characteristic
    environmentService.addCharacteristic(pressureCharacteristic); // Add pressure characteristic
    environmentService.addCharacteristic(colorCharacteristic);    // Add color characteristic

    colorCharacteristic.addDescriptor(colorLabelDescriptor); // Add color characteristic descriptor

    BLE.addService(environmentService); // Add environment service

    tempCharacteristic.setValue(0);     // Set initial temperature value
    humidCharacteristic.setValue(0);    // Set initial humidity value
    pressureCharacteristic.setValue(0); // Set initial pressure value
    colorCharacteristic.setValue("");   // Set initial color value

    BLE.advertise(); // Start advertising
    Serial.print("Peripheral device MAC: ");
    Serial.println(BLE.address());
    Serial.println("Waiting for connections...");
}

void loop() {
    BLEDevice central = BLE.central(); // Wait for a BLE central to connect

    // If central is connected to peripheral
    if (central) {
        Serial.print("Connected to central MAC: ");
        Serial.println(central.address()); // Central's BT address:

        digitalWrite(LED_BUILTIN, HIGH); // Turn on the LED to indicate the connection

        while (central.connected()) {
            long currentMillis = millis();
            // After UPDATE_FREQUENCY ms have passed, check temperature & humidity
            if (currentMillis - previousMillis >= UPDATE_FREQUENCY) {
                previousMillis = currentMillis;
                updateReadings();
            }
        }

        digitalWrite(LED_BUILTIN, LOW); // When the central disconnects, turn off the LED
        Serial.print("Disconnected from central MAC: ");
        Serial.println(central.address());
    }
}

int getTemperature(float calibration) {
    // Get calibrated temperature as signed 16-bit int for BLE characteristic
    return (int) (HTS.readTemperature() * 100) + (int) (calibration * 100);
}

unsigned int getHumidity() {
    // Get humidity as unsigned 16-bit int for BLE characteristic
    return (unsigned int) (HTS.readHumidity() * 100);
}

unsigned int getPressure() {
    // Get humidity as unsigned 32-bit int for BLE characteristic
    return (unsigned int) (BARO.readPressure() * 1000 * 10);
}

void getColor() {
    // check if a color reading is available
    while (!APDS.colorAvailable()) {
        delay(5);
    }

    // Get color as (4) unsigned 16-bit ints
    int tmp_r, tmp_g, tmp_b, tmp_a;
    APDS.readColor(tmp_r, tmp_g, tmp_b, tmp_a);
    r = tmp_r;
    g = tmp_g;
    b = tmp_b;
    a = tmp_a;
}

void updateReadings() {
    int temperature = getTemperature(CALIBRATION_FACTOR);
    unsigned int humidity = getHumidity();
    unsigned int pressure = getPressure();
    getColor();

    if (temperature != previousTemperature) { // If reading has changed
        Serial.print("Temperature: ");
        Serial.println(temperature);
        tempCharacteristic.writeValue(temperature); // Update characteristic
        previousTemperature = temperature;          // Save value
    }

    if (humidity != previousHumidity) { // If reading has changed
        Serial.print("Humidity: ");
        Serial.println(humidity);
        humidCharacteristic.writeValue(humidity);
        previousHumidity = humidity;
    }

    if (pressure != previousPressure) { // If reading has changed
        Serial.print("Pressure: ");
        Serial.println(pressure);
        pressureCharacteristic.writeValue(pressure);
        previousPressure = pressure;
    }

    // Get color reading everytime
    // e.g. "12345,45678,89012,23456"
    String stringColor = "";
    stringColor += r;
    stringColor += ",";
    stringColor += g;
    stringColor += ",";
    stringColor += b;
    stringColor += ",";
    stringColor += a;

    if (stringColor != previousColor) { // If reading has changed

        byte bytes[stringColor.length() + 1];
        stringColor.getBytes(bytes, stringColor.length() + 1);

        Serial.print("r, g, b, a: ");
        Serial.println(stringColor);

        colorCharacteristic.writeValue(bytes, sizeof(bytes));
        previousColor = stringColor;
    }
}
