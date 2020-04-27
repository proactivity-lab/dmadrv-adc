# I2C DAC playback app

The playback app will take in a text file of DAC values and send them to
the DAC at the desired rate. One value per line.

Supported DAC: **MCP4725**

# Usage

`python3 i2c_playback.py --bus /dev/i2c-1 testsignal.txt --frequency 3000`

# Using with a RaspberryPi

Standard RPi I2C configuration is slow, but can be made faster by editing

`/boot/config.txt`.

Look for the line where I2C is enabled and add the baudrate option.

`dtparam=i2c_arm=on,i2c_arm_baudrate=1000000`

If the line is not there, I2C needs to be enabled through raspiconfig first.

# Dependencies

The application needs python3 and the *smbus2* library.

`apt-install python3-pip`

`python3 -m pip install smbus2`
