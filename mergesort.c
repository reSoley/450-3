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

int binary_search(float val, float *array, int left, int right) {
	int low = left;
	int high = left > right + 1 ? left : right + 1;
	int mid;

	while (low < high) {
		mid = (low + high) / 2;

		if (val <= array[mid]) {
			high = mid;
		} else {
			low = mid + 1;
		}
	}

	return high;
}

void merge(float *arr_in, int left_1, int right_1, int left_2, int right_2, float *arr_out, int left_out) {
	int temp_size;
	int temp_index;
	int size_1 = right_1 - left_1 + 1;
	int size_2 = right_2 - left_2 + 1;

	// We want to "merge" the smaller array into the larger array
	// so make sure array 2 is the smaller array
	if (size_1 < size_2) {
		temp_size = size_1;
		size_1 = size_2;
		size_2 = temp_size;

		temp_index = left_1;
		left_1 = left_2;
		left_2 = temp_index;

		temp_index = right_1;
		right_1 = right_2;
		right_2 = temp_index;
	}

	// Nothing to merge
	if (size_1 == 0) {
		return;
	}

	int mid_1 = (left_1 + right_1) / 2;
	int mid_2 = binary_search(arr_in[mid_1], arr_in, left_2, right_2);
	int mid_out = left_out + (mid_1 - left_1) + (mid_2 - left_2);
	arr_out[mid_out] = arr_in[mid_1];

	//merge(arr_in, arr_out, left_1, left_2, left_out, mid_1 - 1, mid_2 - 1);
	merge(arr_in, left_1, mid_1 - 1, left_2, mid_2 - 1, arr_out, left_out);
	merge(arr_in, mid_1 + 1, right_1, mid_2, right_2, arr_out, mid_out + 1);
	//merge(arr_in, arr_out, mid_1 + 1, mid_2, mid_out + 1, right_1, right_2);
}

void parallel_mergesort(float *arr_in, int left, int right, float *arr_out, int left_out) {
	int mid;
	int mid_out;
	int n = right - left + 1;
	float *temp;

	if (n == 1) {
		arr_out[left_out] = arr_in[left];
	} else {
		// Note that in the pseudocode, the sort relies on temp being
		//  [1..n], not [0..n-1]
		temp = (float *) malloc(sizeof(*temp) * n);
		//assert(temp != NULL);
		mid = (left + right) / 2;
		mid_out = mid - left;
//#pragma omp parallel sections
//		{
//#pragma omp section
			parallel_mergesort(arr_in, left, mid, temp, 0);
//#pragma omp section
			parallel_mergesort(arr_in, mid + 1, right, temp, mid_out + 1);
//		}

		//merge(A+low, A+mid, mid-low, high-mid, B+low);
		//merge(A, low, mid, low, mid, high);
		merge(temp, 0, mid_out, mid_out + 1, n, arr_out, left_out);
		free(temp);
		temp = NULL;
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

    parallel_mergesort(A, 0, n - 1, B, 0);

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
