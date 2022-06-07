// main_prioc.c
// Řešení IOS-P2 28.4.2022
// Autor: Nikita Kotvitskiy (xkotvi01), FIT
// Vytváří všechny procesy kyslíků a vodíků, pak čeká na jejich ukončení a uklízí paměť
// Obsahuje definice některých pomocných funkcí

#include "proj2.h"

// Close all shared memory, destroy semaphores and closes output file
void clean(shdata_t * const shdata, const opts_t opts)
{
    // Closing atom queues shared memory
    munmap(shdata->oxygen_queue, sizeof(pid_t) * opts.NO);
    munmap(shdata->hydrogen_queue, sizeof(pid_t) * opts.NH);

    // Destroying all semaphores
    sem_destroy(&shdata->output_file_sem);
    sem_destroy(&shdata->oxygen_queue_sem);
    sem_destroy(&shdata->hydrogen_queue_sem);
    sem_destroy(&shdata->hydrogen_done_sem);
    sem_destroy(&shdata->molecule_creating_sem);
    sem_destroy(&shdata->ended_process_counter);
    sem_destroy(&shdata->free_hydrogen_counter);
    sem_destroy(&shdata->free_oxygen_counter);

    // Closing output file
    fclose(shdata->output_file);

    // Closing main shared data structure shared memory
    munmap(shdata, sizeof(shdata_t));
}

// Provides atoms join queues
void join_queue(shdata_t * const shdata, pid_t pid, enum process_type pt)
{
    switch (pt)
    {
        case OXY:
            shdata->oxygen_queue[shdata->oxygen_in_queue_count] = pid;
            (shdata->oxygen_in_queue_count)++;
            break;
        case HYD:
            shdata->hydrogen_queue[shdata->hydrogen_in_queue_count] = pid;
            (shdata->hydrogen_in_queue_count)++;
            break;
    }
}

// Body of main process
void main_proc(shdata_t * const shdata, const opts_t opts)
{
    pid_t pid;
    // Oxygen atoms creation
    for (long unsigned i = 1; i <= opts.NO; i++)
    {
        pid = fork();
        switch (pid)
        {
            case -1:
                clean(shdata, opts);
                error_exit("fork fail");
                break;
            case 0:
                continue;
            default:
                oxygen(i, shdata, opts);
                break;
        }
    }

    // Hydrogen atom creation
    for (long unsigned i = 1; i <= opts.NH; i++)
    {
        pid = fork();
        switch (pid)
        {
            case -1:
                clean(shdata, opts);
                error_exit("fork fail");
                break;
            case 0:
                continue;
            default:
                hydrogen(i, shdata, opts);
                exit(0);
        }
    }
    
    // Waiting for all processes finish
    for (long unsigned i = 0; i < opts.NO + opts.NH; i++)
        sem_wait(&shdata->ended_process_counter);
    clean(shdata, opts);
}