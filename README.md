# Wake Up Light

Wake up naturally. This small weekend endeavor is all about helping you keep a steady sleep rhythm by mimicking the sunrise according to your schedule. This is especially handy during those long winter mornings or when daylight saving time kicks in.

## Hardware Requirements

You'll need a few bits and bobs to get this project shining:

- **Microcontroller**: Something like an Adafruit ESP32-S2 Feather will do nicely, as long as it supports the protocol and pin config of your RTC.
- **RTC Module with Cell Battery**: I went with the RTC DS1302 which has good library support. Uses the ThreeWire protocol.
- **Power Management**: A buck converter for 12V to 3.3V to power the microcontroller and RTC.
- **Power Supply**: Any 12V power supply will do.
- **LED Strip**: 12V LED strip for that sunrise glow.
- **MOSFET**: For controlling the LED strip with PWM. It should support support a drain voltage of at least 12V and a gate voltage of 3.3V. Make sure it can handle the current of your LED strip.

I'll probably add more MOSFETs to control more LED strips in the future.

## Getting Started

1. **Prep Your Workspace**:
   - Get the PlatformIO extension up and running in VSCode.
2. **Get the Code**:
   - Fork and clone the repo so you've got your own sunrise config URL.
3. **Set Your Config URL**:
   - Update the `SUNRISE_API_URL` constant in your code to point to your GitHub fork.
4. **Make It Yours**:
   - Tweak the `/sunrise.json` file in your repo to set up your preferred sunrise times.
   - Copy the `/src/_secrets.h` file, remove the underscore from the file name and change the WiFi credentials to match your network (`/src/secrets.h` is ignored in `.gitignore`).
   - Commit and push your changes to GitHub.
5. **Flash It**:
   - Build and upload the code to your microcontroller using PlatformIO.

## Debugging

Alright, let's tackle those tricky bugs. In your code, there are a few switches you can flip to help you troubleshoot:

- **`WAIT_FOR_SERIAL_OUTPUT`**: Set this to `1` if you want the board to wait for the Serial Monitor to open before running the rest of the code.

- **`DEBUG_INFO`**: When this one's turned on, it spits out all the details about the current time and your sunrise config right into the Serial Monitor.

- **`DEBUG_LED_PWM`**: Think of this as your LED's workout session. Set it to `1`, and your LED will start doing its fade in and out routine, showing off its smooth moves.

## Contributing

If you spot a way to make this little project even better, don't hesitate to fork the repo, make your changes, and hit me up with a pull request. There's just one source file at `/src/main.cpp` so it should be easy to get started.

## Configuration

You'll find the settings in `/sunrise.json`. Adjust the utcOffset to match your local timezone since the RTC sticks to UTC. For example, if you're waking up in Berlin at 7 AM, set `hour: 7` and `utcOffset: 1`.

Happy hacking, and may every morning be as pleasant as a sunrise!
