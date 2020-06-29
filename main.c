//
//  main.c
//  Virtual Memory Part 2
//
//  Created by Muhammed Okumuş on 23.06.2020.
//  Copyright 2020 Muhammed Okumus. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>  
#include <string.h> 
#include <math.h>       // pow()
#include <time.h>       // time()
#include <limits.h>
#include <time.h>

#define MAX_PATH 1024

// Structs
typedef struct{
    unsigned int addr_virtual;  // VM address space
    int addr_physical;          // RAM address space
    int modified;  
    int present;
    int referenced;
    int owner;
    unsigned long reference_time;   
    unsigned long load_time; 
}Entry;

typedef struct{
    int page_size;
    Entry* page_table;
}VirtualMemory;


// Macros
#define errExit(msg) do{ perror(msg); print_usage(); exit(EXIT_FAILURE); } while(0)
#define _errExit(msg) do{ perror(msg); exit(EXIT_FAILURE); } while(0)

// Globals
const char* PR_TYPES[] = {"NRU", "FIFO", "SC", "LRU", "WSClock"};
const char* AP_TYPES[] = {"global","local"};
const int PR_N =  sizeof(PR_TYPES) / sizeof(PR_TYPES[0]);
const int AP_N =  sizeof(AP_TYPES) / sizeof(AP_TYPES[0]);

// Command line arguments =====
int frame_size  = 0,        
    num_physical = 0,
    num_virtual = 0,
    page_table_print_int = 0;

char page_replacement[8],
     alloc_policy[7],
     disk_file_name[MAX_PATH];
// ============================

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
int status;             // For error checking

// Function Prototypes ======================
// Misc Functions
void print_usage();

// Error Checking Functions
int pr_validity(char* type);
int ap_validity(char* type);

// Integer Array Utility Functions
void fill_array_rand(int* arr, int n);
void print_array(int* arr, int n);

// VM Functions
void set(unsigned int index, int value, char * tName);
int get(unsigned int index, char * tName);
void initilize_vm();
void print_entry(Entry e);
void print_pt();

int to_addr_space(unsigned int i);
int find_free_addr();

// Page Replacement Functions
int algorithm(int owner);
int NRU(int owner);
int FIFO(int owner);
int SC(int owner);
int LRU(int owner);
int WSClock(int owner);

int main(int argc, char* argv[]){
    srand(1000); // Rand seed requested in the pdf


    // Parse commands =============================================
    if (argc == 1 || argc < 8) errExit("Parameters are missing");
    if (argc > 8) errExit("Too many parameters");
    printf("============INPUTS============\n");
    for(int i = 1; i < argc; i++){
        switch(i){
            case 1: frame_size = atoi(argv[i]); printf("Frame size: 2^%d = %d\n", frame_size, (int) pow(2,frame_size)); break;
            case 2: num_physical = atoi(argv[i]); printf("# of physical frames: 2^%d = %d\n", num_physical, (int) pow(2,num_physical)); break;
            case 3: num_virtual = atoi(argv[i]); printf("# of virtual frames: 2^%d = %d\n", num_virtual, (int) pow(2,num_virtual)); break;
            case 4: snprintf(page_replacement, 8,"%s",argv[i]); printf("Page replacemet method: %s\n", page_replacement); break;
            case 5: snprintf(alloc_policy, 7,"%s",argv[i]); printf("Allocation policy: %s\n", alloc_policy); break;
            case 6: page_table_print_int = atoi(argv[i]); printf("Page Table Print Interval: %d\n", page_table_print_int); break;
            case 7: snprintf(disk_file_name, MAX_PATH,"%s",argv[i]); printf("Disk file name: %s\n", disk_file_name); break;
            default: errExit("Logic error occured while parsing commands: Too many parameters");
        }
    }
    // Error check
    if(frame_size <= 1) errExit("frameSize must be a positive integer larger than 1");
    if(num_physical <= 1) errExit("numPhysical size must be a positive integer larger than 1");
    if(num_virtual <= 1)  errExit("numVirtual size must be a positive integer larger than 1");
    if(pr_validity(page_replacement) != 0) errExit("Invalid page replacemet method");
    if(ap_validity(alloc_policy) != 0) errExit("Invalid allocation policy");
    printf("==============================\n");
    // ============================================= Parse commands

    // Initlize calculated properties
    n_words = pow(2,num_virtual) * pow(2,frame_size);
    n_pframes = pow(2, num_physical);
    n_vframes = pow(2, num_virtual);
    n_entries = n_vframes + n_pframes;
    f_size = pow(2, frame_size);
    m_size = n_pframes * f_size;

    // Open VM file
    fd = fopen(disk_file_name, "w+");
    if (fd == NULL) errExit("Error opening file @initilize_vm");

    // Fill VM file with random integers
    initilize_vm(frame_size, num_virtual);

    printf("==============================\n");
    printf("# entries: %d\n", n_entries);
    printf("# words: %d\n", n_words);
    printf("RAM: %.0f MB\n", m_size/pow(2,10));
    printf("VM : %.0f MB\n", n_words/pow(2,10));

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

    

    /*
    for(int i = 0; i < f_size*n_vframes; i += f_size) {
        get(i, NULL);
        printf("===========================\n");

    }
    print_pt();
    //print_array(memory, f_size*n_pframes);

    printf("k: %d\n",memory[126976]); // 1296183442
    //get(4190208,NULL) should be a HIT
    int k = get(4190208, NULL);
    
    k = get(4184311, NULL); // 893511520
    printf("k: %d\n",k);
    */

    // Free resources
    free(memory);
    free(bitmap);
    free(VM.page_table);
    fclose(fd);
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

int pr_validity(char* type){
    for(int i = 0; i < PR_N; i++){
        if(strncmp(type, PR_TYPES[i], strlen(PR_TYPES[i])) == 0)
            return 0;
    }
    return -1;
}

int ap_validity(char* type){
    for(int i = 0; i < AP_N; i++){
        if(strncmp(type, AP_TYPES[i], strlen(AP_TYPES[i])) == 0)
            return 0;
    }
    return -1;
}

void fill_array_rand(int* arr, int n){
    for(int i = 0; i < n; i++)
        arr[i] = rand()%1000;
}

void print_array(int* arr, int n){
    printf("[");
    for(int i = 0; i < n-1; i++)
        printf("%d\n",arr[i]);
    printf("%d]\n",arr[n-1]);
}

void initilize_vm(){
    printf("Initilizing virtual memory with random integers...\n");
    clock_t t = clock();
    // Write to file
    for(int i = 0; i < n_words; i++){
        int r = (int) rand();
        status =  fwrite(&r, sizeof(int), 1, fd); if(status < 0) _errExit("fwrite @initilize_vm: 1");
    }

    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC; // calculate the elapsed time
    printf("Virtual memory initilized in %f seconds.\n", time_taken);
}

void print_entry(Entry e){
    printf("%-12s%-12s%-12s%-12s%-12s%-7s\n", "Virtual", "Physical", "Present", "Modified", "Referenced", "Owner");
    printf("%-12d%-12d%-12d%-12d%-12d%-7d\n",
                                    e.addr_virtual,
                                    e.addr_physical,
                                    e.present,
                                    e.modified,
                                    e.referenced,
                                    e.owner);
}

void print_pt(){
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
}

int get(unsigned int index, char * tName){
    int k, j, result = -1;
    Entry *e;

    if(index >= n_words)
        _errExit("Error: Index out of range @get");

    // Get table entry that covering given index
    k = to_addr_space(index);
    e = &VM.page_table[k];
    
    // If integer in physcial memory
    if(e->present){
        printf("In memory\n");
        int c  = e->addr_physical + index%f_size;
        e->present = 1;
        e->referenced = 1;
        e->reference_time = (unsigned long) time(NULL);
        result = memory[c];
    }
    // If integer in virtual memory 
    else{
        printf("Not in memory\n");
        // Is there a free spot on memory
        j = find_free_addr();
        if(j != -1){
            printf("Free spot found at frame #%d\n", j);
            bitmap[j] = 1;                      // Occupied now
            e->addr_physical = j * f_size;      // physical address

            status = fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(status < 0)  _errExit("Error: fseek @get");
            status = fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(status < 0)  _errExit("Error: fread @get");

            int c  = e->addr_physical + index%f_size;
            e->present = 1;
            e->referenced = 1;
            e->reference_time = (unsigned long) time(NULL);
            e->load_time = (unsigned long) time(NULL);
            result = memory[c];  
        }
        else{
            printf("No free spots, running PR algorithm\n");
            // Find page to swap
            j = algorithm(e->owner); 
            printf("replacing page #%d, with address:%d \n", j, VM.page_table[j].addr_physical);
            e->addr_physical = VM.page_table[j].addr_physical; 


            // Write back if necessary
            if(VM.page_table[j].modified){
                printf("Page %d is modified, write back required\n", j);
                // Write back required
                status = fseek(fd, sizeof(int) * VM.page_table[j].addr_virtual, SEEK_SET);       if(status < 0) _errExit("Error: fseek @get");
                status = fwrite(&memory[VM.page_table[j].addr_virtual], sizeof(int), f_size, fd); if(status < 0) _errExit("Error: fwrite @get");
            }  

            VM.page_table[j].referenced = 0;
            VM.page_table[j].present = 0;
            VM.page_table[j].addr_physical = -1; // Clear physcial address


            status = fseek(fd, sizeof(int) * e->addr_virtual, SEEK_SET);        if(status < 0)  _errExit("Error: fseek @get");
            status = fread(&memory[e->addr_physical], sizeof(int),f_size, fd);  if(status < 0)  _errExit("Error: fread @get");

            e->present = 1;
            e->referenced = 1;
            e->reference_time = (unsigned long) time(NULL);
            e->load_time = (unsigned long) time(NULL);

            int c  = e->addr_physical + index%f_size;
            result = memory[c];
        }
    }
    print_entry(*e);

    return result;
}

void set(unsigned int index, int value, char * tName){
    if(index >= n_words)
        _errExit("Error: Index out of range @set");

    
    if(1){// If integer in virtual memory
        status = fseek(fd, sizeof(int) * index, SEEK_SET); if(status < 0)   _errExit("Error: fseek @set");
        status = fwrite(&value, sizeof(int), 1, fd);       if(status < 0)   _errExit("Error: fwrite @set");
    }
    else{// If integer in physcial memory

    }
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
    errExit("Logic error @NRU");
}

int FIFO(int owner){
    printf("FIFO");
    return 0;
}
int SC(int owner){
    printf("SC");
    return 0;
}
int LRU(int owner){
    printf("LRU");
    return 0;
}
int WSClock(int owner){
    printf("WSClock");
    return 0;
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