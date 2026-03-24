#!/bin/bash
#******************************************************************************
# * @file           : hota_update.sh
# * @brief          : Push Host OTA updates
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

export BOARD='B-U585I-IOT02A'
# export BOARD='STM32H573I-DK'
# export BOARD='STM32N6570-DK'
# export BOARD='NUCLEO-U575ZI-Q'
# export BOARD='NUCLEO-H536ZI'
# export BOARD='NUCLEO-N657x0-Q'

# Check for version argument
if [ -z "$1" ]; then
    echo "Usage: $0 <FILE_VERSION>"
    exit 1
fi

export FILE_VERSION="$1"
export BIN_LOCATION="../../project/MXCHIP_STSAFEA110/"
export BIN_FILE="b_u585_iota02_iot_reference.bin"

# Test
export THING_NAME='eval3-020910DF415AD42AC20139'

# Office (Redmond)
# export THING_NAME=' eval3-0209001E215AD42AC20139'

# Office (Bothell)
# export THING_NAME='eval3-0209C04B825AD42AC20139'

# Living Room
# export THING_NAME='eval3-0209706C215AD42AC20139'

# Garage door
# export THING_NAME='eval3-02093088825AD42AC20139'

# Bed room
# export THING_NAME='eval3-0209203A825AD42AC20139'

# export THING_GROUP_NAME="BU585MXFP"
export THING_GROUP_NAME="BU585MXSTSAFEA110"

export AWS_CLI_PROFILE='default'
export ROLE='ld-st67-OTA_ROLE'
export OTA_SIGNING_PROFILE='ldst67OTA_SIGNER'
export S3BUCKET='ld-st67-ota-bucket-redmond-iot'
# CERT_ARN is used only when creating new signing profile <OTA_SIGNING_PROFILE>. Ignored by the script if using existing signing profile
# export CERT_ARN='arn:aws:acm:us-west-1:006151905315:certificate/a441625c-2041-454c-8e0a-24dec43eae95'

# Current directory
export SCRIPT_PATH=$(pwd)

# AWS keys Start
#Reserved for workshop

# AWS Keys End

#Reserved for workshop
#export AWS_REGION="us-east-1"

clear

# source ../.venv/bin/activate

# Reserved for thing updates
# python $SCRIPT_PATH/hota_update.py --profile=$AWS_CLI_PROFILE --thing=$THING_NAME --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path="$BIN_LOCATION" --certarn=$CERT_ARN --board=$BOARD --version=$FILE_VERSION

# Reserved for thing group updates
python $SCRIPT_PATH/hota_update.py --profile=$AWS_CLI_PROFILE --thing-group=$THING_GROUP_NAME --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path="$BIN_LOCATION" --certarn=$CERT_ARN --board=$BOARD --version=$FILE_VERSION

# Reserved for workshop
# python3 $SCRIPT_PATH/hota_update.py                          --thing=$THING_NAME --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path=$BIN_LOCATION --certarn=$CERT_ARN --region=$AWS_REGION --accesskey=$AWS_ACCESS_KEY_ID --secretkey=$AWS_SECRET_ACCESS_KEY --sessiontoken=$AWS_SESSION_TOKEN
