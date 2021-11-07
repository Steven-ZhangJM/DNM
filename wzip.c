#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int get_nprocs(void);
static int nprocs;

void process(char* ch, int* numb, char* path, bool first) {
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		printf("pzip: cannot open file\n");
		exit(1);
	}
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int index = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		if (first && (index == 0)) {
			(*ch) = line[0];
		}
		for (int j = 0; j < read; j++) {
			if (line[j] == (*ch)) {
				(*numb)++;
			} else {										
				fwrite(numb, sizeof(int), 1 ,stdout);	
				fwrite(ch, sizeof(char), 1 ,stdout);		
				(*numb) = 1;					
		    		(*ch) = line[j];						
	  		}
		}
		index ++; 
	}
	fclose(fp);
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
