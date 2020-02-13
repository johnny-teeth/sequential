/*
 * Copyright John La Velle 2020
 */

#ifndef __SEQUENTIAL_CONSISTENCY_H__
#define __SEQUENTIAL_CONSISTENCY_H__

/* Global update message */ 

char * gather_uid(int mode, int artn);
void* send_messge(void * arg);
void* listen_ack(void * arg);
void handle_reception(char * buf, int sock);

#endif /* __SEQUENTIAL_CONSISTENCY_H__ */
