# iZone2MQTT
This a C program to act as a bridge between iZone's REST interface and an MQTT broker.
The aim is to interface my iZone Airconditioning sontroller to Home Assistant via the magic of MQTT.

This is much a work in progress, the basic functionality is there, but it needs a lot of tiding.

# Dependancies
To compile this program you will need to instal
    libcurl
    jansson
    mosquitto
    
# Building
I'll get around to making a make file eventually. In the meantime try compiling with something like;

  gcc -l curl -l jansson -l mosquitto iZone2MQTT.c
  
# Operation
When the program starts it will send a broadcast message to try to find the iZone CB, hopefully there will only be one on the newtork.
Once it has the CB ip Address it will query it to get the airconditioner settings.
Then it commects to the MQTT broker and sends the current state information.
After this initial stuff, it enters a loop monitoring for a signal from the MQTT broker and the iZone CB. 
If a setting is changed on the airconditioner it is reflected to the MQTT and a request to change from teh MQTT is sent to the airconditioned.

# To Do
 Put in command line options for setting the MQTT broker address
 
 Put in a option to set the root topic.
 
 Make network code more robust
 
 Disconnect if we haven't heard from the iZone CB in a while
 
 Make sure reconnects to MQTT broker is it gets interrupted
 
 Send state infor to MQTT every so often in case Home Assistant gets restarted
 
 Make to auto demonice
 
 Create a startup script
 
 Make a make file

# MQTT Signals


# Home Assistant configuration

