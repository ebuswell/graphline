/*
 * graphline.c
 * 
 * Copyright 2013 Evan Buswell
 * 
 * This file is part of Graphline.
 * 
 * Graphline is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 2.
 * 
 * Graphline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Graphline.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif defined __GNUC__
# define alloca __builtin_alloca
#elif defined _AIX
# define alloca __alloca
#elif defined _MSC_VER
# include <malloc.h>
# define alloca _alloca
#else
# include <stddef.h>
void *alloca(size_t);
#endif

#include <errno.h>
#include <stdarg.h>
#include <atomickit/atomic-array.h>
#include <atomickit/atomic-rcp.h>
#include <atomickit/atomic-queue.h>
#include "graphline.h"

void gln_graph_destroy(struct gln_graph *graph) {
    arcp_store(&graph->nodes, NULL);
    aqueue_destroy(&graph->proc_queue);
}

static void __gln_graph_destroy(struct gln_graph *graph) {
    gln_graph_destroy(graph);
    afree(graph, sizeof(struct gln_graph *));
}

int gln_graph_init(struct gln_graph *graph, void (*destroy)(struct gln_graph *)) {
    int r = -1;
    struct aary *empty_array = aary_create(0);
    if(empty_array == NULL) {
	goto undo0;
    }
    arcp_init(&graph->nodes, empty_array);
    arcp_release(empty_array);
    r = aqueue_init(&graph->proc_queue);
    if(r != 0) {
	goto undo1;
    }
    arcp_region_init(graph, (void (*)(struct arcp_region *)) destroy);
    r = arcp_region_init_weakref(graph);
    if(r != 0) {
	goto undo2;
    }
    return 0;

undo2:
    aqueue_destroy(&graph->proc_queue);
undo1:
    arcp_store(&graph->nodes, NULL);
undo0:
    return r;
}

struct gln_graph *gln_graph_create() {
    int r;

    struct gln_graph *ret = amalloc(sizeof(struct gln_graph));
    if(ret == NULL) {
	return NULL;
    }

    r = gln_graph_init(ret, __gln_graph_destroy);
    if(r != 0) {
	afree(ret, sizeof(struct gln_graph));
	return NULL;
    }

    return ret;
}

void gln_graph_reset(struct gln_graph *graph) {
    struct aary *node_array = (struct aary *) arcp_load(&graph->nodes);
    size_t i;
    for(i = 0; i < aary_length(node_array); i++) {
	struct gln_node *node = (struct gln_node *) arcp_weakref_load((struct arcp_weakref *) aary_load_phantom(node_array, i));
	if(node == NULL) {
	    continue;
	}
	atomic_store(&node->state, GLNN_READY);
	arcp_release(node);
    }
    arcp_release(node_array);
}

void gln_node_destroy(struct gln_node *node) {
    /* Try and remove our weak reference from the associated graph */
    struct gln_graph *graph = (struct gln_graph *) arcp_weakref_load(node->graph);
    arcp_release(node->graph);
    if(graph == NULL) {
	return;
    }
    struct arcp_weakref *weakref = arcp_weakref_phantom(node);
    struct aary *node_list;
    struct aary *new_node_list;
    do {
	node_list = (struct aary *) arcp_load(&graph->nodes);
	new_node_list = aary_dup_set_remove(node_list, weakref);
	if(new_node_list == NULL) {
	    /* Give up */
	    arcp_release(node_list);
	    break;
	}
    } while(!arcp_compare_store_release(&graph->nodes, node_list, new_node_list));
    arcp_release(graph);
}

static void __gln_node_destroy(struct gln_node *node) {
    gln_node_destroy(node);
    afree(node, sizeof(struct gln_node));
}

int gln_node_init(struct gln_node *node, struct gln_graph *graph, gln_process_fp_t process, void (*destroy)(struct gln_node *)) {
    int r;
    node->graph = arcp_weakref(graph);
    node->process = process;
    atomic_init(&node->state, GLNN_READY);
    arcp_region_init(node, (void (*)(struct arcp_region *)) destroy);
    r = arcp_region_init_weakref(node);
    if(r != 0) {
	goto undo1;
    }
    struct arcp_weakref *weakref = arcp_weakref_phantom(node);
    struct aary *node_list;
    struct aary *new_node_list;
    do {
	node_list = (struct aary *) arcp_load(&graph->nodes);
	new_node_list = aary_dup_set_add(node_list, weakref);
	if(new_node_list == NULL) {
	    arcp_release(node_list);
	    r = -1;
	    goto undo2;
	}
    } while(!arcp_compare_store_release(&graph->nodes, node_list, new_node_list));

    return 0;

undo2:
    arcp_region_destroy_weakref(node);
undo1:
    arcp_release(node->graph);
    return r;
}

struct gln_node *gln_node_create(struct gln_graph *graph, gln_process_fp_t process) {
    int r;

    struct gln_node *ret = amalloc(sizeof(struct gln_node));
    if(ret == NULL) {
	return NULL;
    }

    r = gln_node_init(ret, graph, process, __gln_node_destroy);
    if(r != 0) {
	afree(ret, sizeof(struct gln_node));
	return NULL;
    }

    return ret;
}

void gln_socket_destroy(struct gln_socket *socket) {
    arcp_release(socket->node);
    arcp_store(&socket->buffer, NULL);
    /* try and remove ourselves from any other's lists. */
    gln_socket_disconnect(socket); /* ignore errors */
    atxn_destroy(&socket->other);
}

static void __gln_socket_destroy(struct gln_socket *socket) {
    gln_socket_destroy(socket);
    afree(socket, sizeof(struct gln_socket));
}

int gln_socket_init(struct gln_socket *socket, struct gln_node *node,
		    enum gln_socket_direction direction, void (*destroy)(struct gln_socket *)) {
    int r = -1;
    if(direction == GLNS_OUTPUT) {
	struct aary *empty_array = aary_create(0);
	if(empty_array == NULL) {
	    goto undo0;
	}
	r = atxn_init(&socket->other, empty_array);
	if(r != 0) {
	    arcp_release(empty_array);
	    goto undo0;
	}
    } else {
	r = atxn_init(&socket->other, NULL);
	if(r != 0) {
	    goto undo0;
	}
    }
    socket->node = arcp_weakref(node);
    socket->direction = direction;
    arcp_init(&socket->buffer, NULL);
    arcp_region_init(socket, (void (*)(struct arcp_region *)) destroy);
    r = arcp_region_init_weakref(socket);
    if(r != 0) {
	goto undo1;
    }
    return 0;

undo1:
    arcp_release(socket->node);
undo0:
    return r;
}

struct gln_socket *gln_socket_create(struct gln_node *node, enum gln_socket_direction direction) {
    int r;

    struct gln_socket *ret = amalloc(sizeof(struct gln_socket));
    if(ret == NULL) {
	return NULL;
    }

    r = gln_socket_init(ret, node, direction, __gln_socket_destroy);
    if(r != 0) {
	afree(ret, sizeof(struct gln_socket));
	return NULL;
    }

    return ret;
}

int gln_socket_connect(struct gln_socket *socket, struct gln_socket *other) {
    if(socket->direction != GLNS_OUTPUT) {
	if(other->direction != GLNS_OUTPUT) {
	    errno = EINVAL;
	    return -1;
	}
	struct gln_socket *tmp = socket;
	socket = other;
	other = tmp;
    } else {
	if(other->direction != GLNS_INPUT) {
	    errno = EINVAL;
	    return -1;
	}
    }

    struct arcp_weakref *socket_weakref = arcp_weakref_phantom(socket);
    struct arcp_weakref *other_weakref = arcp_weakref_phantom(other);
    enum atxn_status r;
    struct atxn_handle *handle;
    struct arcp_weakref *connected_weakref;
    struct gln_socket *connected_socket;
    struct aary *connection_list;

retry_connect:
    handle = atxn_start();
    if(handle == NULL) {
	return -1;
    }

    /* Load previously connected socket */
    r = atxn_load(handle, &other->other, (struct arcp_region **) &connected_weakref);
    if(r == ATXN_FAILURE) {
	atxn_abort(handle);
	goto retry_connect;
    } else if(r != ATXN_SUCCESS) {
	atxn_abort(handle);
	return -1;
    }

    /* See if we were connected */
    if(connected_weakref == socket_weakref) {
	atxn_abort(handle);
	return 0;
    }

    /* remove other from list of connected sockets on the
     * previously connected socket */
    connected_socket = (struct gln_socket *) arcp_weakref_load(connected_weakref);

    if(connected_socket != NULL) {
	r = atxn_load(handle, &connected_socket->other, (struct arcp_region **) &connection_list);
	if(r == ATXN_FAILURE) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    goto retry_connect;
	} else if(r != ATXN_SUCCESS) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    return -1;
	}
	connection_list = aary_dup_set_remove(connection_list, other_weakref);
	if(connection_list == NULL) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    return -1;
	}
	r = atxn_store(handle, &connected_socket->other, connection_list);
	arcp_release(connection_list);
	arcp_release(connected_socket);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_connect;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}
    }

    /* Add other to list of connected sockets */
    r = atxn_load(handle, &socket->other, (struct arcp_region **) &connection_list);
    if(r == ATXN_FAILURE) {
	atxn_abort(handle);
	goto retry_connect;
    } else if(r != ATXN_SUCCESS) {
	atxn_abort(handle);
	return -1;
    }
    connection_list = aary_dup_set_add(connection_list, other_weakref);
    if(connection_list == NULL) {
	atxn_abort(handle);
	return -1;
    }
    r = atxn_store(handle, &socket->other, connection_list);
    arcp_release(connection_list);
    if(r == ATXN_FAILURE) {
	atxn_abort(handle);
	goto retry_connect;
    } else if(r != ATXN_SUCCESS) {
	atxn_abort(handle);
	return -1;
    }

    /* Set new connected socket */
    r = atxn_store(handle, &other->other, socket_weakref);
    if(r == ATXN_FAILURE) {
	atxn_abort(handle);
	goto retry_connect;
    } else if(r != ATXN_SUCCESS) {
	atxn_abort(handle);
	return -1;
    }

    /* commit */
    r = atxn_commit(handle);
    if(r == ATXN_FAILURE) {
	goto retry_connect;
    } else if(r != ATXN_SUCCESS) {
	return -1;
    }
    return 0;
}

int gln_socket_disconnect(struct gln_socket *socket) {
    struct arcp_weakref *socket_weakref = arcp_weakref_phantom(socket);
    enum atxn_status r;
    struct atxn_handle *handle;
    struct arcp_weakref *connected_weakref;
    struct gln_socket *connected_socket;
    struct aary *connection_list;

    if(socket->direction == GLNS_INPUT) {

    retry_disconnect_input:
	handle = atxn_start();
	if(handle == NULL) {
	    return -1;
	}

	/* Load connected socket */
	r = atxn_load(handle, &socket->other, (struct arcp_region **) &connected_weakref);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_disconnect_input;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}

	/* See if there was a connection */
	if((connected_weakref == NULL)
	   || ((connected_socket = (struct gln_socket *) arcp_weakref_load(connected_weakref)) == NULL)) {
	    atxn_abort(handle);
	    return 0;
	}

	/* remove self from list of connected sockets */
	r = atxn_load(handle, &connected_socket->other, (struct arcp_region **) &connection_list);
	if(r == ATXN_FAILURE) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    goto retry_disconnect_input;
	} else if(r != ATXN_SUCCESS) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    return -1;
	}
	connection_list = aary_dup_set_remove(connection_list, socket_weakref);
	if(connection_list == NULL) {
	    arcp_release(connected_socket);
	    atxn_abort(handle);
	    return -1;
	}
	r = atxn_store(handle, &connected_socket->other, connection_list);
	arcp_release(connection_list);
	arcp_release(connected_socket);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_disconnect_input;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}

	/* disconnect */
	r = atxn_store(handle, &socket->other, NULL);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_disconnect_input;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}

	/* commit */
	r = atxn_commit(handle);
	if(r == ATXN_FAILURE) {
	    goto retry_disconnect_input;
	} else if(r != ATXN_SUCCESS) {
	    return -1;
	}
    } else {

    retry_disconnect_output:
	handle = atxn_start();
	if(handle == NULL) {
	    return -1;
	}

	/* Load list of connected sockets */
	r = atxn_load(handle, &socket->other, (struct arcp_region **) &connection_list);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_disconnect_output;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}

	/* See if we're already disconnected */
	if(aary_length(connection_list) == 0) {
	    atxn_abort(handle);
	    return 0;
	}

	/* Disconnect each socket */
	size_t i;
	for(i = 0; i < aary_length(connection_list); i++) {
	    connected_socket = (struct gln_socket *) arcp_weakref_load((struct arcp_weakref *) aary_load_phantom(connection_list, i));
	    r = atxn_store(handle, &connected_socket->other, NULL);
	    arcp_release(connected_socket);
	    if(r == ATXN_FAILURE) {
		atxn_abort(handle);
		goto retry_disconnect_output;
	    } else if(r != ATXN_SUCCESS) {
		atxn_abort(handle);
		return -1;
	    }
	}

	/* empty out connection list */
	connection_list = aary_create(0);
	if(connection_list == NULL) {
	    atxn_abort(handle);
	    return -1;
	}
	r = atxn_store(handle, &socket->other, connection_list);
	arcp_release(connection_list);
	if(r == ATXN_FAILURE) {
	    atxn_abort(handle);
	    goto retry_disconnect_output;
	} else if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}

	/* commit */
	r = atxn_commit(handle);
	if(r == ATXN_FAILURE) {
	    goto retry_disconnect_output;
	} else if(r != ATXN_SUCCESS) {
	    return -1;
	}
    }
    return 0;
}

#define GLN_BUFFER_OVERHEAD (offsetof(struct gln_buffer, data))

static void __destroy_gln_buffer(struct gln_buffer *buffer) {
    afree(buffer, buffer->size);
}

void *gln_alloc_buffer(struct gln_socket *socket, size_t size) {
    size += GLN_BUFFER_OVERHEAD;
    struct gln_buffer *buffer = (struct gln_buffer *) arcp_load_phantom(&socket->buffer);
    if(buffer != NULL
       && buffer->destroy == (void (*)(struct arcp_region *)) __destroy_gln_buffer
       && buffer->size == size) {
	return &buffer->data;
    }
    buffer = amalloc(size);
    if(buffer == NULL) {
	return NULL;
    }
    buffer->size = size;
    arcp_region_init(buffer, (void (*)(struct arcp_region *)) __destroy_gln_buffer);
    arcp_store(&socket->buffer, buffer);
    arcp_release(buffer);
    return &buffer->data;
}

void gln_set_buffer(struct gln_socket *socket, void *buffer) {
    struct gln_buffer *glnbuffer = (struct gln_buffer *) (buffer - offsetof(struct gln_buffer, data));
    arcp_store(&socket->buffer, glnbuffer);
}

int gln_get_buffers(int count, ...) {
    int i, r;

    va_list ap;
    struct gln_socket **sockets = alloca(count * sizeof(struct gln_socket *));
    void ***buffers_ret = alloca(count * sizeof(void **));

    va_start(ap, count);

    for(i = 0; i < count; i++) {
	sockets[i] = va_arg(ap, struct gln_socket *);
	buffers_ret[i] = va_arg(ap, void **);
    }

    va_end(ap);

    void **buffers = alloca(count * sizeof(void *));
    r = gln_get_buffer_list(count, sockets, buffers);
    for(i = 0; i < count; i++) {
	*buffers_ret[i] = buffers[i];
    }
    return r;
}

int gln_get_buffer_list(int count, struct gln_socket **sockets, void **buffers) {
    int i, r;

    /* Get all connected sockets */
    struct atxn_handle *handle;
    struct arcp_weakref *connected_weakref;
    struct gln_socket **connected_sockets = alloca(sizeof(struct gln_socket *) * count);
    enum atxn_status status;

retry_acquire_connections:
    handle = atxn_start();
    for(i = 0; i < count; i++) {
	status = atxn_load(handle, &sockets[i]->other, (struct arcp_region **) &connected_weakref);
	if(status == ATXN_FAILURE) {
	    atxn_abort(handle);
	    for(i--; i >= 0; i--) {
		arcp_release(connected_sockets[i]);
	    }
	    goto retry_acquire_connections;
	} else if(status != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    return -1;
	}
	connected_sockets[i] = (struct gln_socket *) arcp_weakref_load(connected_weakref);
    }
    atxn_abort(handle);

    /* Get all pending nodes and add them to the queue where
     * needed. */
    struct gln_node **nodes = alloca(sizeof(struct gln_node *) * count);
    int node_count = 0;

    for(i = 0; i < count; i++) {
	if(connected_sockets[i] == NULL) {
	    continue;
	}

	struct gln_node *node = (struct gln_node *) arcp_weakref_load(connected_sockets[i]->node);
	if(node == NULL) {
	    arcp_store(&connected_sockets[i]->buffer, NULL);
	    goto next_socket;
	}

	int lo = 0;
	int hi = node_count;
	int j = 0;
	while(lo < hi) {
	    j = (lo + hi) / 2;
	    if(node < nodes[j]) {
		hi = j;
	    } else if(node > nodes[j]) {
		lo = ++i;
	    } else {
		/* Already present */
		arcp_release(node);
		goto next_socket;
	    }
	}

	enum gln_node_state state = GLNN_READY;
	/* Add it to the queue */
	if(atomic_compare_exchange_strong_explicit(&node->state, (int *) &state, GLNN_PENDING,
						   memory_order_acq_rel, memory_order_relaxed)) {
	    struct gln_graph *graph = (struct gln_graph *) arcp_weakref_load(node->graph);
	    if(graph == NULL) {
		atomic_store_explicit(&node->state, GLNN_READY, memory_order_release);
	    } else {
		r = aqueue_enq(&graph->proc_queue, node);
		arcp_release(graph);
		if(r != 0) {
		    atomic_store_explicit(&node->state, GLNN_READY, memory_order_release);
		    arcp_release(node);
		    goto abort;
		}
	    }
	} else if(state == GLNN_FINISHED) {
	    arcp_release(node);
	    goto next_socket;
	} else if(state == GLNN_ERROR) {
	    arcp_release(node);
	    r = -1;
	    goto abort;
	}
	memmove(&nodes[j + 1], &nodes[j], sizeof(struct gln_node *) * (node_count - j));
	nodes[j] = node;
	node_count++;

    next_socket:
	continue;
    }

    /* Process stuff until the nodes we're waiting on have been processed. */
    for(i = 0; i < node_count; i++) {
	struct gln_node *node = nodes[i];

	for(;;) {
	    /* check on the state of the node */
	    enum gln_node_state state = atomic_load_explicit(&node->state, memory_order_acquire);
	    if(state == GLNN_FINISHED) {
		break;
	    } else if(state == GLNN_ERROR) {
		r = -1;
		goto abort;
	    }
	    struct gln_graph *graph = (struct gln_graph *) arcp_weakref_load(node->graph);
	    if(state == GLNN_READY) {
		/* Somebody else was trying to add it to the queue,
		 * but failed. We'll add it insted. */
		if(atomic_compare_exchange_strong_explicit(&node->state, (int *) &state, GLNN_PENDING,
							   memory_order_acq_rel, memory_order_relaxed)) {
		    if(graph == NULL) {
			/* No queue to add this to. Process it
			 * directly. */
			r = node->process(node);
			if(r != 0) {
			    atomic_store_explicit(&node->state, GLNN_ERROR, memory_order_release);
			} else {
			    atomic_store_explicit(&node->state, GLNN_FINISHED, memory_order_release);
			}
		    } else {
			r = aqueue_enq(&graph->proc_queue, node);
			arcp_release(graph);
			if(r != 0) {
			    atomic_store_explicit(&node->state, GLNN_READY, memory_order_release);
			    goto abort;
			}
		    }
		} else {
		    arcp_release(graph);
		}
		continue;
	    }
	    if(graph == NULL) {
		/* We have no way of knowing whether or not this will
		 * ever complete. Just abort. */
		r = -1;
		goto abort;
	    }

	    struct gln_node *next = (struct gln_node *) aqueue_deq(&graph->proc_queue);
	    if(next == NULL) {
		/* This becomes a spinlock waiting on other threads to finish processing... */
		cpu_yield();
		continue;
	    }
	    r = next->process(next);
	    if(r != 0) {
		atomic_store_explicit(&next->state, GLNN_ERROR, memory_order_release);
	    } else {
		atomic_store_explicit(&next->state, GLNN_FINISHED, memory_order_release);
	    }
	    arcp_release(next);
	}
    }

    for(i = 0; i < node_count; i++) {
	arcp_release(nodes[i]);
    }

    /* Now we can load up our buffers */
    for(i = 0; i < count; i++) {
	if(connected_sockets[i] == NULL) {
	    buffers[i] = NULL;
	    arcp_store(&sockets[i]->buffer, NULL);
	} else {
	    struct gln_buffer *buf = (struct gln_buffer *) arcp_load_phantom(&connected_sockets[i]->buffer);
	    buffers[i] = buf->data;
	    arcp_store(&sockets[i]->buffer, buf);
	    arcp_release(connected_sockets[i]);
	}
    }

    return 0;

abort:
    for(i = 0; i < node_count; i++) {
	arcp_release(nodes[i]);
    }

    for(i = 0; i < count; i++) {
	arcp_release(connected_sockets[i]);
    }

    return r;
}

bool gln_process(struct gln_graph *graph) {
    int r;
    struct gln_node *next = (struct gln_node *) aqueue_deq(&graph->proc_queue);
    if(next == NULL) {
	return false;
    }
    r = next->process(next);
    if(r != 0) {
	atomic_store_explicit(&next->state, GLNN_ERROR, memory_order_release);
    } else {
	atomic_store_explicit(&next->state, GLNN_FINISHED, memory_order_release);
    }
    arcp_release(next);
    return true;
}
