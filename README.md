# dmadrv-adc

Silabs DMADRV based ADC sampling solution. Initialize the module,
tell it what channels to sample and let it get to work. It will start
doing callbacks once the buffer is full and can continue working if a new
buffer is provided.

* Make sure you initialize DMADRV before initializing the dmadrv_adc *

# Sample

There is a sample application that does some sampling cycles under test/sampling.

There is also an I2C DAC playback application under app that can be used to
send data to the device to sample. Really basic playback.

# TODO

It should be possible to select which timer to use, currently it will
take possession of TIMER1.

It should also support Series2 chips.
