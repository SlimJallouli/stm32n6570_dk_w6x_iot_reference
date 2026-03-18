#!/bin/bash

# Define source and destination directories
HOME=~
mbedTLS_VERSON="3.1.1"
mbedTLS_source="$HOME/STM32Cube/Repository/Packs/ARM/mbedTLS/$mbedTLS_VERSON/library/"
mbedTLS_destination="./Middlewares/Third_Party/ARM_Security/"


# Create the destination directory if it doesn't exist
if [ ! -d "$mbedTLS_destination" ]; then
    mkdir -p "$mbedTLS_destination"
fi

echo "Home : " $HOME

# Copy the contents from mbedTLS_source to mbedTLS_destination
cp -r "$mbedTLS_source" "$mbedTLS_destination"
echo "Content copied from $mbedTLS_source to $mbedTLS_destination"

rm -rf "./Middlewares/Third_Party/ARM_RTOS_FreeRTOS/Source/examples"
echo "Removed ./Middlewares/Third_Party/ARM_RTOS_FreeRTOS/Source/examples"

rm -rf "./Middlewares/Third_Party/lwIP_Network_lwIP/rte"
echo "Removed ./Middlewares/Third_Party/lwIP_Network_lwIP/rte"

rm -rf "./Middlewares/Third_Party/lwIP_Network_lwIP/ports/freertos/include"
echo "Removed ./Middlewares/Third_Party/lwIP_Network_lwIP/ports/freertos/include"

rm -f "./Middlewares/ST/ST67W6X_Network_Driver/Api/logging.h"
echo "Removed ./Middlewares/ST/ST67W6X_Network_Driver/Api/logging.h"

rm -f "./Middlewares/ST/ST67W6X_Network_Driver/Api/logging_levels.h"
echo "Removed ./Middlewares/ST/ST67W6X_Network_Driver/Api/logging_levels.h"

rm -f "./Middlewares/ST/ST67W6X_Network_Driver/Api/shell.h"
echo "Removed ./Middlewares/ST/ST67W6X_Network_Driver/Api/shell.h"

rm -f "./Middlewares/ST/ST67W6X_Network_Driver/Api/shell_default_config.h"
echo "Removed ./Middlewares/ST/ST67W6X_Network_Driver/Api/shell_default_config.h"


rm -f "./Middlewares/ST/ST67W6X_Network_Driver/Api/shell_default_config.h"
echo "Removed ./Middlewares/ST/ST67W6X_Network_Driver/Api/shell_default_config.h"

rm -f "./Appli/Core/Src/sysmem.c"
echo "Removed ./Appli/Core/Src/sysmem.c"