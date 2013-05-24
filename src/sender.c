#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "tool.h"
#include <qpid/dispatch/iterator.h>
#include <qpid/dispatch/timer.h>

#define MTU 1500

static const char        *MODULE = "S_TOOL";
static dx_dispatch_t     *dx;
static dx_node_t         *node;
static dx_link_t         *sender;
static uint64_t           tag = 1;
static sys_mutex_t       *lock;
static dx_timer_t        *timer;
static dx_message_t      *msg;

static char *host;
static char *port;
static char *address;
static uint64_t count;
static uint64_t out_message_count;


static void timer_handler(void *unused)
{
    kill(getpid(), SIGINT);
}


static void bridge_rx_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
}


static void bridge_tx_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
    pn_link_t *pn_link = pn_delivery_link(delivery);

    dx_message_send(msg, pn_link);

    pn_delivery_settle(delivery);
    pn_link_advance(pn_link);
    pn_link_offered(pn_link, 32);
    out_message_count++;

    if (out_message_count == count) {
        pn_link_close(pn_link);
        dx_timer_schedule(timer, 1000);
    }
}


static void bridge_disp_handler(void *node_context, dx_link_t *link, pn_delivery_t *delivery)
{
}


static int bridge_incoming_handler(void *node_context, dx_link_t *link)
{
    return 0;
}


static int bridge_outgoing_handler(void *node_context, dx_link_t *link)
{
    return 0;
}


static int bridge_writable_handler(void *node_context, dx_link_t *link)
{
    uint64_t   dtag;
    pn_link_t *pn_link = dx_link_pn(link);

    sys_mutex_lock(lock);
    dtag = tag++;
    sys_mutex_unlock(lock);

    pn_delivery(pn_link, pn_dtag((char*) &dtag, 8));
    pn_delivery_t *delivery = pn_link_current(pn_link);
    if (delivery) {
        bridge_tx_handler(node_context, link, delivery);
        return 1;
    }

    return 0;
}


static int bridge_detach_handler(void *node_context, dx_link_t *link, int closed)
{
    return 0;
}


static void bridge_outbound_conn_open_handler(void *type_context, dx_connection_t *conn)
{
    dx_log(MODULE, LOG_INFO, "AMQP Connection Established");

    sender = dx_link(node, conn, DX_OUTGOING, "tool-sender");

    pn_terminus_set_address(dx_link_remote_target(sender), address);
    pn_terminus_set_address(dx_link_source(sender), address);

    pn_terminus_set_address(dx_link_target(sender), address);
    pn_terminus_set_address(dx_link_remote_source(sender), address);

    pn_link_open(dx_link_pn(sender));
    dx_link_activate(sender);
}


static const dx_node_type_t node_descriptor = {"tool-controller", 0, 0,
                                               bridge_rx_handler,
                                               bridge_tx_handler,
                                               bridge_disp_handler,
                                               bridge_incoming_handler,
                                               bridge_outgoing_handler,
                                               bridge_writable_handler,
                                               bridge_detach_handler,
                                               0, 0, 0,
                                               bridge_outbound_conn_open_handler};


int tool_setup(dx_dispatch_t *_dx, char *_host, char *_port, char *_addr, uint64_t _count)
{
    dx      = _dx;
    host    = _host;
    port    = _port;
    address = _addr;
    count   = _count;

    lock = sys_mutex();

    //
    // Setup periodic timer
    //
    timer = dx_timer(dx, timer_handler, 0);

    //
    // Register self as a container type and instance.
    //
    dx_container_register_node_type(dx, &node_descriptor);
    node = dx_container_create_node(dx, &node_descriptor, "send", 0, DX_DIST_MOVE, DX_LIFE_PERMANENT);

    //
    // Establish an outgoing connection to the server.
    //
    static dx_server_config_t client_config;
    client_config.host            = host;
    client_config.port            = port;
    client_config.sasl_mechanisms = "ANONYMOUS";
    client_config.ssl_enabled     = 0;
    dx_server_connect(dx, &client_config, 0);

    //
    // Compose a message to be sent
    //
    dx_buffer_list_t  buffers;
    dx_buffer_t      *buf;
    DEQ_INIT(buffers);

    buf = dx_allocate_buffer();
    memset(dx_buffer_base(buf), 0, 100);
    dx_buffer_insert(buf, 100);
    DEQ_INSERT_TAIL(buffers, buf);

    msg = dx_allocate_message();
    dx_message_compose_1(msg, address, &buffers);

    return 0;
}

