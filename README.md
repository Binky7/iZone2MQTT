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

# MQTT Signals


# Home Assistant configuration

