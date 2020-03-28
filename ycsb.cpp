#include "CoCuckoo.h"
// #include "OriginalCuckoo.h"
#include <iostream>
#include <stdio.h>

using namespace std;

#define KEY_LEN 16                        // The maximum length of a key
#define VALUE_LEN 15                      // The maximum length of a value
#define READ_WRITE_NUM 100000             // The total number of read and write operations in the workload

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

void printTable(CocuckooHashTable &table);

void ycsb_thread_run(void* arg){
    sub_thread* subthread = (sub_thread *)arg;
    int i = 0;
    printf("Thread %d is opened\n", subthread->id);
    string value = "insert by thread " + to_string(subthread->id) ;
    for(; i < READ_WRITE_NUM/subthread->table->thread_num; i++){
        if( subthread->run_queue[i].operation == 1){
            if (cocuckooInsert(*(subthread->table), subthread->run_queue[i].key, value)==0){   
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
    bool readFromFile = false;

    CocuckooHashTable &table = *cocuckooInit();
    table.thread_num = thread_num;
    uint64_t inserted = 0, queried = 0, t = 0;
    uint64_t key = 0;

	FILE *ycsb, *ycsb_read;
	char *buf = NULL;
	size_t len = 0;
    struct timespec start, finish;
    double single_time;

    thread_queue* run_queue[thread_num];
    int move[thread_num];
    int operation_num = 0;	
    for(t = 0; t < thread_num; t ++){
        run_queue[t] =  (thread_queue *)calloc(READ_WRITE_NUM/thread_num, sizeof(thread_queue));
        move[t] = 0;
    }


    if (readFromFile)
    {
        if((ycsb = fopen("./workloads/rw-50-50-load.txt","r")) == NULL)
        {
            perror("fail to read");
        }

        printf("Load phase begins \n");
        while(getline(&buf,&len,ycsb) != -1){
            if(strncmp(buf, "INSERT", 6) == 0){
                string str = buf + 7;
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
        printf("Load phase finishes: %lu items are inserted \n", inserted);

        if((ycsb_read = fopen("./workloads/rw-50-50-run.txt","r")) == NULL)
        {
            perror("fail to read");
        }
	
        while(getline(&buf,&len,ycsb_read) != -1){
            if(strncmp(buf, "INSERT", 6) == 0){
                string str = buf + 7;
                run_queue[operation_num%thread_num][move[operation_num%thread_num]].key = str;
                run_queue[operation_num%thread_num][move[operation_num%thread_num]].operation = 1;
                move[operation_num%thread_num] ++;
            }
            else if(strncmp(buf, "READ", 4) == 0){
                string str = buf + 5;
                run_queue[operation_num%thread_num][move[operation_num%thread_num]].key = str;
                run_queue[operation_num%thread_num][move[operation_num%thread_num]].operation = 0;
                move[operation_num%thread_num] ++;
            }
            operation_num ++;
        }
        fclose(ycsb_read);
    }
    else {
        //insert into CoCuckoo
        for (int i = 0; i < READ_WRITE_NUM; i++)
        {
            string s = "test";
            s += to_string(i);
            run_queue[operation_num%thread_num][move[operation_num%thread_num]].key = s;
            run_queue[operation_num%thread_num][move[operation_num%thread_num]].operation = 1;
            move[operation_num%thread_num] ++;
            operation_num ++;
        }
    }

	sub_thread* THREADS = (sub_thread*)malloc(sizeof(sub_thread)*thread_num);
    inserted = 0;
	
	printf("Run phase begins:\n");
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
    printf("Run phase finishes: %lu/%lu items are inserted/searched\n", inserted, operation_num - inserted);
    printf("Run phase throughput: %f operations per second \n", READ_WRITE_NUM/single_time);	
    
    //check
    // printTable(table);
    for (int i = 0; i < inserted; i++)
    {
        string s = "test";
        s += to_string(i);
        auto result = cocuckooQuery(table, s);
        if (result == NULL) {
            printf("\"%s\" not exist\n", s.c_str());
        }
    }
    
    return 0;
}

void printTable(CocuckooHashTable &table) {
    int i = 0;
    while (i<table.size)
    {
        if (i % 8 == 0){
            putchar('\n');
        }
        if (table.data[i].occupied)
        {
            printf("%3d: %10.10s |", i, table.data[i].key.c_str());
        }
        else {
            printf("%3d: %10s |", i, "");
        }
        i++;
    }
    putchar('\n');
}
