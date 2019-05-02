#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

//timing routine for reading the time stamp counter
static __inline__ unsigned long long rdtsc(void) {
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static int round_up(int size, int n)
{
    return (n * ((size + (n-1)) / n));
}

float calculateMod(int num_of_iterations, float *x, float *y, float z) {
	for (int i=0; i<num_of_iterations; i++) {
		z += x[i] - y[i]*y[i];
    }
	return z;
}

int main(int argc, char* argv[])
{
	int num_of_mc = atoi(argv[1]);
	int runs = atoi(argv[2]);
	
	//Rounding up to multiples of 80 to match the optimized kernel accuracy
	int numOfIter = round_up(num_of_mc, 80);
	
	//Allocating space to pre-generate numOfIter random (x,y) between (0,0) and (1,1)
	float *x, *y;
        posix_memalign((void**)&x, 64, sizeof(float)*numOfIter);
        posix_memalign((void**)&y, 64, sizeof(float)*numOfIter);
	
	unsigned long long t0 = 0;
	unsigned long long t1 = 0;
	unsigned long long sum = 0;
	float modularity = 0.0;
	
	//Pre-generating and storing random (x,y)
	for (int i = 0; i <= numOfIter; i++) {
		x[i] = (float)i * 0.5;
        y[i] = (float)i * (float)i * 0.008 * 0.008;
	}
	
	//calculateMod is called here runs number of times
	for (int i = 0; i < runs; i++) {	
		t0 = rdtsc();
		modularity = calculateMod(numOfIter, x, y, modularity);
		t1 = rdtsc();
		sum += (t1 - t0);
	}
	float cycles = (float)(sum)/(float)(runs);
    printf("Modularity: %f\n", modularity/runs);
	//Empirical peak = (Base Freq x Num of FLOPs)/Recorded number of cycles 
	float gflops = 2.4 * numOfIter * 4 / cycles;
	printf("GFLOPS : %f\n", gflops);
	free(x);
	free(y);
	return 0;
}
