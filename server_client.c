/*
 * Copyright John La Velle 2020
 */

#include "common.h"
#include "replicate.h"

/*
 * This is for all functions relating to the communication between
 * a replica and a client. Also does writing to coordinator, but
 * separate thread manages responses from coordinator
 */

char * upuid = "SENDUID";
char * repuid = "SENDREPID";
char * pgup = "SENDPAGE";
char * received = "RECEIVED";

void realloc_clients() {
    void * k = realloc(clients->connections, sizeof(int) * (clients->alloc_len + INITIAL_CLIENTS));
    if (k == NULL)
        fprintf(stdout, "Client realloc, failed\n");
    k = realloc(clients->connect_attrs, sizeof(conn_t) * (clients->alloc_len + INITIAL_CLIENTS));
    if (k == NULL)
        fprintf(stdout, "Client realloc, failed\n");
    clients->alloc_len += INITIAL_CLIENTS;
} /* realloc_clients */

/*
 * Replica interleaves clients
 * Does special handling for messages
 */
void* cli_select(void * cli) {
    int i, j, rdsz;
    fd_set readset;
    int sock = *(int *)cli;
    char * buf;
    fprintf(stdout, "Reading: %d\n", sock);
    pthread_mutex_lock(&cli_lock);
    int kmax = clients->max_fd;
    int clen = clients->log_len;
    pthread_mutex_unlock(&cli_lock);
    
    FD_SET(sock, &readset);
    
    while (1) {
        fprintf(stdout, "Selecting clients\n");
    	select(sock + 1, &readset, NULL, NULL, NULL);

        if(FD_ISSET(sock, &readset)) {
            buf = malloc(MAX_MSG);
            rdsz = read(sock, buf, MAX_MSG);
            buf[rdsz] = '\0';
            fprintf(stdout, "Read: %s\n", buf);
            if (strncmp(buf, "REPLY:", 5) == 0) {
                buf = buf + 6;
                char * buf2 = malloc(strlen(buf) + strlen(repuid) + 4);
                sprintf(buf2, "%d:%s:%s",sock,repuid, buf);
                
                pthread_mutex_lock(&write_lock);
                write(loc_sock, buf2, strlen(buf2));
                pthread_mutex_unlock(&write_lock);
                
                free(buf2);
                buf = buf - 6;
            } else if (strncmp(buf, "POST", 4) == 0) {
                memset(buf, 0, rdsz);
                rdsz = sprintf(buf, "%d:%s", sock, upuid);
                
                pthread_mutex_lock(&write_lock);
                write(loc_sock, buf, strlen(buf));
                pthread_mutex_unlock(&write_lock);
            } else if (strncmp(buf, pgup, 8) == 0) {
                buf = buf + 9;
                fprintf(stdout, "Getting page: %s\n", buf);
                int p = atoi(buf);
                if (p > pages->log_len || pages->pages[0]->log_len == 0) {
                    write(sock, "PAGE UNAVAILABLE", 16);
                } else {
                    char * pgresp = assemble_page(p);
                    write(sock, pgresp, strlen(pgresp));
                    fprintf(stdout, "page %d: %s\n", p, pgresp);
                    free(pgresp);
                }
                buf = buf - 9;
            } else if (strncmp(buf, "READ", 4) == 0){
                int i = 0, anum;
                char * resp;
                buf = buf + 5;
                while(buf[i] != '.')
                    i++;
                buf[i] = '\0';
                anum = atoi(buf);
                fprintf(stdout, "Getting article: %d\n", anum);
                resp = assemble_replies(anum);
                write(sock, resp, strlen(resp));
                buf = buf - 5;
            } else if (strncmp(buf, "CLOSE", 5) == 0) {
                close(sock);
                pthread_exit(NULL);
            } else {
                if (buf[strlen(buf) - 1] == '\n') {
                    buf[strlen(buf) - 1] = '\0';
                }
                char * temp = malloc(snprintf(NULL, 0, "%d:%s", sock, buf));
                sprintf(temp, "%d:%s", sock, buf);
                pthread_mutex_lock(&write_lock);
                write(loc_sock, temp, strlen(temp));
                pthread_mutex_unlock(&write_lock);
                free(temp);
            }
            
            free(buf);
        }
    	
    	pthread_mutex_lock(&cli_lock);
    	kmax = clients->max_fd;
    	clen = clients->log_len;
    	pthread_mutex_unlock(&cli_lock);
    }
} /* cli_select */

/*
 * Replica listens for clients
 * Creates thread for each one
 */
void* gather_cli(void * sock) {
    int sel = 0;
    cli_sock = *(int *)sock;
    fd_set readset;
    
    clients = malloc(sizeof(serve_connections_t));
    clients->connections = malloc(sizeof(int) * INITIAL_CLIENTS);
    clients->connect_attrs = malloc(sizeof(conn_t) * INITIAL_CLIENTS);
    clients->log_len = 0;
    clients->max_fd = -1;
    clients->alloc_len = INITAL_CLIENTS;
    fprintf(stdout, "Setup clients\n");
    while (1) {
    	int c_sock, i = 0, rdsz;
    	struct sockaddr_in c_serve;
    	article_entry_t * art;
    	socklen_t slen;
    	pthread_t selecter;
    	
    	c_sock = accept(cli_sock, (struct sockaddr *)&c_serve, &slen);

    	pthread_mutex_lock(&cli_lock);
        if (clients->log_len == clients->alloc_len) {
            realloc_clients();
        }
    	i = clients->log_len;
    	clients->connections[clients->log_len] = c_sock;
    	clients->connect_attrs[clients->log_len].port = 0;
        clients->connect_attrs[clients->log_len].addr = NULL;
    	clients->log_len++;
    	if (c_sock > clients->max_fd)
    		clients->max_fd = rem_sock;
    	pthread_mutex_unlock(&cli_lock);
        fprintf(stdout, "Added client: %d %d\n", i, c_sock);

        pthread_create(&selecter, NULL, cli_select, &c_sock);
        pthread_detach(selecter);
        i = 0;
    }
} /* gather_cli */
