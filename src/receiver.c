#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "tool.h"
#include <qpid/dispatch/timer.h>

#define MTU 1500

int tap_open(char *dev);
int tun_open(char *dev);

typedef struct ip_header_t {
    uint8_t  ver_hlen;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t hcksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} ip_header_t;

//static const char        *MODULE = "R_TOOL";
static dx_dispatch_t     *dx;
static dx_node_t         *node;
//static dx_link_t         *receiver;
//static uint64_t           tag = 1;
static sys_mutex_t       *lock;
static dx_timer_t        *timer;

static char *host;
static char *port;
static char *address;
static dx_listener_t *listener;

static uint64_t in_message_count = 0;

#define CREDIT       1000
#define CREDIT_BATCH 100
static uint64_t credit_pending = 0;

static void timer_handler(void *unused)
{
    dx_timer_schedule(timer, 1000);
}


static void tool_rx_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
    pn_link_t    *pn_link = pn_delivery_link(delivery);
    dx_message_t *msg;

    //
    // Extract the message from the incoming delivery.
    //
    msg = dx_message_receive(delivery);
    if (!msg)
        return;

    //
    // Advance the link and issue flow-control credit.
    //
    pn_link_advance(pn_link);
    credit_pending++;
    if (credit_pending > CREDIT_BATCH) {
        pn_link_flow(pn_link, CREDIT_BATCH);
        credit_pending -= CREDIT_BATCH;
    }
    dx_free_message(msg);
    in_message_count++;

    if ((in_message_count % 5000) == 0)
        printf("Received: %ld\n", in_message_count);

    //
    // No matter what happened with the message, settle the delivery.
    //
    pn_delivery_settle(delivery);
}


static void tool_tx_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
}


static void tool_disp_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
}


static int tool_incoming_handler(void *node_context, dx_link_t *link)
{
    pn_link_t *pn_link = dx_link_pn(link);

    pn_terminus_copy(pn_link_source(pn_link), pn_link_remote_source(pn_link));
    pn_terminus_copy(pn_link_target(pn_link), pn_link_remote_target(pn_link));
    pn_link_open(pn_link);
    pn_link_flow(pn_link, CREDIT);

    return 1;
}


static int tool_outgoing_handler(void *node_context, dx_link_t *link)
{
    return 0;
}


static int tool_writable_handler(void *node_context, dx_link_t *link)
{
    return 0;
}


static int tool_detach_handler(void *node_context, dx_link_t *link, int closed)
{
    printf("LINK DETACHED - Total Received Messages: %ld\n", in_message_count);
    in_message_count = 0;
    return 0;
}


static void tool_inbound_conn_open_handler(void *type_context, dx_connection_t *conn)
{
}


static void tool_outbound_conn_open_handler(void *type_context, dx_connection_t *conn)
{
}


static const dx_node_type_t node_descriptor = {"tool-receiver", 0, 0,
                                               tool_rx_handler,
                                               tool_tx_handler,
                                               tool_disp_handler,
                                               tool_incoming_handler,
                                               tool_outgoing_handler,
                                               tool_writable_handler,
                                               tool_detach_handler,
                                               0, 0,
                                               tool_inbound_conn_open_handler,
                                               tool_outbound_conn_open_handler};


int tool_setup(dx_dispatch_t *_dx, char *_host, char *_port, char *_addr, uint64_t _count)
{
    dx      = _dx;
    host    = _host;
    port    = _port;
    address = _addr;

    lock = sys_mutex();

    //
    // Setup periodic timer
    //
    timer = dx_timer(dx, timer_handler, 0);
    dx_timer_schedule(timer, 0);

    //
    // Register self as a container type and instance.
    //
    dx_container_register_node_type(dx, &node_descriptor);
    node = dx_container_create_node(dx, &node_descriptor, "recv", 0, DX_DIST_MOVE, DX_LIFE_PERMANENT);

    //
    // Establish an incoming listener.
    //
    static dx_server_config_t client_config;
    client_config.host            = host;
    client_config.port            = port;
    client_config.sasl_mechanisms = "ANONYMOUS";
    client_config.ssl_enabled     = 0;
    listener = dx_server_listen(dx, &client_config, 0);

    return 0;
}

