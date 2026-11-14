"""
EE 470 - MQTT Assignment
Section 4.2 F2 - Python MQTT Relay

Receives potentiometer readings from HiveMQ
and sends them to the Hostinger database.

Handles an occasional ADC reading of 1024
by adjusting it to the 10-bit maximum of 1023.
"""

import os
import ssl
from pathlib import Path
from datetime import datetime, timezone
from uuid import uuid4

import requests
import paho.mqtt.client as mqtt
from dotenv import load_dotenv


# --------------------------------------------------
# MQTT settings
# --------------------------------------------------

BROKER = "broker.hivemq.com"
PORT = 8883

TOPIC = "testtopic/temp/outTopic/carlosg470"


# --------------------------------------------------
# Hostinger settings
# --------------------------------------------------

# Load the private API key
load_dotenv(Path(__file__).with_name(".env"))

API_KEY = os.getenv("MQTT_INSERT_API_KEY", "")

INSERT_URL = (
    "https://darkseagreen-stinkbug-987347."
    "hostingersite.com/mqtt/mqtt_insert.php"
)


# --------------------------------------------------
# Send reading to Hostinger
# --------------------------------------------------

def send_to_database(pot_value):

    # Prepare the JSON message
    data = {
        "pot_value": pot_value
    }

    # Authenticate using the existing API key
    headers = {
        "X-API-Key": API_KEY
    }

    try:
        # Send the reading over HTTPS
        response = requests.post(
            INSERT_URL,
            json=data,
            headers=headers,
            timeout=10
        )

        if response.status_code == 201:

            print("Database insert successful!")
            print("Stored value:", pot_value)

        else:

            print("Database insert failed.")
            print("HTTP status:", response.status_code)
            print("Server response:", response.text[:300])

    except requests.RequestException as error:

        print("Error contacting Hostinger:", error)


# --------------------------------------------------
# MQTT connection callback
# --------------------------------------------------

def on_connect(client, userdata, flags,
               reason_code, properties):

    if reason_code == 0:

        print("Connected to HiveMQ!")

        client.subscribe(TOPIC, qos=0)

        print("Subscribed to:", TOPIC)

    else:

        print("MQTT connection failed:", reason_code)


# --------------------------------------------------
# MQTT message callback
# --------------------------------------------------

def on_message(client, userdata, msg):

    # Read the MQTT message
    payload = msg.payload.decode("utf-8").strip()

    try:
        pot_value = int(payload)

    except ValueError:
        print("Invalid MQTT reading:", payload)
        return

    # The ESP8266 uses a 10-bit ADC.
    # Valid readings normally range from 0 to 1023.
    #
    # Occasionally the ESP8266 reports 1024
    # at the maximum potentiometer position.
    # Adjust this one extra count to 1023.

    if pot_value == 1024:

        print("ADC reported 1024.")
        print("Adjusting reading to 1023.")

        pot_value = 1023

    # Reject any other unexpected readings
    elif not 0 <= pot_value <= 1023:

        print("Reading outside expected range:", pot_value)
        return

    # Record when the message arrived
    timestamp = datetime.now(timezone.utc).strftime(
        "%Y-%m-%d %H:%M:%S UTC"
    )

    print()
    print("------------------------------")
    print("Time:", timestamp)
    print("Topic:", msg.topic)
    print("Potentiometer:", pot_value)

    # Send the valid reading to Hostinger
    send_to_database(pot_value)


# --------------------------------------------------
# Main program
# --------------------------------------------------

print("EE 470 - MQTT Python Relay")
print("--------------------------")

# Check that the private API key is available
if not API_KEY:
    raise SystemExit(
        "Missing MQTT_INSERT_API_KEY in relay/.env"
    )

# Create the MQTT client
client = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    client_id="EE470-Relay-" + uuid4().hex[:10]
)

client.on_connect = on_connect
client.on_message = on_message

# Use TLS with certificate verification
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

# Automatically retry MQTT connections
client.reconnect_delay_set(
    min_delay=2,
    max_delay=30
)

try:

    print("Connecting to HiveMQ...")

    client.connect(BROKER, PORT, 60)

    # Keep receiving MQTT messages
    client.loop_forever()

except KeyboardInterrupt:

    print("\nPython relay stopped.")
    client.disconnect()

except (OSError, ssl.SSLError) as error:

    print("MQTT connection error:", error)
