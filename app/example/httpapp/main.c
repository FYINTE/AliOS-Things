/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#include <stdlib.h>
#include <string.h>

#include <aos/kernel.h>
#include <aos/cli.h>
#include <aos/yloop.h>
#include <network/network.h>
#include "ulog/ulog.h"
#include "netmgr.h"
#include "http_api.h"

bool httpc_running = false;
int method;
httpc_handle_t httpc_handle = 0;
char server_name[32] = "http://httpie.org/";

static int httpc_recv_fun(httpc_handle_t httpc, uint8_t *buf, int32_t buf_size,
                          int32_t data_len, bool is_final)
{
    LOG("http session %x, buf size %d bytes, recv %d bytes data, is_final %d\n",
         httpc, buf_size, data_len, is_final);
    return 0;
}

static void httpc_delayed_action(void *arg)
{
    if (httpc_handle == 0 || httpc_running == false) {
        httpc_running = false;
        goto exit;
    }

    LOG("http session %x method %d at %d\n", httpc_handle, method, (uint32_t)aos_now_ms());
    switch (method) {
        case HTTP_GET:
            httpc_send_request(httpc_handle, HTTP_GET, NULL, NULL, 0);
            break;
        default:
            break;
    }

exit:
    aos_post_delayed_action(100, httpc_delayed_action, (void *)(long)method);
}

#define RSP_BUF_SIZE 2000
uint8_t rsp_buf[RSP_BUF_SIZE];
static void httpc_cmd_handle(char *buf, int blen, int argc, char **argv)
{
    const char *type = argc > 1? argv[1]: "";
    httpc_connection_t settings;
    int fd;

    if (strncmp(type, "stop", strlen("stop")) == 0) {
        httpc_running = false;
        return;
    }

    if (httpc_running == false) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            LOG("alloc socket fd fail\n");
            return;
        }
        memset(&settings, 0, sizeof(settings));
        settings.socket = fd;
        settings.recv_fn = httpc_recv_fun;
        settings.server_name = server_name;
        settings.rsp_buf = rsp_buf;
        settings.rsp_buf_size = RSP_BUF_SIZE;
        httpc_handle = httpc_init(&settings);
        if (httpc_handle == 0) {
            LOG("http session init fail\n");
            return;
        }
    }

    if (strncmp(type, "GET", strlen("GET")) == 0) {
        method = HTTP_GET;
        httpc_running = true;
    }
}

static struct cli_command httpc_cmd = {
    .name = "httpc",
    .help = "httpc GET | stop",
    .function = httpc_cmd_handle
};

int application_start(int argc, char *argv[])
{
    aos_set_log_level(AOS_LL_DEBUG);
    netmgr_init();
    netmgr_start(false);
    aos_cli_register_command(&httpc_cmd);
    http_client_intialize();
    aos_post_delayed_action(100, httpc_delayed_action, NULL);
    aos_loop_run();
    return 0;
}