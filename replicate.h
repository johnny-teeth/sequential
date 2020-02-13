/*
 * Copyright John La Velle 2020
 */

#ifndef replicate_h
#define replicate_h

#define INITIAL_CLIENTS 10
int serve_sock;
int rem_sock;

void* cli_select(void * cli);
void* gather_cli();

#endif /* replicate_h */
