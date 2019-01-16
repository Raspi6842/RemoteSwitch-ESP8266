# RemoteSwitch-ESP8266
Sonoff basic (esp8266) custom software

## First steps

<ol>
  <li>Install Arduino IDE</li>
  <li>Install esp8266 by ESP8266 COMMUNITY board in boards manager (Tools > Board > Boards manager)</li>
  <li>Select "Generic ESP8265 Module" (Tools > Board > Generic ESP8265 Module)</li>
  <li>Select flash size "1M (no SPIFFS)" (Tools > Flash Size > 1M (no SPIFFS))</li>
  <li>Helpful might by select Erase Flash "All Flash Content" (Tools > Erase Flash > All Flash Contents)</li>
  <li>Connect USB to Serial Port adapter and select Port (Tools > Port > <code>COMx</code> OR <code>/dev/ttyUSBx</code>). Windows has names "COMx", linux has names "/dev/ttyUSBx"</li>
  <li>Open sketch and upload</li>
  <li>I recommend to enable configuration mode manually, instructions are below</li>
</ol>

## To enable configuration mode:
<ol>
  <li>Disconnect ESP8266 from power source</li>
  <li>Connect ESP8266 to power source</li>
  <li>Push button if LED starts blinking. You have about 1.5s to do this</li>
  <li>ESP8266 will restart in config mode. New WiFi network will be available after this process (default name and password are located in sketch file after includes, defined as <code>AP_SSID</code> and <code>AP_PSK</code>, default it is <code>RemoteSwitch_1_0</code> and <code>switch config</code></li>
  <li>Default IP address is 192.168.4.1</li>
</ol>

## Controlling ESP8266 via network
To control ESP8266 you need know it's IP address. When you got it, you can connect to ESP8266 via browser. If shows <code>It's working</code> everything is ok. To control relay state use <code>/cmd?cmd=set&relay=1</code> and <code>/cmd?cmd=set&relay=0</code>, it always returning text <code>OK</code>. You can also get state of pins using <code>/cmd?cmd=get</code>, result is in JSON format.
