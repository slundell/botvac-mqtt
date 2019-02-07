# botvac-mqtt
WiFi conversion of the non-connected Neato Botvacs. Communication through MQTT to OpenHAB2.
Borrowed a lot of code from https://github.com/i8beef/I8Beef.Neato.Mqtt for the ESP. Thanks!
Adapted it a bit to handle two way communication and command queueing.

#Hardware installation is shown here:
https://github.com/sstadlberger/botvac-wifi
The only regression I have found with this modification is that the LCD does not dim by itself. 
I do that through OpenHAB but that is not ideal. 

#OpenHAB
OpenHAB communicates with the botvac through MQTT. Example .item and .rules files are included.
The rules takes care of some functionality that is lacking in the botvac. e.g. auto setting the clock.
The botvac does not directly support the `dock` command that is supported by the D-series neatos. 
My workaround is to cancel the cleaning cycle at next charge.







