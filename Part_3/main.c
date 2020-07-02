//
//  main.c
//  Virtual Memory Part 3
//
//  Created by Muhammed Okumuş on 23.06.2020.
//  Copyright 2020 Muhammed Okumus. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>  
#include <string.h> 
#include <math.h>       
#include <time.h>       
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <pthread.h> 

#define NAME 32
#define N_THREADS 4
#define DEBUG 0
#define MAX_PATH 1024

/*============================================
=            Page Table Structure            =
============================================*/

typedef struct{
    unsigned int addr_virtual;          // VM address space
    int addr_physical;                  // RAM address space
    int modified;                       // Modified bit
    int present;                        // Present bit
    int referenced;                     // Referenced bit
    int owner;                          // Owner process ID
    int age;                            // Age bit
    struct timespec reference_time;   
    struct timespec load_time; 
}Entry;

typedef struct{
    int page_size;
    Entry* page_table;
}VirtualMemory;

/*==========================================
=            Statistics Structs            =
==========================================*/

typedef struct{
    char name[NAME];
    int owner;
    unsigned long long n_reads;
    unsigned long long n_writes;
    int n_misses;
    int n_replacements;
    int n_dpw;
    int n_dpr;
} Stats;

typedef struct{
    double delta;   // time taken in seconds
    unsigned long start;
    unsigned long end;
} Data;

Stats stats_bs = {0};
Stats stats_ms = {0};
Stats stats_qs = {0};
Stats stats_is = {0};
Stats stats_ch = {0};
Stats stats_other = {0};

Data data_bs;
Data data_ms;
Data data_qs;
Data data_is;




/*==============================
=            Macros            =
==============================*/

#define errExit(msg) do{ perror(msg); print_usage(); exit(EXIT_FAILURE); } while(0)
#define _errExit(msg) do{ perror(msg); exit(EXIT_FAILURE); } while(0)
#define debug(f_, ...) if(DEBUG == 1) printf((f_), ##__VA_ARGS__)

/*========================================
=            Global Constants            =
========================================*/

const char* PR_TYPES[] = {"NRU", "FIFO", "SC", "LRU"};
const char* AP_TYPES[] = {"global","local"};
const int PR_N =  sizeof(PR_TYPES) / sizeof(PR_TYPES[0]);
const int AP_N =  sizeof(AP_TYPES) / sizeof(AP_TYPES[0]);
clockid_t clk_id = CLOCK_MONOTONIC;

/*===================================
=            User Inputs            =
===================================*/
int frame_size  = 6,        
    num_physical = 10,
    num_virtual = 14,
    page_table_print_int = INT_MAX;

char page_replacement[8],
     alloc_policy[7],
     *disk_file_name= "diskFile.dat";


/*========================================
=            Global Variables            =
========================================*/

VirtualMemory VM;       // VM info & page table
int* memory;            // Physical memory
int* bitmap;            // Bitmap like int array to keep track of empty physical frames
FILE * fd;              // VM file
unsigned int n_words;   // # of integers in memory
int n_vframes;          // # of virtual frames
int n_pframes;          // # of physical frames
int n_entries;          // # of total frames
int f_size;             // frame size f_size = (2^N)
int m_size;             // physical memory size

unsigned long long total_mem_access = 0;
pthread_mutex_t mutex_access  = PTHREAD_MUTEX_INITIALIZER;
int exit_requested = 0;

/*===========================================
=            Function Prototypes            =
===========================================*/

// Misc Functions
void print_usage();
int compare_time(struct timespec t1, struct timespec t2);
Stats* whos_stats(char *tName);
void print_stats(Stats s);

// Debug Functions
void print_entry(Entry e);

// Error Checking Functions
int pr_validity(char* type);
int ap_validity(char* type);

// Integer Array Utility Functions
void print_array(int* arr, int n);

// VM Functions
void initilize_vm();
void print_pt();
int to_addr_space(unsigned int i);
int find_free_addr();

// Get/Set Functions
void set(unsigned int index, int value, char * tName);
int get(unsigned int index, char * tName);

// Page Replacement Functions
int algorithm(int owner);
int NRU(int owner);
int FIFO(int owner);
int SC(int owner);
int LRU(int owner);
int WSClock(int owner);

// Clock Interrupt Routines
void reset_r_bit();

// Sorting
void print_disk(int s, int e);
int is_sorted(int s, int e);
void bubble_sort(int s, int e, char* c);
void merge(int start, int mid, int end, char* c);
void merge_sort(int left, int right, char* c);
void quick_sort(int low, int high, char* c);
int partition(int low, int high, char* c);
void swap(int i, int j, char* c);
void index_sort(int s, int e, char* c);

// Threads
void *thread_bubble_sort(void *arg);
void *thread_merge_sort(void *arg);
void *thread_quick_sort(void *arg);
void *thread_index_sort(void *arg);
void *thread_clock_interrupt(void *arg);

int main(int argc, char* argv[]){
    pthread_t thread_ids[N_THREADS];

    srand(1000); // Rand seed requested in the pdf

    /*=======================================================
    =            Initilize Calculated Properties            =
    =======================================================*/
    n_words = pow(2,num_virtual) * pow(2,frame_size);
    n_pframes = pow(2, num_physical);
    n_vframes = pow(2, num_virtual);
    n_entries = n_vframes + n_pframes;
    f_size = pow(2, frame_size);
    m_size = n_pframes * f_size;


    int initial_frame_size = frame_size;
    int initial_num_virtual = num_virtual;
    int initial_num_physical = num_physical;


    unsigned int initial_words = n_words;
    int initial_pframes = n_pframes;
    int initial_vframes = n_vframes;
    int initial_entries = n_entries;
    int initial_f_size = f_size;
    int initial_m_size = m_size;


    /*===================================================
    =            Virtual Memory Initlization            =
    ===================================================*/
    // Open VM file
    fd = fopen(disk_file_name, "w+");
    if (fd == NULL) errExit("Error opening file @initilize_vm");

    // Fill VM file with random integers
    initilize_vm(frame_size, num_virtual);

    // Allacote page table
    VM.page_table = calloc(n_entries, sizeof(Entry));
    // Initlize page table
    for(int i = 0; i < n_entries; i++){
        VM.page_table[i].addr_virtual = i * f_size;
        VM.page_table[i].addr_physical = -1;
    }

    // Allocate physical memory for simulation
    memory = calloc(m_size, sizeof(int));
    // Allocate swap space(backing store)
    bitmap = calloc(n_pframes, sizeof(int));

    /*=====  End of Virtual Memory Initlization  ======*/

    data_bs.start = 0;
    data_bs.end =  n_words / pow(2,8);

    data_ms.start = n_words / 4;
    data_ms.end = n_words / 4 * 2;

    data_qs.start = n_words / 4 * 2;
    data_qs.end = n_words / 4 * 3;

    data_is.start = n_words / 4 * 3;
    data_is.end = n_words;


    for(int pr = 0; pr < PR_N; pr++){
        memset(page_replacement, '\0', sizeof(page_replacement));
        strcpy(page_replacement, PR_TYPES[pr]);

        for(int ap = 0; ap < AP_N; ap++){
            memset(alloc_policy, '\0', sizeof(alloc_policy));
            strcpy(alloc_policy, AP_TYPES[ap]);



            // Reset to initial values for next test group


            frame_size = initial_frame_size;
            num_virtual = initial_num_virtual;
            num_physical = initial_num_physical;

            n_words = initial_words;
            n_pframes = initial_pframes;
            n_vframes = initial_vframes;
            n_entries = initial_entries;
            f_size = initial_f_size;
            m_size = initial_m_size;


            printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            printf("Testing with %s PR and %s AP\n", page_replacement,alloc_policy);

            for(int i = 1; i <= 9; i++){

                    exit_requested = 0;

                    printf("\n**********************TEST %d***************************\n",i);


                    printf("==============================\n");
                    printf("Frame size: %.2fKB\n", f_size/pow(2,10));
                    printf("# entries: %d\n", n_entries);
                    printf("# words: %d\n", n_words);
                    printf("RAM: %.0f KB\n", m_size/pow(2,10));
                    printf("VM : %.0f KB\n", n_words/pow(2,10));
                    printf("==============================\n");

                    printf("Bubble Sort[%lu, %lu] // Reduced range for faster testing\n",data_bs.start, data_bs.end);
                    printf("Merge Sort[%lu, %lu]\n",data_ms.start, data_ms.end);
                    printf("Quick Sort[%lu, %lu]\n",data_qs.start, data_qs.end);
                    printf("Index Sort[%lu, %lu]\n",data_is.start, data_is.end);
                    printf("===============================\n");


                    pthread_create(&thread_ids[0], NULL, thread_bubble_sort, NULL); 
                    pthread_create(&thread_ids[1], NULL, thread_quick_sort, NULL); 
                    pthread_create(&thread_ids[2], NULL, thread_merge_sort, NULL); 
                    pthread_create(&thread_ids[3], NULL, thread_index_sort, NULL); 


                    pthread_t t_int;
                    pthread_create(&t_int, NULL, thread_clock_interrupt, NULL); 

                    for(int x = 0; x < N_THREADS; x++)
                        pthread_join(thread_ids[x], NULL);

                    exit_requested = 1;

                    pthread_join(t_int, NULL);

                    print_stats(stats_bs);
                    printf("Sort success: %s \n", (0 == is_sorted(data_bs.start,data_bs.end)) ? "yes" : "no");
                    printf("Took %f seconds to execute \n", data_bs.delta);
                    printf("===============================\n");
                    print_stats(stats_qs);
                    printf("Sort success: %s \n", (0 == is_sorted(data_qs.start,data_qs.end)) ? "yes" : "no");
                    printf("Took %f seconds to execute \n", data_qs.delta);
                    printf("===============================\n");
                    print_stats(stats_ms);
                    printf("Sort success: %s \n", (0 == is_sorted(data_ms.start,data_ms.end)) ? "yes" : "no");
                    printf("Took %f seconds to execute \n", data_ms.delta);
                    printf("===============================\n");
                    print_stats(stats_is);
                    printf("Sort success: %s \n", (0 == is_sorted(data_is.start,data_is.end)) ? "yes" : "no");
                    printf("Took %f seconds to execute \n", data_is.delta);
                    printf("===============================\n");
                    print_stats(stats_ch); 


                    /*=======================================================
                    =            Update Calculated Properties              =
                    =======================================================*/
                    frame_size++;
                    num_virtual--;
                    num_physical--;
                    n_words = pow(2,num_virtual) * pow(2,frame_size);
                    n_pframes = pow(2, num_physical);
                    n_vframes = pow(2, num_virtual);
                    n_entries = n_vframes + n_pframes;
                    f_size = pow(2, frame_size);
                    m_size = n_pframes * f_size;

                    data_bs.start = 0;
                    data_bs.end =  n_words / pow(2,8);

                    data_ms.start = n_words / 4;
                    data_ms.end = n_words / 4 * 2;

                    data_qs.start = n_words / 4 * 2;
                    data_qs.end = n_words / 4 * 3;

                    data_is.start = n_words / 4 * 3;
                    data_is.end = n_words;

                    // Re-init VM
                    initilize_vm(frame_size, num_virtual);


                    // Initlize page table
                    for(int j = 0; j < n_entries; j++){
                        VM.page_table[j].addr_virtual = j * f_size;
                        VM.page_table[j].addr_physical = -1;
                        VM.page_table[j].referenced = 0;
                        VM.page_table[j].present = 0;
                        VM.page_table[j].modified = 0;
                        VM.page_table[j].owner = 0;
                        VM.page_table[j].age = 0;
                        VM.page_table[j].reference_time.tv_sec = 0;
                        VM.page_table[j].reference_time.tv_nsec = 0;
                        VM.page_table[j].load_time.tv_sec = 0;
                        VM.page_table[j].load_time.tv_nsec = 0;
                    }
                    memset( &stats_bs, 0, sizeof(Stats) );
                    memset( &stats_ms, 0, sizeof(Stats) );
                    memset( &stats_qs, 0, sizeof(Stats) );
                    memset( &stats_is, 0, sizeof(Stats) );
                    memset( &stats_ch, 0, sizeof(Stats) );
                    memset( &stats_other, 0, sizeof(Stats) );
                    
                    for(int k = 0; k < n_pframes; k++)
                        bitmap[k] = 0;
                }
            printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        }
        

    }

    
    



    //print_disk(data_ms.start,data_ms.end);

    /*======================================
    =            Free Resources            =
    ======================================*/
    free(memory);
    free(bitmap);
    free(VM.page_table);
    fclose(fd);
}

/**
 *  Initlizes disk file that represent virtual memory.
 *  File is pointed by global variable fd, it must be open 
 *  before calling this function.
 */
void initilize_vm(){
    debug("Initilizing virtual memory with random integers...\n");
    clock_t t = clock();
    // Write to file
    fseek(fd, 0, SEEK_SET);
    for(int i = 0; i < n_words; i++){
        int r = (int) rand();
        fwrite(&r, sizeof(int), 1, fd); if(errno < 0) _errExit("fwrite @initilize_vm: 1");
    }

    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC; // calculate the elapsed time
    debug("Virtual memory initilized in %f seconds.\n", time_taken);
}

/**
 *  Returns a copy of the integer at index. If the integer is not
 *  in phscial memory, pulls page to the memory.
 *  If the memory is full, a page replacement algorithm is called.
 *  Write back is handled if necessary.
 */

Stats* whos_stats(char *tName){
    Stats *s;
    char c = tName[0];
    switch(c){
        case 'b': 
            s = &stats_bs; 
            snprintf(s->name, NAME,"%s", "Bubble Sort");
            s->owner = (ap_validity(alloc_policy) == 1) ? 1 : 0;
            break;
        case 'q':
            s = &stats_qs; 
            snprintf(s->name, NAME,"%s", "Quick Sort");
            s->owner = (ap_validity(alloc_policy) == 1) ? 2 : 0;
            break;
        case 'm': 
            s = &stats_ms;
            snprintf(s->name, NAME,"%s", "Merge Sort");
            s->owner = (ap_validity(alloc_policy) == 1) ? 3 : 0;
            break;
        case 'i': 
            s = &stats_is; 
            snprintf(s->name, NAME,"%s", "Index Sort");
            s->owner = (ap_validity(alloc_policy) == 1) ? 4 : 0;
            break;
        case 'c': 
            s = &stats_ch; 
            snprintf(s->name, NAME,"%s", "Check");
            s->owner = 0;
            break;
        default: 
            s = &stats_other; 
            snprintf(s->name, NAME,"%s", "Other");
            s->owner = 0;
            break;
    }

    return s;
}

void print_stats(Stats s){
    printf("#%d - %s stats\n"
            "# Reads: %llu\n"
            "# Writes: %llu\n"
            "# Misses: %d\n"
            "# Replacements: %d\n"
            "# DPW: %d\n"
            "# DPR: %d\n",
            s.owner, s.name, s.n_reads, s.n_writes, s.n_misses, s.n_replacements, s.n_dpw, s.n_dpr);
}

int get(unsigned int index, char * tName){
    pthread_mutex_lock(&mutex_access);
    int k, j, result = -1;
    Entry *e;
    Stats *s;

    if(index >= n_words) _errExit("Error: Index out of range @get");

    s = whos_stats(tName);
    s->n_reads++;
    

    // Get table entry that covering given index
    k = to_addr_space(index);
    e = &VM.page_table[k];
    e->owner = s->owner;
    // If integer in physcial memory
    if(e->present){
        debug("Index %d in memory\n", index);
        int c  = e->addr_physical + index%f_size;
        e->present = 1;
        e->referenced = 1;
        clock_gettime(clk_id, &e->reference_time);
        result = memory[c];
    }
    // If integer in virtual memory 
    else{
        s->n_misses++;
        s->n_dpr++;
        debug("Index %d not in memory\n", index);
        // Is there a free spot on memory
        j = find_free_addr();
        if(j != -1){
            debug("Free spot found at frame #%d\n", j);
            bitmap[j] = 1;   // Occupied now


            // New page, no old pages modified here since memory had empty space
            e->referenced = 1;
            e->present = 1;
            clock_gettime(clk_id, &e->reference_time);
            clock_gettime(clk_id, &e->load_time);
            e->addr_physical = j * f_size;      // physical address

            fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(errno < 0)  _errExit("Error: fseek @get");
            fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(errno < 0)  _errExit("Error: fread @get");

            int c  = e->addr_physical + index%f_size;
            result = memory[c];  
        }
        else{
            s->n_replacements++;
            debug("No free spots, running PR algorithm\n");
            // Find page to swap
            j = algorithm(s->owner); 

            if(j == -1){
                debug("Local allocation failed, trying global allocation\n");
                j = algorithm(0);
                if(j == -1)
                    _errExit("Page replacement error");
            }

            debug("replacing page #%d, with address:%d \n", j, VM.page_table[j].addr_physical);

            // Write back if necessary
            if(VM.page_table[j].modified){
                debug("Page %d is modified, write back required\n", j);
                // Write back required, ram to disk
                fseek(fd, sizeof(int) * VM.page_table[j].addr_virtual, SEEK_SET);       if(errno < 0) _errExit("Error: fseek @get");
                fwrite(&memory[VM.page_table[j].addr_physical], sizeof(int), f_size, fd); if(errno < 0) _errExit("Error: fwrite @get");
            }  

            // New page
            e->referenced = 1;
            e->present = 1;
            clock_gettime(clk_id, &e->reference_time);
            clock_gettime(clk_id, &e->load_time);
            e->addr_physical = VM.page_table[j].addr_physical; 

            // Old page
            VM.page_table[j].age = 0;
            VM.page_table[j].referenced = 0;
            VM.page_table[j].present = 0;
            VM.page_table[j].addr_physical = -1; // Clear physcial address

            // Disk to ram
            fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(errno < 0)  _errExit("Error: fseek @get");
            fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(errno < 0)  _errExit("Error: fread @get");

            int c  = e->addr_physical + index%f_size;
            result = memory[c];
        }
    }
    pthread_mutex_unlock(&mutex_access);
    return result;
}

void set(unsigned int index, int value, char * tName){
    pthread_mutex_lock(&mutex_access);
    int k, j;
    Entry *e;
    Stats *s;

    if(index >= n_words) _errExit("Error: Index out of range @set");

    s = whos_stats(tName);
    s->n_writes++;

    


    // Get table entry that covering given index
    k = to_addr_space(index);
    e = &VM.page_table[k];

    e->owner = s->owner;
    // If integer in physcial memory
    if(e->present){
        debug("Index %d in memory\n", index);
        int c  = e->addr_physical + index%f_size;
        e->present = 1;
        e->referenced = 1;
        e->modified = 1;
        clock_gettime(clk_id, &e->reference_time);
        memory[c] = value;
    }
    // If integer in virtual memory 
    else{
        s->n_misses++;
        s->n_dpw++;
        debug("Index %d not in memory\n", index);
        // Is there a free spot on memory
        j = find_free_addr();
        if(j != -1){
            debug("Free spot found at frame #%d\n", j);
            bitmap[j] = 1;   // Occupied now


            // New page, no old pages modified here since memory had empty space
            e->referenced = 1;
            e->present = 1;
            e->modified = 1;
            clock_gettime(clk_id, &e->reference_time);
            clock_gettime(clk_id, &e->load_time);
            e->addr_physical = j * f_size;      // physical address

            fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(errno < 0)  _errExit("Error: fseek @set");
            fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(errno < 0)  _errExit("Error: fread @set");

            int c  = e->addr_physical + index%f_size;
            memory[c] = value;  
        }
        else{
            s->n_replacements++;
            debug("No free spots, running PR algorithm\n");
            // Find page to swap
            j = algorithm(s->owner); 

            if(j == -1){
                debug("Local allocation failed, trying global allocation\n");
                j = algorithm(0);
                if(j == -1)
                    _errExit("Page replacement error");
            }

            debug("replacing page #%d, with address:%d \n", j, VM.page_table[j].addr_physical);
            
            // Write back if necessary
            if(VM.page_table[j].modified){
                debug("Page %d is modified, write back required\n", j);
                // Write back required, ram to disk
                fseek(fd, sizeof(int) * VM.page_table[j].addr_virtual, SEEK_SET);       if(errno < 0) _errExit("Error: fseek @set");
                fwrite(&memory[VM.page_table[j].addr_physical], sizeof(int), f_size, fd); if(errno < 0) _errExit("Error: fwrite @set");
            }  

            // New page
            e->referenced = 1;
            e->present = 1;
            e->modified = 1;
            clock_gettime(clk_id, &e->reference_time);
            clock_gettime(clk_id, &e->load_time);
            e->addr_physical = VM.page_table[j].addr_physical; 

            // Old page
            VM.page_table[j].age = 0;
            VM.page_table[j].referenced = 0;
            VM.page_table[j].present = 0;
            VM.page_table[j].addr_physical = -1; // Clear physcial address

            // Disk to ram
            fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(errno < 0)  _errExit("Error: fseek @set");
            fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(errno < 0)  _errExit("Error: fread @set");

            int c  = e->addr_physical + index%f_size;
            memory[c] = value;
        }
    }

    pthread_mutex_unlock(&mutex_access);
}

/*
    Used to determine page table entry index using the virtual address(i)
    Example for frame size 4096
    i: 1        -> 0
    i: 4096     -> 1
    i: 6144     -> 1
    i: 8192     -> 2
    i: 10240    -> 2
*/
int to_addr_space(unsigned int i){
    return (i/f_size);   // By the power of int division(truncation towards zero)
}

/*
    Return a page table entry index that points to a present frame,
    if owner > 0, perform local allocation, only look for entries.owner == owner
    if owner <=, perform global allocation
*/
int NRU(int owner){
    Entry *e;
    // CLASS 0: !referenced && !modified
    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){
                if(!e->referenced && !e->modified)
                    return i;  
            }
            else{
                if(!e->referenced && !e->modified && e->owner == owner)
                    return i;  
            }
        }
    }
    // CLASS 1: !referenced && modified
    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){
                if(!e->referenced && e->modified)
                    return i;  
            }
            else{
                if(!e->referenced && e->modified && e->owner == owner)
                    return i;  
            }
        }
    }
    // CLASS 2: referenced && !modified
    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){
                if(e->referenced && !e->modified)
                    return i;  
            }
            else{
                if(e->referenced && !e->modified && e->owner == owner)
                    return i;  
            }  
        }
    }
    // CLASS 3: referenced && modified
    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){
                if(e->referenced && e->modified)
                    return i;  
            }
            else{
                if(e->referenced && e->modified && e->owner == owner)
                    return i;  
            }  
        }
    }


    return -1;
}

int FIFO(int owner){
    Entry *e;
    int k = -1;
    struct timespec tmax;
    tmax.tv_sec = INT_MAX;
    tmax.tv_nsec = LONG_MAX;

    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){ // Global alloc
                if(compare_time(e->load_time, tmax) == -1){
                    tmax.tv_sec = e->load_time.tv_sec;
                    tmax.tv_nsec = e->load_time.tv_nsec;
                    k = i;
                }
            }
            else{   // Local alloc
                if(e->owner == owner && compare_time(e->load_time, tmax) == -1){
                    tmax.tv_sec = e->load_time.tv_sec;
                    tmax.tv_nsec = e->load_time.tv_nsec;
                    k = i;
                }
            }
        }
    }
    return k;
}
int SC(int owner){
    Entry *e;
    int k = -1, j = -1, reserved = 0;
    struct timespec tmax;
    tmax.tv_sec = INT_MAX;
    tmax.tv_nsec = LONG_MAX;

    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){ // Global alloc
                if(compare_time(e->load_time, tmax) == -1){
                    if(e->referenced){ // If referenced, clear R bit and update load time
                        e->referenced = 0;
                        clock_gettime(clk_id, &e->load_time);
                        if (reserved == 0){ // This would the tail if the structe was a linked list
                            reserved = 1;
                            j = i;  // Will return this if all present pages are referenced(tail)
                        }
                    }
                    else{   // Update oldest unreferenced page
                        k = i;
                    }
                    // Always update current oldest page time referenced or not
                    tmax.tv_sec = e->load_time.tv_sec;
                    tmax.tv_nsec = e->load_time.tv_nsec;
                    
                }
            }
            else{   // Local alloc
                if(e->owner == owner && compare_time(e->load_time, tmax) == -1){
                    if(e->referenced){ // If referenced, clear R bit and update load time
                        e->referenced = 0;
                        clock_gettime(clk_id, &e->load_time);
                        if (reserved == 0){ // This would the tail if the structe was a linked list
                            reserved = 1;
                            j = i;  // Will return this if all present pages are referenced(tail)
                        }
                    }
                    else{   // Update oldest unreferenced page
                        k = i;
                    }
                    // Always update current oldest page time referenced or not
                    tmax.tv_sec = e->load_time.tv_sec;
                    tmax.tv_nsec = e->load_time.tv_nsec;
                    
                }
            }
        }
    }

    if(k == -1){
        if(j == -1){
            return -1;
        }
        k = j;
    }
    
    return k;
}
int LRU(int owner){
    Entry *e;
    int k = -1;
    struct timespec tmax;
    tmax.tv_sec = INT_MAX;
    tmax.tv_nsec = LONG_MAX;;


    for(int i = 0; i < n_entries; i++) {
        e = &VM.page_table[i];
        if(e->present){
            if(owner <= 0){ // Global alloc
                if(compare_time(e->reference_time, tmax) == -1){
                    tmax.tv_sec = e->reference_time.tv_sec;
                    tmax.tv_nsec = e->reference_time.tv_nsec;
                    k = i;
                }
            }
            else{   // Local alloc
                if(e->owner == owner && compare_time(e->reference_time, tmax) == -1){
                    tmax.tv_sec = e->reference_time.tv_sec;
                    tmax.tv_nsec = e->reference_time.tv_nsec;
                    k = i;
                }
            }
        }
    }
    
    return k;
}
int WSClock(int owner){

    return LRU(owner);
}

void *thread_bubble_sort(void *arg){
    clock_t t; 
    t = clock(); 
    bubble_sort(data_bs.start, data_bs.end , "b");
    t = clock() - t; 
    data_bs.delta = ((double)t)/CLOCKS_PER_SEC; // in seconds 
    pthread_exit(0);
}

void *thread_quick_sort(void *arg){
    clock_t t; 
    t = clock(); 
    merge_sort(data_qs.start, data_qs.end-1  , "q");
    t = clock() - t; 
    data_qs.delta = ((double)t)/CLOCKS_PER_SEC; // in seconds 
    pthread_exit(0);   
}

void *thread_merge_sort(void *arg){
    clock_t t; 
    t = clock(); 
    merge_sort(data_ms.start, data_ms.end-1  , "m");
    t = clock() - t; 
    data_ms.delta = ((double)t)/CLOCKS_PER_SEC; // in seconds 
    pthread_exit(0);
}

void *thread_index_sort(void *arg){
    clock_t t; 
    t = clock(); 
    merge_sort(data_is.start, data_is.end-1  , "i");
    t = clock() - t; 
    data_is.delta = ((double)t)/CLOCKS_PER_SEC; // in seconds 
    pthread_exit(0);
}

void *thread_clock_interrupt(void *arg){
    
    int type = pr_validity(page_replacement);

    while(!exit_requested && type == 0) {
        pthread_mutex_lock(&mutex_access);
        reset_r_bit();
        pthread_mutex_unlock(&mutex_access);
        nanosleep((const struct timespec[]){{0, 400000000L}}, NULL); //40ms
    }
    
    pthread_exit(0);
}

void reset_r_bit() {
    for(int i = 0; i < n_entries; i++){
        VM.page_table[i].referenced = 0;
    }
}

void apply_aging(){
    for(int i = 0; i < n_entries; i++){
        if(VM.page_table[i].present){
            VM.page_table[i].age++;
            VM.page_table[i].referenced = 0;
        }
    }
}

void bubble_sort(int s, int e, char* c){
    for (int i = s; i < e - 1; i++) {
        int swap_flag = 0;
        for (int j = 0; j < e - i - 1; j++) {
            if (get(j,c) > get(j+1,c)) {
                swap_flag = 1;
                int temp = get(j,c);
                set(j, get(j+1,c), c);
                set(j+1, temp, c); 
            }
        }
        if (swap_flag == 0)
            break;
    }
}

void merge(int start, int mid, int end, char* c) {
    int i = 0, j = 0, k;
    int n_left = mid - start + 1;
    int n_right = end - mid;
    int left[n_left], right[n_right];

    for (int n = 0; n < n_left; n++)
        left[n] = get(start+n, c);
    for (int n = 0; n < n_right; n++)
        right[n] = get(mid+1+n, c);

    k = start;

    while (i < n_left && j < n_right) {
        if (left[i] <= right[j])
            set(k++, left[i++], c);
        else 
            set(k++, right[j++], c);
    }

    while (i < n_left) 
        set(k++, left[i++], c);

    while (j < n_right)
        set(k++, right[j++], c);
}

void merge_sort(int left, int right, char* c){
  if (left < right) {
    int mid = left + (right - left) / 2;
    merge_sort(left, mid, c);
    merge_sort(mid + 1, right, c);
    merge(left, mid, right, c);
  }
}
void quick_sort(int low, int high, char* c){
    if(low < high) {
        int pivot = partition(low,high,c);
        quick_sort(low, pivot - 1, c);
        quick_sort(pivot + 1, high, c);
    }
}

int partition(int low, int high, char* c){
    int pivot = get(high,c);
    int i = low -1;

    for(int j = low; j < high; j++) {
        if (get(j,c) <= pivot) {
          i++;
          swap(i, j, c);
        }
    }
    swap(i+1, high, c);
    return i + 1;
}

void swap(int i, int j, char* c){
    int temp = get(i,c);
    set(i, get(j,c), c);
    set(j, temp, c);
}

void index_sort(int s, int e, char* c) {
    int low_ind = s;
    int up_ind = e;
    int counter1 = 0;
    int counter2 = 0;
    int j;
    while(low_ind < up_ind){
                for(j = low_ind+1; j < up_ind; j++){
            if(get(low_ind, c) > get(j, c))
                counter1++;
        }
        if(counter1 > 0){
            int t = get(low_ind, c);
            set(low_ind,get(counter1 + low_ind, c), c);
            set(counter1+low_ind, t, c);
            counter1 = 0;
            
        }
        else{
            low_ind++;
        }

        j = up_ind - 1;
        while(j >= low_ind){
            if(get(up_ind,c) < get(j,c)){
                counter2++;
            }
            j--;
        }

        if(counter2 > 0){
            int t = get(up_ind,c);
            set(up_ind, get(up_ind-counter2,c), c);
            set(up_ind-counter2, t, c);
            counter2 = 0;
            
        }
        else{
            up_ind--;
        }
    }
}


int is_sorted(int s, int e){
    if (s < 0 || s > e || e > n_words) _errExit("Index out of range @print_disk");

    for(int i = s; i < e-1; i++) {
        if (get(i, "c") > get(i+1,"c"))
            return -1;
    }
    return 0;

}
void print_disk(int s, int e){
    if (s < 0 || s > e || e > n_words) _errExit("Index out of range @print_disk");


    printf("==========Disk[%d, %d]============\n", s, e);
    for(int i = s; i < e; i++) {
        printf("%d\n",get(i,"c"));
    }
    printf("==================================");
}

int compare_time(struct timespec t1, struct timespec t2){
    if(t1.tv_sec == t2.tv_sec && t1.tv_nsec == t2.tv_nsec){
        //debug("t1 == t2\n");
        return 0;
    }
    else if(t1.tv_sec >= t2.tv_sec && t1.tv_nsec > t2.tv_nsec){
        //debug("t1 > t2\n");
        return 1;
    }
    else{
        //debug("t1 < t2\n");
        return -1;
    }
}

int algorithm(int owner){
    int type = -1;

    for(int i = 0; i < PR_N; i++){
        if(strncmp(page_replacement, PR_TYPES[i], strlen(PR_TYPES[i])) == 0)
            type = i;
    }

    switch(type) {
        case 0 : return NRU(owner); 
        case 1 : return FIFO(owner);
        case 2 : return SC(owner);
        case 3 : return LRU(owner);
        case 4 : return WSClock(owner);
        default: _errExit("Invalid algorithm @algorithm");
    }
}

// Returns a physical address if a free frame available, -1 otherwise

int find_free_addr(){
    for(int i = 0; i < n_pframes; i++){
        if (bitmap[i] == 0)
           return i;
    }
    return -1;
}

int pr_validity(char* type){
    for(int i = 0; i < PR_N; i++){
        if(strncmp(type, PR_TYPES[i], strlen(PR_TYPES[i])) == 0)
            return i;
    }
    return -1;
}

int ap_validity(char* type){
    for(int i = 0; i < AP_N; i++){
        if(strncmp(type, AP_TYPES[i], strlen(AP_TYPES[i])) == 0)
            return i;
    }
    return -1;
}

void print_usage(){
    printf("\n==========================================\n");
    printf("Usage:\n./sortArrays"
    " frameSize numPhysical numVirtual pageReplacement allocPolicy pageTablePrintInt diskFileName.dat\n\n");

    printf("Supported page replacement methods:\n");
    for(int i = 0; i < PR_N; i++)
            printf("-%s\n", PR_TYPES[i]);

    printf("\nSupported allocation policies:\n");
    for(int i = 0; i < AP_N; i++)
        printf("-%s\n", AP_TYPES[i]);
    
    printf("==========================================\n");
    
}


void print_array(int* arr, int n){
    printf("[");
    for(int i = 0; i < n-1; i++)
        printf("%d\n",arr[i]);
    printf("%d]\n",arr[n-1]);
}


void print_entry(Entry e){
    debug("%-12s%-12s%-12s%-12s%-12s%-7s\n", "Virtual", "Physical", "Present", "Modified", "Referenced", "Owner");
    debug("%-12d%-12d%-12d%-12d%-12d%-7d\n",
                                    e.addr_virtual,
                                    e.addr_physical,
                                    e.present,
                                    e.modified,
                                    e.referenced,
                                    e.owner);
    debug("=================================\n");
}

void print_pt(){
    printf("=========================================================\n");
    printf("%-12s%-12s%-12s%-12s%-12s%-7s\n", "Virtual", "Physical", "Present", "Modified", "Referenced", "Owner");
    for(int i = 0; i < n_entries; i++){
        printf("%-12d%-12d%-12d%-12d%-12d%-7d\n",
                                    VM.page_table[i].addr_virtual,
                                    VM.page_table[i].addr_physical,
                                    VM.page_table[i].present,
                                    VM.page_table[i].modified,
                                    VM.page_table[i].referenced,
                                    VM.page_table[i].owner);
    }
    printf("=========================================================\n");
}