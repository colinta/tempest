 Tempest
---------

Use L/R to spin around, B to shoot.  High score is stored in EEPROM.  Things start getting trickier at ~20 points!

![Arduboy Tempest](http://media.colinta.com/arduboy-tempest.jpg?v4)

I commented the functions a bunch, but it's pretty straightforward anyway, just a couple globals (`game`, `player`) and a couple global arrays (`bullets`, `enemies`).

I enabled the "system commands", so if you hold "UP" when you turn the power on, you'll get a flashlight, and if you hold "B" you can turn the sound on and off (by pressing "UP" or "DOWN").

Uses the `Arduboy2` library.
