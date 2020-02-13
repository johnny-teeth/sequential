/*
 * Copyright John La Velle 2020
 */

#ifndef __COMMON_H__
#define __COMMON_H__

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

#define PAGE_ENTRIES 10
#define INITIAL_PAGES 3
#define INITAL_CLIENTS 4
#define LISTEN_BACK 10
#define MAX_MSG 128

typedef struct page_entries_t page_entries_t;

typedef struct article_entry_t {
    char * article;
    char * unique_id;
    page_entries_t * replies;
} article_entry_t;

typedef struct page_entries_t {
	article_entry_t ** entries;
	int log_len;
} page_entries_t;

typedef struct entry_pages_t {
	page_entries_t ** pages;
	int alloc_len;
	int log_len;
} entry_pages_t;

typedef struct conn_t_ {
    char * addr;
    int port;
} conn_t;

typedef struct serve_connections_t {
	int alloc_len;
	int log_len;
	int max_fd;
    int * connections;
    conn_t * connect_attrs;
} serve_connections_t;

entry_pages_t *pages;
serve_connections_t * connections;
serve_connections_t * clients;

int art_num;
int resp_num;

int start_port;
int loc_sock, cli_sock;
struct sockaddr_in loc_serve, cli_serve;

//bitmap_t kbitmap;

pthread_t join_thread, cli_thread;
pthread_mutex_t conn_lock;
pthread_mutex_t cli_lock;
pthread_mutex_t page_lock;
pthread_mutex_t write_lock;

entry_pages_t * init_pages();
void free_everything(entry_pages_t *pages);
void realloc_pages(entry_pages_t * pages);
void realloc_replies(article_entry_t * art);

void add_entry(article_entry_t *art);
void add_reply(article_entry_t *art, char * uni_id, char *repl);
void handle_update(int sock);
void decode_msg(char * msg);
char * assemble_page(int page);
char * assemble_replies(int artnum);
#endif /* __COMMON_H__ */






