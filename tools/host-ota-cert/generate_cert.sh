#!/bin/bash
#******************************************************************************
# * @file           : generate_cert.sh
# * @brief          : generate a certificate for AWS IoT OTA updates
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

export AWS_CLI_PROFILE="default"
export COUNTRY_NAME="FR"
export STATE_OR_PROVINCE_NAME="Paris"
export LOCALITY_NAME="Paris"
export ORGANIZATION_NAME="STMicroelectronics"
export COMMON_NAME="stm32h501"
export CERT_VALID_DAYS=3650

# AWS keys Start

# AWS Keys End

#export AWS_REGION="us-east-1"
export QC_PATH=$(pwd)

clear

python3 $QC_PATH/generate_cert.py --profile $AWS_CLI_PROFILE --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization $ORGANIZATION_NAME --name $COMMON_NAME --valid-days $CERT_VALID_DAYS
#python $QC_PATH/generate_cert.py --accesskey $AWS_ACCESS_KEY_ID --secretkey $AWS_SECRET_ACCESS_KEY  --sessiontoken $AWS_SESSION_TOKEN --region $AWS_REGION  --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization  $ORGANIZATION_NAME --name $COMMON_NAME --valid-days $CERT_VALID_DAYS
