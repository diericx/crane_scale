#include <ArduinoBLE.h>

// variables for button
const int buttonPin = 4;
int oldButtonState = LOW;
const String PPSUUID = "7e4e1701-1ea6-40c9-9dcc-13d34ffead57";
const String PDPUUID = "7e4e1702-1ea6-40c9-9dcc-13d34ffead57";
const String PCPUUID = "7e4e1703-1ea6-40c9-9dcc-13d34ffead57";

void controlLed(BLEDevice peripheral)
{
  // connect to the peripheral
  Serial.println("Connecting ...");

  if (peripheral.connect())
  {
    Serial.println("Connected");
  }
  else
  {
    Serial.println("Failed to connect!");
    return;
  }

  // discover peripheral attributes
  Serial.println("Discovering attributes ...");
  if (peripheral.discoverAttributes())
  {
    Serial.println("Attributes discovered");
  }
  else
  {
    Serial.println("Attribute discovery failed!");
    peripheral.disconnect();
    return;
  }

  // retrieve the LED characteristic
  BLECharacteristic ledCharacteristic = peripheral.characteristic(PCPUUID.c_str());

  if (!ledCharacteristic)
  {
    Serial.println("Peripheral does not have LED characteristic!");
    peripheral.disconnect();
    return;
  }
  else if (!ledCharacteristic.canWrite())
  {
    Serial.println("Peripheral does not have a writable LED characteristic!");
    peripheral.disconnect();
    return;
  }

  while (peripheral.connected())
  {
    // while the peripheral is connected

    // read the button pin
    int buttonState = digitalRead(buttonPin);

    if (oldButtonState != buttonState)
    {
      // button changed
      oldButtonState = buttonState;

      if (buttonState)
      {
        Serial.println("button pressed");

        // button is pressed, write 0x01 to turn the LED on
        // ledCharacteristic.writeValue((byte)0x01);
      }
      else
      {
        Serial.println("button released");

        // button is released, write 0x00 to turn the LED off
        // ledCharacteristic.writeValue((byte)0x00);
      }
    }
  }

  Serial.println("Peripheral disconnected");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting scan...");

  // configure the button pin as input
  pinMode(buttonPin, INPUT);

  // initialize the Bluetooth® Low Energy hardware
  BLE.begin();

  Serial.println("Bluetooth® Low Energy Central - LED control");

  // start scanning for peripherals
  BLE.scanForUuid(PPSUUID.c_str());
}

void loop()
{
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral)
  {
    // discovered a peripheral, print out address, local name, and advertised service
    Serial.print("Found ");
    Serial.print(peripheral.address());
    Serial.print(" '");
    Serial.print(peripheral.localName());
    Serial.print("' ");
    Serial.print(peripheral.advertisedServiceUuid());
    Serial.println();

    if (peripheral.localName() != "LED")
    {
      return;
    }

    // stop scanning
    BLE.stopScan();

    controlLed(peripheral);

    // peripheral disconnected, start scanning again
    BLE.scanForUuid(PPSUUID);
  }
}
