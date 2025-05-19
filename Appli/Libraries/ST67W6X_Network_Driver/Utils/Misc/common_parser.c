/**
  ******************************************************************************
  * @file    common_parser.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x common parser functions
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

/*
 * Portions of this file are based on LwIP's http client example application, LwIP version is 2.2.0.
 * Which is licensed under modified BSD-3 Clause license as indicated below.
 * See https://savannah.nongnu.org/projects/lwip/ for more information.
 *
 * Reference source :
 * https://github.com/lwip-tcpip/lwip/blob/master/src/core/ipv4/ip4_addr.c
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <ctype.h>
#include "common_parser.h"

/* Private macros ------------------------------------------------------------*/
/** @addtogroup ST67W6X_Utilities_Common_Macros
  * @{
  */

/** Convert a character to a number */
#define CHAR2NUM(x)                     ((x) - '0')

/** Check if the character x is a number */
#define CHARISNUM(x)                    isdigit((unsigned char)(x))

/** Check if the character x is a hexadecimal number */
#define CHARISHEXNUM(x)                 isxdigit((unsigned char)(x))

/** Check if the character x is lowercase */
#define CHARISLOWER(x)                  islower((unsigned char)(x))

/** Check if the character x is a space character */
#define CHARISSPACE(x)                  isspace((unsigned char)(x))

/** @} */
/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W6X_Utilities_Common_Functions
  * @{
  */

/**
  * @brief  Convert char in Hex format to integer.
  * @param  a: character to convert
  * @retval integer value.
  */
static uint8_t Hex2Num(char a);

/* Functions Definition ------------------------------------------------------*/
uint32_t ParseHexNumber(char *ptr, uint8_t *cnt)
{
  uint32_t sum = 0;
  uint8_t i = 0;

  /* Loop on pointer content while it is a hexadecimal character */
  while (CHARISHEXNUM(*ptr))
  {
    sum <<= 4;
    sum += Hex2Num(*ptr);
    ptr++;
    i++;
  }

  /* Save number of characters used for number */
  if (cnt != NULL)
  {
    *cnt = i;
  }

  /* Return the concatenated number */
  return sum;
}

int32_t ParseNumber(char *ptr, uint8_t *cnt)
{
  uint8_t minus = 0;
  uint8_t i = 0;
  int32_t sum = 0;

  /* Check for minus character */
  if (*ptr == '-')
  {
    minus = 1;
    ptr++;
    i++;
  }

  /* Loop on pointer content while it is a numeric character */
  while (CHARISNUM(*ptr))
  {
    sum = 10 * sum + CHAR2NUM(*ptr);
    ptr++;
    i++;
  }

  /* Save number of characters used for number */
  if (cnt != NULL)
  {
    *cnt = i;
  }

  /* Minus detected */
  if (minus)
  {
    return 0 - sum;
  }

  /* Return the concatenated number */
  return sum;
}

void ParseIP(char *ptr, uint8_t ip[4])
{
  uint8_t hexnum = 0;
  uint8_t hexcnt;
  int32_t tmp_nb = 0;
  int32_t ip_string_len = 15; /* Maximum length of a IP address string */

  /* Loop on pointer content while non empty */
  while ((* ptr) && (ip_string_len > 0))
  {
    hexcnt = 1;

    /* Check if the character is a number */
    if (CHARISNUM(*ptr))
    {
      tmp_nb = ParseNumber(ptr, &hexcnt);
      if ((tmp_nb >= 0) && (tmp_nb < 256))
      {
        ip[hexnum++] = tmp_nb;
      }
    }
    ptr += hexcnt;
    ip_string_len -= hexcnt; /* Decrement the length of the IP address string */
  }

  /* If we did not catch 4 numbers, IP is invalid and is set to 0 */
  if (hexnum != 4)
  {
    memset(ip, 0, 4);
  }
}

int32_t Is_ip_valid(uint8_t ip[4])
{
  uint32_t count_full = 0;
  uint32_t count_zero = 0;

  for (int32_t i = 0; i < 4; i++)
  {
    if (ip[i] == 0xFF)
    {
      count_full++; /* Count the number of 255 */
    }
    if (ip[i] == 0)
    {
      count_zero++; /* Count the number of 0 */
    }

    /* If the IP contains only 255 or only 0, the IP address is invalid */
    if ((count_full == 4) || (count_zero == 4))
    {
      return -1;
    }
  }

  return 0;
}

void ParseMAC(char *ptr, uint8_t mac[6])
{
  uint8_t hexnum = 0;
  uint8_t hexcnt;
  int32_t mac_string_len = 17; /* Maximum length of a MAC address string */

  /* Loop on pointer content while non empty */
  while ((* ptr) && (mac_string_len > 0))
  {
    hexcnt = 1;
    if (*ptr != ':') /* Skip ':' */
    {
      mac[hexnum++] = ParseHexNumber(ptr, &hexcnt);
    }
    ptr = ptr + hexcnt;
    mac_string_len -= hexcnt; /* Decrement the length of the MAC address string */
  }
}

int8_t IPv4_Addr_Aton(const char *cp, uint32_t *addr)
{
  uint32_t val;
  uint8_t base;
  char c;
  uint32_t parts[4];
  uint32_t *pp = parts;

  c = *cp;
  for (;;)
  {
    /*
     * Collect number up to ``.''.
     * Values are specified as for C:
     * 0x=hex, 0=octal, 1-9=decimal.
     */
    if (!CHARISNUM(c))
    {
      return 0;
    }
    val = 0;
    base = 10;
    if (c == '0')
    {
      c = *++cp;
      if (c == 'x' || c == 'X')
      {
        base = 16;
        c = *++cp;
      }
      else
      {
        base = 8;
      }
    }
    for (;;)
    {
      if (CHARISNUM(c))
      {
        if ((base == 8) && ((uint32_t)(c - '0') >= 8))
        {
          break;
        }
        val = (val * base) + (uint32_t)(c - '0');
        c = *++cp;
      }
      else if (base == 16 && CHARISHEXNUM(c))
      {
        val = (val << 4) | (uint32_t)(c + 10 - (CHARISLOWER(c) ? 'a' : 'A'));
        c = *++cp;
      }
      else
      {
        break;
      }
    }
    if (c == '.')
    {
      /*
       * Internet format:
       *  a.b.c.d
       *  a.b.c   (with c treated as 16 bits)
       *  a.b (with b treated as 24 bits)
       */
      if (pp >= parts + 3)
      {
        return 0;
      }
      *pp++ = val;
      c = *++cp;
    }
    else
    {
      break;
    }
  }
  /*
   * Check for trailing characters.
   */
  if (c != '\0' && !CHARISSPACE(c))
  {
    return 0;
  }
  /*
   * Concoct the address according to
   * the number of parts specified.
   */
  switch (pp - parts + 1)
  {

    case 0:
      return 0;       /* initial nondigit */

    case 1:             /* a -- 32 bits */
      break;

    case 2:             /* a.b -- 8.24 bits */
      if (val > 0xffffffUL)
      {
        return 0;
      }
      if (parts[0] > 0xff)
      {
        return 0;
      }
      val |= parts[0] << 24;
      break;

    case 3:             /* a.b.c -- 8.8.16 bits */
      if (val > 0xffff)
      {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff))
      {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16);
      break;

    case 4:             /* a.b.c.d -- 8.8.8.8 bits */
      if (val > 0xff)
      {
        return 0;
      }
      if ((parts[0] > 0xff) || (parts[1] > 0xff) || (parts[2] > 0xff))
      {
        return 0;
      }
      val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
      break;
    default:
      return 0;
      break;
  }

  if (addr)
  {
    /** no PP_HTONL is done so the result stored in addr is not in network byte order*/
    *addr = val;
  }

  return 1;
}

/* Private Functions Definition ----------------------------------------------*/
static uint8_t Hex2Num(char a)
{
  /* Char is num */
  if (a >= '0' && a <= '9')
  {
    return a - '0';
  }
  /* Char is lowercase character A - Z (hex) */
  else if (a >= 'a' && a <= 'f')
  {
    return (a - 'a') + 10;
  }
  /* Char is uppercase character A - Z (hex) */
  else if (a >= 'A' && a <= 'F')
  {
    return (a - 'A') + 10;
  }

  return 0;
}

/** @} */
