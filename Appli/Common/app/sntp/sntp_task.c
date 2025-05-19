/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : sntp_task.c
  * @brief          : SNTP Service
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "logging_levels.h"
#define LOG_LEVEL    LOG_INFO

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "sys_evt.h"

#include <string.h>
#include <time.h>

#include "w6x_api.h"
#include "w6x_types.h"

#include "sntp_task.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;

static const char *WeekDayString[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char *MonthString[]   = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* Private function prototypes -----------------------------------------------*/
static void WaitForNetworkConnection(void);
static W6X_Status_t ConfigureSNTPClient(uint8_t *SNTP_Enable, int16_t *SNTP_Timezone);
static W6X_Status_t GetSNTPTime(uint8_t *Time);
static W6X_Status_t ProcessAndSetTime(uint8_t *Time);

/* Private user code ---------------------------------------------------------*/
/**
  * @brief  Main SNTP Task
  * @retval void
  */
void vSNTPTask(void *pvParameters)
{
    uint8_t Time[32] = {0};
    uint8_t SNTP_Enable = 0;
    int16_t SNTP_Timezone = -8;
    W6X_Status_t xStatus;

    LogInfo("%s started", __func__);
    (void) pvParameters;

    WaitForNetworkConnection();

    xStatus = ConfigureSNTPClient(&SNTP_Enable, &SNTP_Timezone);

    while (xStatus == W6X_STATUS_OK)
    {
      if (xStatus == W6X_STATUS_OK)
      {
          xStatus = GetSNTPTime(Time);
      }

      if (xStatus == W6X_STATUS_OK)
      {
          xStatus = ProcessAndSetTime(Time);
      }

      vTaskDelay(pdMS_TO_TICKS(1000 * 60));
    }

    vTaskSuspend(NULL);
}

/**
  * @brief Wait until connected to the network
  * @retval None
  */
static void WaitForNetworkConnection(void)
{
    (void) xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
}

/**
  * @brief Configure the SNTP client
  * @retval W6X_Status_t
  */
static W6X_Status_t ConfigureSNTPClient(uint8_t *SNTP_Enable, int16_t *SNTP_Timezone)
{
    W6X_Status_t xStatus;

    xStatus = W6X_Net_GetSNTPConfiguration(SNTP_Enable, SNTP_Timezone, NULL, NULL, NULL);

    if (xStatus != W6X_STATUS_OK)
    {
        LogError("SNTP Time Failure");
        return xStatus;
    }

    if ((*SNTP_Enable == 0) || (*SNTP_Timezone != WIFI_SNTP_TIMEZONE))
    {
        xStatus = W6X_Net_SetSNTPConfiguration(1, WIFI_SNTP_TIMEZONE, (uint8_t *)"0.pool.ntp.org", (uint8_t *)"time.google.com", NULL);
        vTaskDelay(5000); /* Wait a few seconds to execute the first request */
    }

    return xStatus;
}

/**
  * @brief Get SNTP time
  * @retval W6X_Status_t
  */
static W6X_Status_t GetSNTPTime(uint8_t *Time)
{
    W6X_Status_t xStatus = W6X_Net_GetTime(Time);

    if (xStatus != W6X_STATUS_OK)
    {
        LogError("SNTP GetTime Failure");
    }

    return xStatus;
}

/**
  * @brief Get SNTP time
  * @retval W6X_Status_t
  */
/* Subroutine: Process the time string and set RTC */
static W6X_Status_t ProcessAndSetTime(uint8_t *Time)
{
    int32_t year, month, weekday, day, hour, minute, second;
    char mday[4] = {0};
    char mon[4] = {0};
    RTC_TimeTypeDef rtc_time = {0};
    RTC_DateTypeDef rtc_date = {0};

    LogInfo("SNTP Time: %s", Time);

    if (sscanf((char *)Time, "%s %s  %ld %2ld:%2ld:%2ld %ld", mday, mon, &day, &hour, &minute, &second, &year) != 7)
    {
        LogError("Process Time failed");
        return W6X_STATUS_ERROR;
    }

    for (weekday = 0; weekday < 7; weekday++)
    {
        if (strcmp(mday, WeekDayString[weekday]) == 0)
        {
            break;
        }
    }

    for (month = 0; month < 12; month++)
    {
        if (strcmp(mon, MonthString[month]) == 0)
        {
            break;
        }
    }

    if ((weekday == 7) || (month == 12))
    {
        LogError("Process Time failed");
        return W6X_STATUS_ERROR;
    }

    rtc_time.Hours = hour;
    rtc_time.Minutes = minute;
    rtc_time.Seconds = second;
    rtc_time.TimeFormat = RTC_HOURFORMAT12_AM;
    rtc_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        LogError("Process Time failed");
        return W6X_STATUS_ERROR;
    }

    rtc_date.Year = year - 2000;
    rtc_date.Month = month + 1;
    rtc_date.Date = day;
    rtc_date.WeekDay = weekday + 1;

    if (HAL_RTC_SetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        LogError("Process Time failed");
        return W6X_STATUS_ERROR;
    }

    return W6X_STATUS_OK;
}

