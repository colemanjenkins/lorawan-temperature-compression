#!/usr/bin/env python3

# Authors: Alex Hails & Richard Wang

import base64
import configparser
from datetime import datetime
import json
import numpy as np
import matplotlib.pyplot as plt

import paho.mqtt.client as mqtt

from tkinter import *
from tkinter.ttk import *
import matplotlib
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from fractions import Fraction

from groups import group13


allTimestamps = []  #Time
allTemps = []  #Temp
numberOfBytes = 0
# matplotlib.interactive(True)

# Read in config file with MQTT details.
config = configparser.ConfigParser()
config.read("config.ini")

# MQTT broker details
broker_address = config["mqtt"]["broker"]
username = config["mqtt"]["username"]
password = config["mqtt"]["password"]

# MQTT topic to subscribe to. We subscribe to all uplink messages from the
# devices.
topic = "v3/+/devices/+/up"

decoders = {
    13: group13.decode,
}

#allTimestamps = []
#allTemps = []
def compEff(totalBytes):
    numerator = len(allTimestamps) * 8 #also need to multiply by number of entries in rec window

    ratio = Fraction(numerator, totalBytes)
    ratioString = str(ratio.numerator) + " : " + str(ratio.denominator)
    ratioPercent = ratio.denominator/ratio.numerator*100


    # num of pairs * 8 / bytes recieved
    print("\n Compression Ratio = " + str(ratioString))
    print("\n Compression Percentage: " + str(ratioPercent) + "%")

# Callback when successfully connected to MQTT broker.
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker.")

    if rc != 0:
        print(" Error, result code: {}".format(rc))


# Callback function to handle incoming MQTT messages
def on_message(client, userdata, message):
    # Timestamp on reception.
    current_date = datetime.now()

    # Handle TTN packet format.
    message_str = message.payload.decode("utf-8")
    message_json = json.loads(message_str)
    encoded_payload = message_json["uplink_message"]["frm_payload"]
    raw_payload = base64.b64decode(encoded_payload)

    if len(raw_payload) == 0:
        # Nothing we can do with an empty payload.
        return

    # First byte should be the group number, remaining payload must be parsed.
    # group_number = raw_payload[0]
    group_number = 13
    remaining_payload = raw_payload

    # See if we can decode this payload.
    if group_number in decoders:
        try:
            # Get the decompressed temperature data
            temperatureData = decoders[group_number](remaining_payload)
        except:
            print("Failed to decode payload for Group {}".format(group_number))
            print("  payload: {}".format(remaining_payload))
            return

        if temperatureData == None:
            print("Undecoded message from Group {}".format(group_number))
        else:
            # Update this to plot the data / do other processing with it
            # print("{} temperature: {}".format(current_date.isoformat(), temperatureData))
            temp, timestamp, currentBytes = temperatureData
            global allTimestamps 
            allTimestamps += timestamp
            global allTemps
            allTemps += temp
            global numberOfBytes
            numberOfBytes += currentBytes
            print(temp, timestamp)
            compEff(numberOfBytes)
            fig = plt.figure()
            plt.plot(allTimestamps, allTemps)
            plt.show()
            #currentPlot += 1

    else:
        print("Received message with unknown group: {}".format(group_number))

# plt.ion()

# MQTT client setup
client = mqtt.Client()

# Setup callbacks.
client.on_connect = on_connect
client.on_message = on_message

# Connect to broker.
client.username_pw_set(username, password)
client.tls_set()
client.connect(broker_address, 8883)

# Subscribe to the MQTT topic and start the MQTT client loop
client.subscribe(topic)
client.loop_forever()


