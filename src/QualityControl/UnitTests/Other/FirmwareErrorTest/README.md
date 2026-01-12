# Unit test: custom firmware error message

## Purpose and summary

To test that custom firmware error messages are shown
when not having an USB-TO-UART chip on ESP32S3 boards.

## Hardware setup

An ESP32S3 board is required.
We are using the fully featured USB plug,
**not** the USB-to-UART plug (if available).
We are testing two board configurations: "A" and "B".

| Board configuration | "A"                   | "B"               |
| ------------------- | --------------------- | ----------------- |
| USB Mode            | Hardware CDC and JTAG | USB-OTG (TinyUSB) |
| USB CDC on boot     | Enabled               | Enabled           |
| Upload mode         | USB-OTG (TinyUSB)     | USB-OTG (TinyUSB) |

## Procedure and expected output

**Repeat** this procedure on each board configuration:

1. Hold `Boot`, press `Reset` and release `Boot`.
   The serial port number may change.
2. Upload the test sketch using the required Arduino IDE board configuration.
3. Hit `Reset`. The serial port number may change.
4. Select the proper serial port number and open the serial monitor.
5. Wait for two seconds.
6. The following message must appear (this is OK):

   ```text
   **CUSTOM FIRMWARE ERROR**
   TEST MESSAGE
   ```

7. The previous messages will repeat at timed intervals.
