# ESP32 TWI Relay

The program is designed to run on a Seeed XIAO ESP32C3 to relay events published to an AMQP topic to a TWI bus to the [train-motor-controller](https://github.com/dnbeesley/train-motor-controller) and [train-points-controller](https://github.com/dnbeesley/train-points-controller) modules via a 3.3V to 5V level shifter.

It also controls an IR beam breaker circuit, which publishes events to an AMQP topic when the beam is broken.

This uses the [AduinoJson](https://arduinojson.org/) and [EspMQTTClient](https://github.com/plapointe6/EspMQTTClient) libraries.

## Configuration

A header file, config.h, which is not committed to the repo, needs to define the following cstring constants:

- AMQP_IP: The AMQP broker IP address
- AMQP_DETECTOR_PUBLISH_TOPIC: The topic to which to publish events about beam breaking
- AMQP_MOTOR_CONTROL_PUBLISH_TOPIC: The topic to which to publish events about the amount of current drawn from the motor controller.
- AMQP_MOTOR_CONTROL_SUBSCRIBE_TOPIC: The topic to which to subscribe to receive events about changes to the motors state
- AMQP_PASSWORD: The password used to log into the broker
- AMQP_POINTS_CONTROL_SUBSCRIBE_TOPIC: The topic to which to subscribe to receive events about changes to the motors state
- AMQP_USERNAME: The user name used to log into the broker
- DEVICE_NAME: The device name to use when connecting to the AMQP broker
- WIFI_PASSWORD: The WiFi password
- WIFI_SSID: The WiFi SSID

It also needs an integer constant defined:

- AMQP_PORT: The port number to use
- MOTOR_CONTROL_ADDRESS: The TWI address of the motor control module
- POINTS_CONTROL_ADDRESS: The TWI address of the points control module

## TWI Relay

The device subscribes to the AMQP topics defined by AMQP_MOTOR_CONTROL_SUBSCRIBE_TOPIC and AMQP_POINTS_CONTROL_SUBSCRIBE_TOPIC. The contents of the messages should be JSON integer arrays. These are transmitted on the TWI bus to the addresses set by MOTOR_CONTROL_ADDRESS and POINTS_CONTROL_ADDRESS respectively.

Roughly every 100ms the device reads two bytes from the motor control unit via the TWI bus and published an event to the AMQP topic named by AMQP_MOTOR_CONTROL_PUBLISH_TOPIC.

## IR beam detection

The IR detectors used detect pulses of 780nm infra-red modulated at 38KHz. Upon detection the output of the IR receive is set to low for a brief time. Through a set of interrupts the ESP32 records when a pulse was last recorded.

The anode of a IR LED and ~ 100&#x03A9; resistor needs to be connected to D8. The output pins of the IR detectors need to be connected to D9 and D10.

### Behaviour

- If the last recorded pulse received by both detectors was more than 100ms ago, the IR LED is turned on with 38KHz.
- If both detectors have a pulse recorded in the last 100ms then the IR led turns off.
- For each IR detector, if it has been more than 100ms since a pulse was received, but more than 100ms since an event was published to AMQP for that pin, a new event is published to indicate the beam is broken.
