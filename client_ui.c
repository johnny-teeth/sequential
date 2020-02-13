/*
 * Copyright John La Velle 2020
 */

#include <stdlib.h>
#include <time.h>
#include "client.h"
#include "client_ui.h"
#include "common.h"

char ** welcome = (char *[]){ "#############################################################\n",
    "~               WELCOME TO THE BULLETIN BOARD               ~\n",
    "#############################################################\n",
    "~ 1.) READ THE BOARD                                        ~\n",
	"~ 2.) CHOOSE AN ARTICLE BY ID                               ~\n",
	"~ 3.) POST AN ARTICLE                                       ~\n",
	"~ 4.) REPLY TO AN ARTICLE BY ID                             ~\n",
	"~ 5.) EXIT                                                  ~\n",
	"# --- Selection: " };

char ** choose_footer = (char *[])
{ "~ 1.) REPLY TO ARTICLE                                      ~\n",
    "~ 2.) READ REPLIES                                          ~\n",
    "~ 3.) RETURN TO MENU                                        ~\n",
    "~ 2.) RETURN TO ARTICLE                                     ~\n" };

char ** read_footer = (char *[])
{ "~ 1.) CHOOSE ARTICLE                                        ~\n",
  "~ 2.) POST ARTICLE                                          ~\n",
  "~ 3.) NEXT PAGE                                             ~\n",
  "~ 4.) RETURN TO MAIN MENU                                   ~\n"
};
					
char * read_action_hdr = 
	"~                       BULLETIN BOARD                      ~\n";
char * choose_action_hdr =
	"~                       CHOOSE ARTICLE                      ~\n";
char * reply_action_hdr =
	"~                      CREATE  RESPONSE                     ~\n";
char * post_action_hdr =
	"~                       CREATE  ARTICLE                     ~\n";
					
void print_welcome() {
	fprintf(stdout, "%s%s%s%s%s%s%s%s%s", welcome[0], welcome[1], welcome[2], welcome[3],
		welcome[4], welcome[5], welcome[6], welcome[7], welcome[0]);
    printf("%s", welcome[8]);
}

void read_pages(int page) {
    int rdsz, sel;
    clock_t timer;
    char * req = malloc(snprintf(NULL, 0, "SENDPAGE:%d",page));
    sprintf(req, "SENDPAGE:%d", page);
    timer = clock();
    write(loc_sock, req, strlen(req));
    free(req);
    req = malloc(MAX_PAGE_MSG);
    rdsz = read(loc_sock, req, MAX_PAGE_MSG);
    timer = clock() - timer;
    req[rdsz] = '\0';

    fprintf(stdout, "%s%s%s%s\nPerformed in %lu clockticks\n",welcome[0], read_action_hdr, welcome[0], req, timer);
refootpg:
    fprintf(stdout, "%s%s%s%s",read_footer[0], read_footer[1], read_footer[2], read_footer[3]);
    printf("%s", welcome[8]);
    scanf("%d", &sel);
    free(req);
    
    switch(sel) {
        case 1:
            choose_menu(0);
            break;
        case 2:
            post_menu();
            break;
        case 3:
            read_pages(++page);
            break;
        case 4:
            run_ui(loc_sock);
            break;
        default:
            fprintf(stdout, "Illegal Argument: %d\n", sel);
            goto refootpg;
            break;
    }
}

void choose_menu(int artnum) {
    int i = 0;
	char artn[15], * req;
	size_t rdsz;
    clock_t timer;
    
	if (artnum == 0) {
		fprintf(stdout, "%s%s%s",
			welcome[0], choose_action_hdr, welcome[0]);
        printf("Article Number: ");
        scanf("%d", &artnum);
	} else {
		fprintf(stdout, "%s%s%s", welcome[0], choose_action_hdr, welcome[0]);
	}
	
    req = malloc(snprintf(NULL, 0, "READ:%d.0", artnum));
    sprintf(req, "READ:%d.0", artnum);
    timer = clock();
    write(serve_sock, req, strlen(req));
    memset(artn, 0, 15);
    free(req);
    
    req = malloc(MAX_MSG);
    rdsz = read(loc_sock, req, MAX_MSG);
    timer = clock() - timer;
    req[rdsz] = '\0';
    
	/* print article */
    fprintf(stdout, "\n%s\nPerformed in: %lu clockticks\n%s", req, timer, welcome[0]);
	/* print actions */
	fprintf(stdout, "%s%s%s", choose_footer[0], choose_footer[1], choose_footer[2]);
    printf("%s", welcome[8]);
    scanf("%d", &i);
	
	switch(i) {
		case 1:
            free(req);
			reply_menu(artnum);
			break;
		case 2:
			display_replies(artnum);
			break;
		case 3:
            free(req);
			run_ui(loc_sock);
			break;
		default:
			fprintf(stdout, "\n\n\nError: invalid argument\n");
			choose_menu(artnum);
			break;
	}		
}

void display_replies(int artnum) {
	char resp[15], * msg;
	size_t rdsz;
    int sel;
    clock_t timer;
	if (artnum == 0) {
		fprintf(stdout, "%s%s%s",
			welcome[0], reply_action_hdr, welcome[0]);
        printf("--- Enter Article Number (Without Decimal): ");
        scanf("%d", &artnum);
	}
	
    msg = malloc(snprintf(NULL, 0, "READ:%d.0", artnum));
    sprintf(msg, "READ:%d.0", artnum);
    timer = clock();
	write(loc_sock, msg, strlen(msg));
	
	free(msg);
	msg = malloc(MAX_PAGE_MSG);
	rdsz = read(serve_sock, msg, MAX_MSG);
    timer = clock() - timer;
	msg[rdsz] = '\0';
	memset(resp, 0, 15);
	
    fprintf(stdout, "%s%s\nPerform in: %lu clockticks\n%s", welcome[0], msg, timer, welcome[0]);
	fprintf(stdout, "%s%s%s",
		choose_footer[0], choose_footer[3], choose_footer[2]);
    printf("--- Selection: ");
    scanf("%d", &sel);
    free(msg);
	switch(sel) {
		case 1:
			reply_menu(artnum);
			break;
		case 2:
			choose_menu(artnum);
			break;
		case 3:
			run_ui(loc_sock);
			break;
		default:
			fprintf(stdout, "Error: invalid selection\n");
			display_replies(artnum);
			break;
	}
}	 

void reply_menu(int artnum) {
	char resp[18], * msg, * new_msg;
	int rdsz;
    clock_t timer;
    clock_t time2;
	if (artnum == 0) {
		fprintf(stdout, "%s%s%s",
			welcome[0], reply_action_hdr, welcome[0]);
        printf("--- ENTER ARTICLE NUMBER: ");
        scanf("%d", &artnum);
	} else {
		fprintf(stdout, "%s%s%s", 
			welcome[0], reply_action_hdr, welcome[0]);
	}
    timer = clock();
	snprintf(resp, 18, "REPLY:%d", artnum);
	if(write(loc_sock, resp, strlen(resp)) <= 0 ) {
		fprintf(stdout, "Write failed: reply_menu [1]\n");
	}
    
	memset(resp, 0, 18);
	if((rdsz = read(loc_sock, resp, 15)) <= 0) {
		fprintf(stdout, "Read failed: reply_menu [2]\n");
		return;
	}
    timer = clock() - timer;
	resp[rdsz] = '\0';
	
	msg = malloc(MAX_MSG);
	printf("--- ENTER RESPONSE: \n");
	rdsz = read(0, msg, MAX_MSG);
	msg[rdsz] = '\0';
	
	new_msg = malloc(snprintf(NULL, 0, "%s:%s", resp, msg));
	sprintf(new_msg, "%s:%s", resp, msg);
    fprintf(stdout, "Reply: %s\n",new_msg);
    time2 = clock();
	if(write(loc_sock, new_msg, strlen(new_msg)) <= 0 ) {
		fprintf(stdout, "Write failed: reply_menu [2]\n");
	}
	memset(resp, 0, 18);
	read(loc_sock, resp, 1);
    time2 = clock() - time2;
	if (resp[0] == '1') {
		fprintf(stdout, "#                        REPLY POSTED                       #\n");
        fprintf(stdout, "Performed in: %lu clockticks\n", timer + time2);
		fprintf(stdout, "%s", welcome[0]);
	}
    free(new_msg);
    free(msg);
	run_ui(loc_sock);
}

void post_menu() {
	char ask[] = "POST";
	char * resp, * cntn, * msg;
	int rdsz;
    clock_t timer;
    clock_t time2;
    timer = clock();
	if( write(loc_sock, ask, strlen(ask)) <= 0) {
		fprintf(stdout, "Write failed: post_menu\n");
		return;
	}
	resp = malloc(15);
	if ((rdsz = read(loc_sock, resp, 15)) <= 0) {
		fprintf(stdout, "Read failed: post_menu\n");
		return;
	}
    timer = clock() - timer;
	resp[rdsz] = '\0';
    
	fprintf(stdout, "%s%s%s", welcome[0], post_action_hdr,
		welcome[0]);
        printf("--- ENTER ARTICLE CONTENTS: \n");
	cntn = malloc(MAX_MSG);
	if ((rdsz = read(0, cntn, MAX_MSG)) <= 0) {
		fprintf(stdout, "Read failed: post_menu\n");
		return;
	}
	cntn[rdsz] = '\0';
	
	msg = malloc(snprintf(NULL, 0, "%s:%s", resp, cntn));
	sprintf(msg, "%s:%s", resp, cntn);
    time2 = clock();
	if(write(loc_sock, msg, strlen(msg)) <= 0) {
		fprintf(stdout, "Post failed: write error \n");
		return;
	}
	fprintf(stdout, "Check reception: %s\n", msg);
	memset(resp, 0, 15);
	if (read(loc_sock, resp, 1) <= 0) {
		fprintf(stdout, "Read failed: no post response\n");
	}
    time2 = clock() - time2;
	if (resp[0] != '1') {
        fprintf(stdout, "Post failed: fail response \nPerformed in: %lu clockticks\n", timer + time2);
		return;
	}
    fprintf(stdout, "Article successfully posted!\nPerformed in: %lu clockticks\n", timer + time2);
	free(resp);
	free(cntn);
	free(msg);
	run_ui(loc_sock);
}

void run_ui(int sock) {
    loc_sock = sock;
    serve_sock = sock;
	int resp;
    
	print_welcome();
    scanf("%d", &resp);
	
	switch(resp) {
		case 1:
			read_pages(0);
			break;
		case 2:
			choose_menu(0);
			break;
		case 3:
			post_menu();
			break;
		case 4:
			reply_menu(0);
			break;
		case 5:
            write(sock, "CLOSE", strlen("CLOSE"));
            close(sock);
			return;
		default:
			fprintf(stdout, "Error: invalid argument\n");
			run_ui(loc_sock);
			break;
	}
}
