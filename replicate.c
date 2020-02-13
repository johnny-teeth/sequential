/*
 * Copyright John La Velle 2020
 */

#include <stdio.h>
#include "common.h"
#include "replicate.h"

#define LISTEN_BACK 10
#define MSGS 512

char * receipt = "RECEIVED";
int serve_port = 7773;
int cli_sock, loc_sock;
struct sockaddr_in cli_serve, co_serve, udp_serve;

entry_pages_t * pages;
serve_connections_t * clients;
serve_connections_t * connections;

pthread_t join_thread, cli_thread;
pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cli_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t page_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Listens for messages from coordinator
 */
void start_up() {
    int rdsz, i = 0;
    char * tmp;
    fd_set readset;
    fprintf(stdout, "Connected to server\n");
    
    while (1) {
        tmp = malloc(MSGS);
        FD_SET(loc_sock, &readset);
        
        select(loc_sock + 1, &readset, NULL, NULL, NULL);

        if (FD_ISSET(loc_sock, &readset)) {
            fprintf(stdout, "Reading from server...\n");
            if ((rdsz = read(loc_sock, tmp, MSGS)) < 0) {
                fprintf(stdout, "Read failed: server start-up\n");
                return;
            } else if (rdsz > 0) {
                tmp[rdsz] = '\0';
                fprintf(stdout, "From server: %s\n",tmp);

                while (tmp[i] != '\0') {
                    if (tmp[i] == ':') {
                        decode_msg(tmp);
                        break;
                    }
                    i++;
                }
                if (tmp[i] == '\0') {
                    i = 0;
                    while (tmp[i] != 0) {
                        if (tmp[i] == ';') {
                            char * sockstr = malloc(i);
                            memcpy(sockstr, tmp, i);
                            tmp[i] = '\0';
                            int respsock = atoi(sockstr);
                            tmp = tmp + (i + 1);
                            fprintf(stdout, "Sending: %d %s\n", respsock, tmp);
                            write(respsock, tmp, strlen(tmp));
                            tmp = tmp - (i + 1);
                            break;
                        }
                        i++;
                    }
                    if (i == rdsz) {
                        i = atoi(tmp);
                        write(i, "1", 1);
                    }
                }
                
                if (write(loc_sock, receipt, strlen(receipt)) < 0) {
                    fprintf(stdout, "Receipt error: %s\n", tmp);
                }
            }
        }
        free(tmp);
        tmp = NULL;
        FD_ZERO(&readset);
        i = 0;
    }
}

void create_connection (char * addr, int port, char * saddr) {
    int j;
    char * asks;
    asks = malloc(snprintf(NULL, 0, "SENDPGS:%d:%s",port, addr));
    sprintf(asks, "SENDPGS:%d:%s", port, addr);

    cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    cli_serve.sin_family = AF_INET;
    cli_serve.sin_port = htons(port);
    cli_serve.sin_addr.s_addr = inet_addr(addr);
    
    setsockopt(cli_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int));
    bind(cli_sock, (struct sockaddr *)&cli_serve, sizeof(cli_serve));
    listen(cli_sock, LISTEN_BACK);
    fprintf(stdout, "Threading for client\n");
    pthread_create(&cli_thread, NULL, gather_cli, &cli_sock);

    serve_port = 7773;
    loc_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (loc_sock < 0){
        fprintf(stdout, "unable to socket\n");
    }
    
    memset(&co_serve, 0, sizeof(co_serve));
    co_serve.sin_family = AF_INET;
    co_serve.sin_port = htons(serve_port);
    co_serve.sin_addr.s_addr = inet_addr(saddr);
    if (connect(loc_sock, (struct sockaddr *)&co_serve, sizeof(co_serve)) < 0) {
        fprintf(stdout, "Unable to connect to coordinator\n");
        return;
    }
    
    if (write(loc_sock, asks, strlen(asks)) < 0){
        fprintf(stdout, "failed write\n");
    }
    
    free(asks);
    start_up();
}

int main(int argc, char ** argv) {
    pages = NULL;
    clients = NULL;
    connections = NULL;
    int i, port = -1;
    char * addr = NULL, * saddr = NULL;
    if (argc < 5) {
        fprintf(stdout, "Usage: %s -p [port] -a [address]\n", argv[0]);
        return 1;
    }
    
    for (i = 1; i < argc; i++) {
        if (strlen(argv[i]) == 2) {
            switch(argv[i][1]) {
                case 'p':
                    port = atoi(argv[++i]);
                    break;
                case 'a':
                    addr = strdup(argv[++i]);
                    break;
                case 's':
                    saddr = strdup(argv[++i]);
                    break;
                default:
                    fprintf(stdout, "Unknown argument: %s\n", argv[i]);
                    fprintf(stdout, "Usage: %s -p [port] -a [address] -s [address]\n", argv[0]);
                    fprintf(stdout, "-p [port]    => port number for replica\n");
                    fprintf(stdout, "-a [address] => adddress for replica\n");
                    fprintf(stdout, "-s [address] => address for coordinator\n");
                    return 1;
            }
        } else {
            fprintf(stdout, "Unknown argument: %s\n", argv[i]);
            fprintf(stdout, "Usage: %s -p [port] -a [address] -s [address]\n", argv[0]);
            fprintf(stdout, "-p [port]    => port number for replica\n");
            fprintf(stdout, "-a [address] => adddress for replica\n");
            fprintf(stdout, "-s [address] => address for coordinator\n");
            return 1;
        }
    }
    if (addr != NULL && port > 0 && saddr != NULL) {
        init_pages(pages);
        create_connection(addr, port, saddr);
    } else {
        fprintf(stdout, "Usage: %s -p [port] -a [address] -s [address]\n", argv[0]);
        fprintf(stdout, "-p [port]    => port number for replica\n");
        fprintf(stdout, "-a [address] => adddress for replica\n");
        fprintf(stdout, "-s [address] => address for coordinator\n");
        return 1;
    }
    return 0;
}
