/*
 * Copyright John La Velle 2020
 */

#include "common.h"

/*
 * Allocator algorithms
 * Pages, entries, articles, replies
 */

entry_pages_t * init_pages() {
	int i,j,k;
    fprintf(stdout, "Page foundation building\n");
	pages = malloc(sizeof(entry_pages_t));
	pages->pages = malloc(sizeof(page_entries_t *) * PAGE_ENTRIES);
    
    if (pages == NULL || pages->pages == NULL) {
        fprintf(stderr, "Malloc Failure: init_pages -> pages\n");
        exit(1);
    }
    
    for (i = 0; i < INITIAL_PAGES; i++) {
        fprintf(stdout, "Entry building: %d\n", i);
        pages->pages[i] = malloc(sizeof(page_entries_t *));
        pages->pages[i]->entries = malloc(sizeof(article_entry_t *) * PAGE_ENTRIES);
        
        if(pages->pages[i] == NULL || pages->pages[i]->entries == NULL) {
            fprintf(stderr, "Malloc Failure: init_pages -> pages\n");
            exit(1);
        }
        pages->pages[i]->log_len = 0;
    }
    
	pages->alloc_len = INITIAL_PAGES;
	pages->log_len = 0;
    return pages;
} /* init_pages */

void free_everything(entry_pages_t *pages) {
	int i, j, k;
	for(i = 0; i < pages->log_len; i++){
		page_entries_t * enter = pages->pages[i];
		for(j = 0; j < enter->log_len; j++) {
			article_entry_t * art = enter->entries[j];
			for(k = 0; k < art->replies->log_len; k++){
				free(art->replies->entries[k]->article);
				free(art->replies->entries[k]->unique_id);
				free(art->replies->entries[k]);
			}
			free(art->replies->entries);
			free(art->replies);
			free(art->article);
			free(art->unique_id);
			free(art);
		}
		free(enter->entries);
		free(enter);
	}
	free(pages->pages);
	free(pages);
} /* free_everything */

void realloc_pages(entry_pages_t * pages) {
	void * k = realloc(pages->pages,
		sizeof(page_entries_t *) * (pages->alloc_len + INITIAL_PAGES));
	pages->alloc_len += INITIAL_PAGES;
    if( k == NULL ) {
        fprintf(stdout, "Realloc failed: replies\n");
    }
} /* realloc_pages */

void realloc_replies(article_entry_t * art) {
	void * k = realloc(art->replies->entries,
		sizeof(article_entry_t *) * (art->replies->log_len + INITIAL_PAGES));
    if( k == NULL ) {
        fprintf(stdout, "Realloc failed: replies\n");
    }
} /* realloc replies */

/*
 * Adding to structs functions
 * Different use cases -> new article or new reply
 */
void add_reply(article_entry_t *art, char * uni_id, char *repl) {
    if(art == NULL || repl == NULL) {
    	fprintf(stderr, "Error: Unable to add reply, bad address\n");
    	free_everything(pages);
    	exit(1);
    }
    
	if (((art->replies->log_len % 10) == 0) && 
		(art->replies->log_len > 0)) {
        fprintf(stdout, "Reallocating replies\n");
		realloc_replies(art);
	}
    
    
    int i,k;
    for (i = 0; i < art->replies->log_len; i++) {
        if (strcmp(uni_id, art->replies->entries[i]->unique_id) == 0) {
            char * newu;
            k = 0;
            while (uni_id[k] != '.')
                k++;
            uni_id[k - 1] = (char)(art->unique_id[k - 1] + 1);
            
        }
    }
    
    article_entry_t * temp = malloc(sizeof(article_entry_t));
    temp->replies = NULL;
	temp->article = repl;
	temp->unique_id = uni_id;
    art->replies->entries[art->replies->log_len] = temp;
	art->replies->log_len++;
    fprintf(stdout, "Committing %s %s\n",art->unique_id, art->article);
    for (i = 0; i < art->replies->log_len; i++) {
        fprintf(stdout, "\t%s %s\n",art->replies->entries[i]->unique_id, art->replies->entries[i]->article);
    }
} /* add_reply */

void add_entry(article_entry_t *art) {
    if (pages->pages[pages->log_len]->log_len + 1 == 10) {
        pages->log_len++;
    }
    
	if (pages->alloc_len == pages->log_len) {
		realloc_pages(pages);
	}
    
    int i,j,k;
    for (i = 0; i < pages->log_len; i++) {
        for (j = 0; j < pages->pages[i]->log_len; j++) {
            if (strcmp(art->unique_id, pages->pages[i]->entries[j]->unique_id) == 0) {
                char * newu;
                k = 0;
                while (art->unique_id[k] != '.')
                    k++;
                art->unique_id[k - 1] = (char)(art->unique_id[k - 1] + 1);
                
            }
        }
    }

    int plen = pages->log_len;
    int alen = pages->pages[plen]->log_len;
    fprintf(stdout, "add: %s %s\n", art->unique_id, art->article);
	pages->pages[plen]->entries[alen] = art;
    pages->pages[plen]->log_len++;
    fprintf(stdout, "Added: %s %s\n", pages->pages[plen]->entries[alen]->unique_id, pages->pages[plen]->entries[alen]->article);
} /* add_entry */
/*
void handle_update(int sock) {
	char * uni_id, * rep_id, * art_id, * resp;
	int pg, pgnum, entnum;
	int k = 0, j = 0;

	char * art_buf = malloc(MAX_MSG);
	size_t rdbts = read(sock, art_buf, MAX_MSG);

	art_buf[rdbts] = '\0';
	while(art_buf[k] != ':')
		k++;
	uni_id = malloc(k);
	strncpy(uni_id, art_buf, k);

	k++;
	j = k;

	while(art_buf[k] != ':')
		k++;
	rep_id = malloc(k - j);
	strncpy(rep_id, art_buf + j, k - j);
	
	resp = malloc(rdbts - k);
	strncpy(resp, art_buf + k, rdbts - k);
	
	pg = atoi(uni_id);
	pgnum = pg / 10;
	entnum = pg % 10;
	
	art_id = malloc(k + (k-j) + 1);
	if( atoi(rep_id) > 0 ) {
		sprintf(art_id, "%s.%s", uni_id, rep_id);
		
		while ( pgnum > pages->log_len ) {
			sleep(1);
		}
		
		while ( entnum > pages->pages[pgnum]->log_len ) {
			sleep(1);
		}
		
		article_entry_t * art = pages->pages[pgnum]->entries[entnum];
		
		pthread_mutex_lock(&page_lock);
		add_reply(art, art_id, resp);
		pthread_mutex_unlock(&page_lock);
	} else {
		sprintf(art_id, "%s.0", uni_id);
		
		article_entry_t * art = malloc(sizeof(article_entry_t));
		art->article = resp;
		art->unique_id = art_id;
		art->replies->entries = malloc(sizeof(article_entry_t **));
		*(art->replies->entries) =
			malloc(sizeof(article_entry_t *) * PAGE_ENTRIES);
		
		pthread_mutex_lock(&page_lock);
		add_entry(art);
		pthread_mutex_unlock(&page_lock);
	}
	
	free(uni_id);
	free(rep_id);
}
*/

/*
 * Build char * with article and replies
 */
char * assemble_replies(int artnum) {
	char * rep_str;
	size_t totstr;
	int j = 0;
	
	int pg = artnum /10;
	int ent = (artnum %10) - 1;
    fprintf(stdout, "Gathering: %s %s\n", pages->pages[pg]->entries[ent]->unique_id,
            pages->pages[pg]->entries[ent]->article);
    
	totstr = snprintf(NULL, 0, "-- %s ) %s\n", pages->pages[pg]->entries[ent]->unique_id,
                       pages->pages[pg]->entries[ent]->article);
	while (j < pages->pages[pg]->entries[ent]->replies->log_len){
		totstr += snprintf(NULL, 0, "\t-- %s ) %s\n",
			pages->pages[pg]->entries[ent]->replies->entries[j]->unique_id,
			pages->pages[pg]->entries[ent]->replies->entries[j]->article);
        j++;
	}
	j = 0;
	rep_str = malloc(totstr);
    totstr = sprintf(rep_str, "-- %s ) %s\n", pages->pages[pg]->entries[ent]->unique_id,
             pages->pages[pg]->entries[ent]->article);
	while (j <  pages->pages[pg]->entries[ent]->replies->log_len) {
        fprintf(stdout, "TOT: %zu STR: %s\n", totstr, rep_str);
		totstr += sprintf(rep_str + totstr, "\t-- %s ) %s\n",
			pages->pages[pg]->entries[ent]->replies->entries[j]->unique_id, 
			pages->pages[pg]->entries[ent]->replies->entries[j]->article);
        j++;
	}
	return rep_str;
} /* assemble_replies */

/*
 * Assemble 10 articles per page --> char *
 */
char * assemble_page(int page) {
	size_t totstr = 1;
    struct article_entry_t * tmp;
	char * page_str;
	int j = 0;
    
	while (j < pages->pages[page]->log_len){
        tmp = (struct article_entry_t*)pages->pages[page]->entries[j];
        fprintf(stdout, "retpg: %s %s\n", tmp->unique_id, tmp->article);
		totstr += snprintf(NULL, 0, "-- %s ) %.25s...\n", tmp->unique_id, tmp->article);
        j++;
	}
	j = 0;
	page_str = malloc(totstr);
	totstr = 0;
	while (j < pages->pages[page]->log_len) {
        tmp = (struct article_entry_t*)pages->pages[page]->entries[j];
		totstr += sprintf(page_str + totstr, "-- %s ) %.25s...\n", tmp->unique_id, tmp->article);
        fprintf(stdout, "TOT: %zu STR: %s\n", totstr, page_str);
        j++;
	}
	return page_str;
} /* assemble_page */

/*
 * Does a bunch of string manipulation to perform
 *   adding entries and replies
 */
void decode_msg(char * msg) {
	article_entry_t * art;
	int i = 0, j, page, ent;
	char * unique_id, * resp, * tmp;
    fprintf(stdout, "msg: %s\n", msg);
	
    while (msg[i] != ':')
		i++;
	unique_id = malloc(i + 1);
	memcpy(unique_id, msg, i);
	unique_id[i] = '\0';
	i++;
	fprintf(stdout, "uid: %s\n", unique_id);
    
	resp = malloc(strlen(msg) - i);
	memcpy(resp, msg + i, (strlen(msg) - i) + 1);
	resp[(strlen(msg) - i) + 1] = '\0';
    fprintf(stdout, "resp: %s\n", resp);
	
	i = 0;
	while(unique_id[i] != '.')
		i++;
	tmp = malloc(i + 1);
	memcpy(tmp, unique_id, i);
	tmp[i] = '\0';
	page = atoi(tmp);
	free(tmp);
	
    i = 0;
	while(unique_id[i] != '.')
		i++;
    i++;
	
	if (unique_id[i] > '0') {
        char * entstr = malloc(i);
        memcpy(entstr, unique_id, i - 1);
        entstr[i] = '\0';
        ent = atoi(entstr);
        fprintf(stdout, "ent: %d\n\n", ent);
		art = pages->pages[page / 10]->entries[(page % 10) -1];
		pthread_mutex_lock(&page_lock);
		add_reply(art, unique_id, resp);
		pthread_mutex_unlock(&page_lock);
	} else {
		art = malloc(sizeof(article_entry_t));
		art->unique_id = unique_id;
		art->article = resp;
		art->replies = malloc(sizeof(page_entries_t));
		art->replies->entries = malloc(sizeof(article_entry_t **));
		*(art->replies->entries) = malloc(sizeof(article_entry_t *) * PAGE_ENTRIES);
		pthread_mutex_lock(&page_lock);
        add_entry(art);
		pthread_mutex_unlock(&page_lock);
	}
} /* decode_msg */
