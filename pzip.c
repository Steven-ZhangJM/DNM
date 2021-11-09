#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/stat.h> 
#include <sys/sysinfo.h>
#include <unistd.h>

int n_procs; 
int page_size; 
int numb_file; 
int flag = 0;
int numb_pages; 
int head=0; 
int tail=0; 
#define q_capacity 10
int queue_size=0; 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER, filelock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER, fill = PTHREAD_COND_INITIALIZER;
int* page_file;

struct zip {
	char* ch;
	int* num;
	int n;
}*zip;

struct buffer {
    char* address; 
    int file_number; 
    int page_number; 
    int last_size; 
}buf[q_capacity];

struct fd{
	char* addr;
	int size;
}*files;
 
void put(struct buffer b){
  	buf[head] = b; 
  	head = (head + 1) % q_capacity;
  	queue_size++;
}

struct buffer get(){
  	struct buffer b = buf[tail]; 
	tail = (tail + 1) % q_capacity;
  	queue_size--;
  	return b;
}

void* producer(void *arg){
	char** paths = (char **)arg;
	struct stat st;
	char* map; 
	int fp;

	for(int i = 0;i < numb_file;i++){
		fp = open(paths[i], O_RDONLY);
		int pages_in_file=0; 
		int last_size=0; 
		if(fp == -1){ 
			continue;
		}		
		if(fstat(fp,&st) < 0){ 
			close(fp);
			printf("Error: pzip can't access file stats");
			exit(1);
		}
        	if(st.st_size==0){
               		continue;
        	}
		pages_in_file = (st.st_size/page_size);
		if(((double)st.st_size/page_size) > pages_in_file){ 
			pages_in_file += 1;
			last_size = st.st_size % page_size;
		} else{ 
			last_size = page_size;
		}
		numb_pages += pages_in_file;
		page_file[i] = pages_in_file;

		map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fp, 0); 															  
		if (map == MAP_FAILED) { 
			close(fp);
			printf("mmapp fails\n");
			exit(1);
    		}	

		for(int j = 0; j < pages_in_file; j++){
			pthread_mutex_lock(&lock);
			while(queue_size == q_capacity){
			    	pthread_cond_broadcast(&fill);
				pthread_cond_wait(&empty,&lock); 
			}
			pthread_mutex_unlock(&lock);
			struct buffer temp;
			if(j == pages_in_file-1){ 
				temp.last_size = last_size;
			} else {
				temp.last_size = page_size;
			}
			temp.address = map;
			temp.page_number = j;
			temp.file_number = i;
			map += page_size; 
			pthread_mutex_lock(&lock);
			put(temp);
			pthread_mutex_unlock(&lock);
			pthread_cond_signal(&fill);
		}
		close(fp);
	}
	flag = 1; 
	pthread_cond_broadcast(&fill); 
	return 0;
}

struct zip comp_file(struct buffer temp){
	struct zip compressed;
	compressed.num = malloc(temp.last_size * sizeof(int));
	char* string = malloc(temp.last_size);
	int index = 0;
	for(int i = 0; i < temp.last_size; i++){
		if (temp.address[i] == '\0'){
			continue;
		}
		string[index] = temp.address[i];
		compressed.num[index] = 1;
		while(i+1 < temp.last_size && temp.address[i] == temp.address[i+1]){
			compressed.num[index]++;
			i++;
		}
		index++;
	}
	compressed.n = index;
	compressed.ch = realloc(string, index);
	return compressed;
}

void *consumer(){
	do{
		pthread_mutex_lock(&lock);
		while(queue_size == 0 && flag == 0){
		    	pthread_cond_signal(&empty);
			pthread_cond_wait(&fill,&lock); 
		}
		if(flag == 1 && queue_size==0){ 
			pthread_mutex_unlock(&lock);
			return NULL;
		}
		struct buffer temp = get();
		if(flag == 0){
		    pthread_cond_signal(&empty);
		}	
		pthread_mutex_unlock(&lock);
		int position = temp.page_number;
		for(int i = 0; i < temp.file_number; i++){
			position += page_file[i];
		}
		zip[position] = comp_file(temp);
	} while(!(flag ==1 && queue_size == 0));
	return NULL;
}

void output(){
	char* res = malloc(numb_pages * page_size * (sizeof(int)+sizeof(char)));
    	char* init = res; 
	for(int i = 0; i < numb_pages; i++){
		if(i < numb_pages-1){
			if(zip[i].ch[zip[i].n-1] == zip[i+1].ch[0]){ 
				zip[i+1].num[0] += zip[i].num[zip[i].n-1];
				zip[i].n--;
			}
		}
		
		for(int j = 0; j < zip[i].n; j++){
			int num = zip[i].num[j];
			char character = zip[i].ch[j];
			*((int*) res) = num;
			res += sizeof(int);
			*((char*) res) = character;
            		res += sizeof(char);
		}
	}
	fwrite(init, res-init, 1,stdout);
}

int main(int argc, char* argv[]){
	if(argc<2){
		printf("pzip: file1 [file2 ...]\n");
		exit(1);
	}

	page_size = 10000000;
	numb_file = argc - 1; 
	n_procs = get_nprocs(); 
	page_file = malloc(sizeof(int)*numb_file); 

    	zip = malloc(sizeof(struct zip)* 512000*2); 
	pthread_t prod, cons [n_procs];
	pthread_create(&prod, NULL, producer, argv+1); 

	for (int i = 0; i < n_procs; i++) {
        	pthread_create(&cons[i], NULL, consumer, NULL);
    	}

	for (int i = 0; i < n_procs; i++) {
        	pthread_join(cons[i], NULL);
    	}
    	pthread_join(prod,NULL);
	output();
	return 0;
}

