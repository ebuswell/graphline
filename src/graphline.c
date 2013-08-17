/*
 * graphline.c
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

#include <atomickit/atomic-array.h>
#include <errno.h>
#include "graphline.h"

int gln_graph_init(struct gln_graph *graph, void (*destroy)(struct gln_graph *)) {
    struct aary *empty_array = aary_new(0);
    if(empty_array == 0) {
	return -1;
    }
    int r = aqueue_init(&graph->proc_queue);
    if(r != 0) {
	arcp_release((struct arcp_region *) empty_array);
	return -1;
    }
    arcp_init(&graph->nodes, (struct arcp_region *) empty_array);
    arcp_release((struct arcp_region *) empty_array);
    arcp_region_init((struct arcp_region *) graph, (void (*)(struct arcp_region *)) destroy);
    return 0;
}

void gln_graph_destroy(struct gln_graph *graph) {
    arcp_store(&graph->nodes, NULL);
    aqueue_destroy(&graph->proc_queue);
}

int gln_graph_reset(struct gln_graph *graph) {
    struct aary *node_array = (struct aary *) arcp_load(&graph->nodes);
    if(node_array == NULL) {
	errno = EINVAL;
	return -1;
    }
    size_t i;
    for(i = 0; i < aary_length(node_array); i++) {
	struct gln_node *node = (struct gln_node *) aary_load_weak(node_array, i);
	struct aary *socket_array = (struct aary *) arcp_load(&node->sockets);
	if(socket_array == NULL) {
	    errno = EINVAL;
	    return -1;
	}
	size_t j;
	for(j = 0; j < aary_length(socket_array); j++) {
	    struct gln_socket *socket = (struct gln_socket *) aary_load_weak(socket_array, j);
	    arcp_store(&socket->buffer, NULL);
	}
	atomic_store(&node->state, GLNN_READY);
    }
    return 0;
}


static void __destroy_node(struct gln_node *node) {
    arcp_store(&node->graph, NULL);
    arcp_store(&node->sockets, NULL);
    node->destroy(node);
}

int gln_node_init(struct gln_node *node, struct gln_graph *graph, gln_process_fp_t process, void (*destroy)(struct gln_node *)) {
    struct aary *empty_array = aary_new(0);
    if(empty_array == 0) {
	return -1;
    }
    arcp_init(&node->sockets, (struct arcp_region *) empty_array);
    node->destroy = destroy;
    node->process = process;
    atomic_init(&node->state, GLNN_READY);
    arcp_init(&node->graph, (struct arcp_region *) graph);
    arcp_region_init((struct arcp_region *) node, (void (*)(struct arcp_region *)) __destroy_node);
    struct aary *array;
    struct aary *new_array;
    do {
	array = (struct aary *) arcp_load(&graph->nodes);
	if(array == NULL) {
	    errno = EINVAL;
	    arcp_release((struct arcp_region *) node);
	    return -1;
	}
	new_array = aary_dup_set_add(array, (struct arcp_region *) node);
	if(new_array == NULL) {
	    arcp_release((struct arcp_region *) array);
	    arcp_release((struct arcp_region *) node);
	    return -1;
	}
    } while(!arcp_compare_store_release(&graph->nodes, (struct arcp_region *) array, (struct arcp_region *) new_array));
    return 0;
}

int gln_node_unlink(struct gln_node *node) {
    int r = 0;
    /* disconnect all sockets and clear the socket list */
    struct aary *socket_list = (struct aary *) arcp_exchange(&node->sockets, NULL);
    if(socket_list != NULL) {
	size_t i;
	for(i = 0; i < aary_length(socket_list); i++) {
	    r = gln_socket_disconnect((struct gln_socket *) aary_load_weak(socket_list, i));
	    if(r != 0) {
		arcp_store(&node->sockets, (struct arcp_region *) socket_list);
		return r;
	    }
	}
    }
    arcp_release((struct arcp_region *) socket_list);
    return 0;
}

static void __destroy_socket(struct gln_socket *socket) {
    arcp_store(&socket->node, NULL);
    arcp_store(&socket->graph, NULL);
    arcp_store(&socket->buffer, NULL);
    atxn_destroy(&socket->other);
    if(socket->destroy != NULL) {
	socket->destroy(socket);
    }
}

int gln_socket_init(struct gln_socket *socket, struct gln_node *node, 
		    enum gln_socket_direction direction, void (*destroy)(struct gln_socket *)) {
    int r;
    if(direction == GLNS_OUTPUT) {
	struct aary *empty_array = aary_new(0);
	if(empty_array == NULL) {
	    return -1;
	}
	r = atxn_init(&socket->other, (struct arcp_region *) empty_array);
	if(r != 0) {
	    arcp_release((struct arcp_region *) empty_array);
	    return -1;
	}
    } else {
	r = atxn_init(&socket->other, NULL);
	if(r != 0) {
	    return -1;
	}
    }
    socket->destroy = destroy;
    socket->direction = direction;
    arcp_init(&socket->buffer, NULL);
    arcp_init(&socket->node, (struct arcp_region *) node);
    struct gln_graph *graph = (struct gln_graph *) arcp_load(&node->graph);
    arcp_init(&socket->graph, (struct arcp_region *) graph);
    arcp_release((struct arcp_region *) graph);
    arcp_region_init((struct arcp_region *) socket, (void (*)(struct arcp_region *)) __destroy_socket);
    struct aary *array;
    struct aary *new_array;
    do {
	array = (struct aary *) arcp_load(&node->sockets);
	if(array == NULL) {
	    errno = EINVAL;
	    arcp_release((struct arcp_region *) socket);
	    return -1;
	}
	new_array = (struct aary *) aary_dup_set_add(array, (struct arcp_region *) socket);
	if(new_array == NULL) {
	    arcp_release((struct arcp_region *) array);
	    arcp_release((struct arcp_region *) socket);
	    return -1;
	}
    } while(!arcp_compare_store_release(&node->sockets, (struct arcp_region *) array, (struct arcp_region *) new_array));
    return 0;
}

#define CONTINUE_OR_ABORT_LOOP()		\
    atxn_abort(handle);				\
    if(r == ATXN_FAILURE) {			\
	continue;				\
    } else if(r == ATXN_ERROR) {		\
	return -1;				\
    } else {					\
	errno = EINVAL;				\
	return -1;				\
    }						\
    do { } while(0)


int gln_socket_connect(struct gln_socket *socket, struct gln_socket *other) {
    if(socket->direction == GLNS_INPUT) {
	return gln_socket_connect(other, socket);
    }
    if(other->direction != GLNS_INPUT) {
	errno = EINVAL;
	return -1;
    }
    for(;;) {
	atxn_handle_t *handle = atxn_start();
	if(handle == NULL) {
	    return -1;
	}
	enum atxn_status r;
	struct gln_socket *connected_socket;

	/* Load previously connected socket */
	r = atxn_load(handle, &other->other, (struct arcp_region **) &connected_socket);
	if(r != ATXN_SUCCESS) {
	    CONTINUE_OR_ABORT_LOOP();
	}

	/* See if we were connected */
	if(connected_socket == socket) {
	    atxn_abort(handle);
	    return 0;
	}

	/* Add other to list of connected sockets */
	struct aary *array;
	r = atxn_load(handle, &socket->other, (struct arcp_region **) &array);
	if(r != ATXN_SUCCESS) {
	    CONTINUE_OR_ABORT_LOOP();
	}
	if(array == NULL) {
	    atxn_abort(handle);
	    errno = EINVAL;
	    return -1;
	}
	array = aary_set_add(array, (struct arcp_region *) other);
	if(array == NULL) {
	    atxn_abort(handle);
	    return -1;
	}
	r = atxn_store(handle, &socket->other, (struct arcp_region *) array);
	if(r != ATXN_SUCCESS) {
	    arcp_release((struct arcp_region *) array);
	    CONTINUE_OR_ABORT_LOOP();
	}
	arcp_release((struct arcp_region *) array);

	/* remove other from list of connected sockets on the
	 * previously connected socket */
	if(connected_socket != NULL) {
	    r = atxn_load(handle, &connected_socket->other, (struct arcp_region **) &array);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }
	    array = aary_set_remove(array, (struct arcp_region *) other);
	    if(array == NULL) {
		atxn_abort(handle);
		return -1;
	    }
	    r = atxn_store(handle, &connected_socket->other, (struct arcp_region *) array);
	    if(r != ATXN_SUCCESS) {
		arcp_release((struct arcp_region *) array);
		CONTINUE_OR_ABORT_LOOP();
	    }
	    arcp_release((struct arcp_region *) array);
	}

	/* Set new connected socket */
	r = atxn_store(handle, &other->other, (struct arcp_region *) socket);
	if(r != ATXN_SUCCESS) {
	    CONTINUE_OR_ABORT_LOOP();
	}

	/* commit */
	r = atxn_commit(handle);
	if(r != ATXN_SUCCESS) {
	    CONTINUE_OR_ABORT_LOOP();
	}
	return 0;
    }
}

int gln_socket_disconnect(struct gln_socket *socket) {
    if(socket->direction == GLNS_INPUT) {
	for(;;) {
	    atxn_handle_t *handle = atxn_start();
	    if(handle == NULL) {
		return -1;
	    }
	    enum atxn_status r;
	    struct gln_socket *connected_socket;

	    /* Load connected socket */
	    r = atxn_load(handle, &socket->other, (struct arcp_region **) &connected_socket);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }

	    /* See if there was a connection */
	    if(connected_socket == NULL) {
		atxn_abort(handle);
		return 0;
	    }

	    /* remove self from list of connected sockets */
	    struct aary *array;
	    r = atxn_load(handle, &connected_socket->other, (struct arcp_region **) &array);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }
	    array = aary_set_remove(array, (struct arcp_region *) socket);
	    if(array == NULL) {
		atxn_abort(handle);
		return -1;
	    }
	    r = atxn_store(handle, &connected_socket->other, (struct arcp_region *) array);
	    if(r != ATXN_SUCCESS) {
		arcp_release((struct arcp_region *) array);
		CONTINUE_OR_ABORT_LOOP();
	    }
	    arcp_release((struct arcp_region *) array);

	    /* disconnect */
	    r = atxn_store(handle, &socket->other, NULL);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }

	    /* commit */
	    r = atxn_commit(handle);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }
	    return 0;
	}
    } else {
	for(;;) {
	    atxn_handle_t *handle = atxn_start();
	    if(handle == NULL) {
		return -1;
	    }

	    /* Load list of connected sockets */
	    struct aary *array;
	    enum atxn_status r = atxn_load(handle, &socket->other, (struct arcp_region **) &array);
	    if(r != ATXN_SUCCESS) {
		atxn_abort(handle);
		return 0;
	    }

	    if(array == NULL) {
		atxn_abort(handle);
		errno = EINVAL;
		return 0;
	    }

	    /* See if we're already disconnected */
	    if(aary_length(array) == 0) {
		atxn_abort(handle);
		return 0;
	    }

	    /* Disconnect each socket */
	    size_t i;
	    for(i = 0; i < aary_length(array); i++) {
		struct gln_socket *other = (struct gln_socket *) aary_load_weak(array, i);
		r = atxn_store(handle, &other->other, NULL);
		if(r != ATXN_SUCCESS) {
		    CONTINUE_OR_ABORT_LOOP();
		}
	    }

	    /* empty out connection list */
	    array = aary_new(0);
	    if(array == NULL) {
		atxn_abort(handle);
		return -1;
	    }
	    r = atxn_store(handle, &socket->other, (struct arcp_region *) array);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }

	    /* commit */
	    r = atxn_commit(handle);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }
	    return 0;
	}
    }
}

int gln_socket_unlink(struct gln_socket *socket) {
    if(socket->direction == GLNS_INPUT) {
	return gln_socket_disconnect(socket);
    } else {
	for(;;) {
	    atxn_handle_t *handle = atxn_start();
	    if(handle == NULL) {
		return -1;
	    }

	    /* Load list of connected sockets */
	    struct aary *array;
	    enum atxn_status r = atxn_load(handle, &socket->other, (struct arcp_region **) &array);

	    if(array == NULL) {
		atxn_abort(handle);
		return 0;
	    }

	    /* Disconnect each socket */
	    size_t i;
	    for(i = 0; i < aary_length(array); i++) {
		struct gln_socket *other = (struct gln_socket *) aary_load_weak(array, i);
		r = atxn_store(handle, &other->other, NULL);
		if(r != ATXN_SUCCESS) {
		    CONTINUE_OR_ABORT_LOOP();
		}
	    }

	    /* null out connection list */
	    r = atxn_store(handle, &socket->other, NULL);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }

	    /* commit */
	    r = atxn_commit(handle);
	    if(r != ATXN_SUCCESS) {
		CONTINUE_OR_ABORT_LOOP();
	    }
	    return 0;
	}
    }
}

#define GLN_BUFFER_OVERHEAD (offsetof(struct gln_buffer, data))

void __destroy_gln_buffer(struct gln_buffer *buffer) {
    afree(buffer, GLN_BUFFER_OVERHEAD + buffer->size);
}

void *gln_alloc_buffer(struct gln_socket *socket, size_t size) {
    struct gln_buffer *buffer = amalloc(GLN_BUFFER_OVERHEAD + size);
    if(buffer == NULL) {
	return NULL;
    }
    buffer->size = size;
    arcp_region_init((struct arcp_region *) buffer, (void (*)(struct arcp_region *)) __destroy_gln_buffer);
    arcp_store(&socket->buffer, (struct arcp_region *) buffer);
    arcp_release((struct arcp_region *) buffer);
    return &buffer->data;
}

int gln_get_buffers(struct gln_socket **sockets, void **buffers, int count) {
    /* Get a list of nodes from the sockets */
    struct gln_graph *graph = NULL;
    struct gln_node **nodes = alloca(sizeof(struct gln_node *) * count);
    struct gln_socket **connected_sockets = alloca(sizeof(struct gln_socket *) * count);
    int i;
    atxn_handle_t *handle;
retry_acquire_connections:
    handle = atxn_start();
    for(i = 0; i < count; i++) {
	enum atxn_status r = atxn_load(handle, &sockets[i]->other, (struct arcp_region **) &connected_sockets[i]);
	if(r != ATXN_SUCCESS) {
	    atxn_abort(handle);
	    if(r == ATXN_FAILURE) {
		goto retry_acquire_connections;
	    } else if(r == ATXN_ERROR) {
		return -1;
	    } else {
		errno = EINVAL;
		return -1;
	    }
	}
    }
    int j;
    int node_count = 0;
    for(i = 0; i < count; i++) {
	if(connected_sockets[i] == NULL) {
	    continue;
	}
	int lo = 0;
	int hi = node_count;
	struct gln_node *node = (struct gln_node *) arcp_load_weak(&connected_sockets[i]->node);
	j = 0;
	while(lo < hi) {
	    j = (lo + hi) / 2;
	    if(node < nodes[j]) {
		hi = j;
	    } else if(node > nodes[j]) {
		lo = ++i;
	    } else {
		goto next_socket;
	    }
	}
	if(graph == NULL) {
	    graph = (struct gln_graph *) arcp_load_weak(&node->graph);
	}
	memmove(&nodes[j + 1], &nodes[j], sizeof(struct gln_node *) * (node_count - j));
	nodes[j] = node;
	node_count++;
    next_socket:
	continue;
    }
    /* add all pending nodes to the queue */
    for(j = 0; j < node_count; j++) {
	struct gln_node *node = nodes[j];
	int state = GLNN_READY;
	/* Add it to the queue */
	if(atomic_compare_exchange_strong_explicit(&node->state, &state, GLNN_PENDING,
						   memory_order_acq_rel, memory_order_relaxed)) {
	    int r = aqueue_enq(&graph->proc_queue, (struct arcp_region *) node);
	    if(r != 0) {
		atomic_store_explicit(&node->state, GLNN_READY, memory_order_release);
		atxn_abort(handle);
		return r;
	    }
	}
    }
    for(j = 0; j < node_count; j++) {
	struct gln_node *node = nodes[j];
	for(;;) {
	    enum gln_node_state state = atomic_load_explicit(&node->state, memory_order_acquire);
	    if(state == GLNN_FINISHED) {
		break;
	    } else if(state == GLNN_ERROR) {
		atxn_abort(handle);
		return -1;
	    } else if(state == GLNN_READY) {
		if(atomic_compare_exchange_strong_explicit(&node->state, &state, GLNN_PENDING,
							   memory_order_acq_rel, memory_order_relaxed)) {
		    int r = aqueue_enq(&graph->proc_queue, (struct arcp_region *) node);
		    if(r != 0) {
			atomic_store_explicit(&node->state, GLNN_READY, memory_order_release);
			atxn_abort(handle);
			return r;
		    }
		}
		continue;
	    }
	    struct gln_node *next = (struct gln_node *) aqueue_deq(&graph->proc_queue);
	    if(next == NULL) {
		/* This becomes a spinlock waiting on other threads to finish processing... */
		cpu_relax();
		continue;
	    }
	    int r = next->process(next);
	    if(r != 0) {
		atomic_store_explicit(&next->state, GLNN_ERROR, memory_order_release);
	    } else {
		atomic_store_explicit(&next->state, GLNN_FINISHED, memory_order_release);
	    }
	    arcp_release((struct arcp_region *) next);
	}
    }
    /* Now we can load up our buffers */
    for(i = 0; i < count; i++) {
	if(connected_sockets[i] == NULL) {
	    buffers[i] = NULL;
	    arcp_store(&sockets[i]->buffer, buffers[i]);
	} else {
	    struct gln_buffer *buf = (struct gln_buffer *) arcp_load_weak(&connected_sockets[i]->buffer);
	    buffers[i] = buf->data;
	    arcp_store(&sockets[i]->buffer, (struct arcp_region *) buf);
	}
    }
    return 0;
}

void gln_process(struct gln_graph *graph) {
    struct gln_node *next = (struct gln_node *) aqueue_deq(&graph->proc_queue);
    if(next == NULL) {
	/* This becomes a spinlock waiting for something to be queued... */
	cpu_relax();
    }
    int r = next->process(next);
    if(r != 0) {
	atomic_store_explicit(&next->state, GLNN_ERROR, memory_order_release);
    } else {
	atomic_store_explicit(&next->state, GLNN_FINISHED, memory_order_release);
    }
    arcp_release((struct arcp_region *) next);
}
