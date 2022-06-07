// proj2.h
// Řešení IOS-P2 28.4.2022
// Autor: Nikita Kotvitskiy (xkotvi01), FIT
// Hlavičkový soubor projektu

#ifndef PROJ2_H
#define PROJ2_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define OUTPUT_FILE_NAME "proj2.out"
#define MIKRO_IN_MILLI 1000

enum process_type { OXY, HYD };
enum message_type { START, QUEUE, MOLECULE , MOLECULE_FINISH , NOT_ENOUGH };

struct prog_options
{
    unsigned long NO;   // Oxygen count
    unsigned long NH;   // Hydrogen count
    unsigned TI;        // Max time to wait after atom creation
    unsigned TB;        // Max time of moluceule creating
};
typedef struct prog_options opts_t;

// Main shared data structure. Contains all semaphores, queues and other data
struct shared_data
{
    sem_t output_file_sem;              // This semaphore regulates usage of action_counter and output_file
                                        // It guarantees, that some process cannot report about action earlier than other process finish reporting

    sem_t oxygen_queue_sem;             // This semapore regulates usage of oxygen_queue and oxygen_in_queue_count
                                        // It guarantees, that some new oxygen atom process will not replace another oxygen in queue
                                        // It also guarantees, that processes cannot get the count of atoms in oxygen queue earlier than some oxygen finish joining the queue
    
    sem_t hydrogen_queue_sem;           // This semapore regulates usage of hydrogen_queue and hydrogen_in_queue_count
                                        // It guarantees, that some new hydrogen atom process will not replace another hydrogen in queue
                                        // It also guarantees, that processes cannot get the count of atoms in hydrogen queue earlier than some hydrogen finish joining the queue

    sem_t hydrogen_done_sem;            // Not-zero value of this semaphore means, that hydrogen used in molecule creating has finished it's task and is waiting for next signal
                                        // Example: oxygen sends signals of molecule creation start to two hydrogens used in molecule creation
                                        // Then oxygen decrements this semaphore by two and only then starts molecule creation
                                        // It guarantees, that creation will not start earlier than all hydrogen atoms will be ready for creation
    
    sem_t molecule_creating_sem;        // This semaphore regulates usage of molecule_count and queues cursors
                                        // It guarentees, that two atoms of oxygen will not create two molecules at the same time
                                        // It also guarantees, that tho molecules cannot have the same ID

    sem_t ended_process_counter;        // Each atom process increments this semaphore just before exit()
                                        // Main process decrements this semaphore by one (NO+NH) times, then finish
                                        // So main process cannot finish before all atom processes finish

    sem_t free_hydrogen_counter;        // Each hydrogen atom process increments this semaphore just after joining the queue
                                        // If oxygen process wants to get next hydrogen in queue, then decrements this semaphore
                                        // So oxygen process cannot try to get hydrogen PID form hydrogen queue earlier than some hydrogen join the queue
    
    sem_t free_oxygen_counter;          // Each hydrogen atom process increments this semaphore just after joining the queue
                                        // It is used when hydrogen is over and all next oxygen must be finished
                                        // Last used oxygen atom decrements this semaphore and then send terminate signal to next oxygen in queue
                                        // So oxygen process cannot try to get oxygen PID form oxygen queue earlier than some oxygen join the queue

    unsigned action_counter;            // Contains ID of last done action
    pid_t *oxygen_queue;                // Array of oxygen atom processes PIDs.
    pid_t *hydrogen_queue;              // Array of hydrogen atom processes PIDs.
    unsigned oxygen_in_queue_count;     // Contains number of oxygen atoms which have joined the queue
    unsigned hydrogen_in_queue_count;   // Contains number of hydrogen atoms which have joined the queue
    unsigned oxygen_queue_cursor;       // Index of next atom in oxygen queue. Exit the oxygen queue means increment this cursor
    unsigned hydrogen_queue_cursor;     // Index of next atom in hydrogen queue. Exit the hydrogen queue means increment this cursor
    int molecule_count;                 // Index of creating molecule

    FILE * output_file;                 // Output file for action reporting
};
typedef struct shared_data shdata_t;

void error_exit(char * message);                                            // Print error message and exit with code 0
void clean(shdata_t * const shdata, const opts_t opts);                     // Close all shared memory, destroy semaphores and closes output file
void main_proc(shdata_t * const shdata, const opts_t opts);                 // Body of main process
void oxygen(const int id, shdata_t * const shdata, const opts_t opts);      // Body of oxygen process
void hydrogen(const int id, shdata_t * const shdata, const opts_t opts);    // Body of hydrogen process
void join_queue(shdata_t * const shdata, pid_t pid, enum process_type pt);  // Provides atoms join queues

#endif