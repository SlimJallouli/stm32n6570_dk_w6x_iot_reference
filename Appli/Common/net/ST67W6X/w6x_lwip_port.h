#ifndef W6X_LWIP_PORT
#define W6X_LWIP_PORT

#include "w6x_api.h"
#include "w6x_types.h"
#include <stdint.h>
#include <sys\select.h>

#define NET_IPV4 1

#define LWIP_IPV4 NET_IPV4
#define LWIP_IPV6 NET_IPV6
#define IP4ADDR_STRLEN_MAX  16
#if 1
struct addrinfo
{
  int ai_flags; /* Input flags. */
  int ai_family; /* Address family of socket. */
  int ai_socktype; /* Socket type. */
  int ai_protocol; /* Protocol of socket. */
  socklen_t ai_addrlen; /* Length of socket address. */
  struct sockaddr *ai_addr; /* Socket address of socket. */
  char *ai_canonname; /* Canonical name of service location. */
  struct addrinfo *ai_next; /* Pointer to next in list. */
};
#endif

int W6X_Net_GetAddrInfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);
void W6X_Net_FreeAddrInfo(struct addrinfo *res);
char* ip4addr_ntoa_r(const struct in_addr *addr, char *buf, int buflen);
int W6X_Net_select  ( int   maxfdp1, fd_set *  readset, fd_set *  writeset, fd_set *  exceptset, struct timeval *  timeout);

#define lwip_socket        W6X_Net_Socket
#define lwip_close         W6X_Net_Close
#define lwip_shutdown      W6X_Net_Shutdown
#define lwip_bind          W6X_Net_Bind
#define lwip_connect       W6X_Net_Connect
#define lwip_listen        W6X_Net_Listen
#define lwip_accept        W6X_Net_Accept
#define lwip_send          W6X_Net_Send
#define lwip_recv          W6X_Net_Recv
#define lwip_setsockopt    W6X_Net_Setsockopt
#define lwip_getsockopt    W6X_Net_Getsockopt
#define lwip_sendto        W6X_Net_Sendto
#define lwip_recvfrom      W6X_Net_Recvfrom
#define lwip_gethostbyname W6X_Net_GetHostAddress
#define lwip_getaddrinfo   W6X_Net_GetAddrInfo
#define lwip_freeaddrinfo  W6X_Net_FreeAddrInfo
#define lwip_inet_ntop     W6X_Net_Inet_ntop
#define lwip_inet_pton     W6X_Net_Inet_pton
#define lwip_select        W6X_Net_select

#define lwip_htons         PP_HTONS
#define htons              PP_HTONS
#define inet_addr          ATON_R

#define IPADDR_ANY         ((uint32_t)0x00000000UL)
#define INADDR_ANY         IPADDR_ANY

#define sin_addr           sin_addr_t

#define ip4_addr_get_u32(src_ipaddr) ((src_ipaddr)->s_addr)

#define inet_ntoa_r ip4addr_ntoa_r

#endif /* W6X_LWIP_PORT */
