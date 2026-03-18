/*
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include "logging_levels.h"
#define LOG_LEVEL    LOG_INFO
#include "logging.h"
#include "kvstore_prv.h"
#include <string.h>
#include <stdbool.h>

#if (KV_STORE_NVIMPL_STSAFE) && (defined(__USE_STSAFE__))
#include <stsafe_key_value_store.h>

/*
 * @brief Get the length of a value stored in the KVStore implementation
 * @param[in] xKey Key to lookup
 * @return length of the value stored in the KVStore or 0 if not found.
 */
size_t xprvGetValueLengthFromImpl(const KVStoreKey_t xKey)
{
  size_t xLength = 0;

  KVStoreTLVHeader_t xTlvHeader;

  if (pfKvs_getKeyLength(xKey,  &xTlvHeader) == true)
  {
    xLength = xTlvHeader.length;
  }

  return xLength;
}

/*
 * @brief Read the value for the given key into a given buffer.
 * @param[in] xKey The key to lookup
 * @param[out] pxType The type of the value returned.
 * @param[out] pxLength Pointer to store the length of the read value in.
 * @param[out] pvBuffer The buffer to copy the value to.
 * @param[in] xBufferSize The length of the provided buffer.
 * @return pdTRUE on success, otherwise pdFALSE.
 */
BaseType_t xprvReadValueFromImpl(const KVStoreKey_t xKey, KVStoreValueType_t *pxType, size_t *pxLength, void *pvBuffer, size_t xBufferSize)
{
  bool xResult = true;

  KVStoreTLVHeader_t xTlvHeader;

  if ((pvBuffer == NULL) || (xBufferSize == 0))
  {
    xResult = false;
  }

  /* Read header */
  if (xResult)
  {
    xTlvHeader.length = xBufferSize;

    xResult = pfKvs_getKeyValue(xKey, pvBuffer, &xTlvHeader);

    if (pxType != NULL)
    {
      if (xResult == true)
      {
        *pxType = xTlvHeader.type;
      }
      else
      {
        *pxType = KV_TYPE_NONE;
      }
    }

    if (pxLength != NULL)
    {
      if (xResult == true)
      {
        *pxLength = xTlvHeader.length;
      }
      else
      {
        *pxLength = 0;
      }
    }
  }

  return xResult;
}

/*
 * @brief Write a value for a given key to non-volatile storage.
 * @param[in] xKey Key to store the given value in.
 * @param[in] xType Type of value to record.
 * @param[in] xLength length of the value given in pxDataUnion.
 * @param[in] pxData Pointer to a buffer containing the value to be stored.
 * The caller must free any heap allocated buffers passed into this function.
 */
BaseType_t xprvWriteValueToImpl(const KVStoreKey_t xKey, const KVStoreValueType_t xType, const size_t xLength, const void *pvData)
{
  bool xResult = true;

  KVStoreTLVHeader_t xTlvHeader;

  if ((xKey > CS_NUM_KEYS) || (xType == KV_TYPE_NONE) || (xLength < 0) || (pvData == NULL))
  {
    xResult = false;
  }

  if (xResult)
  {
    xTlvHeader.type = xType;
    xTlvHeader.length = xLength;

    xResult = pfKvs_writeKeyValue(xKey, pvData, xTlvHeader);
  }

  return xResult;
}

void vprvNvImplInit(void)
{
  LogInfo("* Conf from STSAFE *");

  pfKvs_init();
}

#endif /* KV_STORE_NVIMPL_ARM_PSA */
