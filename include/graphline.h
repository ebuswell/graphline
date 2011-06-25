/*
 * graphline.h
 * 
 * Copyright 2011 Evan Buswell
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

#include <glib.h>
#include <stdbool.h>
#include <atomickit/atomic-list.h>
#include <atomickit/atomic-types.h>

struct gln_graph {
    size_t buffer_size;
    GTrashStack *buffer_heap;
    atomic_list_t nodes;
};

struct gln_graph *gln_create_graph(size_t buffer_size);
void gln_destroy_graph(struct gln_graph *graph);
char *gln_graph_to_string(struct gln_graph *graph);
/* call this at the start or end of processing, as appropriate */
int gln_reset_graph(struct gln_graph *graph);

typedef int (*gln_process_fp_t)(void *);

struct gln_node {
    struct gln_graph *graph;
    gln_process_fp_t process;
    void *arg;

    atomic_list_t sockets;
};

struct gln_node *gln_create_node(struct gln_graph *graph, gln_process_fp_t process, void *arg);
void gln_destroy_node(struct gln_node *node);
char *gln_node_to_string(struct gln_node *node);

enum gln_socket_direction {
    INPUT,
    OUTPUT
};

struct gln_socket {
    struct gln_graph *graph;
    struct gln_node *node;
    enum gln_socket_direction direction;

    union {
	atomic_ptr_t ptr;
	atomic_list_t list;
    } other;
    void *buffer;
};

struct gln_socket *gln_create_socket(struct gln_node *node,
				     enum gln_socket_direction direction);
void gln_destroy_socket(struct gln_socket *socket);
char *gln_socket_to_string(struct gln_socket *socket);
int gln_socket_connect(struct gln_socket *socket, struct gln_socket *other);
void gln_socket_disconnect(struct gln_socket *socket);
/* only use this within the process callback */
void *gln_socket_get_buffer(struct gln_socket *socket);

#endif /* ! GRAPHLINE_H */
