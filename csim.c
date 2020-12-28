/*
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *
 *  2. Instruction loads (I) are ignored, since we are interested in evaluating
 *  data cache performance.
 *
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * IMPORTANT: This is crucial for the driver to evaluate your work.
 *
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#define DEBUG_ON
#define ADDRESS_LENGTH 64

/* Type: Memory address */
typedef unsigned long long int mem_addr_t;

/*
 * Data structures to represent the cache we are simulating
 *
 * TODO: Define your own!
 *
 * E.g., Types: cache, cache line, cache set
 * (you can use a counter to implement LRU replacement policy)
 */
typedef struct line_struct{

unsigned short valid;

mem_addr_t tag;

unsigned char *block;//block

int age;
}line_struct_t;


typedef struct set_struct{

line_struct_t *pset_n;


}set_struct_t;






/* Globals set by command line args */
int verbosity = 0; /* print trace if set */
int s = 0; /* set index bits */
int b = 0; /* block offset bits */
int E = 0; /* associativity */
char* trace_file = NULL;
line_struct_t *numE;
set_struct_t *pSets;

unsigned long long int tempMask = 0;


/* Derived from command line args */
int S; /* number of sets */
int B; /* block size (bytes) */

/* Counters used to record cache statistics */
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;
int lru_counter = 0;



/*
 * initCache - Allocate memory (with malloc) for cache data structures (i.e., for each of the sets and lines per set),
 * writing 0's for valid and tag and LRU
 *
 * TODO: Implement
 *
 */
void initCache()
{

  pSets=malloc(S*sizeof(set_struct_t*)); //allocate space for pointer to sets
  for(int i = 0; i < S; i ++){
    pSets[i].pset_n=malloc(E*sizeof(line_struct_t)); //allocate space for actual sets for each pointer to sets
      for(int j=0; j < E; j++){
        pSets[i].pset_n[j].block = malloc(B); //allocate memory for the block within the line
        pSets[i].pset_n[j].valid = 0;         //init to invalid
        pSets[i].pset_n[j].tag = 0;           //init to zero/invalid
        pSets[i].pset_n[j].age=0;
      }
  }

}


/*
 * freeCache - free allocated memory
 *
 * This function deallocates (with free) the cache data structures of each
 * set and line.
 *
 * TODO: Implement
 */
void freeCache()
{
  for(int i = 0; i < S; i ++){

    for(int j=0; j < E; j++){
      free(pSets[i].pset_n[j].block);

    }

    free(pSets[i].pset_n);
  }

    free(pSets);

}


/*
 * accessData - Access data at memory address addr
 *   If it is already in cache, increase hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 *
 * TODO: Implement
 */
int accessData(mem_addr_t addr)
{
  //printf("\n%lx",addr);

  mem_addr_t addrcopy=addr;
  mem_addr_t addrccopy=addr;
  addrcopy = addrcopy >> b;
  int currset = addrcopy & tempMask;

  int hitflag =0;
  int oldest=0;


  int currtag = addrccopy >> (b+s);

  printf("\n %lx, %lx, %d, %d, %d, %d",(long unsigned int)addr,(long unsigned int)tempMask,currset,currtag,b,s);

  for(int j=0; j < E; j++){

    if (pSets[currset].pset_n[j].valid == 1 && pSets[currset].pset_n[j].tag == currtag){
      hit_count++;
      hitflag=1;
      pSets[currset].pset_n[j].age=0;

    }
    else{
        pSets[currset].pset_n[j].age++;
    }

  }//end for j=0

  if (hitflag==1){
     return 0;
  }

  miss_count++;

 for(int i = 0; i <E; i++){                           //if theres a miss but an empty line
   if(pSets[currset].pset_n[i].valid == 0){          //check for empty lines
     pSets[currset].pset_n[i].valid = 1;             //update that line
     pSets[currset].pset_n[i].tag = currtag;
     pSets[currset].pset_n[i].age=0;
     return 1;
   }

 }
 lru_counter=0;
 for(int k=0; k<E; k++){                            //a miss but no empty lines

    if( pSets[currset].pset_n[k].age > oldest){    //
      oldest = pSets[currset].pset_n[k].age;
      lru_counter=k;

      }

  }

    pSets[currset].pset_n[lru_counter].valid = 1;              //update that line
    pSets[currset].pset_n[lru_counter].tag = currtag;
    pSets[currset].pset_n[lru_counter].age=0;
    eviction_count++;
    return 2;
}//end accessdata


/*
 * replayTrace - replays the given trace file against the cache
 *
 * This function:
 * - opens file trace_fn for reading (using fopen)
 * - reads lines (e.g., using fgets) from the file handle (may name `trace_fp` variable)
 * - skips lines not starting with ` S`, ` L` or ` M`
 * - parses the memory address (unsigned long, in hex) and len (unsigned int, in decimal)
 *   from each input line
 * - calls `access_data(address)` for each access to a cache line
 *
 * TODO: Implement
 *
 */
void replayTrace(char* trace_fn)
{
    //accessData(0);
    char line[80] = {0};
    unsigned long int memAdd;
    unsigned int access_size;
    char access_type;
    int returnval;


    FILE* traces = fopen(trace_fn, "r");
    if(traces == NULL){
      printf("\nfile not found");
      return;
    }

      while(fgets(line, 80, traces)){   //read in
        if (line[0] == 'I'){
          continue;
        }
        else{
          sscanf(line, " %c %lx,%u", &access_type, &memAdd, &access_size );  //parse input line
          //printf("%lx", memAdd);
          printf("\n %c %c %c %c\n",line[1], line[2],line[3], line[4]);
          printf("%c %lx,%u", access_type, memAdd, access_size);
          returnval = accessData((mem_addr_t)memAdd);
          if (returnval == 0)
          {
            printf(" hit");
          }
          else if (returnval ==1)
          {
            printf(" miss");
          }
          else{
            printf(" miss eviction");
          }
          if(access_type == 'M'){
            returnval = accessData((mem_addr_t)memAdd);
            if (returnval == 0)
            {
              printf(" hit");
            }
            else if (returnval ==1)
            {
              printf(" miss");
            }
            else
            {
              printf(" miss eviction");
            }
          }
        }
        printf("\n");
      }

    fclose(traces);

}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 *
 * !! DO NOT MODIFY !!
 *
 * printSummary - Summarize the cache simulation statistics. Student cache simulators
 *                must call this function in order to be properly autograded.
 */
void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}

/*
 * main - Main routine
 */
int main(int argc, char* argv[])
{
    char c;

    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* Compute S, E and B from command line args */
    S = (unsigned int) pow(2, s);
    B = (unsigned int) pow(2, b);


    tempMask = ~tempMask;

    tempMask = tempMask << s;

    tempMask = ~tempMask;





    /* Initialize cache */
    initCache();

#ifdef DEBUG_ON
    printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file);
#endif

    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}
