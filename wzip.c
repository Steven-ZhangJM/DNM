#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define Pthread_create(thread, attr, start_routine, arg)                       \
	  assert(pthread_create(thread, attr, start_routine, arg) == 0);
#define Pthread_join(thread, value_ptr)                                        \
	  assert(pthread_join(thread, value_ptr) == 0);

int get_nprocs(void);
static int nprocs;

typedef struct zip_arg_t {
	char *ptr;
        int offset;
	int length;
} zip_arg_t;

typedef struct zip_ret_t {
	char   val[10000000];
        int    num[10000000];
	int    n;
} zip_ret_t;

void *thread_zip(void *args) {
	zip_ret_t* ret = malloc(sizeof(zip_ret_t));
	ret->n = 0;
	char *ptr = ((zip_arg_t *)args)->ptr;
	int offset = ((zip_arg_t *)args)->offset;
	int length = ((zip_arg_t *)args)->length;
	char ch = *(ptr + offset);
	int numb = 0;
	for (int i = 0; i < length; i++) {
		if (*(ptr + offset + i) == ch) {
			numb++;
		} else {
			ret->val[ret->n] = ch;
			ret->num[ret->n] = numb;					
	 		ret->n ++;									
			numb = 1;		
	 		ch = *(ptr + offset + i);			
			assert(ret->n < 10000000);							
		}
	}
	

		ret->val[ret->n] = ch;
		ret->num[ret->n] = numb;
	
	return (void*)ret;
}

void merge(char* ch, int* numb, zip_ret_t* rets[], int nthread, bool first) {
	if (first) {
		(*ch) = rets[0]->val[0];
		(*numb) = 0; 	
	}
	        
	for (int i=0; i < nthread; i++) {
		for (int j = 0; j <= rets[i]->n; j++) {
			if ((*ch) == rets[i]->val[j]) {
				(*numb) += rets[i]->num[j]; 
			} else {
				if(*ch != '\0') {

					fwrite(numb, sizeof(int), 1 ,stdout);	
					fwrite(ch, sizeof(char), 1 ,stdout);
				}
				(*ch) = rets[i]->val[j];
				(*numb) = rets[i]->num[j];
			}						            
		}
	}
}

void process(char* ch, int* numb, char* path, bool first) {
	int fp = open(path, O_RDONLY);
	if (fp < 0) {
		printf("pzip: cannot open file\n");
		exit(1);
	}

	struct stat sb;
	if(fstat(fp, &sb) < 0) {
		close(fp);
		printf("fstat fails\n");
		exit(1);
	}

	int file_size = sb.st_size;
	int offset = 0;
	char *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fp, offset);
	if (map == MAP_FAILED) {
		close(fp);
		printf("mmap fails\n");
		exit(1);
	}

	int len = 0;
    	pthread_t threads[nprocs];
    	zip_arg_t args[nprocs];

    	for (int i = 0; i < nprocs; i++) {
	        len = ((i+1) * file_size / nprocs) - offset;
    		args[i] = (struct zip_arg_t){map, offset, len};
		Pthread_create(&threads[i], NULL, thread_zip, &args[i]);
	        offset += len;
        }

	zip_ret_t* rets[nprocs];
    	for (int i = 0; i < nprocs; i++) {
	        Pthread_join(threads[i], (void*)(&rets[i]));
	}

    	merge(ch, numb, rets, nprocs, first);

    	for (int i = 0; i < nprocs; i++) {
	        free(rets[i]);
        }

    
    	if (munmap(map, file_size) != 0) {
		printf("munmap fails\n");
    		exit(1);
	}	    
	close(fp);
}

int main(int argc, char *argv[]) {
	if(argc < 2) {
		printf("pzip: file1 [file2 ...]\n");
	        exit(1);	
	}

	nprocs = get_nprocs();
	char ch;
	int numb = 0;
	process(&ch, &numb, argv[1], true);
	
        for (int i=2; i < argc; i++) {
		process(&ch, &numb, argv[i], false);
	}
	fwrite(&numb, sizeof(int), 1 ,stdout);
	fwrite(&ch, sizeof(char), 1 ,stdout);
	return 0;
}
