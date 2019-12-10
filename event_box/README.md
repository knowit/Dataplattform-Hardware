# Event box

Denne mappen inneholder oppsettet for feedback-knappen som kan brukes til å gi
en negativ/nøytral/positiv rating for et event og feedback knappen som brukes
til å si om man har hatt en bra dag på jobb (eventBox = tre knapper, den andre = to knapper).
Denne utgaven av oppsettet bruker Mongoose OS istedenfor arduino.

## Installere Mongoose OS

https://mongoose-os.com/docs/mongoose-os/quickstart/setup.md

## Bygging

`mos build --platform esp32 --build-var TARGET:[event | mood]`

- mood er for boksen med to knapper
- event er for boksen med tre knapper

## Flashing

1. Koble til boksen med usb
2. `mos flash`

## WiFi oppsett

- boksen starter et AP med SSID "knowit_event_box" med passord "knowit-event"
- Koble på AP og fyll inn ssid og passord på nettverket boksen skal kobles til.

## lage AWS sertifikater

`mos aws-iot-setup --aws-region eu-central-1 --aws-iot-policy mos-default`
