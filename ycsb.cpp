#include "CoCuckoo.h"
#include <iostream>
#include <stdio.h>

using namespace std;

#define KEY_LEN 16                        // The maximum length of a key
#define VALUE_LEN 15                      // The maximum length of a value
#define READ_WRITE_NUM 350000             // The total number of read and write operations in the workload

typedef struct thread_queue{
    string key;
    uint8_t operation;                    // 0: read, 1: insert;
} thread_queue;

typedef struct sub_thread{
    pthread_t thread;
    uint32_t id;
    uint64_t inserted;
    CocuckooHashTable *table;
    thread_queue* run_queue;
} sub_thread;

void ycsb_thread_run(void* arg){
    sub_thread* subthread = (sub_thread *)arg;
    int i = 0;
    printf("Thread %d is opened\n", subthread->id);
    for(; i < READ_WRITE_NUM/subthread->table->thread_num; i++){
        if( subthread->run_queue[i].operation == 1){
            if (cocuckooInsert(*(subthread->table), subthread->run_queue[i].key, subthread->run_queue[i].key)==0){   
                subthread->inserted ++;
            }
        }else{
            if(cocuckooQuery(*(subthread->table), subthread->run_queue[i].key)!=NULL)
                // Get value
                ;
        }
    }
    pthread_exit(NULL);
}

/*  YCSB test:
    This is a simple test example to test the concurrent level hashing 
    using a YCSB workload with 50%/50% search/insertion ratio
*/
int main(int argc, char* argv[])
{        
    int thread_num = 4;             // INPUT: the number of threads

    CocuckooHashTable &table = *cocuckooInit();
    table.thread_num = thread_num;
    uint64_t inserted = 0, queried = 0, t = 0;
    uint64_t key = 0;

	FILE *ycsb, *ycsb_read;
	char *buf = NULL;
	size_t len = 0;
    struct timespec start, finish;
    double single_time;

    if((ycsb = fopen("./workloads/rw-50-50-load.txt","r")) == NULL)
    {
        perror("fail to read");
    }

    printf("Load phase begins \n");
	while(getline(&buf,&len,ycsb) != -1){
		if(strncmp(buf, "INSERT", 6) == 0){
			memcpy(&key, buf+7, KEY_LEN-1);
            string str = to_string(key);
			if (cocuckooInsert(table, str, str)==0)                      
			{
				inserted ++;
			}
			else{
				break;
			}
		}
	}
	fclose(ycsb);
    printf("Load phase finishes: %llu items are inserted \n", inserted);

    if((ycsb_read = fopen("./workloads/rw-50-50-run.txt","r")) == NULL)
    {
        perror("fail to read");
    }

    thread_queue* run_queue[thread_num];
    int move[thread_num];
    for(t = 0; t < thread_num; t ++){
        run_queue[t] =  (thread_queue *)calloc(READ_WRITE_NUM/thread_num, sizeof(thread_queue));
        move[t] = 0;
    }

	int operation_num = 0;		
	while(getline(&buf,&len,ycsb_read) != -1){
		if(strncmp(buf, "INSERT", 6) == 0){
            memcpy(&key, buf+7, KEY_LEN-1);
            string str = to_string(key);
            run_queue[operation_num%thread_num][move[operation_num%thread_num]].key = str;
			run_queue[operation_num%thread_num][move[operation_num%thread_num]].operation = 1;
			move[operation_num%thread_num] ++;
		}
		else if(strncmp(buf, "READ", 4) == 0){
            memcpy(&key, buf+5, KEY_LEN-1);
            string str = to_string(key);
            run_queue[operation_num%thread_num][move[operation_num%thread_num]].key = str;
			run_queue[operation_num%thread_num][move[operation_num%thread_num]].operation = 0;
			move[operation_num%thread_num] ++;
		}
		operation_num ++;
	}
	fclose(ycsb_read);

	sub_thread* THREADS = (sub_thread*)malloc(sizeof(sub_thread)*thread_num);
    inserted = 0;
	
	// printf("Run phase begins: SEARCH/INSERTION ratio is 50\%/50\% \n");
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for(t = 0; t < thread_num; t++){
        THREADS[t].id = t;
        THREADS[t].table = &table;
        THREADS[t].inserted = 0;
        THREADS[t].run_queue = run_queue[t];
        pthread_create(&THREADS[t].thread, NULL, (void* (*)(void*))ycsb_thread_run, &THREADS[t]);
    }

    for(t = 0; t < thread_num; t++){
        pthread_join(THREADS[t].thread, NULL);
    }

	clock_gettime(CLOCK_MONOTONIC, &finish);
	single_time = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    for(t = 0; t < thread_num; ++t){
        inserted +=  THREADS[t].inserted;
    }
    printf("Run phase finishes: %llu/%llu items are inserted/searched\n", operation_num - inserted, inserted);
    printf("Run phase throughput: %f operations per second \n", READ_WRITE_NUM/single_time);	
    
    return 0;
}

