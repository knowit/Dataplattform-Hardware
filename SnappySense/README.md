# SnappySense

## Overview

An esp32 that gathers sensor data from a co2 sensor, a microphone and a DHT (Digital Humidity and Temperature sensor).

Upload new code to esp
mos build --arch esp32 && mos flash

Needs to be connected to wifi with the command
mos wifi SSID password

Needs certificates in order to send the data using mqtt to aws iot MQTT server.
Connect to aws with the command
mos aws-iot-setup --aws-profile hakgimknowit2 --aws-region eu-central-1 --aws-iot-policy mos-default
The aws-profile have to match the profile set up in ~/.aws/credentials



