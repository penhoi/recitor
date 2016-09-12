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
#include <assert.h>


typedef struct word_t{
	char *word;
	int len, step, idx;
	// used for outputing combination details
	struct word_t *next, *prev;
}word_t;


word_t* word_set_init(const char *words) 
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
		wset[i].next = &wset[i+1];
		wset[i].prev = &wset[i-1];
	}
	wset[0].prev = wset[npos-1].next = NULL;

	//fill wset with information about words;
	for (i = 0; i < npos; i++) {
		wset[i].word = ws[i];
		wset[i].len = strlen(ws[i]);
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
		DICT[i][j].ws[DICT[i][j].pos] = strndup(line, len);
		DICT[i][j].pos++;
		if (DICT[i][j].pos > DICT[i][j].alloc) {
			char **buf = DICT[i][j].ws;
			DICT[i][j].alloc *= 2;
			buf = (char**)realloc(buf, DICT[i][j].alloc * sizeof(char*));
			if (buf == NULL) {
				perror("No memory");
				exit(0);
			}	
			DICT[i][j].ws = buf;
		}
	}

	fclose (f);

	return true;
}

/* return true if overflow; or else return false. */
bool iter_word_set(word_t *ws,  char *pcomb)
{
	//append characters;
	word_t *w = ws;
	char *pword = w->word + w->idx;
	pcomb = (char*)stpncpy(pcomb, pword, w->step);

	// exit conidtion of recusive call
	if (ws->next == NULL) {
		w->idx ++;
		*pcomb = '\0';

		if ((w->idx+w->step) > w->len) {
			w->idx = 0;
			return true;
		}
		else 
			return false;	
	}

	//more than one words
	bool bovflnxt = iter_word_set(ws->next, pcomb);

	if (!bovflnxt)
		return false;

	w->idx ++;
	if ((w->idx+w->step) <= w->len)
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


bool VERBOSE = false;
void produce_combine_word(word_t *ws, int combine_len)
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
	char *idxstep = NULL;
	bool bexit = false;

	if (VERBOSE) 
		idxstep = (char*)malloc(256);

	do {
		if (VERBOSE) {
			word_t *w = ws;
			char *p = idxstep;
			while (w != NULL) {
				p += sprintf(p, "%d %d\t", w->idx, w->step);
				w = w->next;
			}
		}

		*comb = '\0';
		bexit = iter_word_set(ws, comb);

		int i, j;
		i = tolower(comb[0]) - 'a';
		j = tolower(comb[1]) - 'a';

		int pos;
		for (pos = 0; pos < DICT[i][j].pos; pos++) {
			if (!strncasecmp(comb, DICT[i][j].ws[pos], 64)) {
				printf("%s\n", comb);
				if (VERBOSE)
					printf("%s\n", idxstep);

				break;
			}
			//printf("%s\n", comb);
		}
	}while (!bexit);

}


void Usage(const char *prog)
{
	printf("Extract characters from word to combine a new word for recite. Usage: \n" 
			"\t%s [--min decimal] [--max decimal] [--dict fdict] string_list\n", prog);

	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	char *words = NULL;
	char *fdict = NULL;
	int min = 0, max=0;

	static struct option options[] = {
		{"min", required_argument, NULL, 'i'},
		{"max", required_argument, NULL, 'a'},
		{"dict", required_argument, NULL, 'd'},
		{"verbose", no_argument, NULL, 'v'},
	};

	int opt, optidx = 0;
	while ((opt = getopt_long(argc, argv, "v", options, &optidx)) != -1) {
		switch(opt) {
			case 'i':
				min = strtol(optarg, NULL, 10);
				break;
			case 'a':
				max = strtol(optarg, NULL, 10);
				break;
			case 'd':
				fdict = strdup(optarg);
				break;
			case 'v':
				VERBOSE = true;
				break;
			default:
				Usage(argv[0]);
				break;
		}
	}

	if (optind + 1 != argc)
		Usage(argv[0]);

	words = strdup(argv[optind]);
	word_t* ws = word_set_init(words);
	if (ws == NULL) {
		perror("Failed to parse word list!\n");
		Usage(argv[0]);
	}

	if (fdict == NULL)
		fdict = "/usr/share/dict/american-english";

	if (!read_dict(fdict)) {
		perror("Cannot open dictionary!\n");
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

	int i;
	for (i = min; i <= max; i++) { 
		printf("Results of word length %d:\n", i);
		produce_combine_word(ws, i);
	}

	return EXIT_SUCCESS;
}
