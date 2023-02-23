# Introduction
## What technology does it contain 
MQTT<br>
Smart-config<br>
Web-config<br>
Captive Portal<br>
Wifi<br>
## What is it
It is a little night light that is controlled by a esp8266 chip. I can contol the light with homeassistant, and link them with MQTT<br>
Here is what the light looks like:<br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/light.jpg" width="400px"><br>
Here is what the control unit looks:<br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/esp8266.jpg" width="400px"><br>
## How to use it
For a total new device that has not connect to WIFI before, you will see a open hotspot called "nightlight", once you connect to it, a page would jumps out and leads you to input the WIFI details that you want to connect to, if not, just open any website and should be redirected to it.<br>
The WIFI details would be stored inside the EEPROM so you do not have to re-input the details if the light lost power.<br>
The light would perform as a breathing light when it is connecting to the WIFI, if the light still could not connect to the WIFI (input wrong details, the router power is down etc.), the hotspot "nightlight" would open again and allows you re-input the details, in that case, the WIFI details stored inside would be wiped out.<br>
In the program, there is a I/O pin (GPIO 2 for esp8266) avaliable for the external button, you can give this pin a low-level to swith the status of the light, also, the light status would be updated as well to the MQTT server.
## Performance
###High brightness<br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/bnh1.jpg" width="400px"><br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/bnh2.jpg" width="400px"><br>
###Low brightness<br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/bnl1.jpg" width="400px"><br>
<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/bnl2.jpg" width="400px"><br>
## Hardware Design
I used EasyEDA designed those hardware, here is the circuit so you can design your own one.
The circuit<br>

<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/circuit.png" width="400px"><br>
The PCB<br>

<img src="https://github.com/appleshaman/Night-Light-esp8266/blob/main/docs/pcb.png" width="400px"><br>

