#include "ha_entities_sensors.h"

#include <stdio.h>
#include <string.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "sensor";

typedef struct EnvSensorDescriptor_t
{
    const char * field;
    const char * name;
    const char * unit;
    const char * class;
    BaseType_t enabled;
} EnvSensorDescriptor_t;

#if (DEMO_LIGHT_SENSOR == 1)
    #define ENV_LUX_ENABLED pdTRUE
#else
    #define ENV_LUX_ENABLED pdFALSE
#endif

static const EnvSensorDescriptor_t xEnvSensors[] = {
    { "temp_0_c"  , "Temperature 0", "°C"   , "temperature", pdTRUE },
    { "temp_1_c"  , "Temperature 1", "°C"   , "temperature", pdTRUE },
    { "rh_pct"    , "Humidity"     , "%"    , "humidity"   , pdTRUE },
    { "baro_mbar" , "Pressure"     , "mbar" , "pressure"   , pdTRUE },
    { "white_lux" , "White Light"  , "lx"   , "illuminance", ENV_LUX_ENABLED },
    { "als_lux"   , "Ambient Light", "lx"   , "illuminance", ENV_LUX_ENABLED }
};

typedef struct MotionSensorDescriptor_t
{
    const char * root;
    const char * label;
    const char * unit;
    const char * axis;
    BaseType_t enabled;
} MotionSensorDescriptor_t;

static const MotionSensorDescriptor_t xMotionSensors[] = {
    { "acceleration", "Acceleration" , "mG"    , "x", pdTRUE },
    { "acceleration", "Acceleration" , "mG"    , "y", pdTRUE },
    { "acceleration", "Acceleration" , "mG"    , "z", pdTRUE },
    { "gyro"        , "Gyroscope"    , "mDPS"  , "x", pdTRUE },
    { "gyro"        , "Gyroscope"    , "mDPS"  , "y", pdTRUE },
    { "gyro"        , "Gyroscope"    , "mDPS"  , "z", pdTRUE },
    { "magnetometer", "Magnetometer" , "mGauss", "x", pdTRUE },
    { "magnetometer", "Magnetometer" , "mGauss", "y", pdTRUE },
    { "magnetometer", "Magnetometer" , "mGauss", "z", pdTRUE },
};

typedef struct RangingSensorDescriptor_t
{
    const char * field;
    const char * name;
    const char * unit;
    const char * class;
    BaseType_t enabled;
} RangingSensorDescriptor_t;

static const RangingSensorDescriptor_t xRangingSensors[] = {
    { "distance_mm", "Ranging Distance", "mm", "distance", pdTRUE }
};

#define ARRAY_SIZE(arr) ( sizeof( (arr) ) / sizeof( (arr)[0] ) )

/*-----------------------------------------------------------*/

void HA_EnvSensors_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xEnvSensors ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           xEnvSensors[ i ].field );

        ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

void HA_EnvSensors_PublishConfig( const char * pcThingName,
                                 char * pcPayloadBuffer )
{
#if (DEMO_ENV_SENSOR == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xEnvSensors ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           xEnvSensors[ i ].field );

        if( pdTRUE == xEnvSensors[ i ].enabled )
        {
            size_t xPayloadLength = ( size_t ) snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"name\": \"%s\","
                  "\"unique_id\": \"%s_%s\","
                  "\"state_topic\": \"%s/sensor/env\","
                  "\"value_template\": \"{{ value_json.%s }}\","
                  "\"unit_of_measurement\": \"%s\","
                  "\"device_class\": \"%s\","
                  "\"availability_topic\": \"%s/status/availability\","
                  "\"payload_available\": \"online\","
                  "\"payload_not_available\": \"offline\","
                  "\"retain\": false,"
                  "\"device\": {"
                    "\"identifiers\": [\"%s\"],"
                    "\"manufacturer\": \"STMicroelectronics\","
                    "\"model\": \"%s\","
                    "\"name\": \"%s\""
                  "}"
                "}",
                xEnvSensors[ i ].name,
                pcThingName,
                xEnvSensors[ i ].field,
                pcThingName,
                xEnvSensors[ i ].field,
                xEnvSensors[ i ].unit,
                xEnvSensors[ i ].class,
                pcThingName,
                pcThingName,
                BOARD,
                pcThingName );

            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                ( void ) HA_PublishToTopic( MQTTQoS0,
                                            true,
                                            configPUBLISH_TOPIC,
                                            ( const uint8_t * ) pcPayloadBuffer,
                                            xPayloadLength );
            }
            else
            {
                LogError( ( "Env sensor %d payload truncated", i ) );
            }
        }
        else
        {
            ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
#else
    ( void ) pcThingName;
    ( void ) pcPayloadBuffer;
#endif
}

/*-----------------------------------------------------------*/

void HA_MotionSensors_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xMotionSensors ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s_%s/config",
                           entity,
                           pcThingName,
                           xMotionSensors[ i ].root,
                           xMotionSensors[ i ].axis );

        ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

void HA_MotionSensors_PublishConfig( const char * pcThingName,
                                     char * pcPayloadBuffer )
{
#if (DEMO_MOTION_SENSOR == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xMotionSensors ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s_%s/config",
                           entity,
                           pcThingName,
                           xMotionSensors[ i ].root,
                           xMotionSensors[ i ].axis );

        if( pdTRUE == xMotionSensors[ i ].enabled )
        {
            size_t xPayloadLength = ( size_t ) snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"name\": \"%s %s\","
                  "\"unique_id\": \"%s_%s_%s\","
                  "\"state_topic\": \"%s/sensor/motion\","
                  "\"value_template\": \"{{ value_json.%s.%s }}\","
                  "\"unit_of_measurement\": \"%s\","
                  "\"availability_topic\": \"%s/status/availability\","
                  "\"payload_available\": \"online\","
                  "\"payload_not_available\": \"offline\","
                  "\"retain\": false,"
                  "\"device\": {"
                    "\"identifiers\": [\"%s\"],"
                    "\"manufacturer\": \"STMicroelectronics\","
                    "\"model\": \"%s\","
                    "\"name\": \"%s\""
                  "}"
                "}",
                xMotionSensors[ i ].label,
                xMotionSensors[ i ].axis,
                pcThingName,
                xMotionSensors[ i ].root,
                xMotionSensors[ i ].axis,
                pcThingName,
                xMotionSensors[ i ].root,
                xMotionSensors[ i ].axis,
                xMotionSensors[ i ].unit,
                pcThingName,
                pcThingName,
                BOARD,
                pcThingName );

            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                ( void ) HA_PublishToTopic( MQTTQoS0,
                                            true,
                                            configPUBLISH_TOPIC,
                                            ( const uint8_t * ) pcPayloadBuffer,
                                            xPayloadLength );
            }
            else
            {
                LogError( ( "Motion sensor %s %s payload truncated",
                            xMotionSensors[ i ].label,
                            xMotionSensors[ i ].axis ) );
            }
        }
        else
        {
            ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
#else
    ( void ) pcThingName;
    ( void ) pcPayloadBuffer;
#endif
}

/*-----------------------------------------------------------*/

void HA_RangingSensor_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xRangingSensors ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           xRangingSensors[ i ].field );

        ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

void HA_RangingSensor_PublishConfig( const char * pcThingName,
                                     char * pcPayloadBuffer )
{
#if ( DEMO_RANGING_SENSOR == 1 )
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xRangingSensors ); i++ )
    {
        const RangingSensorDescriptor_t * pxSensor = &xRangingSensors[ i ];

        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           pxSensor->field );

        if( pxSensor->enabled == pdTRUE )
        {
            size_t xPayloadLength = ( size_t ) snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"name\": \"%s\","
                  "\"unique_id\": \"%s_%s\","
                  "\"state_topic\": \"%s/sensor/ranging/reported\","
                  "\"value_template\": \"{{ value_json.%s }}\","
                  "\"unit_of_measurement\": \"%s\","
                  "\"device_class\": \"%s\","
                  "\"state_class\": \"measurement\","
                  "\"availability_topic\": \"%s/status/availability\","
                  "\"payload_available\": \"online\","
                  "\"payload_not_available\": \"offline\","
                  "\"retain\": false,"
                  "\"device\": {"
                    "\"identifiers\": [\"%s\"],"
                    "\"manufacturer\": \"STMicroelectronics\","
                    "\"model\": \"%s\","
                    "\"name\": \"%s\""
                  "}"
                "}",
                pxSensor->name,
                pcThingName,
                pxSensor->field,
                pcThingName,
                pxSensor->field,
                pxSensor->unit,
                pxSensor->class,
                pcThingName,
                pcThingName,
                BOARD,
                pcThingName );

            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                ( void ) HA_PublishToTopic( MQTTQoS0,
                                            true,
                                            configPUBLISH_TOPIC,
                                            ( const uint8_t * ) pcPayloadBuffer,
                                            xPayloadLength );
            }
            else
            {
                LogError( ( "Ranging sensor %s payload truncated", pxSensor->field ) );
            }
        }
        else
        {
            ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
#else
    ( void ) pcThingName;
    ( void ) pcPayloadBuffer;
#endif
}
