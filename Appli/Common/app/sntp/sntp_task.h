/*
 * sntp_task.h
 *
 *  Created on: Dec 5, 2024
 *      Author: stred
 */

#ifndef _SNTP_TASK_H_
#define _SNTP_TASK_H_

/** SNTP timezone configuration */
#define WIFI_SNTP_TIMEZONE          1

void vSNTPTask(void *pvParameters);

#endif /* _SNTP_TASK_H_ */
