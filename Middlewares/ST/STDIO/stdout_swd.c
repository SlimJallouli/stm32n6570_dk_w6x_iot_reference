/**
******************************************************************************
* @file           : stdout_swd.c 
* @version        : v 1.0.0
* @brief          : This file implements stdout SWD channel
******************************************************************************
* @attention
*
* <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
* All rights reserved.</center></h2>
*
* This software component is licensed by ST under BSD 3-Clause license,
* the "License"; You may not use this file except in compliance with the
* the "License"; You may not use this file except in compliance with the
*                             opensource.org/licenses/BSD-3-Clause
*
******************************************************************************
*/

#include "main.h"
#include <stdio.h>

#ifdef __CC_ARM /* Keil */
int fputc(int ch, FILE *f);
#endif

#ifndef __ICCARM__ /* If not IAR */
int _write(int file, char *ptr, int len);
#endif /* __CC_ARM */

#ifdef __ICCARM__
int __write(int file, char *ptr, int len)
#else
int _write(int file, char *ptr, int len)
#endif
{
	int i = 0;

	for (i = 0; i < len; i++)
	{
		ITM_SendChar(ptr[i]);
	}

  return len;
}

#ifdef __CC_ARM /* Keil */
int fputc(int ch, FILE *f)
{
  _write(0, (char *)&ch, 1);

  return ch;
}
#endif
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
