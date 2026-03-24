#******************************************************************************
# * @file           : delete_retained_mqtt_messages.py
# * @brief          : 
# ******************************************************************************
# * @attention
# *
# * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
# * All rights reserved.</center></h2>
# *
# * This software component is licensed by ST under BSD 3-Clause license,
# * the "License"; You may not use this file except in compliance with the
# * License. You may obtain a copy of the License at:
# *                        opensource.org/licenses/BSD-3-Clause
# ******************************************************************************

import paho.mqtt.client as mqtt
import time
import argparse

# AWS IoT Core settings
MQTT_HOST = "a1qwhobjtvew8t-ats.iot.us-west-1.amazonaws.com"
MQTT_PORT = 8883

# TLS credentials
CA_FILE   = "certs/AmazonRootCA1.pem"
CERT_FILE = "certs/b281fb1a88c494eefd0eeea44a2cda413f9b6915a46a1411f425527cef89b6b9-certificate.pem.crt"
KEY_FILE  = "certs/b281fb1a88c494eefd0eeea44a2cda413f9b6915a46a1411f425527cef89b6b9-private.pem.key"

def get_retained_topics(thing_name):
    return [
        f"homeassistant/sensor/{thing_name}_magnetometer_mGauss_z/config",
        f"homeassistant/sensor/{thing_name}_magnetometer_mGauss_y/config",
        f"homeassistant/sensor/{thing_name}_magnetometer_mGauss_x/config",
        f"homeassistant/sensor/{thing_name}_gyro_mDPS_z/config",
        f"homeassistant/sensor/{thing_name}_gyro_mDPS_y/config",
        f"homeassistant/sensor/{thing_name}_gyro_mDPS_x/config",
        f"homeassistant/sensor/{thing_name}_acceleration_mG_z/config",
        f"homeassistant/sensor/{thing_name}_acceleration_mG_y/config",
        f"homeassistant/sensor/{thing_name}_acceleration_mG_x/config",
        f"homeassistant/sensor/{thing_name}_baro_mbar/config",
        f"homeassistant/sensor/{thing_name}_rh_pct/config",
        f"homeassistant/sensor/{thing_name}_temp_1_c/config",
        f"homeassistant/sensor/{thing_name}_temp_0_c/config",
        f"homeassistant/sensor/{thing_name}_lux_sensor/config",
        f"homeassistant/binary_sensor/{thing_name}_button/config",
        f"homeassistant/switch/{thing_name}_led/config",
        f"homeassistant/update/{thing_name}_fw/config",
        f"{thing_name}/led/reported",
        f"{thing_name}/led/desired",
        f"{thing_name}/sensor/button/reported",
        f"{thing_name}/status/availability",
        f"{thing_name}/fw/state"
    ]

def on_connect(client, userdata, flags, rc):
    print(f"✅ Connected to AWS IoT Core with code {rc}")

def clear_retained_messages(client, retained_topics, debug=False):
    for topic in retained_topics:
        if debug:
            print(f"🗑️ Clearing: {topic}")
        client.publish(topic, payload="", qos=1, retain=True)
        time.sleep(0.1)

def main():
    parser = argparse.ArgumentParser(description="Clear retained MQTT messages for a specific Thing.")
    parser.add_argument("--thing-name", type=str, required=True, help="Specify the AWS IoT Thing Name")
    parser.add_argument("-d", action="store_true", help="Debug output flag", required=False)
    args = parser.parse_args()

    retained_topics = get_retained_topics(args.thing_name)

    client = mqtt.Client(client_id="HA-Cleanup")
    client.tls_set(ca_certs=CA_FILE, certfile=CERT_FILE, keyfile=KEY_FILE)
    client.on_connect = on_connect

    print("🔌 Connecting to AWS IoT Core...")
    client.connect(MQTT_HOST, MQTT_PORT)
    client.loop_start()
    time.sleep(2)

    clear_retained_messages(client, retained_topics, debug=args.d)

    client.loop_stop()
    client.disconnect()
    print("✅ All retained messages cleared.")

if __name__ == "__main__":
    main()
