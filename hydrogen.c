// hydrogen.c
// Řešení IOS-P2 28.4.2022
// Autor: Nikita Kotvitskiy (xkotvi01), FIT
// Proces vodíku

#include "proj2.h"

static shdata_t * glob_shdata;
static int glob_id;

// Increments action_counter and write message in output file
static void print_message(enum message_type mes)
{
    sem_wait(&glob_shdata->output_file_sem);
    (glob_shdata->action_counter)++;
    switch (mes)
    {
        case START:
            fprintf(glob_shdata->output_file, "%d: H %d: started\n", glob_shdata->action_counter, glob_id);
            break;
        case QUEUE:
            fprintf(glob_shdata->output_file, "%d: H %d: going to queue\n", glob_shdata->action_counter, glob_id);
            break;
        case MOLECULE:
            fprintf(glob_shdata->output_file, "%d: H %d: creating molecule %d\n", glob_shdata->action_counter, glob_id, glob_shdata->molecule_count);
            break;
        case NOT_ENOUGH:
            fprintf(glob_shdata->output_file, "%d: H %d: not enough O or H\n", glob_shdata->action_counter, glob_id);
            break;
        case MOLECULE_FINISH:
            fprintf(glob_shdata->output_file, "%d: H %d: molecule %d created\n", glob_shdata->action_counter, glob_id, glob_shdata->molecule_count);
            break;
    }
    fflush(glob_shdata->output_file);
    sem_post(&glob_shdata->output_file_sem);
}

static void create_molecule();
static void finish_molecule_create();
static void atom_lack();
// Sets handlers of signals. Just after atom creation this function is called with non-zero argument,
// then it is called from  create_molecule() with zero argument
static void signal_set(int start)
{
    struct sigaction act;
    sigset_t set;
    sigemptyset(&set);
    act.sa_mask = set;
    if (start)
        act.sa_handler = create_molecule;
    else
        act.sa_handler = finish_molecule_create;
    sigaction(SIGUSR1, &act, 0);

    act.sa_handler = atom_lack;
    sigaction(SIGUSR2, &act, 0);
}

// Finishes process
static void hydrogen_finish()
{
    sem_post(&glob_shdata->ended_process_counter);
    exit(0);
}

// Prints message about molecule creation start and change SIGUSR1 signal handler to finish_molecule_create()
static void create_molecule()
{
    print_message(MOLECULE);
    signal_set(0);

    // Informs oxygen process about task is done
    sem_post(&glob_shdata->hydrogen_done_sem);
}

// Prints message about molecule creation finish and finishes process
static void finish_molecule_create()
{
    print_message(MOLECULE_FINISH);

    // Informs oxygen process about task is done
    sem_post(&glob_shdata->hydrogen_done_sem);
    hydrogen_finish();
}

// Handles SIGUSR2 signal, which means atoms lack. Prints message about atoms lack and finishes process
static void atom_lack()
{
    print_message(NOT_ENOUGH);
    hydrogen_finish();
}

// Body of hydrogen process
void hydrogen(const int id, shdata_t * const shdata, const opts_t opts)
{
    // These variables must be global to functions called by signal processing could use them
    glob_shdata = shdata;
    glob_id = id;

    // Reporting abouthydrogen atom start and going sleep for random time in range <0, TI>
    print_message(START);
    usleep((rand() % (opts.TI + 1)) * MIKRO_IN_MILLI);

    // Getting access to hydrogen_queue to join it
    sem_wait(&shdata->hydrogen_queue_sem);
    print_message(QUEUE);
    join_queue(shdata, getpid(), HYD);
    sem_post(&shdata->hydrogen_queue_sem);

    // Setting signal handlers (SIGUSR1 to create_molecule(), SIGUSR2 to atom_lack())
    signal_set(1);

    // Incrementing free_hydrogen_counter to indicate that this atom is ready for H2O creation
    sem_post(&shdata->free_hydrogen_counter);

    // Waiting for first SIGUSR1 (start creation)
    pause();
    // Waiting for second SIGUSR1 (end creation)
    pause();
}