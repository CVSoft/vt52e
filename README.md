# vt52e
VT52-ish serial terminal emulator for TI 68k calculators.  

Right now it does little more than display keycodes. It tries to I/O data, but it probably doesn't work. Press F4 to quit. 

## Todo
Literally everything except managing the display buffer. The keycode displaying code breaks the other display code because printf. At least it no longer needs a battery pull to exit. 
