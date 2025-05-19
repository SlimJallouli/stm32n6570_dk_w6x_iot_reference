#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include "w6x_api.h"
#include "w6x_lwip_port.h"
#include "w6x_types.h"
#include "FreeRTOS.h"
#include <sys\select.h>
#include <sys\time.h>
#include "sys_evt.h"

int W6X_Net_GetAddrInfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
    if (nodename == NULL || res == NULL)
    {
        return -1; // Invalid input
    }

    uint8_t ipaddr[4];
    W6X_Status_t status = W6X_Net_GetHostAddress(nodename, ipaddr);

    if (status != W6X_STATUS_OK)
    {
        return -1; // Failed to resolve hostname
    }

    // Allocate memory for the result
    struct addrinfo *result = (struct addrinfo *) pvPortMalloc(sizeof(hints));

    if (result == NULL)
    {
        return -1; // Memory allocation failure
    }

    struct sockaddr_in *sock_addr = (struct sockaddr_in *)pvPortMalloc(sizeof(struct sockaddr_in));

    if (sock_addr == NULL)
    {
      vPortFree(result);
        return -1;
    }

    // Fill the sockaddr_in structure
    memset(sock_addr, 0, sizeof(struct sockaddr_in));
    sock_addr->sin_family = AF_INET;
    sock_addr->sin_port = (servname != NULL) ? lwip_htons(atoi(servname)) : 0; // Convert service name to port
    memcpy(&(sock_addr->sin_addr_t.s_addr), ipaddr, sizeof(ipaddr));

    // Fill the addrinfo structure
    result->ai_family = AF_INET;
    result->ai_socktype = (hints != NULL) ? hints->ai_socktype : SOCK_STREAM;
    result->ai_protocol = (hints != NULL) ? hints->ai_protocol : IPPROTO_TCP;
    result->ai_addrlen = sizeof(struct sockaddr_in);
    result->ai_addr = (struct sockaddr *)sock_addr;
    result->ai_next = NULL;

    *res = result;
    return 0; // Success
}

void W6X_Net_FreeAddrInfo(struct addrinfo *res)
{
//  vPortFree(res->ai_addr);
  vPortFree(res);
}

/**
 * Same as ip4addr_ntoa, but reentrant since a user-supplied buffer is used.
 *
 * @param addr ip address in network order to convert
 * @param buf target buffer where the string is stored
 * @param buflen length of buf
 * @return either pointer to buf which now holds the ASCII
 *         representation of addr or NULL if buf was too small
 */
char* ip4addr_ntoa_r(const struct in_addr *addr, char *buf, int buflen)
{
  uint32_t s_addr;
  char inv[3];
  char *rp;
  uint8_t *ap;
  uint8_t rem;
  uint8_t n;
  uint8_t i;
  int len = 0;

  s_addr = ip4_addr_get_u32(addr);

  rp = buf;
  ap = (uint8_t*) &s_addr;
  for (n = 0; n < 4; n++)
  {
    i = 0;
    do
    {
      rem = *ap % (uint8_t) 10;
      *ap /= (uint8_t) 10;
      inv[i++] = (char) ('0' + rem);
    }
    while (*ap);
    while (i--)
    {
      if (len++ >= buflen)
      {
        return NULL;
      }
      *rp++ = inv[i];
    }
    if (len++ >= buflen)
    {
      return NULL;
    }
    *rp++ = '.';
    ap++;
  }
  *--rp = 0;
  return buf;
}


