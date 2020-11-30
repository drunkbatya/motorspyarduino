# motorspyarduino
The MotorSpy Arduino firmware

MotorSpy is a Centrogas's project for automatical getting information about 3phase AC engine on gas cylinders recertification tool. Arduino board measures the current, cunsumed by engine in particular cycle of recertification, and vector of rotaring (screw and unscrew the valve). Then arduino send samples to BigBro(Raspberry Pi) through ModBus(RS-485).
The program algoritm looks like this:
Then engine starts to rotare, arduino get the interrupt, and start calculate current (ACS756->ADC) on maximum CPU(Atmega 328P) frequency, when the buffer will be overflowed, the "ZIP" function compress all values in 2 times and sets divider on 2(4-8 etc. in next overflows), that means what arduino will starts calculating on each second (fourth, eighth etc.) interrupt. Then interrupts isn't comming during trashhold time (2 seconds), arduino sets "READY" flag, and any next Modbus reading request starts transfering samples, collected by this cycle. Then transfer was finished, arduino clears the buffer, and starts wait next interrupt.
