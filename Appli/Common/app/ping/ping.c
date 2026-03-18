/*
 * ping_tcp.c
 *
 * Reliable TCP-based connectivity check for lwIP.
 * Works on all lwIP ports including ST67W6X where ICMP sockets are not supported.
 *
 * Behavior:
 * - Resolves hostname (or IP string)
 * - Attempts a TCP connect to the target host/port
 * - Measures RTT
 * - Logs success/failure
 */

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"
#include "lwip/sys.h"

#include "logging.h"
#include "sys_evt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "kvstore.h"

#define PING_INTERVAL_MS   1000
#define PING_TIMEOUT_MS    1000
#define PING_COUNT         4

/* ---------------------------------------------------------
 * DNS + address resolution
 * --------------------------------------------------------- */
static BaseType_t resolve_hostname_v4(const char *hostname,
                                      struct sockaddr_in *out_addr)
{
    if (!hostname || !out_addr || hostname[0] == '\0')
    {
        LogError("resolve_hostname_v4: invalid args");
        return pdFALSE;
    }

    struct addrinfo hints;
    struct addrinfo *res = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int gai = lwip_getaddrinfo(hostname, NULL, &hints, &res);
    if (gai != 0 || !res || !res->ai_addr)
    {
        LogError("DNS failed for %s (gai=%d)", hostname, gai);
        return pdFALSE;
    }

    memcpy(out_addr, res->ai_addr, sizeof(struct sockaddr_in));
    lwip_freeaddrinfo(res);

    LogInfo("Resolved hostname %s to IP: %s",
            hostname, inet_ntoa(out_addr->sin_addr));

    return pdTRUE;
}

/* ---------------------------------------------------------
 * TCP "ping" — connect + measure RTT
 * --------------------------------------------------------- */
static BaseType_t tcp_ping(const struct sockaddr_in *addr,
                           uint16_t port,
                           uint32_t timeout_ms)
{
    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        LogError("tcp_ping: socket() failed (errno=%d)", errno);
        return pdFALSE;
    }

    struct sockaddr_in target = *addr;
    target.sin_port = lwip_htons(port);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    uint32_t start = sys_now();
    int ret = lwip_connect(s, (struct sockaddr *)&target, sizeof(target));
    uint32_t rtt = sys_now() - start;

    lwip_close(s);

    if (ret == 0)
    {
        LogInfo("TCP ping OK: %s:%u time=%ums",
                inet_ntoa(target.sin_addr),
                port,
                (unsigned)rtt);
        return pdTRUE;
    }

    LogError("TCP ping FAILED: %s:%u (errno=%d)",
             inet_ntoa(target.sin_addr),
             port,
             errno);

    return pdFALSE;
}

/* ---------------------------------------------------------
 * Task
 * --------------------------------------------------------- */
void ping_task(void *pvParameters)
{
    (void)pvParameters;

    LogInfo("%s started", __func__);
    LogInfo("Waiting for network connection...");

    (void)xEventGroupWaitBits(xSystemEvents,
                              EVT_MASK_NET_CONNECTED,
                              pdFALSE,
                              pdTRUE,
                              portMAX_DELAY);

    LogInfo("Network connected");

    /* -----------------------------------------
     * Choose your target
     * ----------------------------------------- */
#if defined(LFS_CONFIG)
    size_t host_len = 0;
    char *host = KVStore_getStringHeap(CS_CORE_MQTT_ENDPOINT, &host_len);
    if (!host)
    {
        LogError("KVStore_getStringHeap(%s) failed", CS_CORE_MQTT_ENDPOINT);
        vTaskDelete(NULL);
    }
#else
    char *host = pvPortMalloc(128);
    snprintf(host, 128, "%s", "www.google.com");   /* or your MQTT host */
#endif

    struct sockaddr_in addr;
    if (resolve_hostname_v4(host, &addr) != pdTRUE)
    {
        vPortFree(host);
        vTaskDelete(NULL);
    }

    vPortFree(host);

    /* -----------------------------------------
     * Perform TCP "pings"
     * ----------------------------------------- */
    for (int i = 0; i < PING_COUNT; i++)
    {
        LogDebug("TCP ping %d/%d...", i + 1, PING_COUNT);

        tcp_ping(&addr, 80, PING_TIMEOUT_MS);   /* Port 80 is always reachable */

        vTaskDelay(pdMS_TO_TICKS(PING_INTERVAL_MS));
    }

    LogInfo("Ping process completed. Terminating task.");
    vTaskDelete(NULL);
}
