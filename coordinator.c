/*
 * Copyright John La Velle 2020
 */

#include "common.h"
#include "sequential_consistency.h"

int art_num = 1;
int loc_sock = 0;
int start_port = 7773;
int server_choose = 0;
int cli_sock;

int num_serve;
struct sockaddr_in loc_serve, cli_serve;
char * try_err = "RETRY";

serve_connections_t * connections = NULL;
entry_pages_t * pages = NULL;
serve_connections_t * clients = NULL;

pthread_t cli_thread, join_thread;
pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cli_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t page_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Reallocate for number of replicas
 */
void realloc_connections() {
    void * k = realloc(connections->connections, sizeof(int) * (connections->log_len + PAGE_ENTRIES));
    if (k == NULL) {
	fprintf(stdout, "Null allocation for connection realloc\n");
	return;
    }
    k = realloc(connections->connect_attrs, sizeof(conn_t) * (connections->log_len + PAGE_ENTRIES));
    if (k == NULL) {
        fprintf(stdout, "Null allocation for connection realloc\n");
        return;
    }
    connections->alloc_len += PAGE_ENTRIES;
}

/*
 * Listen for clients connecting
 * Sends replica connection info
 */
void gather_cli() {
    int c_sock, rdsz;
    struct sockaddr_in c_serve;
    conn_t serve;
    socklen_t slen;
    char resp[15];
    fprintf(stdout, "In gather cli\n");
    
     while (1) {
        char * msg;
        c_sock = accept(cli_sock, (struct sockaddr *)&c_serve, &slen);
        if ((rdsz = read(c_sock, resp, 15)) <= 0) {
            fprintf(stdout, "Failed read: client reading \n");
            return;
        }
        resp[rdsz] = '\0';
        if (strncmp(resp, "SENDREP", 7) != 0 ) {
            fprintf(stdout, "Invalid client response: %s\n", resp);
            return;
        }

        if (connections->log_len >= 1) {
            serve =
                connections->connect_attrs[server_choose % connections->log_len];
            server_choose++;
        } else {
            fprintf(stdout, "Unable to connect client: No replicates");
            write(c_sock, try_err, strlen(try_err));
        }
        fprintf(stdout, "%d:%s", serve.port, serve.addr);
        msg = malloc(snprintf(NULL, 0, "%d:%s", serve.port, serve.addr));
        sprintf(msg, "%d:%s", serve.port, serve.addr);
        if (write(c_sock, msg, strlen(msg)) <= 0) {
            fprintf(stdout, "Failed client replicate response -- [1]\n");
            return;
        }

        close(c_sock);
        memset(resp, 0, 15);
    }
}

/* Multiple threads with each their own interleaving
 *  select function send either unique id or handles
 *  reads and writes to server
 */
void* select_serve(void * rs) {
    int j, rdsz;
    int index = *(int *)rs;
    int rem_sock = connections->connections[index];
    fd_set readset;
    struct timeval tm;
    char * buf;
    fprintf(stdout, "in replicate thread\n");
    
    while (1) {
        j = 0;
        buf = malloc(MAX_MSG);
        FD_SET(rem_sock, &readset);
        tm.tv_usec = 100;
        select(rem_sock + 1, &readset, NULL, NULL, &tm);
        
        if(FD_ISSET(rem_sock, &readset)) {
            if((rdsz = read(rem_sock, buf, MAX_MSG)) <= 0) {
                fprintf(stdout, "Read error: select_serve\n");
                sleep(1);
                continue;
            }
            buf[rdsz] = '\0';
            fprintf(stdout, "Rep: %s\n", buf);
            if (strncmp(buf, "RECEIVED", 8) == 0)
                continue;
            while(buf[j] != ':')
                j++;
            j++;
            if (strncmp(buf + j, "SENDUID", 7) == 0) {
                char * retsock = malloc(j - 1);
                char * retmsg = malloc(j + 6);
                memcpy(retsock, buf, j - 1);
                retsock[j - 1] = '\0';
                free(buf);
                pthread_mutex_lock(&write_lock);
                buf = gather_uid(0, 0);
                rdsz = sprintf(retmsg, "%s;%s", retsock, buf);
                retmsg[rdsz] = '\0';
                write(rem_sock, retmsg, strlen(retmsg));
                pthread_mutex_unlock(&write_lock);
                fprintf(stdout, "New entry: %s %s\n", buf, retmsg);
                free(buf);
                free(retsock);
                free(retmsg);
            } else if (strncmp(buf + j, "SENDREPID:", 9) == 0) {
                char * retmsg2 = malloc(j + 6);
                char * retsock2 = malloc(j);
                memcpy(retsock2, buf, j - 1);
                retsock2[j - 1] = '\0';
                j++;
                while (buf[j] != ':')
                    j++;
                j++;
                buf = buf + j;
                j = atoi(buf);
                memset(buf, 0, rdsz);
                fprintf(stdout, "New reply: %d\n", j);
                pthread_mutex_lock(&write_lock);
                buf = gather_uid(1,j);
                rdsz = sprintf(retmsg2, "%s;%s", retsock2, buf);
                retmsg2[rdsz] = '\0';
                write(rem_sock, retmsg2, strlen(retmsg2));
                pthread_mutex_unlock(&write_lock);
                free(buf);
                free(retmsg2);
                free(retsock2);
            } else {
                fprintf(stdout, "handling reception: %s\n", buf);
                handle_reception(buf, rem_sock);  // In sequential_consistency.c
            }
        }
    }
}

/*
 * Listens for connecting replicas
 * Add replicas to client structure
 */
void* gather_nodes() {
    int i, j, k;
    char * msg;
    fprintf(stdout, "in replicate\n");
    if (connections == NULL) {
        connections = malloc(sizeof(serve_connections_t));
        connections->connections = malloc(sizeof(int) * PAGE_ENTRIES);
        connections->connect_attrs = malloc(sizeof(conn_t) * PAGE_ENTRIES);
        connections->log_len = 0;
        connections->max_fd = -1;
        connections->alloc_len = PAGE_ENTRIES;
        if (connections == NULL) {
            fprintf(stdout, "Malloc failure: server connections\n");
            exit(1);
        }
    }
    fprintf(stdout, "Listening...\n");
    while (1) {
        int rem_sock, local, rdsz;
        struct sockaddr_in rem_serve;
        socklen_t slen;
        pthread_t selector;
        char * tmp;
        char * buf = malloc(32);
        
        rem_sock = accept(loc_sock, (struct sockaddr *)&rem_serve, &slen);
        fprintf(stdout, "Adding replicate\n");
        
        /* update replicate with all current updates
         * no need to lock since all pages/entries
         * are persistent
         */
        rdsz = read(rem_sock, buf, 32);
        buf[rdsz] = '\0';
        if (strncmp(buf, "SENDPGS",7) == 0) {
            fprintf(stdout, "Sending pages\n");
        } else {
            fprintf(stdout, "Failed to read: %s\n", buf);
            pthread_exit(NULL);
        }
        
        buf = buf + 8;
        i = 0;
        while(buf[i] != ':')
            i++;
        tmp = malloc(i);
        memcpy(tmp, buf, i);
        tmp[i] = '\0';
        i++;
        
        buf = buf + i;
        
        pthread_mutex_lock(&conn_lock);
        if (connections->log_len + 1 == connections->alloc_len) {
            realloc_connections();
        }
        connections->connections[connections->log_len] = rem_sock;
        connections->connect_attrs[connections->log_len].port = atoi(tmp);
        connections->connect_attrs[connections->log_len].addr = malloc(strlen(buf));
        strcpy(connections->connect_attrs[connections->log_len].addr, buf);
        local = connections->log_len;
        connections->log_len++;
        if (rem_sock > connections->max_fd)
            connections->max_fd = rem_sock;
        pthread_mutex_unlock(&conn_lock);
        
        fprintf(stdout, "Conn: %s %d\n", connections->connect_attrs[0].addr,
            connections->connect_attrs[0].port);
        
        for( i = 0; i <= pages->log_len; i++) {
            for (j = 0; j < pages->pages[i]->log_len; j++) {
                msg = malloc(snprintf(NULL, 0, "%s:%s",
                                      pages->pages[i]->entries[j]->unique_id,
                                      pages->pages[i]->entries[j]->article));
                sprintf(msg, "%s:%s",
                        pages->pages[i]->entries[j]->unique_id,
                        pages->pages[i]->entries[j]->article);
                fprintf(stdout, "s->r: %s\n", msg);
                write(rem_sock, msg, strlen(msg));
                read(rem_sock, msg, MAX_MSG);
                free(msg);
                for (k = 0; k < pages->pages[i]->entries[j]->replies->log_len; k++) {
                    msg = malloc(snprintf(NULL, 0, "%s:%s",
                                          pages->pages[i]->entries[j]->replies->entries[k]->unique_id,
                                          pages->pages[i]->entries[j]->replies->entries[k]->article));
                    sprintf(msg, "%s:%s",
                            pages->pages[i]->entries[j]->replies->entries[k]->unique_id,
                            pages->pages[i]->entries[j]->replies->entries[k]->article);
                    fprintf(stdout, "s->r: %s\n", msg);
                    write(rem_sock, msg, strlen(msg));
                    read(rem_sock, msg, MAX_MSG);
                    free(msg);
                }
            }
        }
        fprintf(stdout, "pages complete\n");
        pthread_create(&selector, NULL, select_serve, &local);
    }
}

/*
 * Setup sockets for replicas and clients
 * Always port 7773 for replicas
 * 7776 for clients
 */
void setup_socket(char * ipadr) {
    int j;
    loc_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(loc_sock < 0) {
        fprintf(stdout, "unavel to sock\n");
    }
    memset(&loc_serve, 0, sizeof(loc_serve));
    loc_serve.sin_family = AF_INET;
    loc_serve.sin_port = htons(start_port);
    loc_serve.sin_addr.s_addr = inet_addr(ipadr);
    
    if (setsockopt(loc_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int)) == -1) {
        fprintf(stdout, "setsockopt fail\n");
        return;
    }
    if (bind(loc_sock, (struct sockaddr *)&loc_serve, sizeof(loc_serve)) == -1) {
        fprintf(stdout, "bind fail\n");
        return;
    }
    if (listen(loc_sock, LISTEN_BACK) == -1) {
        fprintf(stdout, "listen fail\n");
        return;
    }

    start_port += 3;
    pthread_create(&join_thread, NULL, gather_nodes, NULL);
    
    fprintf(stdout, "Join at %d %s", start_port, ipadr);
    cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    cli_serve.sin_family = AF_INET;
    cli_serve.sin_port = htons(start_port);
    cli_serve.sin_addr.s_addr = inet_addr(ipadr);
    
    if (setsockopt(cli_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int)) == -1) {
        fprintf(stdout, "set fail\n");
    }
    if (bind(cli_sock, (struct sockaddr *)&cli_serve, sizeof(cli_serve)) == -1) {
        fprintf(stdout, "bind failll\n");
    }
    if (listen(cli_sock, LISTEN_BACK) == -1) {
        fprintf(stdout, "listn fail\n");
    }
    gather_cli();
 
}


int main (int argc, char ** argv) {
    int i;
    char * addr = NULL;
    for (i = 1; i < argc; i++) {
        if (strlen(argv[i]) == 2) {
            switch (argv[i][1]) {
                case 'a':
                    addr = argv[++i];
                    break;
                default:
                    fprintf(stdout, "Unknown argument: %s\n", argv[i]);
                    exit(1);
                    break;
            }
        }
    }
    
    if (addr == NULL) {
        fprintf(stdout, "Invalid Usage: %s -a [ipaddr]\n", argv[0]);
        exit(1);
    }

    fprintf(stdout, "Setting up bulletin board\n");
    pages = init_pages();
    fprintf(stdout, "Setting up sockets\n");
    setup_socket(addr);
    free_everything(pages);
}


