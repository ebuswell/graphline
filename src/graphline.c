/*
 * graphline.c
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

#include <stdlib.h>
#include <glib.h>
#include <graphline.h>
#include <atomickit/atomic-list.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

struct gln_graph *gln_create_graph(size_t buffer_size) {
    struct gln_graph *graph = malloc(sizeof(struct gln_graph));
    if(graph == NULL) {
	return NULL;
    }
    graph->buffer_size = buffer_size;
    int r;
    r = atomic_list_init(&graph->nodes);
    if(r != 0) {
	free(graph);
	return NULL;
    }
    graph->buffer_heap = NULL;
    return graph;
}

int gln_destroy_graph(struct gln_graph *graph) {
    int r = 0;
    while(1) {
	struct gln_node *node = (struct gln_node *) atomic_list_last(&graph->nodes);
	if(node == ALST_ERROR) {
	    r = -1;
	    break;
	} else if(node == ALST_EMPTY) {
	    break;
	}
	r |= gln_destroy_node(node);
    }
    r |= atomic_list_destroy(&graph->nodes);
    void *buffer;
    while((buffer = g_trash_stack_pop(&graph->buffer_heap)) != NULL) {
	free(buffer);
    }
    free(graph);
    return r;
}

struct gln_node *gln_create_node(struct gln_graph *graph, gln_process_fp_t process, void *arg) {
    struct gln_node *node = malloc(sizeof(struct gln_node));
    if(node == NULL) {
	return NULL;
    }
    int r;
    r = atomic_list_init(&node->sockets);
    if(r != 0) {
	free(node);
	return NULL;
    }
    node->graph = graph;
    node->process = process;
    node->arg = arg;
    r = atomic_list_push(&graph->nodes, node);
    if(r != 0) {
	atomic_list_destroy(&node->sockets);
	free(node);
	return NULL;
    }
    return node;
}

int gln_destroy_node(struct gln_node *node) {
    int r = 0;
    while(1) {
	struct gln_socket *socket = (struct gln_socket *) atomic_list_last(&node->sockets);
	if(socket == ALST_ERROR) {
	    r = -1;
	    break;
	} else if(socket == ALST_EMPTY) {
	    break;
	}
	r |= gln_destroy_socket(socket);
    }
    r |= atomic_list_remove_by_value(&node->graph->nodes, node);
    r |= atomic_list_destroy(&node->sockets);
    free(node);
    return r;
}

struct gln_socket *gln_create_socket(struct gln_node *node,
				     enum gln_socket_direction direction) {
    struct gln_socket *socket = malloc(sizeof(struct gln_socket));
    if(socket == NULL) {
	return NULL;
    }
    int r;
    if(direction == INPUT) {
	atomic_ptr_set(&socket->other.ptr, NULL);
    } else {
	r = atomic_list_init(&socket->other.list);
	if(r != 0) {
	    free(socket);
	    return NULL;
	}
    }
    socket->node = node;
    socket->graph = node->graph;
    socket->direction = direction;
    socket->buffer = NULL;
    r = atomic_list_push(&node->sockets, socket);
    if(r != 0) {
	if(direction == OUTPUT) {
	    atomic_list_destroy(&socket->other.list);
	}
	free(socket);
	return NULL;
    }
    return socket;
}

#define MARK_BUFFER(b) ((void *) (((intptr_t) (b)) | ((intptr_t) 1)))
#define UNMARK_BUFFER(b) ((void *) (((intptr_t) (b)) & ~((intptr_t) 1)))
#define BUFFER_MARKED(b) ((void *) (((intptr_t) (b)) & ((intptr_t) 1)))

int gln_destroy_socket(struct gln_socket *socket) {
    int r;
    r = gln_socket_disconnect(socket);
    r |= atomic_list_remove_by_value(&socket->node->sockets, socket);
    if(socket->direction == OUTPUT) {
	r = atomic_list_destroy(&socket->other.list);
    }
    if((socket->buffer != NULL)
       && (BUFFER_MARKED(socket->buffer))) {
	g_trash_stack_push(&socket->graph->buffer_heap, socket->buffer);
    }
    free(socket);
    return r;
}

int gln_socket_connect(struct gln_socket *output, struct gln_socket *input) {
    if(input->direction == OUTPUT) {
	if(output->direction == OUTPUT) {
	    errno = EINVAL;
	    return -1;
	}
	return gln_socket_connect(input, output);
    }

    if(output->direction == INPUT) {
	errno = EINVAL;
	return -1;
    }

    struct gln_socket *prev_output = atomic_ptr_read(&input->other.ptr);
    atomic_ptr_set(&input->other.ptr, output);
    int r;
    if(prev_output != NULL) {
	r = atomic_list_remove_by_value(&prev_output->other.list, input);
	if(r != 0) {
	    atomic_ptr_set(&input->other.ptr, prev_output);
	    return r;
	}
    }
    r = atomic_list_push(&output->other.list, input);
    if(r != 0) {
	atomic_ptr_set(&input->other.ptr, NULL);
	return r;
    }
    return 0;
}

int gln_socket_disconnect(struct gln_socket *socket) {
    if(socket->direction == OUTPUT) {
	int r = 0;
	while(1) {
	    struct gln_socket *other = (struct gln_socket *) atomic_list_last(&socket->other.list);
	    if(other == ALST_ERROR) {
		r = -1;
		break;
	    } else if(other == ALST_EMPTY) {
		break;
	    }
	    r |= gln_socket_disconnect(other);
	}
	return r;
    } else {
	struct gln_socket *other = atomic_ptr_read(&socket->other.ptr);
	if(other == NULL) {
	    return 0;
	}
	atomic_ptr_set(&socket->other.ptr, NULL);
	int r;
	r = atomic_list_remove_by_value(&other->other.list, socket);
	return r;
    }
}

void gln_start_processing(struct gln_graph *graph) {
    atomic_list_readlock(&graph->nodes);
    size_t nodes_length = nonatomic_list_length(&graph->nodes);
    struct gln_node **nodes = (struct gln_node **) nonatomic_list_ary(&graph->nodes);
    size_t i;
    for(i = 0; i < nodes_length; i++) {
	atomic_list_readlock(&nodes[i]->sockets);
	size_t sockets_length = nonatomic_list_length(&nodes[i]->sockets);
	struct gln_socket **sockets = (struct gln_socket **) nonatomic_list_ary(&nodes[i]->sockets);
	size_t j;
	for(j = 0; j < sockets_length; j++) {
	    if(sockets[j]->direction == OUTPUT) {
		atomic_list_readlock(&sockets[j]->other.list);
	    }
	}
    }
}

void gln_finish_processing(struct gln_graph *graph) {
    size_t nodes_length = nonatomic_list_length(&graph->nodes);
    struct gln_node **nodes = (struct gln_node **) nonatomic_list_ary(&graph->nodes);
    size_t i;
    for(i = 0; i < nodes_length; i++) {
	size_t sockets_length = nonatomic_list_length(&nodes[i]->sockets);
	struct gln_socket **sockets = (struct gln_socket **) nonatomic_list_ary(&nodes[i]->sockets);
	size_t j;
	for(j = 0; j < sockets_length; j++) {
	    if(sockets[j]->direction == OUTPUT) {
		atomic_list_readunlock(&sockets[j]->other.list);
	    }
	}
	atomic_list_readunlock(&nodes[i]->sockets);
    }
    atomic_list_readunlock(&graph->nodes);
}

void gln_reset_graph(struct gln_graph *graph) {
    /* gather up all buffers */
    size_t nodes_length = nonatomic_list_length(&graph->nodes);
    struct gln_node **nodes = (struct gln_node **) nonatomic_list_ary(&graph->nodes);
    size_t i;
    for(i = 0; i < nodes_length; i++) {
	size_t sockets_length = nonatomic_list_length(&nodes[i]->sockets);
	struct gln_socket **sockets = (struct gln_socket **) nonatomic_list_ary(&nodes[i]->sockets);
	size_t j;
	for(j = 0; j < sockets_length; j++) {
	    if(BUFFER_MARKED(sockets[j]->buffer)) {
		g_trash_stack_push(&graph->buffer_heap, UNMARK_BUFFER(sockets[j]->buffer));
	    }
	    sockets[j]->buffer = NULL;
	}
    }
}

static void *gln_graph_get_buffer(struct gln_graph *graph) {
    void *buffer = g_trash_stack_pop(&graph->buffer_heap);
    if(buffer == NULL) {
	buffer = malloc(graph->buffer_size);
    }
    return buffer;
}

void *gln_socket_get_buffer(struct gln_socket *socket) {
    if(socket->buffer == NULL) {
	if(socket->direction == OUTPUT) {
	    socket->buffer = gln_graph_get_buffer(socket->graph);
	    if(socket->buffer == NULL) {
		return NULL;
	    }
	    socket->buffer = MARK_BUFFER(socket->buffer);
	} else { /* INPUT */
	    struct gln_socket *other = atomic_ptr_read(&socket->other.ptr);
	    if(other == NULL) {
		socket->buffer = gln_graph_get_buffer(socket->graph);
		if(socket->buffer == NULL) {
		    return NULL;
		}
		memset(socket->buffer, 0, socket->graph->buffer_size);
		socket->buffer = MARK_BUFFER(socket->buffer);
	    } else {
		if(other->buffer != NULL) {
		    socket->buffer = UNMARK_BUFFER(other->buffer);
		} else {
		    int r = other->node->process(other->node->arg);
		    if(r != 0) {
			return NULL;
		    }
		    if(other->buffer != NULL) {
			socket->buffer = UNMARK_BUFFER(other->buffer);
		    } else {
			socket->buffer = gln_graph_get_buffer(socket->graph);
			if(socket->buffer == NULL) {
			    return NULL;
			}
			memset(socket->buffer, 0, socket->graph->buffer_size);
			other->buffer = socket->buffer;
			socket->buffer = MARK_BUFFER(socket->buffer);
		    }
		}
	    }
	}
    }
    return UNMARK_BUFFER(socket->buffer);
}
