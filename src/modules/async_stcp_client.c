/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the Async project.
 */

#include "async/modules/stcp_client.h"

static void on_ssl_connected(struct async_ssl_connection_t *connection_p, int res)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(connection_p, typeof(*self_p), ssl.connection);
    self_p->on_connected(self_p, res);
}

static void on_ssl_disconnected(struct async_ssl_connection_t *connection_p)
{
    (void)connection_p;
}

static void on_ssl_input(struct async_ssl_connection_t *connection_p)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(connection_p, typeof(*self_p), ssl.connection);
    self_p->on_input(self_p);
}

static ssize_t ssl_transport_read(struct async_ssl_connection_t *connection_p,
                                  void *buf_p,
                                  size_t size)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(connection_p, typeof(*self_p), ssl.connection);

    return (async_tcp_client_read(&self_p->tcp, buf_p, size));
}

static void ssl_transport_write(struct async_ssl_connection_t *connection_p,
                                const void *buf_p,
                                size_t size)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(connection_p, typeof(*self_p), ssl.connection);
    async_tcp_client_write(&self_p->tcp, buf_p, size);
}

static void on_tcp_connected(struct async_tcp_client_t *tcp_p, int res)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(tcp_p, typeof(*self_p), tcp);

    if (res == 0) {
        if (self_p->ssl.context_p == NULL) {
            self_p->on_connected(self_p, res);
        } else {
            async_ssl_connection_open(&self_p->ssl.connection,
                                      self_p->ssl.context_p,
                                      on_ssl_connected,
                                      on_ssl_disconnected,
                                      on_ssl_input,
                                      ssl_transport_read,
                                      ssl_transport_write,
                                      tcp_p->async_p);
        }
    } else {
        self_p->on_connected(self_p, res);
    }
}

static void on_tcp_disconnected(struct async_tcp_client_t *tcp_p)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(tcp_p, typeof(*self_p), tcp);
    self_p->on_disconnected(self_p);
}

static void on_tcp_input(struct async_tcp_client_t *tcp_p)
{
    struct async_stcp_client_t *self_p;

    self_p = async_container_of(tcp_p, typeof(*self_p), tcp);

    if (self_p->ssl.context_p == NULL) {
        self_p->on_input(self_p);
    } else {
        async_ssl_connection_on_transport_input(&self_p->ssl.connection);
    }
}

void async_stcp_client_init(struct async_stcp_client_t *self_p,
                            struct async_ssl_context_t *ssl_context_p,
                            async_stcp_client_connected_t on_connected,
                            async_stcp_client_disconnected_t on_disconnected,
                            async_stcp_client_input_t on_input,
                            struct async_t *async_p)
{
    self_p->on_connected = on_connected;
    self_p->on_disconnected = on_disconnected;
    self_p->on_input = on_input;
    self_p->ssl.context_p = ssl_context_p;
    async_tcp_client_init(&self_p->tcp,
                          on_tcp_connected,
                          on_tcp_disconnected,
                          on_tcp_input,
                          async_p);
}

void async_stcp_client_connect(struct async_stcp_client_t *self_p,
                               const char *host_p,
                               int port)
{
    async_tcp_client_connect(&self_p->tcp, host_p, port);
}

void async_stcp_client_disconnect(struct async_stcp_client_t *self_p)
{
    /* ToDo: Add clean SSL close. */
    async_tcp_client_disconnect(&self_p->tcp);
}

void async_stcp_client_write(struct async_stcp_client_t *self_p,
                             const void *buf_p,
                             size_t size)
{
    if (self_p->ssl.context_p == NULL) {
        async_tcp_client_write(&self_p->tcp, buf_p, size);
    } else {
        async_ssl_connection_write(&self_p->ssl.connection, buf_p, size);
    }
}

size_t async_stcp_client_read(struct async_stcp_client_t *self_p,
                              void *buf_p,
                              size_t size)
{
    if (self_p->ssl.context_p == NULL) {
        size = async_tcp_client_read(&self_p->tcp, buf_p, size);
    } else {
        size = async_ssl_connection_read(&self_p->ssl.connection, buf_p, size);
    }

    return (size);
}
