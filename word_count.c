#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#ifdef _OPENMP
#include <omp.h>
#endif

// From the provided flt_val_sort.c file
static double timer() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return ((double) (tp.tv_sec) + 1e-6 * tp.tv_usec);
}


struct wc_pair {
	char 	*word;
	int 	count;
	struct 	wc_pair *next;
};

struct hashtable {
	struct wc_pair **table;
	int num_unique_words;
	int size;
};

struct wc_pair *init_wc_pair(char *new_word) {
	struct wc_pair *new_wc_pair = (struct wc_pair *) malloc(sizeof(struct wc_pair));
	new_wc_pair -> word = new_word;
	new_wc_pair -> count = 1;
	new_wc_pair -> next = NULL;

	return new_wc_pair;
}

struct hashtable *init_hashtable() {
	//TODO: SMARTER SIZE CHOICE
	int new_size = 701;

	int new_num_unique_words = 0;
	struct wc_pair **new_table = (struct wc_pair **) malloc(new_size * sizeof(struct wc_pair *));

	struct hashtable *new_hashtable = (struct hashtable *) malloc(sizeof(struct hashtable));
	new_hashtable -> table = new_table;
	new_hashtable -> num_unique_words = new_num_unique_words;
	new_hashtable -> size = new_size;

	return new_hashtable;
}

// TODO: DOCUMENT SOURCE
unsigned long hash(unsigned char *word) {
	unsigned long hash = 5381;
	int c;

	while (c = *word++) {
		hash = ((hash << 5) + hash) + c;
	}

	return hash;
}

void insert_serial(struct hashtable *table, char *ins_word) {
	unsigned long hashed = hash(ins_word);
	int index = hashed % (table -> size);
	int isUpdated = 0;

	struct wc_pair *cur = table -> table[index];
	struct wc_pair *prev;
	if (!cur) {
		table -> table[index] = init_wc_pair(ins_word);
		table -> num_unique_words++;
	} else {
		while (cur && isUpdated == 0) {
			if (strcmp(cur -> word, ins_word) == 0) {
				(cur -> count)++;
				isUpdated = 1;
			} else {
				prev = cur;
				cur = cur -> next;
			}
		}
		
		if (isUpdated == 0) {
			prev -> next = init_wc_pair(ins_word);
			table -> num_unique_words++;
		}
	}
}

double wc_serial(struct hashtable *ht, char **str_loc_array, int num_strings) {
	double elt;
	elt = timer();

	for (int i = 0; i < num_strings; i++) {
		insert_serial(ht, str_loc_array[i]);
	}

	return timer() - elt;
}

void insert_parallel(struct hashtable *table, char *ins_word, omp_lock_t *locks) {
	unsigned long hashed = hash(ins_word);
	int index = hashed % (table -> size);
	int isUpdated = 0;

	struct wc_pair *prev;

	omp_set_lock(&(locks[index]));
	struct wc_pair *cur = table -> table[index];
	if (!cur) {
		table -> table[index] = init_wc_pair(ins_word);
#pragma omp atomic
		table -> num_unique_words++;
	} else {
		while (cur && isUpdated == 0) {
			if (strcmp(cur -> word, ins_word) == 0) {
				(cur -> count)++;
				isUpdated = 1;
			} else {
				prev = cur;
				cur = cur -> next;
			}
		}
		
		if (isUpdated == 0) {
			prev -> next = init_wc_pair(ins_word);
#pragma omp atomic
			table -> num_unique_words++;
		}
	}
	omp_unset_lock(&(locks[index]));
}

double wc_parallel(struct hashtable *ht, char **str_loc_array, int num_strings) {
	omp_lock_t locks[ht -> size];
	for (int i = 0; i < ht -> size; i++) {
		omp_init_lock(&(locks[i]));
	}

	double elt;
	elt = timer();

#pragma omp parallel for
	for (int i = 0; i < num_strings; i++) {
		insert_parallel(ht, str_loc_array[i], locks);
	}

	elt = timer() - elt;

	for (int i = 0; i < ht -> size; i++) {
		omp_destroy_lock(&(locks[i]));
	}

	return elt;
}

int main(int argc, char **argv) {

	if (argc != 4) {
		fprintf(stderr, "%s <input_file> <n> <algorithm>\n", argv[0]);
		fprintf(stderr, "    algorithm: 0 serial\n               1 parallel\n");

		exit(1);
	}

	char *filename = argv[1];
	int num_strings = atoi(argv[2]);
	int algorithm = atoi(argv[3]);

	FILE *fp;
	fp = fopen(filename, "rb");
	//CHECK ERROR

	fseek(fp, 0L, SEEK_END);
	long num_bytes = ftell(fp);
	rewind(fp);

	char *str_array = (char *) malloc(num_bytes * sizeof(char));
	fread(str_array, num_bytes, 1, fp);
	//CHECK ERROR

	fclose(fp);

	char **str_loc_array = (char **) malloc(num_strings * sizeof(char *));
	str_loc_array[0] = &str_array[0];

	int num_strings_actual = 0;
	for (int i = 0; i < num_bytes; i++) {
		if (str_array[i] == '\n') {
			str_array[i] = '\0';
			num_strings_actual++;

			if (i != num_bytes - 1) {
				str_loc_array[num_strings_actual] = &str_array[i+1];
			}
		}
	}
	assert(num_strings_actual == num_strings);

	//DO STUFF HERE
	struct hashtable *ht = init_hashtable();

	double elt;
	if (algorithm == 0) {
		elt = wc_serial(ht, str_loc_array, num_strings);
	} else if (algorithm == 1) {
		elt = wc_parallel(ht, str_loc_array, num_strings);
	}

	for (int i = 0; i < (ht -> size); i++) {
		if (ht -> table[i]) {
			struct wc_pair *cur = ht -> table[i];

			while (cur) {
				printf("(%s, %d)\n", cur -> word, cur -> count);
				cur = cur -> next;
			}
		}
	}
	printf("\n");

	printf("%d unique words\n", ht -> num_unique_words);
	printf("Time: %9.3lf ms.\n", elt*1e3);

	//END STUFF

	free(str_array);

	return 0;
}
