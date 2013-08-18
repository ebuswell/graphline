/*
 * test.c
 * 
 * Copyright 2013 Evan Buswell
 * 
 * This file is part of Cshellsynth.
 * 
 * Cshellsynth is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
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

#define MYBUFSIZ 80

#define CHECKING(function)			\
    printf("Checking " #function "...")		\

#define OK()							\
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
    struct gln_node node;
    struct gln_socket out;
};

int alphabetgenerator_f(struct alphabetgenerator *self) {
    char *out_buffer = (char *) gln_alloc_buffer(&self->out, MYBUFSIZ + 1);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    for(i = 0; i < MYBUFSIZ; i++) {
	out_buffer[i] = 'a' + (i % 26);
    }
    out_buffer[i] = '\0';
    return 0;
}

struct uppercaser {
    struct gln_node node;
    struct gln_socket in;
    struct gln_socket out;
};

int uppercaser_f(struct uppercaser *self) {
    char *buffers[1];
    struct gln_socket *sockets[1];
    sockets[0] = &self->in;
    int r = gln_get_buffers(sockets, (void **) buffers, 1);
    if(r != 0) {
	return r;
    }
    char *in_buffer = buffers[0];

    char *out_buffer = (char *) gln_alloc_buffer(&self->out, MYBUFSIZ + 1);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    for(i = 0; i < MYBUFSIZ; i++) {
	if(in_buffer != NULL) {
	    out_buffer[i] = toupper(in_buffer[i]);
	} else {
	    out_buffer[i] = '\0';
	}
    }
    out_buffer[i] = '\0';
    return 0;
}

struct interpolator {
    struct gln_node node;
    struct gln_socket in1;
    struct gln_socket in2;
    struct gln_socket out;
};

int interpolate_f(struct interpolator *self) {
    char *buffers[2];
    struct gln_socket *sockets[2];
    sockets[0] = &self->in1;
    sockets[1] = &self->in2;
    int r = gln_get_buffers(sockets, (void **) buffers, 2);
    if(r != 0) {
	return r;
    }
    char *in1_buffer = buffers[0];
    char *in2_buffer = buffers[1];

    char *out_buffer = (char *) gln_alloc_buffer(&self->out, MYBUFSIZ + 1);
    if(out_buffer == NULL)
	return -1;

    size_t i;
    size_t j = 0;
    for(i = 0; i < MYBUFSIZ; i++) {
	if(i % 2) {
	    if(in2_buffer == NULL) {
		out_buffer[i] = '\0';
	    } else {
		out_buffer[i] = in2_buffer[j++];
	    }
	} else {
	    if(in1_buffer == NULL) {
		out_buffer[i] = '\0';
	    } else {
		out_buffer[i] = in1_buffer[j];
	    }
	}
    }
    out_buffer[i] = '\0';

    return 0;
}

int main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
    struct gln_graph graph;
    int r;
    char *result;

    setbuf(stdout, NULL);
    CHECKING(gln_graph_init);
    r = gln_graph_init(&graph, gln_graph_destroy);
    CHECK_R();
    OK();

    CHECKING(gln_node_init);
    struct alphabetgenerator ag;
    r = gln_node_init(&ag.node, &graph, (gln_process_fp_t) alphabetgenerator_f, gln_node_destroy);
    CHECK_R();
    struct uppercaser uc;
    r = gln_node_init(&uc.node, &graph, (gln_process_fp_t) uppercaser_f, gln_node_destroy);
    CHECK_R();
    struct interpolator itp;
    r = gln_node_init(&itp.node, &graph, (gln_process_fp_t) interpolate_f, gln_node_destroy);
    CHECK_R();
    struct gln_node self;
    r = gln_node_init(&self, &graph, NULL, gln_node_destroy);
    CHECK_R();
    OK();

    CHECKING(gln_socket_init);
    r = gln_socket_init(&ag.out, &ag.node, GLNS_OUTPUT, gln_socket_destroy);
    CHECK_R();
    r = gln_socket_init(&uc.in, &uc.node, GLNS_INPUT, gln_socket_destroy);
    CHECK_R();
    r = gln_socket_init(&uc.out, &uc.node, GLNS_OUTPUT, gln_socket_destroy);
    CHECK_R();
    r = gln_socket_init(&itp.in1, &itp.node, GLNS_INPUT, gln_socket_destroy);
    CHECK_R();
    r = gln_socket_init(&itp.in2, &itp.node, GLNS_INPUT, gln_socket_destroy);
    CHECK_R();
    r = gln_socket_init(&itp.out, &itp.node, GLNS_OUTPUT, gln_socket_destroy);
    CHECK_R();
    struct gln_socket in;
    r = gln_socket_init(&in, &self, GLNS_INPUT, gln_socket_destroy);
    CHECK_R();
    OK();

    CHECKING(gln_socket_connect);
    r = gln_socket_connect(&ag.out, &uc.in);
    CHECK_R();
    r = gln_socket_connect(&ag.out, &itp.in1);
    CHECK_R();
    r = gln_socket_connect(&uc.out, &itp.in2);
    CHECK_R();
    r = gln_socket_connect(&itp.out, &in);
    CHECK_R();
    OK();

    CHECKING(gln_get_buffers);
    char *buffers[1];
    struct gln_socket *sockets[1];
    sockets[0] = &in;
    r = gln_get_buffers(sockets, (void **) buffers, 1);
    CHECK_R();
    result = buffers[0];
    CHECK_NULL(result);
    if(memcmp(result, "aAbBcCdDeE", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    OK();

    CHECKING(gln_graph_reset);
    gln_graph_reset(&graph);
    OK();

    CHECKING(gln_socket_disconnect);

    gln_socket_disconnect(&itp.in1);

    r = gln_get_buffers(sockets, (void **) buffers, 1);
    CHECK_R();
    result = buffers[0];
    CHECK_NULL(result);
    if(memcmp(result, "\0A\0B\0C\0D\0E", 10) != 0) {
	printf("Error: unexpected result: %.10s\n", result);
	exit(1);
    }
    gln_graph_reset(&graph);

    r = gln_socket_connect(&ag.out, &itp.in1);
    CHECK_R();

    OK();


    /* CHECKING(gln_alloc_buffer); */

    /* char *buffer = gln_alloc_buffer(&itp.in1, MYBUFSIZ); */
    /* CHECK_NULL(buffer); */
    /* memset(buffer, 'q', 10); */

    /* result = (char *) gln_socket_get_buffer(&in); */
    /* CHECK_NULL(result); */
    /* if(memcmp(result, "qAqBqCqDqE", 10) != 0) { */
    /* 	printf("Error: unexpected result: %.10s\n", result); */
    /* 	exit(1); */
    /* } */
    /* r = gln_graph_reset(&graph); */
    /* CHECK_R(); */

    /* r = gln_socket_connect(&ag.out, &itp.in1); */
    /* CHECK_R(); */

    /* OK(); */

    r = gln_socket_connect(&ag.out, &itp.in1);
    CHECK_R();

    /* CHECKING(gln_socket_reset); */

    /* result = (char *) gln_socket_get_buffer(&in); */
    /* CHECK_NULL(result); */
    /* if(memcmp(result, "aAbBcCdDeE", 10) != 0) { */
    /* 	printf("Error: unexpected result: %.10s\n", result); */
    /* 	exit(1); */
    /* } */

    /* buffer = gln_socket_alloc_buffer(&itp.in1); */
    /* CHECK_NULL(buffer); */
    /* memset(buffer, 'q', 10); */
    /* gln_socket_reset(&itp.out); */
    /* gln_socket_reset(&in); */

    /* result = (char *) gln_socket_get_buffer(&in); */
    /* CHECK_NULL(result); */
    /* if(memcmp(result, "qAqBqCqDqE", 10) != 0) { */
    /* 	printf("Error: unexpected result: %.10s\n", result); */
    /* 	exit(1); */
    /* } */

    /* r = gln_graph_reset(&graph); */
    /* CHECK_R(); */

    /* OK(); */

    /* CHECKING(gln_socket_destroy); */

    /* gln_socket_destroy(&uc.out); */

    /* result = (char *) gln_socket_get_buffer(&in); */
    /* CHECK_NULL(result); */
    /* if(memcmp(result, "a\0b\0c\0d\0e\0", 10) != 0) { */
    /* 	printf("Error: unexpected result: %.10s\n", result); */
    /* 	exit(1); */
    /* } */
    /* r = gln_graph_reset(&graph); */
    /* CHECK_R(); */

    /* r = gln_socket_init(&uc.out, &uc.node, OUTPUT); */
    /* CHECK_R(); */
    /* r = gln_socket_connect(&uc.out, &itp.in2); */
    /* CHECK_R(); */

    /* OK(); */

    /* CHECKING(gln_graph_to_string); */
    /* char *str = gln_graph_to_string(&graph); */
    /* CHECK_NULL(str); */
    /* printf("%s\n", str); */
    /* free(str); */
    /* OK(); */

    /* CHECKING(gln_node_destroy); */
    /* gln_socket_destroy(&in); */
    /* gln_node_destroy(&self); */
    /* OK(); */

    /* CHECKING(gln_graph_destroy); */
    /* gln_socket_destroy(&ag.out); */
    /* gln_socket_destroy(&uc.in); */
    /* gln_socket_destroy(&uc.out); */
    /* gln_socket_destroy(&itp.in1); */
    /* gln_socket_destroy(&itp.in2); */
    /* gln_socket_destroy(&itp.out); */
    /* gln_node_destroy(&ag.node); */
    /* gln_node_destroy(&uc.node); */
    /* gln_node_destroy(&itp.node); */
    /* gln_graph_destroy(&graph); */
    /* OK(); */

    exit(0);
}
