/*
 * test.c
 * 
 * Copyright 2010 Evan Buswell
 * 
 * This file is part of Cshellsynth.
 * 
 * Cshellsynth is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Cshellsynth is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Cshellsynth.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <graphline.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <string.h>

#define CHECKING(function)			\
    printf("Checking " #function "...")		\

#define OK()					\
    printf("OK\n")

#define CHECK_NULL(obj)				\
    do {					\
	if(obj == NULL) {			\
	    perror("Error: ");			\
	    exit(1);				\
	}					\
    } while(0)

#define CHECK_R()				\
    do {					\
	if(r != 0) {				\
	    perror("Error: ");			\
	    exit(1);				\
	}					\
    } while(0)


struct alphabetgenerator {
    struct gln_node *node;
    struct gln_socket *out;
};

int alphabetgenerator_f(void *arg) {
    struct alphabetgenerator *self = (struct alphabetgenerator *) arg;

    char *out_buffer = (char *) gln_socket_get_buffer(self->out);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    for(i = 0; i < self->node->graph->buffer_size; i++) {
	out_buffer[i] = 'a' + (i % 26);
    }
    return 0;
}

struct uppercaser {
    struct gln_node *node;
    struct gln_socket *in;
    struct gln_socket *out;
};

int uppercaser_f(void *arg) {
    struct uppercaser *self = (struct uppercaser *) arg;

    char *in_buffer = (char *) gln_socket_get_buffer(self->in);
    if(in_buffer == NULL)
	return -1;
    char *out_buffer = (char *) gln_socket_get_buffer(self->out);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    for(i = 0; i < self->node->graph->buffer_size; i++) {
	out_buffer[i] = toupper(in_buffer[i]);
    }
    return 0;
}

struct interpolator {
    struct gln_node *node;
    struct gln_socket *in1;
    struct gln_socket *in2;
    struct gln_socket *out;
};

int interpolate_f(void *arg) {
    struct interpolator *self = (struct interpolator *) arg;

    char *in1_buffer = (char *) gln_socket_get_buffer(self->in1);
    if(in1_buffer == NULL)
	return -1;
    char *in2_buffer = (char *) gln_socket_get_buffer(self->in2);
    if(in2_buffer == NULL)
	return -1;
    char *out_buffer = (char *) gln_socket_get_buffer(self->out);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    size_t j = 0;
    for(i = 0; i < self->node->graph->buffer_size; i++) {
	if(i % 2) {
	    out_buffer[i] = in2_buffer[j++];
	} else {
	    out_buffer[i] = in1_buffer[j];
	}
    }

    return 0;
}

int main(int argc, char **argv) {
    struct gln_graph *graph;
    int r;
    char *result;

    CHECKING(gln_create_graph);
    graph = gln_create_graph(10);
    if(graph == NULL) {
	perror("Error: ");
	exit(1);
    }
    OK();

    CHECKING(gln_create_node);
    struct alphabetgenerator ag;
    ag.node = gln_create_node(graph, alphabetgenerator_f, &ag);
    CHECK_NULL(ag.node);
    struct uppercaser uc;
    uc.node = gln_create_node(graph, uppercaser_f, &uc);
    CHECK_NULL(uc.node);
    struct interpolator itp;
    itp.node = gln_create_node(graph, interpolate_f, &itp);
    CHECK_NULL(itp.node);
    struct gln_node *self = gln_create_node(graph, NULL, NULL);
    CHECK_NULL(self);
    OK();

    CHECKING(gln_create_socket);
    ag.out = gln_create_socket(ag.node, OUTPUT);
    CHECK_NULL(ag.out);
    uc.in = gln_create_socket(uc.node, INPUT);
    CHECK_NULL(uc.in);
    uc.out = gln_create_socket(uc.node, OUTPUT);
    CHECK_NULL(uc.out);
    itp.in1 = gln_create_socket(itp.node, INPUT);
    CHECK_NULL(itp.in1);
    itp.in2 = gln_create_socket(itp.node, INPUT);
    CHECK_NULL(itp.in2);
    itp.out = gln_create_socket(itp.node, OUTPUT);
    CHECK_NULL(itp.out);
    struct gln_socket *in = gln_create_socket(self, INPUT);
    CHECK_NULL(in);
    OK();

    CHECKING(gln_socket_connect);
    r = gln_socket_connect(ag.out, uc.in);
    CHECK_R();
    r = gln_socket_connect(ag.out, itp.in1);
    CHECK_R();
    r = gln_socket_connect(uc.out, itp.in2);
    CHECK_R();
    r = gln_socket_connect(itp.out, in);
    CHECK_R();
    OK();

    CHECKING(gln_start_processing);
    gln_start_processing(graph);
    OK();

    CHECKING(gln_socket_get_buffer);
    result = (char *) gln_socket_get_buffer(in);
    CHECK_NULL(result);
    if(memcmp(result, "aAbBcCdDeE", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    OK();

    CHECKING(gln_reset_graph);
    gln_reset_graph(graph);
    OK();

    CHECKING(gln_finish_processing);
    gln_finish_processing(graph);
    OK();


    CHECKING(gln_socket_disconnect);

    r = gln_socket_disconnect(itp.in1);
    CHECK_R();

    gln_start_processing(graph);
    result = (char *) gln_socket_get_buffer(in);
    CHECK_NULL(result);
    if(memcmp(result, "\0A\0B\0C\0D\0E", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    gln_reset_graph(graph);
    gln_finish_processing(graph);

    r = gln_socket_connect(ag.out, itp.in1);
    CHECK_R();

    OK();

    
    CHECKING(gln_destroy_socket);

    r = gln_destroy_socket(uc.out);
    CHECK_R();

    gln_start_processing(graph);
    result = (char *) gln_socket_get_buffer(in);
    CHECK_NULL(result);
    if(memcmp(result, "a\0b\0c\0d\0e\0", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    gln_reset_graph(graph);
    gln_finish_processing(graph);

    uc.out = gln_create_socket(uc.node, OUTPUT);
    CHECK_NULL(uc.out);
    r = gln_socket_connect(uc.out, itp.in2);
    CHECK_R();

    OK();


    CHECKING(gln_destroy_node);

    r = gln_destroy_node(uc.node);
    CHECK_R();

    gln_start_processing(graph);
    result = (char *) gln_socket_get_buffer(in);
    CHECK_NULL(result);
    if(memcmp(result, "a\0b\0c\0d\0e\0", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    gln_reset_graph(graph);
    gln_finish_processing(graph);

    OK();

    CHECKING(gln_destroy_graph);
    r = gln_destroy_graph(graph);
    CHECK_R();
    OK();

    exit(0);
}
