#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "http_api.h"
#include "alert.h"
#include "sel.h"

#define HTTP_PORT_DEFAULT 8080
#define RECV_BUF_SIZE     2048
#define SEND_BUF_SIZE     8192
#define SEL_MAX_LINES     20

/* ── Shared state ──────────────────────────────────────────────── */
static pthread_mutex_t      snap_mu  = PTHREAD_MUTEX_INITIALIZER;
static http_sensor_snapshot_t g_snap = {0};

static volatile int g_running = 0;
static pthread_t    g_thread;
static int          g_port = HTTP_PORT_DEFAULT;

/* ── Snapshot update (called from main loop) ───────────────────── */
void http_api_update(const http_sensor_snapshot_t *snap) {
    pthread_mutex_lock(&snap_mu);
    g_snap = *snap;
    pthread_mutex_unlock(&snap_mu);
}

/* ── Response helpers ──────────────────────────────────────────── */
static void send_response(int fd, int code, const char *status,
                          const char *body) {
    char hdr[256];
    int  hdr_len = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status, strlen(body));
    write(fd, hdr, (size_t)hdr_len);
    write(fd, body, strlen(body));
}

/* ── Route handlers ────────────────────────────────────────────── */

/* GET /redfish/v1/ */
static void handle_root(int fd) {
    const char *body =
        "{\n"
        "  \"@odata.type\": \"#ServiceRoot.v1_0_0.ServiceRoot\",\n"
        "  \"@odata.id\": \"/redfish/v1/\",\n"
        "  \"Name\": \"Pi Lab Service Root\",\n"
        "  \"Chassis\": { \"@odata.id\": \"/redfish/v1/Chassis\" },\n"
        "  \"Systems\": { \"@odata.id\": \"/redfish/v1/Systems\" }\n"
        "}\n";
    send_response(fd, 200, "OK", body);
}

/* GET /redfish/v1/Chassis/1/Thermal */
static void handle_thermal(int fd) {
    http_sensor_snapshot_t snap;
    pthread_mutex_lock(&snap_mu);
    snap = g_snap;
    pthread_mutex_unlock(&snap_mu);

    char ts[20];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    char pico_reading[16];
    if (snap.pico_valid)
        snprintf(pico_reading, sizeof(pico_reading), "%.1f", snap.pico_temp_c);
    else
        snprintf(pico_reading, sizeof(pico_reading), "null");

    const char *health = (snap.level == ALERT_CRITICAL) ? "Critical" :
                         (snap.level == ALERT_WARNING)  ? "Warning"  : "OK";

    char body[SEND_BUF_SIZE];
    snprintf(body, sizeof(body),
        "{\n"
        "  \"@odata.type\": \"#Thermal.v1_3_0.Thermal\",\n"
        "  \"@odata.id\": \"/redfish/v1/Chassis/1/Thermal\",\n"
        "  \"Name\": \"Thermal\",\n"
        "  \"Temperatures\": [\n"
        "    {\n"
        "      \"MemberId\": \"0\",\n"
        "      \"Name\": \"SHT35 Ambient\",\n"
        "      \"ReadingCelsius\": %.1f,\n"
        "      \"UpperThresholdNonCritical\": 30.0,\n"
        "      \"UpperThresholdCritical\": 40.0,\n"
        "      \"Status\": { \"State\": \"Enabled\", \"Health\": \"%s\" }\n"
        "    },\n"
        "    {\n"
        "      \"MemberId\": \"1\",\n"
        "      \"Name\": \"Pico2 CPU\",\n"
        "      \"ReadingCelsius\": %s,\n"
        "      \"Status\": { \"State\": \"%s\", \"Health\": \"OK\" }\n"
        "    }\n"
        "  ],\n"
        "  \"Humidity\": [\n"
        "    {\n"
        "      \"MemberId\": \"0\",\n"
        "      \"Name\": \"SHT35 Humidity\",\n"
        "      \"ReadingPercent\": %.1f\n"
        "    }\n"
        "  ],\n"
        "  \"ReadingTime\": \"%s\"\n"
        "}\n",
        snap.temp_c, health,
        pico_reading, snap.pico_valid ? "Enabled" : "Absent",
        snap.humidity_pct, ts);
    send_response(fd, 200, "OK", body);
}

/* GET /redfish/v1/Systems/1/LogServices/SEL/Entries */
static void handle_sel(int fd) {
    /* Read the last SEL_MAX_LINES lines from sel.log */
    FILE *f = fopen("logs/sel.log", "r");
    char  body[SEND_BUF_SIZE];
    int   pos = 0;

    pos += snprintf(body + pos, sizeof(body) - (size_t)pos,
        "{\n"
        "  \"@odata.type\": \"#LogEntryCollection.LogEntryCollection\",\n"
        "  \"@odata.id\": \"/redfish/v1/Systems/1/LogServices/SEL/Entries\",\n"
        "  \"Name\": \"System Event Log\",\n"
        "  \"Members\": [\n");

    if (f) {
        /* Collect up to SEL_MAX_LINES lines into a ring buffer */
        char lines[SEL_MAX_LINES][128];
        int  count = 0, idx = 0;
        while (fgets(lines[idx], sizeof(lines[idx]), f)) {
            /* strip trailing newline */
            char *nl = strchr(lines[idx], '\n');
            if (nl) *nl = '\0';
            idx = (idx + 1) % SEL_MAX_LINES;
            if (count < SEL_MAX_LINES) count++;
        }
        fclose(f);

        int start = (count == SEL_MAX_LINES) ? idx : 0;
        for (int i = 0; i < count; i++) {
            const char *line = lines[(start + i) % SEL_MAX_LINES];
            if ((size_t)pos >= sizeof(body) - 64) break;
            pos += snprintf(body + pos, sizeof(body) - (size_t)pos,
                "    { \"Message\": \"%s\" }%s\n",
                line, (i < count - 1) ? "," : "");
        }
    }

    pos += snprintf(body + pos, sizeof(body) - (size_t)pos,
        "  ]\n}\n");
    (void)pos;
    send_response(fd, 200, "OK", body);
}

static void handle_not_found(int fd) {
    send_response(fd, 404, "Not Found",
                  "{ \"error\": { \"code\": \"Base.1.0.ResourceNotFound\" } }\n");
}

/* ── Request dispatch ──────────────────────────────────────────── */
static void handle_request(int fd) {
    char buf[RECV_BUF_SIZE];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    /* Only handle GET */
    if (strncmp(buf, "GET ", 4) != 0) {
        send_response(fd, 405, "Method Not Allowed",
                      "{ \"error\": \"Only GET is supported\" }\n");
        return;
    }

    char path[256] = {0};
    sscanf(buf + 4, "%255s", path);

    if (strcmp(path, "/redfish/v1/") == 0 || strcmp(path, "/redfish/v1") == 0)
        handle_root(fd);
    else if (strcmp(path, "/redfish/v1/Chassis/1/Thermal") == 0)
        handle_thermal(fd);
    else if (strcmp(path, "/redfish/v1/Systems/1/LogServices/SEL/Entries") == 0)
        handle_sel(fd);
    else
        handle_not_found(fd);
}

/* ── Listener thread ───────────────────────────────────────────── */
static void *listener_thread(void *arg) {
    (void)arg;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("http_api: socket"); return NULL; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons((uint16_t)g_port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("http_api: bind");
        close(srv);
        return NULL;
    }
    if (listen(srv, 4) < 0) {
        perror("http_api: listen");
        close(srv);
        return NULL;
    }

    printf("[http] Redfish API listening on :%d\n", g_port);

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (g_running) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (!g_running) break;
            continue;
        }
        handle_request(client);
        close(client);
    }

    close(srv);
    return NULL;
}

/* ── Public API ────────────────────────────────────────────────── */
int http_api_start(int port) {
    g_port    = (port > 0) ? port : HTTP_PORT_DEFAULT;
    g_running = 1;
    if (pthread_create(&g_thread, NULL, listener_thread, NULL) != 0) {
        perror("http_api: pthread_create");
        g_running = 0;
        return -1;
    }
    return 0;
}

void http_api_stop(void) {
    g_running = 0;
    pthread_join(g_thread, NULL);
}
