#include <stdio.h>
#include <stdlib.h>
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

int binary_search(float val, float *array, int left,  int size) {
	int index = 0;
	int split = size / 2;

	if (size == 0) {
		return size;
	}

	if (val == array[split] && (split == 0 || val != array[split - 1])) {
		index = split;
	} else if (val <= array[split]) {
		if (split == 0 || val > array[split - 1]) {
			index = left + split;
		} else {
			index = binary_search(val, array, left, split);
		}
	} else if (val > array[split]) {
		if (split == size - 1 || val < array[split + 1]) {
			index = left + split + 1;
		} else {
			index = binary_search(val, array + split + 1, split + 1, size - (split + 1));
		}
	}

	return index;
}

void merge(float *arr_1, float *arr_2, int size_1, int size_2, float *arr_out) {
	float *temp_arr;
	int temp_size;

	// TODO Check if this is actually a problem, but I think switching two
	// pointers in the same array will screw things up really badly

	// We want to "merge" the smaller array into the larger array
	// so make sure array 2 is the smaller array
	if (size_1 < size_2) {
		temp_arr = arr_1;
		arr_1 = arr_2;
		arr_2 = temp_arr;

		temp_size = size_1;
		size_1 = size_2;
		size_2 = temp_size;
	}

	// Nothing to merge
	if (size_1 <= 0) {
		return;
	}

	int mid_1 = size_1 / 2;
	int mid_2 = binary_search(arr_1[mid_1], arr_2, 0, size_2);
	int mid_out = mid_1 + mid_2;
	arr_out[mid_out] = arr_1[mid_1];

#pragma omp parallel sections
{
#pragma omp section
	merge(arr_1, arr_2, mid_1, mid_2, arr_out);
#pragma omp section
	merge(arr_1 + mid_1 + 1, arr_2 + mid_2, size_1 - (mid_1 + 1), size_2 - mid_2, arr_out + mid_out + 1);
}
}

void parallel_mergesort(float *A, float *B, int low, int high) {
	int mid;

	if (low + 1 < high) {
		mid = (low + high + 1) / 2;		// Add 1 so that left split is larger
#pragma omp parallel sections
		{
#pragma omp section
			parallel_mergesort(A, B, low, mid);
#pragma omp section
			parallel_mergesort(A, B, mid, high);
		}

		merge(A+low, A+mid, mid-low, high-mid, B+low);
		//fprintf(stderr, "low: %d mid: %d high: %d\n", low, mid, high);
		//fprintf(stderr, "half_1_size: %d half_2_size: %d\n", mid-low, high-mid);
	}
}

int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "%s <n> <input_type>\n", argv[0]);
        fprintf(stderr, "input_type 0: uniform random\n");
        fprintf(stderr, "           1: already sorted\n");
        fprintf(stderr, "           2: almost sorted\n");
        fprintf(stderr, "           3: single unique value\n");
        fprintf(stderr, "           4: sorted in reverse\n");
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

    gen_input(A, n, input_type);
	for (int i = 0; i < n - 1; i++) {
		printf("%f, ", A[i]);
	}
	printf("%f\n", A[n - 1]);

    double elt;
    elt = timer();

    parallel_mergesort(A, B, 0, n);

    elt = timer() - elt;

	for (int i = 0; i < n - 1; i++) {
		printf("%f, ", B[i]);
	}
	printf("%f\n", B[n - 1]);

    free(A);
    free(B);

    fprintf(stderr, "Time: %9.3lf ms.\n", elt*1e3);
    fprintf(stderr, "Sort rate: %6.3lf MB/s\n", 4.0*n/(elt*1e6));

    return 0;
}
