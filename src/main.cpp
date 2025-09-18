#include <ArduinoBLE.h>

// Progressor Service and Characteristic UUIDs
const char *PROGRESSOR_SERVICE_UUID = "7e4e1701-1ea6-40c9-9dcc-13d34ffead57";
const char *DATA_POINT_UUID = "7e4e1702-1ea6-40c9-9dcc-13d34ffead57";
const char *CONTROL_POINT_UUID = "7e4e1703-1ea6-40c9-9dcc-13d34ffead57";

// BLE Service and Characteristics
BLEService progressorService(PROGRESSOR_SERVICE_UUID);
BLECharacteristic dataPointCharacteristic(DATA_POINT_UUID, BLENotify, 20);
BLECharacteristic controlPointCharacteristic(CONTROL_POINT_UUID, BLEWrite, 20);

// Timing variables
unsigned long lastWeightSend = 0;
const unsigned long WEIGHT_INTERVAL = 2000; // Send weight every 2 seconds
bool measurementActive = false;
unsigned long measurementStartTime = 0;

// Mock weight values
float mockWeight = 25.5; // Starting weight in kg
bool weightIncreasing = true;

void sendWeightMeasurement()
{
  // Update mock weight (simulate varying load)
  if (weightIncreasing)
  {
    mockWeight += 0.5;
    if (mockWeight > 50.0)
      weightIncreasing = false;
  }
  else
  {
    mockWeight -= 0.3;
    if (mockWeight < 5.0)
      weightIncreasing = true;
  }

  // Calculate timestamp (microseconds since measurement started)
  uint32_t timestamp = (millis() - measurementStartTime) * 1000;

  // Prepare data packet
  uint8_t data[10];
  data[0] = 0x01; // Response code for weight measurement
  data[1] = 0x08; // Length (8 bytes: 4 for float + 4 for uint32)

  // Convert float to bytes (little endian)
  union
  {
    float f;
    uint8_t bytes[4];
  } weightUnion;
  weightUnion.f = mockWeight;

  data[2] = weightUnion.bytes[0];
  data[3] = weightUnion.bytes[1];
  data[4] = weightUnion.bytes[2];
  data[5] = weightUnion.bytes[3];

  // Convert timestamp to bytes (little endian)
  data[6] = (timestamp) & 0xFF;
  data[7] = (timestamp >> 8) & 0xFF;
  data[8] = (timestamp >> 16) & 0xFF;
  data[9] = (timestamp >> 24) & 0xFF;

  // Send notification
  dataPointCharacteristic.writeValue(data, 10);

  Serial.print("Sent weight: ");
  Serial.print(mockWeight);
  Serial.print(" kg, timestamp: ");
  Serial.println(timestamp);
}

void sendBatteryVoltage()
{
  // Mock battery voltage (3.7V = 3700mV)
  uint32_t batteryVoltage = 3700;

  uint8_t data[6];
  data[0] = 0x00; // Response code for battery voltage
  data[1] = 0x04; // Length (4 bytes for uint32)

  // Convert voltage to bytes (little endian)
  data[2] = (batteryVoltage) & 0xFF;
  data[3] = (batteryVoltage >> 8) & 0xFF;
  data[4] = (batteryVoltage >> 16) & 0xFF;
  data[5] = (batteryVoltage >> 24) & 0xFF;

  // Send notification
  dataPointCharacteristic.writeValue(data, 6);

  Serial.print("Sent battery voltage: ");
  Serial.print(batteryVoltage);
  Serial.println(" mV");
}

void onControlPointWrite(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Control point written");

  if (characteristic.valueLength() > 0)
  {
    uint8_t opcode = characteristic.value()[0];

    Serial.print("Received opcode: 0x");
    Serial.println(opcode, HEX);

    switch (opcode)
    {
    case 0x64: // Tare scale
      Serial.println("Tare scale command received");
      mockWeight = 0.0;
      break;

    case 0x65: // Start weight measurement
      Serial.println("Start weight measurement command received");
      measurementActive = true;
      measurementStartTime = millis();
      lastWeightSend = 0; // Send immediately
      break;

    case 0x66: // Stop weight measurement
      Serial.println("Stop weight measurement command received");
      measurementActive = false;
      break;

    case 0x6E: // Shutdown
      Serial.println("Shutdown command received");
      measurementActive = false;
      break;

    case 0x6F: // Sample battery voltage
      Serial.println("Battery voltage command received");
      sendBatteryVoltage();
      break;

    default:
      Serial.print("Unknown opcode: 0x");
      Serial.println(opcode, HEX);
      break;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("Progressor Emulator Starting...");

  // Initialize BLE
  if (!BLE.begin())
  {
    Serial.println("Starting BLE failed!");
    while (1)
      ;
  }

  // Set device name and local name
  BLE.setLocalName("Progressor");
  BLE.setDeviceName("Progressor");

  // Add characteristics to service
  progressorService.addCharacteristic(dataPointCharacteristic);
  progressorService.addCharacteristic(controlPointCharacteristic);

  // Add service
  BLE.addService(progressorService);

  // Set up control point characteristic callback
  controlPointCharacteristic.setEventHandler(BLEWritten, onControlPointWrite);

  // Start advertising
  BLE.advertise();

  Serial.println("Progressor emulator ready!");
  Serial.println("Waiting for connections...");
}

void loop()
{
  // Poll for BLE events
  BLE.poll();

  // Check if we have a central connected
  BLEDevice central = BLE.central();

  if (central)
  {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    while (central.connected())
    {
      BLE.poll();

      // Send mock weight data if measurement is active
      if (measurementActive && (millis() - lastWeightSend >= WEIGHT_INTERVAL))
      {
        sendWeightMeasurement();
        lastWeightSend = millis();
      }

      delay(10);
    }

    Serial.println("Disconnected from central");
    measurementActive = false;
  }
}
