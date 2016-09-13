/*
 * A simple tool for helping you to remember information.
 * It extracts chracters from a word lists to combine a new word.
 * This new word may help you to remember the original stuff.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>


typedef struct word_t{
	char *word;
	int step, idx, max_idx, len;
	// used for words permutation.
	int nth;
	struct word_t *next, *prev;
}word_t;


word_t* word_set_init(const char *words, int max_sum_idx) 
{
	int nalloc = 20;
	int npos = 0;
	char **ws = (char**)malloc(nalloc * sizeof(char*));

	//parse word list
	char *str = strdup(words);
	char *pl = str;
	char *pw = NULL;
	while ((pw = strsep(&pl, " ")) != NULL) {
		if (strlen(pw) == 0)
			continue;
		ws[npos] = strdup(pw);
		npos++;
		if (npos >= nalloc) {
			nalloc *= 2;
			ws = (char**)malloc(nalloc * sizeof(char*));
		}
	}

	word_t *wset = (word_t*)calloc(npos, sizeof(word_t));
	int i;
	for (i = 0; i < npos; i++) {
		wset[i].nth = i;
		wset[i].next = &wset[i+1];
		wset[i].prev = &wset[i-1];
	}
	wset[0].prev = wset[npos-1].next = NULL;

	//fill wset with information about words;
	for (i = 0; i < npos; i++) {
		int len;
		wset[i].word = ws[i];
		len = wset[i].len = strlen(ws[i]);
		wset[i].max_idx = ((len-1) < max_sum_idx)?(len-1):max_sum_idx;
	}

	free(ws);
	free(str);
	return wset;
}

typedef struct dict_t {
	int alloc;
	int pos;
	char **ws;
}dict_t;

dict_t DICT[26][26];

bool read_dict(char *fdict)
{
	FILE *f = fopen(fdict, "r");
	if (f == NULL)
		return false;

	//init DICT
	int i, j;
	for (i = 0; i<26; i++) {
		for (j = 0; j < 26; j++) {
			DICT[i][j].alloc = 64;
			DICT[i][j].pos = 0;
			DICT[i][j].ws = (char**)calloc(DICT[i][j].alloc, sizeof(char*));
			if (DICT[i][j].ws == NULL) {
				perror("No memory");
				exit(0);
			}
		}
	}

	size_t size = 40;
	char *line = (char*)malloc(size);
	int nread;

	while ((nread = getline(&line, &size, f)) != EOF) {
		if (strlen(line) < 3)
			continue;
		// we use the dictionary: american-english
		char *p;
		p = strchr(line, '\'');
		if (p != NULL)
			continue;

		if (!isalpha(line[0]) || !isalpha(line[1]))
			continue;

		int len;
		p = strpbrk(line, "\n");
		if (p == NULL)
			len = strlen(line);
		else
			len = p - line;

		i = tolower(line[0]) - 'a';
		j = tolower(line[1]) - 'a';
		dict_t *d = &DICT[i][j];
		if ((d->alloc == 0) || (d->pos+1 > d->alloc)) {
			if (d->alloc == 0) {
				d->alloc == 16;
				d->ws == NULL;
			}
			char **buf = d->ws;
			d->alloc *= 2;
			buf = (char**)realloc(buf, d->alloc * sizeof(char*));
			if (buf == NULL) {
				perror("No memory");
				exit(0);
			}	
			d->ws = buf;
		}

		d->ws[d->pos] = strndup(line, len);
		d->pos++;
	}//end while

	fclose (f);

	return true;
}

/* return true if overflow; or else return false. */
bool iter_characters_combination(word_t *ws,  char *pcomb)
{
	//append characters;
	word_t *w = ws;
	char *pword = w->word + w->idx;
	pcomb = (char*)stpncpy(pcomb, pword, w->step);

	// exit conidtion of recusive call
	if (ws->next == NULL) {
		w->idx ++;
		*pcomb = '\0';

		if ((w->idx > w->max_idx) || ((w->idx + w->step) > w->len)) {
			w->idx = 0;
			return true;
		}
		else 
			return false;	
	}

	//more than one words
	bool bovflnxt = iter_characters_combination(ws->next, pcomb);

	if (!bovflnxt)
		return false;

	w->idx ++;
	if ((w->idx <= w->max_idx) && ((w->idx + w->step) <= w->len)) 
		return false;

	// we have arrived at the end of current word, 
	// while it was also true to the next word. So
	// we need to adjust steps.
	bool bstat = true;
	word_t *n;

	n = w;
	while (n != NULL) {
		n->idx = 0;
		n = n->next;
	}

	n = w->next;
	while (n != NULL) {
		if (n->step < n->len) {
			bstat = false;
			break;
		}
		n = n->next;
	}

	if ((w->step == 1) || bstat) {
		// reallocate steps
		int sup = 0;
		n = w;
		while (n != NULL) {
			sup += n->step - 1;
			n->step = 1;
			n = n->next;
		}
		n = w;
		while (n != NULL) {
			if (sup > (n->len-1)) {
				n->step = n->len;
				sup -= n->len-1;
				n = n->next;
			}
			else {
				n->step += sup;
				break;
			}
		}
		return true;
	}

	// w->step > 1 && !bstat
	// take one step from current word
	w->step --;
	n = w->next;
	while (n != NULL) {
		if (n->step < n->len) {
			n->step++;
			break;
		}
		else
			n = n->next;
	}
	return false;
}

word_t* words_in_order(word_t *head)
{
	if ((head == NULL) || (head->next == NULL))
		return head;

	word_t *orghead = head;
	word_t *newhead = NULL;
	word_t *newtail = NULL;

	while (orghead != NULL) {
		word_t *w = orghead;
		int min = w->nth;
		word_t *n = orghead->next;
		while (n != NULL) {
			if (n->nth < min) {
				w = n;
				min = n->nth;
			}
			n = n->next;
		}
		// delete n from the original list;
		if (w == orghead) {
			orghead = orghead->next;
		}
		else if (w->next == NULL) {
			w->prev->next = NULL;
		}
		else {
			n = w->next;
			w->prev->next = n;
			n->prev = w->prev;
		}
		w->next = w->prev = NULL;

		// Add n to the new list;
		if (newhead == NULL) {
			newtail = newhead = w;
		}
		else {
			newtail->next = w;
			w->prev = newtail;
			newtail = w;
		}
	}
	return newhead;
}

/* return true for continue; or else return false. */
bool iter_words_perm(word_t * const proot, int order)
{
	if (proot == NULL)
		return false;

	word_t *n = proot;
	int nwords;

	nwords = 0;
	while (n != NULL) {
		n = n->next;
		nwords++;
	}

	if (nwords != order) {
		perror("Unexpected order");
		exit(EXIT_FAILURE);
	}

	//end recusive call
	if (order == 1)
		return false;

	bool breverse = true;
	int min, i;

	min = proot->nth;
	n  = proot->next;
	while (n != NULL) {
		if (n->nth < min) {
			min = n->nth;
			n = n->next;
		}
		else {
			breverse = false;
			break;
		}
	}
	if (breverse)
		return false;

	bool b = iter_words_perm(proot->next, order-1);
	if (!b) {
		if (order <= 2) 
			n = proot->next;
		else {
			word_t *h = words_in_order(proot->next);
			proot->next = h;
			h->prev = proot;

			n = h;
			while (n->next != NULL) {
				if (n->nth > proot->nth)
					break;
				n = n->next;
			}
		}

		//swap the conext of n and proot;
		word_t t, *prev, *next;
		memcpy(&t, n, sizeof(word_t));
		memcpy(n, proot, sizeof(word_t));
		memcpy(proot, &t, sizeof(word_t));

		prev = proot->prev, next = proot->next;
		proot->prev = n->prev, proot->next = n->next;
		n->prev = prev, n->next=next;

	}
	return true;
}

bool VERBOSE = false;
bool AUTOSELECT = false;
bool PERMUTATION = false;
#define MAX_SUM_IDX 10
int AUTOSELECT_MAX_IDX = 2;

typedef struct autores_t {
	char *line;
	dict_t res[MAX_SUM_IDX];
}autores_t;

autores_t *AUTORES;

void produce_combine_word(word_t *ws, int combine_len, dict_t res[MAX_SUM_IDX])
{
	assert(combine_len >= 2);

	word_t *n = ws;

	int nword = 0;
	n = ws;
	while (n != NULL) {
		nword++;
		n = n->next;
	}
	if (nword > combine_len)
		return;

	int len = 0;
	n = ws;
	while (n != NULL) {
		len += n->len;
		n = n->next;
	}
	if (combine_len > len)
		return;

	// reallocate steps
	int sup = combine_len;
	n = ws;	
	while (n != NULL) {
		n->step = 1;
		sup--;
		n->idx = 0;
		n = n->next;
	}

	n = ws;	
	while (n != NULL) {
		if (sup > (n->len-1)) {
			n->step = n->len;
			sup -= n->len-1;
			n = n->next;
		}
		else {
			n->step += sup;
			break;
		}
	}

	char *comb = (char*)malloc(64);
	char *idxstep = (char*)malloc(256);
	int idx = 0;
	bool bexit = false;

	if (!AUTOSELECT)
		printf("Results of word length %d:\n", combine_len);

	do {
		if (VERBOSE) {
			word_t *w = ws;
			char *p = idxstep;
			idx  = 0;
			while (w != NULL) {
				p += sprintf(p, "%d %d\t", w->idx, w->step);
				idx += w->idx;
				w = w->next;
			}
		}

		*comb = '\0';
		bexit = iter_characters_combination(ws, comb);

		int i, j;
		i = tolower(comb[0]) - 'a';
		j = tolower(comb[1]) - 'a';

		int pos;
		for (pos = 0; pos < DICT[i][j].pos; pos++) {
			if (!strncasecmp(comb, DICT[i][j].ws[pos], 64)) {
				if (AUTOSELECT) {
					if (idx > AUTOSELECT_MAX_IDX)
						continue;

					if ((res[idx].alloc == 0) || (res[idx].pos+2 > res[idx].alloc))	{
						if (res[idx].alloc == 0) {
							res[idx].alloc = 16;
							res[idx].ws = NULL;
						}
						char **buf = res[idx].ws;
						res[idx].alloc *= 2;
						buf = (char**)realloc(buf, res[idx].alloc * sizeof(char*));
						res[idx].ws = buf;
					}

					int pos = res[idx].pos;
					res[idx].ws[pos] = strdup(comb);
					res[idx].ws[pos+1] = strdup(idxstep);
					res[idx].pos += 2;
				}
				else if (VERBOSE)
					printf("%s\n%s\n", comb, idxstep);
				else
					printf("%s\n", comb);

				break;
			}
			//printf("%s\n", comb);
		} //end for
	}while (!bexit);

	free(idxstep);
}


void Usage(const char *prog)
{
	printf("Extract characters from word to combine a new word for recite. Usage: \n" 
			"%s [--min-length decimal] [--max-length decimal]\n"
			"\t [--max-sum-idx decimal]\n"
			"\t [--dict fdict]\n"
			"\t [--autoselect]\n"
			"\t [--permutation]\n"
			"\t [--verbose]\n"
			"\t [--help]\n"
			"\t string_list\n", prog);

	printf(" --min-length --max-length: the minimun or maxmum length of generated new words.\n"
			" --max-sum-idx: the maximun value of sum of idx (idx means offset to each word).\n"
			" --dict: specify the path of your dictionary. use /usr/share/dict/american-english in default.\n"
			" --permutation: let the system permutate the word list and search the combinations on each permutation.\n"
			" --autoselect: let the system select the best candidates for you.\n"
			" --verbose: print out more details. This option is automatically enabled when autoselect or permutaion is enabled.\n");

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	char *words = NULL;
	char *fdict = NULL;
	int min = 0, max=0;
	AUTOSELECT_MAX_IDX = INT_MAX;

	static struct option options[] = {
		{"min-length", required_argument, NULL, 'i'},
		{"max-length", required_argument, NULL, 'a'},
		{"max-sum-idx", required_argument, NULL, 'x'},
		{"dict", required_argument, NULL, 'd'},
		{"autoselect", no_argument, NULL, 's'},
		{"permutation", no_argument, NULL, 'p'},
		{"verbose", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
	};

	int opt, optidx = 0;
	while ((opt = getopt_long(argc, argv, "i:a:x:d:spvh", options, &optidx)) != -1) {
		switch(opt) {
			case 'i':
				min = strtol(optarg, NULL, 10);
				break;
			case 'a':
				max = strtol(optarg, NULL, 10);
				break;
			case 'x':
				AUTOSELECT_MAX_IDX = strtol(optarg, NULL, 10);
				break;
			case 'd':
				fdict = strdup(optarg);
				break;
			case 's':
				AUTOSELECT = true;
				break;
			case 'p':
				PERMUTATION = true;
				break;
			case 'v':
				VERBOSE = true;
				break;
			case 'h':
			default:
				Usage(argv[0]);
				break;
		}
	}

	if (optind + 1 != argc)
		Usage(argv[0]);

	if (AUTOSELECT) {
		VERBOSE = true;
		if (AUTOSELECT_MAX_IDX > MAX_SUM_IDX)
			AUTOSELECT_MAX_IDX = MAX_SUM_IDX-1;
	}

	words = strdup(argv[optind]);
	word_t* ws = word_set_init(words, AUTOSELECT_MAX_IDX);

	if (ws == NULL) {
		perror("Failed to parse word list!\n");
		Usage(argv[0]);
	}

	/* check inputs for validity */
	word_t *w = ws;
	int nword = 0;
	int nlen = 0;
	while (w != NULL) {
		nword ++;
		nlen += w->len;
		w = w->next;
	}

	if (min < nword)
		min = nword;
	if (max < min)
		max = min;	
	if (max > nlen)
		max = nlen;
	if (min > max)
		min = max;

	if (fdict == NULL)
		fdict = "/usr/share/dict/american-english";

	if (!read_dict(fdict)) {
		perror("Cannot open dictionary!\n");
		Usage(argv[0]);
	}


	int fact = 1;
	int i, j;
	//sotre the results with dynamically allocated arrays.
	if (PERMUTATION) {
		i = nword;
		while (i > 1) {
			fact *= i;
			i--;
		}
	}
	if (AUTOSELECT) {
		AUTORES = (autores_t*)calloc(fact, sizeof(autores_t));
	}

	char *line = (char*)malloc(512);
	i = 0;
	do {
		// ouput the word list as string.
		word_t *w = ws;
		char *pl = line;
		*pl = '\0';
		while (w != NULL) {
			pl += sprintf(pl, "%s ", w->word);
			w = w->next;
		}

		if (AUTOSELECT) 
			AUTORES[i].line = strdup(line);
		else if (VERBOSE || PERMUTATION)
			printf("Word list: %s\n", line);

		for (j = min; j <= max; j++) { 
			produce_combine_word(ws, j, AUTORES[i].res);
		}
		i++;
		if (!PERMUTATION)
			break;
	}while(iter_words_perm(ws, nword));

	// output results stored in arrays.
	if (AUTOSELECT) {
		int i, j,  k;
		bool c;
		for (i = 0; i < fact; i++) {
			dict_t *res = AUTORES[i].res;

			c = true; 
			for (j = 0; j <= AUTOSELECT_MAX_IDX; j++) {
				if (res[j].pos != 0) {
					c = false;
					break;
				}
			}
			if (c)
				continue;

			printf("\nWords list: %s\n", AUTORES[i].line);
			for (j = 0; j <= AUTOSELECT_MAX_IDX; j++) {
				if (res[j].pos == 0)
					continue;

				char **data = res[j].ws;
				printf("words of sum idx: %d\n", j);
				for (k = 0; k < res[j].pos; k+=2, data +=2)
					printf("%s\n%s\n", data[0], data[1]);
			}
		}
	}

	return EXIT_SUCCESS;
}
