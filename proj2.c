// proj2.c
// Řešení IOS-P2 28.4.2022
// Autor: Nikita Kotvitskiy (xkotvi01), FIT
// Zpracovává argumenty programu, alokuje sdílenou paměť, otevírá semafory, pak se stává hlavním procesem,
// Obsahuje definici funkce error_exit()

#include "proj2.h"

// Print error message and exit with code 1
void error_exit(char * message)
{
    fprintf(stderr, "ERROR: %s\n", message);
    exit(1);
}

#define NEEDED_OPTION_COUNT 5
#define NO_OPTION_IND 1
#define NH_OPTION_IND 2
#define TI_OPTION_IND 3
#define TB_OPTION_IND 4
#define TEN 10

//Processes options of the program, write them into option structure
static void options_processing(const int argc, char * const argv[], opts_t *opts)
{
    if (argc != NEEDED_OPTION_COUNT)
        error_exit("Wrong argument count");
    long no, nh, ti, tb;
    no = strtol(argv[NO_OPTION_IND], NULL, TEN);
    if (no <= 0 || no == LONG_MAX)
        error_exit("Wrong NO argument");
    nh = strtol(argv[NH_OPTION_IND], NULL, TEN);
    if (nh <= 0 || nh == LONG_MAX)
        error_exit("Wrong NH argument");
    ti = strtol(argv[TI_OPTION_IND], NULL, TEN);
    if (ti < 0 || ti > 1000)
        error_exit("Wrong TI argument");
    tb = strtol(argv[TB_OPTION_IND], NULL, TEN);
    if (tb < 0 || tb > 1000)
        error_exit("Wrong TB argument");
    
    opts->NO = (unsigned long)no;
    opts->NH = (unsigned long)nh;
    opts->TI = (unsigned)ti;
    opts->TB = (unsigned)tb;
}

// Allocate shared memory for atom queues, set start values of shared variables and opens output file
static void shared_data_init(shdata_t * const shdata, const opts_t opts)
{
    shdata->oxygen_queue = (pid_t *)mmap(NULL, sizeof(pid_t) * opts.NO, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shdata->hydrogen_queue = (pid_t *)mmap(NULL, sizeof(pid_t) * opts.NH, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shdata->oxygen_queue == MAP_FAILED || shdata->hydrogen_queue == MAP_FAILED)
    {
        clean(shdata, opts);
        error_exit("mmap fail");
    }

    shdata->hydrogen_in_queue_count = 0;
    shdata->hydrogen_queue_cursor = 0;
    shdata->oxygen_in_queue_count = 0;
    shdata->oxygen_queue_cursor = 0;
    shdata->action_counter = 0;
    shdata->molecule_count = 0;

    shdata->output_file = fopen(OUTPUT_FILE_NAME, "w");
    if (shdata->output_file == NULL)
        error_exit("output file open fail");
}

// Creates all semaphores 
static void semaphores_creator(shdata_t * const shdata, const opts_t opts)
{
    if (sem_init(&shdata->output_file_sem, 1, 1) < 0 ||
        sem_init(&shdata->oxygen_queue_sem, 1, 1) < 0 ||
        sem_init(&shdata->hydrogen_queue_sem, 1, 1) < 0 ||
        sem_init(&shdata->free_hydrogen_counter, 1, 0) < 0 ||
        sem_init(&shdata->molecule_creating_sem, 1, 1) < 0 ||
        sem_init(&shdata->hydrogen_done_sem, 1, 0) < 0 ||
        sem_init(&shdata->ended_process_counter, 1, 0) < 0 ||
        sem_init(&shdata->free_oxygen_counter, 1, 1) < 0)
    {
        clean(shdata, opts);
        error_exit("sem_init fail");
    }
}

int main(int argc, char * argv[])
{
    opts_t opts;
    options_processing(argc, argv, &opts);
    shdata_t *shdata = (shdata_t *)mmap(NULL, sizeof(shdata_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shdata == MAP_FAILED)
        error_exit("mmap fail");
    shared_data_init(shdata, opts);
    semaphores_creator(shdata, opts);
    main_proc(shdata, opts);
}