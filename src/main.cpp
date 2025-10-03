#include <ArduinoBLE.h>
#include "HX711.h"
#include "esp_sleep.h"

// Progressor Service and Characteristic UUIDs
const char *PROGRESSOR_SERVICE_UUID = "7e4e1701-1ea6-40c9-9dcc-13d34ffead57";
const char *DATA_POINT_UUID = "7e4e1702-1ea6-40c9-9dcc-13d34ffead57";
const char *CONTROL_POINT_UUID = "7e4e1703-1ea6-40c9-9dcc-13d34ffead57";

// HX711 Load Cell Configuration
#define LOADCELL_SCK_PIN 2  // D0 pin (GPIO2)
#define LOADCELL_DOUT_PIN 3 // D1 pin (GPIO3)
HX711 scale;

// 236250
float calibration_factor = 14300; // Adjusted for better overall accuracy

// BLE Service and Characteristics
BLEService progressorService(PROGRESSOR_SERVICE_UUID);
BLECharacteristic dataPointCharacteristic(DATA_POINT_UUID, BLENotify, 20);
BLECharacteristic controlPointCharacteristic(CONTROL_POINT_UUID, BLEWrite, 20);

// Timing variables
unsigned long lastWeightSend = 0;
const unsigned long WEIGHT_INTERVAL = 50; // Send weight every 50ms (20Hz)
unsigned long lastWeightPrint = 0;
const unsigned long WEIGHT_PRINT_INTERVAL = 250; // Print weight every 250ms
bool measurementActive = false;
unsigned long measurementStartTime = 0;
float lastValidWeight = 0.0; // Store last valid weight reading

// Idle hibernation variables
unsigned long lastActivityTime = 0;
const unsigned long HIBERNATION_TIMEOUT = 10 * 60 * 1000; // 10 minutes in milliseconds
const unsigned long HIBERNATION_WARNING_TIME = 30 * 1000; // Warn 30 seconds before hibernation
bool hibernationWarningShown = false;

// Forward declarations
void enterDeepSleep();

void resetIdleTimer()
{
  lastActivityTime = millis();
  hibernationWarningShown = false;
}

void checkIdleTimeout()
{
  unsigned long currentTime = millis();
  unsigned long idleTime = currentTime - lastActivityTime;

  // Check if we're approaching hibernation timeout
  if (!hibernationWarningShown && idleTime >= (HIBERNATION_TIMEOUT - HIBERNATION_WARNING_TIME))
  {
    Serial.println("Warning: Device will hibernate in 30 seconds due to inactivity");
    hibernationWarningShown = true;
  }

  // Check if hibernation timeout has been reached
  if (idleTime >= HIBERNATION_TIMEOUT)
  {
    Serial.println("Hibernating device due to 10 minutes of inactivity");
    enterDeepSleep();
  }
}

void enterDeepSleep()
{
  Serial.println("Preparing for deep sleep...");

  // Stop BLE advertising and disconnect
  BLE.stopAdvertise();
  BLE.end();

  // Power down the HX711
  scale.power_down();

  Serial.println("Entering deep sleep mode. Press reset button to wake up.");
  Serial.flush(); // Ensure all serial data is sent before sleeping

  // Enter deep sleep (no wake-up sources configured, only reset will wake)
  esp_deep_sleep_start();
}

float getWeightInKg()
{
  if (scale.is_ready())
  {
    float weight_lbs = scale.get_units(5); // Use 5 samples for accuracy
    return weight_lbs * 0.453592;          // Convert pounds to kg
  }
  return 0.0;
}

float getWeightInLbs()
{
  if (scale.is_ready())
  {
    return scale.get_units(5); // Use 5 samples for accuracy
  }
  return 0.0;
}

void sendWeightMeasurement()
{
  // Only get new reading if scale is ready, otherwise use last valid weight
  float currentWeight;
  if (scale.is_ready())
  {
    float newWeight = scale.get_units(1) * 0.453592; // Convert pounds to kg directly
    if (newWeight != 0.0 || lastValidWeight == 0.0)
    { // Accept zero only if it's the first reading
      lastValidWeight = newWeight;
    }
    currentWeight = lastValidWeight;
  }
  else
  {
    currentWeight = lastValidWeight; // Use last valid reading when scale not ready
  }

  Serial.print("Current weight in KG: ");
  Serial.println(currentWeight);

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
  weightUnion.f = currentWeight;

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

  // Serial.print("Sent weight: ");
  // Serial.print(currentWeight);
  // Serial.print(" kg, timestamp: ");
  // Serial.println(timestamp);
}

void sendDeviceInfo()
{
  // Device info response according to Progressor API
  uint8_t data[20];
  data[0] = 0x02; // Response code for device info
  data[1] = 0x12; // Length (18 bytes)

  // Device name: "Progressor" (11 chars + null terminator, padded to 16 bytes)
  const char *deviceName = "Progressor";
  memset(&data[2], 0, 16);                   // Clear the name field
  strncpy((char *)&data[2], deviceName, 15); // Copy name, leave room for null terminator

  // Firmware version (2 bytes): e.g., 1.0 -> 0x0100
  data[18] = 0x00; // Minor version
  data[19] = 0x01; // Major version

  // Send notification
  dataPointCharacteristic.writeValue(data, 20);

  Serial.println("Sent device info: Progressor v1.0");
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
  resetIdleTimer(); // Reset idle timer on any BLE activity

  if (characteristic.valueLength() > 0)
  {
    uint8_t opcode = characteristic.value()[0];

    Serial.print("Received opcode: 0x");
    Serial.println(opcode, HEX);

    switch (opcode)
    {
    case 0x64: // Tare scale
      Serial.println("Tare scale command received");
      scale.tare(); // Reset the scale to 0
      Serial.println("Scale tared");
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
      // Enter deep sleep after a short delay
      delay(100); // Give time for BLE response
      enterDeepSleep();
      break;

    case 0x6F: // Sample battery voltage
      Serial.println("Battery voltage command received");
      sendBatteryVoltage();
      break;

    case 0x70: // Get device info
      Serial.println("Get device info command received");
      sendDeviceInfo();
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

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.println("Wakeup was not caused by deep sleep (normal boot or reset)");
    break;
  }

  Serial.println("Progressor Emulator Starting...");

  // Initialize HX711 load cell
  Serial.println("Initializing HX711 load cell...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);

  Serial.println("Taring scale... Please ensure no weight is on the scale.");
  delay(2000);  // Give time to remove any weight
  scale.tare(); // Reset the scale to 0
  Serial.println("Scale tared and ready!");

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

  // Initialize idle timer
  resetIdleTimer();
}

void loop()
{
  // Poll for BLE events
  BLE.poll();

  // Check for idle timeout (only when no device is connected)
  BLEDevice central = BLE.central();
  if (!central)
  {
    checkIdleTimeout();
  }

  if (central)
  {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    resetIdleTimer(); // Reset idle timer when device connects

    while (central.connected())
    {
      BLE.poll();

      // Send weight data if measurement is active
      if (measurementActive && (millis() - lastWeightSend >= WEIGHT_INTERVAL))
      {
        sendWeightMeasurement();
        lastWeightSend = millis();
        resetIdleTimer(); // Reset idle timer on weight measurement activity
      }

      delay(10);
    }

    Serial.println("Disconnected from central");
    measurementActive = false;
    resetIdleTimer(); // Reset idle timer when device disconnects
  }
}
