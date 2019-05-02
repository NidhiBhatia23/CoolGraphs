#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

//Timing routine for reading the time stamp counter
static __inline__ unsigned long long rdtsc(void) {
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

//Function Description : Rounds size up to next multiple of n
static int round_up(int size, int n)
{
    return (n * ((size + (n-1)) / n));
}

//Function Description : kernel for determining modularity
//@args
//$argv1 - pointer to the start of 80 elements in array x of random floats
//$argv2 - pointer to the start of 80 elements in array y of random floats
//$argv3 - variable storing modularity.
//Function return - function returns the updated value of 'modularity'
void kernel(float *x, float *y, float* z) {
	__m256 tmp = _mm256_set_ps(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
	
	__m256 y0 = _mm256_load_ps(y+0); 
	__m256 y1 = _mm256_load_ps(y+8); 
	y0 = _mm256_mul_ps(y0, y0);
	y1 = _mm256_mul_ps(y1, y1);
	__m256 tmp1 = _mm256_load_ps(x+0);
	__m256 tmp2 = _mm256_load_ps(x+8);
	y0 = _mm256_fmadd_ps(tmp, y0, tmp1);
	y1 = _mm256_fmadd_ps(tmp, y1, tmp2);
    y0 = _mm256_add_ps(y0, y1);

	__m256 y2 = _mm256_load_ps(y+16); 
	__m256 y3 = _mm256_load_ps(y+24); 
	y2 = _mm256_mul_ps(y2, y2);
	y3 = _mm256_mul_ps(y3, y3);
	tmp1 = _mm256_load_ps(x+16);
	tmp2 = _mm256_load_ps(x+24);
	y2 = _mm256_fmadd_ps(tmp, y2, tmp1);
	y3 = _mm256_fmadd_ps(tmp, y3, tmp2);
    y2 = _mm256_add_ps(y2, y3);

	__m256 y4 = _mm256_load_ps(y+32); 
	__m256 y5 = _mm256_load_ps(y+40); 
	y4 = _mm256_mul_ps(y4, y4);
	y5 = _mm256_mul_ps(y5, y5);
	tmp1 = _mm256_load_ps(x+32);
	tmp2 = _mm256_load_ps(x+40);
	y4 = _mm256_fmadd_ps(tmp, y4, tmp1);
	y5 = _mm256_fmadd_ps(tmp, y5, tmp2);
    y4 = _mm256_add_ps(y4, y5);
    
    y0 = _mm256_add_ps(y0, y2);
    
	__m256 y6 = _mm256_load_ps(y+48); 
	__m256 y7 = _mm256_load_ps(y+56); 
	y6 = _mm256_mul_ps(y6, y6);
	y7 = _mm256_mul_ps(y7, y7);
	tmp1 = _mm256_load_ps(x+48);
	tmp2 = _mm256_load_ps(x+56);
	y6 = _mm256_fmadd_ps(tmp, y6, tmp1);
	y7 = _mm256_fmadd_ps(tmp, y7, tmp2);
    y6 = _mm256_add_ps(y6, y7);


	__m256 y8 = _mm256_load_ps(y+64); 
	__m256 y9 = _mm256_load_ps(y+72); 
	y8 = _mm256_mul_ps(y8, y8);
	y9 = _mm256_mul_ps(y9, y9);
	tmp1 = _mm256_load_ps(x+64);
	tmp2 = _mm256_load_ps(x+72);
	y8 = _mm256_fmadd_ps(tmp, y8, tmp1);
	y9 = _mm256_fmadd_ps(tmp, y9, tmp2);
    y8 = _mm256_add_ps(y8, y9);
    
    y1 = _mm256_add_ps(y4, y6);
    y0 = _mm256_add_ps(y0, y8);
    y0 = _mm256_add_ps(y0, y1);

    y2 = _mm256_permute2f128_ps(y0, y0, 1);
    y0 = _mm256_add_ps(y0, y2);

    y0 = _mm256_hadd_ps (y0, y0);

    y0 = _mm256_hadd_ps (y0, y0);

    _mm256_store_ps(z, y0);
	return;
}

//Function Description : main
int main(int argc, char* argv[])
{
	int num_of_mc = atoi(argv[1]);
	int runs = atoi(argv[2]);

	//Round up to multiples of 80 - because kernel can simulate 80 points
	//at a time
	int numOfIter = round_up(num_of_mc, 80);
	
	unsigned long long t0 = 0;
	unsigned long long t1 = 0;
	unsigned long long sum = 0;
    float modularity = 0.0;

	//allocating memory to store randomly generated values of x and y
	//There will be 'numOfIter' number of values generated 
	float *x, *y, *z;
	posix_memalign((void**)&x, 64, sizeof(float)*numOfIter);	
	posix_memalign((void**)&y, 64, sizeof(float)*numOfIter);	
	posix_memalign((void**)&z, 64, sizeof(float)*8);	
	
	//Pre-generating 'numOfIter' number of x and y random float numbers
	for (int i = 0; i < numOfIter; i++) {
        x[i] = (float)i * 0.5;
        y[i] = (float)i * (float)i * 0.008 * 0.008;
	}
	
	for (int i = 0; i < runs; i++) {	
	    modularity = 0.0;	
		//This loop runs the kernel for numOfIter/80 times
		//Recommended to run in multiples of 80		
		t0 = rdtsc();
		for (int j = 0; j < numOfIter; j+=80) {
            kernel(x+j, y+j, z);
            modularity += z[0];
		}
		t1 = rdtsc();
		sum += (t1 - t0);
	}
	//Calculate the number of cycles
	float cycles = (float)(sum)/(float)(runs);
    printf("Modularity: %f\n", modularity);
	float gflops = (2.4 * numOfIter * 3)/(cycles);
	printf("GFLOPS : %f\n", gflops);

	//De-allocating the previously allocated memory for storing random x and y floats
	free(x);
	free(y);
	
	return 0;
}
