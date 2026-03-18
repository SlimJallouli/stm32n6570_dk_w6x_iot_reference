/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
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

/****************************************************************************** */
/* */
/* Include OS functionality. */
/* */
/****************************************************************************** */

/* ------------------------ System architecture includes ----------------------------- */
#include "arch/sys_arch.h"
#include "logging.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/opt.h"

#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "main.h"


#if LWIP_NETCONN_SEM_PER_THREAD
    #if configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0

/*---------------------------------------------------------------------------*
* Routine:  sys_arch_netconn_sem_get
*---------------------------------------------------------------------------*
* Description:
*      Lookup the task-specific semaphore; create one if necessary.
*      The semaphore pointer lives in the 0th slot of the
*      TCB local storage array.  Once allocated, it is never released.
*---------------------------------------------------------------------------*/
        sys_sem_t * sys_arch_netconn_sem_get( void )
        {
            void * ret;
            TaskHandle_t task = xTaskGetCurrentTaskHandle();

            configASSERT( task != NULL );

            ret = pvTaskGetThreadLocalStoragePointer( task, 0 );

            if( ret == NULL )
            {
                sys_sem_t * sem;
                err_t err;
                /* allocate memory for this semaphore */
                sem = mem_malloc( sizeof( sys_sem_t ) );
                configASSERT( sem != NULL );
                err = sys_sem_new( sem, 0 );
                configASSERT( err == ERR_OK );
                configASSERT( sys_sem_valid( sem ) );
                vTaskSetThreadLocalStoragePointer( task, 0, sem );
                ret = sem;
            }

            return ret;
        }
    #else /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */
        #error LWIP_NETCONN_SEM_PER_THREAD needs configNUM_THREAD_LOCAL_STORAGE_POINTERS
    #endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */

#endif /* LWIP_NETCONN_SEM_PER_THREAD */
