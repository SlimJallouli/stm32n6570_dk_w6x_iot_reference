#******************************************************************************
# * @file           : mqtt_test.py
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
import cbor2
import time
import logging

# MQTT Broker details
# BROKER = "broker.emqx.io"
BROKER = "test.mosquitto.org"

PORT = 1883
FP_CBOR_CREATE_CERT_PUBLISH_TOPIC  = "aws/certificates/create-from-csr/cbor"
FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC = "aws/certificates/create-from-csr/cbor/accepted"
FP_CBOR_CREATE_CERT_REJECTED_TOPIC = "aws/certificates/create-from-csr/cbor/rejected"

FP_CBOR_REGISTER_PUBLISH_TOPIC     = "aws/provisioning-templates/STM32_FP_Template/provision/cbor"
FP_CBOR_REGISTER_ACCEPTED_TOPIC    = "aws/provisioning-templates/STM32_FP_Template/provision/cbor/accepted"
FP_CBOR_REGISTER_REJECTED_TOPIC    = "aws/provisioning-templates/STM32_FP_Template/provision/cbor/rejected"


# Certificate and Ownership Token Data
certificate_data = {
    "certificatePem": """-----BEGIN CERTIFICATE-----
MIICnDCCAYSgAwIBAgIVAK0zcwFkjFYB2T13NOlTLwg5sPWTMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNTA1MDIyMjIw
NDhaFw00OTEyMzEyMzU5NTlaMCsxKTAnBgNVBAMMIHN0bTMydTUtMDAyQzAwM0Y0
ODQxNTAwNTIwMzYzMjMwMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEzpCCFr6L
4OVfVaobiBoRhnyNCLFJg8C8i/pt339yjio91k3HK/2Z4VztfAms7PFNEyPrhHh+
n3lpg8eHrMl0R6NgMF4wHwYDVR0jBBgwFoAUVDhjbmEJLEvM7bacA+zHMYGLQh8w
HQYDVR0OBBYEFBDzVa8RvvApbHVF6tp9Yl8FXD+RMAwGA1UdEwEB/wQCMAAwDgYD
VR0PAQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQB+TlHc5yZ2CzrwtKvAjucW
O8EXfrEW8DLE6W79B7WNBB5ui0FcFzdTiBaffwssLpnMgVGHedLy/3029o6M7h0E
C7yl/mEkK9Xqv2OxrA/hsRDghKcu2IuLm6jFQ254V6rre9ofLQ3GFh3J86bQnVHg
595jU9UpkBcjAuggFQgpHJ5r7/Ei6OmiJYEYUposzGVAIhwBX0LDeNPdPVvZvzRU
tNOYvoGq01oNdA2Wg38Hn5ieVkYtQW0y5/+OoaARFDw+A89BTUY+PTg3wzu+n4V0
q+TB5eBxn11e5vC/V/wij7APbMY6lHXldQplqLWjjBiKVDThPhd8uuQux62Wg78d
-----END CERTIFICATE-----""",
    "certificateId": "87ca762d6d8ca222fe65a991fffad241b2775c08ad2ce8753669880023272050",
    "certificateOwnershipToken": "J2ZXJzaW9uIjoiMjAxOTEwMjMiLCJjaXBoZXIiOiJBaEZsNGRRN1NGUFEzWTE4ZDJIS2R5bGtuUTBLMXJuenRTMzduZUxsMjhxZndjamlrVzRSR3ZEdUl6ckhRZVp1SXBoUno5R1J5RTVsekxONzRDOXFEbXpEZW4vK1EvR1pDTFIrQUhGTHBVZUNsYkkzYUFMWDhxYUFMbjNhUGVXa0V1eG9yaHFTWndzeWhIVkx6VmtYYmZDMjU1VzBDQWpEdjd0TUozZ201c3NtWEx0a0taUklaMkhEM1pLS0FUNzRRTTFXNTl0RHB6ejNWL2pOVnphVEVBZXBCa1E2dG9XLy9mM0IrSVBJa2Y1KzdEcUdtU0xxWEs2dFdibFlGM2FSZzNNVXZ2OGxEbHpHVVcwRHhhUm95N0ZuRG9vR0JXTGpXanlWYXVkdXpyNEw2RkdMT0NMSldhZz0ifQ=="
}

certificate_data2 = {
    "certificatePem": """-----BEGIN CERTIFICATE-----
MIICnDCCAYSgAwIBAgIVAK0zcwFkjFYB2T13NOlTLwg5sPWTMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNTA1MDIyMjIw
NDhaFw00OTEyMzEyMzU5NTlaMCsxKTAnBgNVBAMMIHN0bTMydTUtMDAyQzAwM0Y0
ODQxNTAwNTIwMzYzMjMwMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEzpCCFr6L
4OVfVaobiBoRhnyNCLFJg8C8i/pt339yjio91k3HK/2Z4VztfAms7PFNEyPrhHh+
n3lpg8eHrMl0R6NgMF4wHwYDVR0jBBgwFoAUVDhjbmEJLEvM7bacA+zHMYGLQh8w
HQYDVR0OBBYEFBDzVa8RvvApbHVF6tp9Yl8FXD+RMAwGA1UdEwEB/wQCMAAwDgYD
VR0PAQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQB+TlHc5yZ2CzrwtKvAjucW
O8EXfrEW8DLE6W79B7WNBB5ui0FcFzdTiBaffwssLpnMgVGHedLy/3029o6M7h0E
C7yl/mEkK9Xqv2OxrA/hsRDghKcu2IuLm6jFQ254V6rre9ofLQ3GFh3J86bQnVHg
595jU9UpkBcjAuggFQgpHJ5r7/Ei6OmiJYEYUposzGVAIhwBX0LDeNPdPVvZvzRU
tNOYvoGq01oNdA2Wg38Hn5ieVkYtQW0y5/+OoaARFDw+A89BTUY+PTg3wzu+n4V0
q+TB5eBxn11e5vC/V/wij7APbMY6lHXldQplqLWjjBiKVDThPhd8uuQux62Wg78
-----END CERTIFICATE-----""",
}

# Callback function when a message is received
# Callback function when a message is received
def on_message(client, userdata, msg):
    try:
        # Print received topic and raw payload
        print(f"Received message on topic: {msg.topic}")
        print(f"Message size: {len(msg.payload)}")
        print(f"Message : \n{msg.payload}")
        print()

        # Respond only if the topic matches FP_CBOR_CREATE_CERT_PUBLISH_TOPIC
        if msg.topic == FP_CBOR_CREATE_CERT_PUBLISH_TOPIC:
            # Prepare the response CBOR message
            response_cbor = cbor2.dumps(certificate_data)

            # Publish CBOR response message
            client.publish(FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC, response_cbor)
            print(f"Published CBOR message to {FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC}")
            print()

            # Print CBOR response before publishing
            print("CBOR response to be published: \n", response_cbor)
            print()

    except Exception as e:
        print("Error processing message:", e)

# Callback function when connected to broker
def on_connect(client, userdata, flags, rc):
    print(f"Connected to {BROKER} broker with result code {str(rc)}")

    client.subscribe(FP_CBOR_CREATE_CERT_PUBLISH_TOPIC)
    print(f"Subscribed to topic: {FP_CBOR_CREATE_CERT_PUBLISH_TOPIC}")
    client.subscribe(FP_CBOR_REGISTER_PUBLISH_TOPIC)
    print(f"Subscribed to topic: {FP_CBOR_REGISTER_PUBLISH_TOPIC}")
    print()

logging.basicConfig(level=logging.DEBUG)
# Initialize MQTT client
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.max_inflight_messages_set(20)

# Connect to broker
client.connect(BROKER, PORT, 60)

# Start loop to listen for messages
client.loop_forever()
