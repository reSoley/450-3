#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include "qsort.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define TAIL_OPT_SIZE	64
#define inline_qs_cmpf(a,b) ((*a)<(*b))

int min_tail_size = 4;

// From the provided flt_val_sort.c file
static double timer() {
    
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return ((double) (tp.tv_sec) + 1e-6 * tp.tv_usec);
}

// From the provided flt_val_sort.c file
int gen_input(float *A, int n, int input_type) {
    int i;

    /* uniform random values */
    if (input_type == 0) {
        srand(123);
        for (i=0; i<n; i++) {
            A[i] = ((float) rand())/((float) RAND_MAX);
        }

    /* sorted values */    
    } else if (input_type == 1) {
        for (i=0; i<n; i++) {
            A[i] = (float) i;
        }

    /* almost sorted */    
    } else if (input_type == 2) {
        for (i=0; i<n; i++) {
            A[i] = (float) i;
        }

        /* do a few shuffles */
        int num_shuffles = (n/100) + 1;
        srand(1234);
        for (i=0; i<num_shuffles; i++) {
            int j = (rand() % n);
            int k = (rand() % n);

            /* swap A[j] and A[k] */
            float tmpval = A[j];
            A[j] = A[k];
            A[k] = tmpval;
        }

    /* array with single unique value */    
    } else if (input_type == 3) {
        for (i=0; i<n; i++) {
            A[i] = 1.0;
        }

    /* sorted in reverse */    
    } else {
        for (i=0; i<n; i++) {
            A[i] = (float) (n + 1.0 - i);
        }

    }

    return 0;
}

void insertion_sort(float *A, int left, int right) {
	int j;
	int temp;

	for (int i = left + 1; i < right; i++) {
		temp = A[i];
		j = i - 1;

		while (j >= left && A[j] > temp) {
			A[j + 1] = A[j];
			j--;
		}

		A[j + 1] = temp;
	}
}

void merge(float *A, int left, int middle, int right, float *B) {
	int i = left;
	int j = middle;

	for (int k = left; k < right; k++) {
		if (i < middle && (j >= right || A[i] <= A[j])) {
			B[k] = A[i];
			i++;
		} else {
			B[k] = A[j];
			j++;
		}
	}
}

void mergesortrec(float *B, int left, int right, float *A, int sort_type) {
	int middle;

	if (right - left <= min_tail_size) {
		insertion_sort(A, left, right);
	} else {
		middle = (right + left) / 2;
		if (sort_type == 1) {
#pragma omp task firstprivate(A, left, middle, B)
			{
			mergesortrec(A, left, middle, B, sort_type);
			}
#pragma omp task firstprivate(A, middle, right, B)
			{
			mergesortrec(A, middle, right, B, sort_type);
			}
		} else {
			mergesortrec(A, left, middle, B, sort_type);
			mergesortrec(A, middle, right, B, sort_type);
		}
		merge(B, left, middle, right, A);
	}
}

// n is exclusive
void mergesort(float *A, float *B, int n, int sort_type) {
	memcpy(B, A, n * sizeof(float));
	mergesortrec(B, 0, n, A, sort_type);
}

int main(int argc, char **argv) {

    if (argc < 3 || argc > 6) {
        fprintf(stderr, "%s <n> <input_type> <sort_type> (tail_min_size)\n", argv[0]);
        fprintf(stderr, "input_type 0: uniform random\n");
        fprintf(stderr, "           1: already sorted\n");
        fprintf(stderr, "           2: almost sorted\n");
        fprintf(stderr, "           3: single unique value\n");
        fprintf(stderr, "           4: sorted in reverse\n");
		fprintf(stderr, "sort_type  0: sequential merge sort\n");
		fprintf(stderr,	"           1: parallel merge sort\n");
		fprintf(stderr, "           2: quick sort\n");
        exit(1);
    }

    int n;

    n = atoi(argv[1]);

    assert(n > 0);
    assert(n <= 1000000000);

    float *A;
    A = (float *) malloc(n * sizeof(float));
    assert(A != 0);

    float *B;
    B = (float *) malloc(n * sizeof(float));
	assert(B != 0);

    int input_type = atoi(argv[2]);
    assert(input_type >= 0);
    assert(input_type <= 4);

	int sort_type = 0;
	if (argc > 3) {
		sort_type = atoi(argv[3]);
		assert(sort_type >= 0);
		assert(sort_type < 3);
	}

	if (argc == 5) {
		min_tail_size = atoi(argv[4]);
		assert(min_tail_size >= 0);
	}

    gen_input(A, n, input_type);

    double elt;
    elt = timer();

    //parallel_mergesort(A, 0, n - 1, B, 0);
	if (sort_type < 2) {
#pragma omp parallel
		mergesort(A, B, n, sort_type);
	}
    elt = timer() - elt;

    free(A);
    free(B);

    fprintf(stderr, "Time: %9.3lf ms.\n", elt*1e3);
    fprintf(stderr, "Sort rate: %6.3lf MB/s\n", 4.0*n/(elt*1e6));

    return 0;
}
