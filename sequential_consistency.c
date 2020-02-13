/*
 * Copyright John La Velle 2020
 */

#include <sys/time.h>
#include "common.h"
#include "sequential_consistency.h"

struct thread_args {
    int sock;
    char * update_msg;
};

/*
 * Provides unique identifier
 */
char * gather_uid(int mode, int artn) {
    char * u_id = NULL;
    int page, pgent;
    
    if (mode) {
    	page = artn / 10;
    	pgent = (artn % 10) -1;

    	u_id = malloc(snprintf(NULL, 0, "%d.%d", artn, 
    		pages->pages[page]->entries[pgent]->replies->log_len + 1));
    	sprintf(u_id, "%d.%d", artn, 
    		pages->pages[page]->entries[pgent]->replies->log_len + 1);
    } else {
    	u_id = malloc(snprintf(NULL, 0, "%d.0",
    		pages->pages[pages->log_len]->log_len + 1 + (10 * pages->log_len)));
    	sprintf(u_id, "%d.0",
    		pages->pages[pages->log_len]->log_len + 1 + (10 * pages->log_len));
    }
    fprintf(stdout, "ID: %s\n", u_id);
    return u_id;
} /* gather_uid */

/* thread function to send msg */
void* send_update(void * arg) {
    struct thread_args ta = *(struct thread_args *)arg;
    int sock = connections->connections[ta.sock];
    write(sock, ta.update_msg, strlen(ta.update_msg));
    pthread_exit(NULL);
} /* send_update */

/* thread function to listen for response
 * from update messge
 */
void* listen_ack(void * arg) {
    int sock = connections->connections[*(int *)arg];
    int readlen;
    fd_set readset;
    struct timeval tm;
    char * resp = malloc(MAX_MSG);
    tm.tv_usec = 300;
    readlen = 0;
    
    
    FD_SET(sock, &readset);
    select(sock + 1, &readset, NULL, NULL, &tm);
    
    if(FD_ISSET(sock, &readset)) {
    	readlen = read(sock, resp, MAX_MSG);
    	if(readlen < 1) {
    		readlen = 0;
    		pthread_exit(&readlen);
    	}
    	resp[readlen] = '\0';
    	if(strncmp(resp, "RECEIVED", readlen)) {
    		pthread_exit(&readlen);
    	}
    } 
    
    pthread_exit(&readlen);
} /* send_update */

/*
 * Decodes the message and handles dependent of content
 * For writes, we send all update message from here
 */
void handle_reception(char * buf, int sock) {
    size_t rdbyte;
    int i = 0, j = 0, artn;
    int hasContent = 0;
    pthread_t updater_thread[connections->log_len];
    pthread_t ack_thread[connections->log_len];
    
    char * unique_id, * writebk, * update_msg;
    article_entry_t * art;
    
    fprintf(stdout, "Received: %s\n", buf);
 
    if (strncmp(buf, "RECEIVED", 8) == 0) {
        free(buf);
        return;
    }

    while (buf[i] != '\0') {
        if (buf[i] == ':') {
            hasContent = 1;
            break;
        }
        i++;
    }
    i = 0;
    
    /* handle writes */
    if (hasContent) {
        fprintf(stdout, "performing write: %s\n", buf);
        int reply = 0;
        
        while (buf[i] != ':')
            i++;
        writebk = malloc(i);
        memcpy(writebk, buf, i);
        writebk[i] = '\0';
        buf = buf + i + 1;
        i = 0;
        fprintf(stdout, "writebk: %s\n", writebk);
        
    	while (buf[i] != ':')
    		i++;
    	unique_id = malloc(i + 1);
    	memcpy(unique_id, buf, i);
    	unique_id[i] = '\0';
    	i++;
    	
        while(unique_id[j] != '.')
            j++;
        j++;
        
        if (unique_id[j] > '0') {
            /* No need to lock for a persistent article */
            char * repn = malloc(j);
            buf = buf + i;
            memcpy(repn, unique_id, j - 1);
            repn[j] = '\0';
            fprintf(stdout, "%s %s\n", buf, repn);
            artn = atoi(repn);
            art = pages->pages[artn / 10]->entries[(artn % 10) - 1];
            rdbyte = strlen(unique_id) + strlen(buf) + 1;
            update_msg = malloc(snprintf(NULL, 0, "%s:%s", unique_id, buf));
            sprintf(update_msg, "%s:%s", unique_id, buf);
            update_msg[rdbyte] = '\0';
            pthread_mutex_lock(&page_lock);
            fprintf(stdout, "Adding reply: %s %s\n", unique_id, buf);
            add_reply(art, unique_id, buf);
            /* adding new article */
        } else {
            /* Take care of allocation before lock */
            fprintf(stdout, "Article: %s %s\n", unique_id, buf + i);
            art = malloc(sizeof(article_entry_t));
            art->article = malloc((strlen(buf) - i) + 1);
            strncpy(art->article, buf + i, (strlen(buf) - i) + 1);
            art->unique_id = unique_id;
            art->replies = malloc(sizeof(page_entries_t));
            art->replies->entries = malloc(sizeof(article_entry_t *) * PAGE_ENTRIES);
            art->replies->log_len = 0;
            update_msg = malloc(snprintf(NULL, 0, "%s:%s", art->unique_id, art->article));
            sprintf(update_msg, "%s:%s", art->unique_id, art->article);
            pthread_mutex_lock(&page_lock);
            fprintf(stdout, "adding entry: %s %s\n", art->unique_id, art->article);
            add_entry(art);
        }
        pthread_mutex_unlock(&page_lock);
    
        /* update message consists of article number:article contents */
        artn = connections->log_len; // avoid new servers messing up updates
        fprintf(stdout, "Sending: %s\n", update_msg);
        
        /* multithread message sending to simulate a pseudo-multicast */
        for ( i = 0; i < artn; i++) {
            if (connections->connections[i] == sock) {
                continue;
            }
            struct thread_args ta;
            ta.sock = i;
            ta.update_msg = update_msg;
            pthread_create(&updater_thread[i], NULL, send_update, &ta);
            pthread_detach(updater_thread[i]);
        }
        
        write(sock, update_msg, strlen(update_msg));
        read(sock, update_msg, MAX_MSG);
        write(sock, writebk, strlen(writebk));
        read(sock, update_msg, MAX_MSG);
        
        fprintf(stdout, "Acks\n");
        /* multithread listeners for acks -- time sensitive */
        for (i = 0; i < artn; i++) {
            if (connections->connections[i] == sock)
                continue;
            pthread_create(&ack_thread[i], NULL, listen_ack, &i);
        }
        
        /* join listeners for errors */
        int * result = malloc(sizeof(int));
        *result = 0;
        for (i = 0; i < artn; i++) {
            pthread_join(ack_thread[i], (void*)result);
            *result = 0;
        }
        free(result);
        free(update_msg);
        update_msg = NULL;
    }
} /* handle_reception */
