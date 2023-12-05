# ESP32 TWI Relay

The program is designed to run on a Seeed XIAO ESP32C3 to relay events published to an AMQP topic to a TWI bus as well as reads the state of devices on the bus and relays them back.

This uses the [AduinoJson](https://arduinojson.org/) and [EspMQTTClient](https://github.com/plapointe6/EspMQTTClient) libraries.

## Configuration

JSON file, written to the SPIFFS, needs to define the following:

```JSON
{
  "auth": {
    "username": "username",
    "password": "password"
  },
  "server": {
    "ipAddress": "192.168.1.2",
    "port": 5672
  },
  "readDevices": [
    {
      "address": 80, // Corresponding to device: 0x50
      "length": 2
    },
    {
      "address": 81, // Corresponding to device: 0x51
      "length": 3
    }
  ],
  "readInterval": 1, // seconds
  "topicPrefix": "i2c-agent",
  "wifi": {
    "ssid": "Some-WIFI-SSID",
    "password": "wifi-password"
  }
}
```

## TWI Relay

If the topicPrefix has been set to i2c-agent the micro-controller will subscribe to the topics with a pattern: i2c-agent/output/#. To write to a device with address 0x50 the topic: i2c-agent/output/50 should be published to with a JSON array of the bytes to write to the device.

For the example config above the micro-controller with read 2 bytes from 0x50 and 3 bytes from 0x51 and publish the byte arrays as JSON array of numbers to the topics: i2c-agent/input/50 and i2c-agent/input/51 respectively.
