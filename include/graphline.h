/*
 * graphline.h
 * 
 * Copyright 2013 Evan Buswell
 * 
 * This file is part of Graphline.
 * 
 * Graphline is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 * 
 * Graphline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Graphline.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRAPHLINE_H
#define GRAPHLINE_H

#include <atomickit/atomic.h>
#include <atomickit/atomic-rcp.h>
#include <atomickit/atomic-queue.h>
#include <atomickit/atomic-txn.h>

struct gln_graph {
    struct arcp_region_header header;
    arcp_t nodes;
    aqueue_t proc_queue;
};

int gln_graph_init(struct gln_graph *graph, void (*destroy)(struct gln_graph *));
void gln_graph_destroy(struct gln_graph *graph);
int gln_graph_reset(struct gln_graph *graph);

struct gln_node;

typedef int (*gln_process_fp_t)(struct gln_node *);

enum gln_node_state {
    GLNN_READY,
    GLNN_PENDING,
    GLNN_ERROR,
    GLNN_FINISHED
};

struct gln_node {
    struct arcp_region_header header;
    void (*destroy)(struct gln_node *);
    arcp_t graph;
    arcp_t sockets;
    gln_process_fp_t process;

    volatile atomic_int state;
};

int gln_node_init(struct gln_node *node, struct gln_graph *graph, gln_process_fp_t process, void (*destroy)(struct gln_node *));
int gln_node_unlink(struct gln_node *node);

enum gln_socket_direction {
    GLNS_INPUT,
    GLNS_OUTPUT
};

struct gln_socket {
    struct arcp_region_header header;
    arcp_t graph;
    arcp_t node;
    void (*destroy)(struct gln_socket *);
    enum gln_socket_direction direction;

    atxn_t other;
    arcp_t buffer;
};

int gln_socket_init(struct gln_socket *socket, struct gln_node *node,
		    enum gln_socket_direction direction, void (*destroy)(struct gln_socket *));
int gln_socket_unlink(struct gln_socket *socket);
int gln_socket_connect(struct gln_socket *socket, struct gln_socket *other);
int gln_socket_disconnect(struct gln_socket *socket);

struct gln_buffer {
    struct arcp_region_header header;
    size_t size;
    uint8_t data[1] __attribute__((aligned(ARCP_ALIGN)));
};

void *gln_alloc_buffer(struct gln_socket *socket, size_t size);

/* use this to initiate processing */
int gln_get_buffers(struct gln_socket **sockets, void **buffers, int count);

#endif /* ! GRAPHLINE_H */
