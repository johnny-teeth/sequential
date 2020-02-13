/*
 * Copyright John La Velle 2020
 */

#ifndef __CLIENT_UI_H__
#define __CLIENT_UI_H__

#include "common.h"

#define MAX_MSG 128
#define MAX_PAGE_MSG (MAX_MSG*10)+(50)

void run_ui(int sock);
void display_replies(int artnum);
void reply_menu(int artnum);
void choose_menu(int artnum);
void post_menu();
void print_welcome();
void read_pages(int page);

#endif /* __CLIENT_UI_H__ */
