/*
 * Copyright John La Velle 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#include "client_ui.h"
#include "client.h"

/* connect to coordinator and connect to replicate 
 * run UI after connection is complete
 */
void connect_server(int port, char * addr) {
    char * update_write = "SENDREP", * tmp1, * tmp2;
	struct sockaddr_in local_serve, update_serve;
    loc_sock = socket(AF_INET, SOCK_STREAM, 0);
	int rdsz, i = 0;
	char * ipport;
    
	local_serve.sin_family = AF_INET;
	local_serve.sin_port = htons(port);
	local_serve.sin_addr.s_addr = inet_addr(addr);
    
	if(connect(loc_sock, (struct sockaddr *)&local_serve, sizeof(local_serve)) < 0 ) {
		fprintf(stderr, "Cannot connect to server: connect_serve %s\n", addr);
		return;
	}
    
	if(write(loc_sock, update_write, strlen(update_write)) < 0) {
		fprintf(stdout, "Write failed: connect_server\n");
		return;
	}
    ipport = malloc(128);
    update_serve.sin_family = AF_INET;
    if ((rdsz = read(loc_sock, ipport, 128)) <= 0){
        fprintf(stdout, "Read failed: connect_server [2]\n");
        return;
    }
    
    while(ipport[i] != ':')
        i++;
    tmp2 = malloc(i + 1);
    memcpy(tmp2, ipport, i);
    tmp2[i] = '\0';
    tmp1 = ipport + i + 1;
    
    update_serve.sin_addr.s_addr = inet_addr(tmp1);
    update_serve.sin_port = htons(atoi(tmp2));
	memset(ipport, 0, rdsz);
	free(ipport);
	close(loc_sock);
    memset(&loc_sock, 0, sizeof(int));
	loc_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(connect(loc_sock, (struct sockaddr *)&update_serve, sizeof(update_serve)) < 0) {
		fprintf(stderr, "Cannot connect to replicate: connect_serve\n");
	}
	run_ui(loc_sock);
}

int main (int argc, char ** argv) {
	int i, port = 0;
	char * addr = NULL;

	if (argc < 5) {
		fprintf(stdout, "%s -a [address] -p [port]\n", argv[0]);
        fprintf(stdout, "   a   =>   address to coordinator\n");
        fprintf(stdout, "   p   =>   port for coordinator\n");
		exit(0);
	}
	
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0) {
			port = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-a") == 0) {
			addr = strdup(argv[++i]);
		}
	}	

	if (port > 0 && addr != NULL) {
		connect_server(port, addr);
    } else {
        fprintf(stdout, "%s -a [address] -p [port]\n", argv[0]);
        fprintf(stdout, "   a   =>   address to coordinator\n");
        fprintf(stdout, "   p   =>   port for coordinator\n");
        exit(0);
    }
    free(addr);
	return 0;
}
