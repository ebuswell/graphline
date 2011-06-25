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
#include <atomickit/atomic-ptr.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static char *address_to_string(void *thing, int indentlevel) {
    char *str = alloca(19);
    if(str == NULL) {
	return NULL;
    }
    int r = sprintf(str, "%p", thing);
    if(r < 0) {
	return NULL;
    }
    char *ret = malloc(strlen(str) + 1);
    if(ret == NULL) {
	return NULL;
    }
    strcpy(ret, str);
    return ret;
}

static char *gln_socket_to_string_indent(struct gln_socket *socket, int indentlevel) {
    char *address_str = alloca(19);
    int r = sprintf(address_str, "%p", socket);
    if(r < 0) {
	return NULL;
    }
    char *graph_str = alloca(19);
    r = sprintf(graph_str, "%p", socket->graph);
    if(r < 0) {
	return NULL;
    }
    char *node_str = alloca(19);
    r = sprintf(node_str, "%p", socket->node);
    if(r < 0) {
	return NULL;
    }
    char *direction_str = socket->direction == OUTPUT ? "OUTPUT" : socket->direction == INPUT ? "INPUT" : "CORRUPT";
    char *buffer_str = alloca(19);
    r = sprintf(buffer_str, "%p", socket->buffer);
    if(r < 0) {
	return NULL;
    }

    char *other_str;
    if(socket->direction == OUTPUT) {
	other_str = atomic_list_to_string_indent(&socket->other.list, indentlevel + 4, address_to_string);
	if(other_str == NULL) {
	    return NULL;
	}
    } else {
	other_str = alloca(19);
	r = sprintf(other_str, "%p", atomic_ptr_read(&socket->other.ptr));
	if(r < 0) {
	    return NULL;
	}
    }
    char *ret = malloc(strlen(address_str) + 3 /* {\n*/
		       + indentlevel + 4 + strlen("graph = ") + strlen(graph_str) + 2 /*,\n*/
		       + indentlevel + 4 + strlen("node = ") + strlen(node_str) + 2
		       + indentlevel + 4 + strlen("direction = ") + strlen(direction_str) + 2
		       + indentlevel + 4 + strlen("buffer = ") + strlen(buffer_str) + 2
		       + indentlevel + 4 + strlen("other = ") + strlen(other_str) + 1
		       + indentlevel + 2 /*}\0*/);
    if(ret == NULL) {
	if(socket->direction == OUTPUT) {
	    free(other_str);
	}
	return NULL;
    }

    char *ret_i = ret;
    strcpy(ret_i, address_str);
    ret_i += strlen(address_str);
    strcpy(ret_i, " {\n");
    ret_i += 3;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "graph = ");
    ret_i += strlen("graph = ");
    strcpy(ret_i, graph_str);
    ret_i += strlen(graph_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "node = ");
    ret_i += strlen("node = ");
    strcpy(ret_i, node_str);
    ret_i += strlen(node_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "direction = ");
    ret_i += strlen("direction = ");
    strcpy(ret_i, direction_str);
    ret_i += strlen(direction_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "buffer = ");
    ret_i += strlen("buffer = ");
    strcpy(ret_i, buffer_str);
    ret_i += strlen(buffer_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;
    
    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "other = ");
    ret_i += strlen("other = ");
    strcpy(ret_i, other_str);
    ret_i += strlen(other_str);
    strcpy(ret_i, "\n");
    ret_i += 1;
    
    if(socket->direction == OUTPUT) {
	free(other_str);
    }

    memset(ret_i, ' ', indentlevel);
    ret_i += indentlevel;
    strcpy(ret_i, "}");

    return ret;
}

char *gln_socket_to_string(struct gln_socket *socket) {
    return gln_socket_to_string_indent(socket, 0);
}

static char *gln_node_to_string_indent(struct gln_node *node, int indentlevel) {
    char *address_str = alloca(19);
    int r = sprintf(address_str, "%p", node);
    if(r < 0) {
	return NULL;
    }
    char *graph_str = alloca(19);
    r = sprintf(graph_str, "%p", node->graph);
    if(r < 0) {
	return NULL;
    }
    char *process_str = alloca(19);
    r = sprintf(process_str, "%p", node->process);
    if(r < 0) {
	return NULL;
    }
    char *arg_str = alloca(19);
    r = sprintf(arg_str, "%p", node->arg);
    if(r < 0) {
	return NULL;
    }
    char *sockets_str = atomic_list_to_string_indent(&node->sockets, indentlevel + 4, (char * (*)(void *, int)) gln_socket_to_string_indent);
    if(sockets_str == NULL) {
	return NULL;
    }    

    char *ret = malloc(strlen(address_str) + 3 /* {\n*/
		       + indentlevel + 4 + strlen("graph = ") + strlen(graph_str) + 2 /*,\n*/
		       + indentlevel + 4 + strlen("process = ") + strlen(process_str) + 2
		       + indentlevel + 4 + strlen("arg = ") + strlen(arg_str) + 2
		       + indentlevel + 4 + strlen("sockets = ") + strlen(sockets_str) + 1
		       + indentlevel + 2 /*}\0*/);
    if(ret == NULL) {
	free(sockets_str);
	return NULL;
    }

    char *ret_i = ret;
    strcpy(ret_i, address_str);
    ret_i += strlen(address_str);
    strcpy(ret_i, " {\n");
    ret_i += 3;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "graph = ");
    ret_i += strlen("graph = ");
    strcpy(ret_i, graph_str);
    ret_i += strlen(graph_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "process = ");
    ret_i += strlen("process = ");
    strcpy(ret_i, process_str);
    ret_i += strlen(process_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "arg = ");
    ret_i += strlen("arg = ");
    strcpy(ret_i, arg_str);
    ret_i += strlen(arg_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;
    
    memset(ret_i, ' ', indentlevel + 4);
    ret_i += indentlevel + 4;
    strcpy(ret_i, "sockets = ");
    ret_i += strlen("sockets = ");
    strcpy(ret_i, sockets_str);
    ret_i += strlen(sockets_str);
    strcpy(ret_i, "\n");
    ret_i += 1;
    
    free(sockets_str);

    memset(ret_i, ' ', indentlevel);
    ret_i += indentlevel;
    strcpy(ret_i, "}");

    return ret;
}

char *gln_node_to_string(struct gln_node *node) {
    return gln_node_to_string_indent(node, 0);
}

char *gln_graph_to_string(struct gln_graph *graph) {
    char *address_str = alloca(19);
    int r = sprintf(address_str, "%p", graph);
    if(r < 0) {
	return NULL;
    }
    char *buffer_heap_str = alloca(19);
    r = sprintf(buffer_heap_str, "%p", graph->buffer_heap);
    if(r < 0) {
	return NULL;
    }
    char *buffer_size_str = alloca(21);
    r = sprintf(buffer_size_str, "%zd", graph->buffer_size);
    if(r < 0) {
	return NULL;
    }
    char *nodes_str = atomic_list_to_string_indent(&graph->nodes, 4, (char * (*)(void *, int)) gln_node_to_string_indent);
    if(nodes_str == NULL) {
	return NULL;
    }
    char *ret = malloc(strlen(address_str) + 3 /* {\n*/
		       + 4 + strlen("buffer_size = ") + strlen(buffer_size_str) + 2 /*,\n*/
		       + 4 + strlen("buffer_heap = ") + strlen(buffer_heap_str) + 2
		       + 4 + strlen("nodes = ") + strlen(nodes_str) + 1
		       + 2 /*}\0*/);
    if(ret == NULL) {
	free(nodes_str);
	return NULL;
    }

    char *ret_i = ret;
    strcpy(ret_i, address_str);
    ret_i += strlen(address_str);
    strcpy(ret_i, " {\n");
    ret_i += 3;

    memset(ret_i, ' ', 4);
    ret_i += 4;
    strcpy(ret_i, "buffer_size = ");
    ret_i += strlen("buffer_size = ");
    strcpy(ret_i, buffer_size_str);
    ret_i += strlen(buffer_size_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;

    memset(ret_i, ' ', 4);
    ret_i += 4;
    strcpy(ret_i, "buffer_heap = ");
    ret_i += strlen("buffer_heap = ");
    strcpy(ret_i, buffer_heap_str);
    ret_i += strlen(buffer_heap_str);
    strcpy(ret_i, ",\n");
    ret_i += 2;
    
    memset(ret_i, ' ', 4);
    ret_i += 4;
    strcpy(ret_i, "nodes = ");
    ret_i += strlen("nodes = ");
    strcpy(ret_i, nodes_str);
    ret_i += strlen(nodes_str);
    strcpy(ret_i, "\n");
    ret_i += 1;
    
    free(nodes_str);

    strcpy(ret_i, "}");

    return ret;
}

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

void gln_destroy_graph(struct gln_graph *graph) {
    while(1) {
	struct gln_node *node = (struct gln_node *) atomic_list_last(&graph->nodes);
	if(node == ALST_EMPTY) {
	    break;
	}
	gln_destroy_node(node);
    }
    atomic_list_destroy(&graph->nodes);
    void *buffer;
    while((buffer = g_trash_stack_pop(&graph->buffer_heap)) != NULL) {
	free(buffer);
    }
    free(graph);
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

void gln_destroy_node(struct gln_node *node) {
    while(1) {
	struct gln_socket *socket = (struct gln_socket *) atomic_list_last(&node->sockets);
	if(socket == ALST_EMPTY) {
	    break;
	}
	gln_destroy_socket(socket);
    }
    atomic_list_remove_by_value(&node->graph->nodes, node);
    atomic_list_destroy(&node->sockets);
    free(node);
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

void gln_destroy_socket(struct gln_socket *socket) {
    gln_socket_disconnect(socket);
    atomic_list_remove_by_value(&socket->node->sockets, socket);
    if(socket->direction == OUTPUT) {
	atomic_list_destroy(&socket->other.list);
    }
    if((socket->buffer != NULL)
       && (BUFFER_MARKED(socket->buffer))) {
	g_trash_stack_push(&socket->graph->buffer_heap, socket->buffer);
    }
    free(socket);
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
	atomic_list_remove_by_value(&prev_output->other.list, input);
    }
    r = atomic_list_push(&output->other.list, input);
    if(r != 0) {
	atomic_ptr_set(&input->other.ptr, NULL);
	return r;
    }
    return 0;
}

void gln_socket_disconnect(struct gln_socket *socket) {
    if(socket->direction == OUTPUT) {
	while(1) {
	    struct gln_socket *other = (struct gln_socket *) atomic_list_last(&socket->other.list);
	    if(other == ALST_EMPTY) {
		break;
	    }
	    gln_socket_disconnect(other);
	}
    } else {
	struct gln_socket *other = atomic_ptr_read(&socket->other.ptr);
	if(other == NULL) {
	    return;
	}
	atomic_ptr_set(&socket->other.ptr, NULL);
	atomic_list_remove_by_value(&other->other.list, socket);
    }
}

int gln_reset_graph(struct gln_graph *graph) {
    /* gather up all buffers */
    int r;
    atomic_iterator_t i;
    r = atomic_iterator_init(&graph->nodes, &i);
    if(r != 0) {
	return r;
    }
    struct gln_node *node;
    while((node = (struct gln_node *) atomic_iterator_next(&graph->nodes, &i)) != ALST_EMPTY) {
	atomic_iterator_t j;
	r = atomic_iterator_init(&node->sockets, &j);
	if(r != 0) {
	    atomic_iterator_destroy(&graph->nodes, &i);
	    atomic_iterator_destroy(&node->sockets, &j);
	    return r;
	}
	struct gln_socket *socket;
	while((socket = (struct gln_socket *) atomic_iterator_next(&node->sockets, &j)) != ALST_EMPTY) {
	    if(BUFFER_MARKED(socket->buffer)) {
		g_trash_stack_push(&graph->buffer_heap, UNMARK_BUFFER(socket->buffer));
	    }
	    socket->buffer = NULL;
	}
	atomic_iterator_destroy(&node->sockets, &j);
    }
    atomic_iterator_destroy(&graph->nodes, &i);
    return 0;
}

static void *gln_graph_get_buffer(struct gln_graph *graph) {
    void *buffer = g_trash_stack_pop(&graph->buffer_heap);
    if(buffer == NULL) {
	buffer = malloc(graph->buffer_size);
    }
    memset(buffer, 0, graph->buffer_size);
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
