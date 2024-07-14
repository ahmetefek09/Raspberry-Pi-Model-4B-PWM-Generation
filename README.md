Various serial communication protocols or signal generation can be implemented with the GPIO pins of the Raspberry Pi 4B model.
The relevant code was coded at the register level, without using a library, for an RPi that does not have an OS.
A similar application can also be done using the Python library for RPi's that have an OS.
If you want to use an external library on an RPi that does not have an OS, you have to install the library via an ethernet connection.

The RPi model 4B uses the BCM2711 processor. So you have to read the datasheet for register addresses and pin functions.
GPIO12 pin was used for PWM generation, so the address of GPFSEL1 register was used. The register address you need will also vary depending on the pin you will use.
