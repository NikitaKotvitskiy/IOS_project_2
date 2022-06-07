// oxygen.c
// Řešení IOS-P2 28.4.2022
// Autor: Nikita Kotvitskiy (xkotvi01), FIT
// Proces kyslíku

#include "proj2.h"

static shdata_t * glob_shdata;
static int glob_id;
static const opts_t * glob_opts;

// Increments action_counter and write message in output file
static void print_message(enum message_type mes)
{
    sem_wait(&glob_shdata->output_file_sem);
    (glob_shdata->action_counter)++;
    switch (mes)
    {
        case START:
            fprintf(glob_shdata->output_file, "%d: O %d: started\n", glob_shdata->action_counter, glob_id);
            break;
        case QUEUE:
            fprintf(glob_shdata->output_file, "%d: O %d: going to queue\n", glob_shdata->action_counter, glob_id);
            break;
        case MOLECULE:
            fprintf(glob_shdata->output_file, "%d: O %d: creating molecule %d\n", glob_shdata->action_counter, glob_id, glob_shdata->molecule_count);
            break;
        case NOT_ENOUGH:
            fprintf(glob_shdata->output_file, "%d: O %d: not enough H\n", glob_shdata->action_counter, glob_id);
            break;
        case MOLECULE_FINISH:
            fprintf(glob_shdata->output_file, "%d: O %d: molecule %d created\n", glob_shdata->action_counter, glob_id, glob_shdata->molecule_count);
            break;
    }
    fflush(glob_shdata->output_file);
    sem_post(&glob_shdata->output_file_sem);
}

// Finishes process. Can be called from another function (s = 0) or by signal (s != 0)
// Case of calling by signal means that the process must be finished because of atom lack, so it will print appropriate message
// Case of calling from function means, that process can be finished normally, no messages will be printed
static void oxygen_finish(int s)
{
    if (s)
        print_message(NOT_ENOUGH);
    sem_post(&glob_shdata->ended_process_counter);
    exit(0); 
}

// Is called in case of atom lack. Waits for left hydrogen atoms of hydrogen in hydrogen queue and sends them signal to finish
static void hydrogen_clear()
{
    int left_hydrogen = glob_opts->NH - glob_shdata->hydrogen_queue_cursor;
    for (int i = 0; i < left_hydrogen; i++)
    {
        sem_wait(&glob_shdata->free_hydrogen_counter);
        kill(glob_shdata->hydrogen_queue[glob_shdata->hydrogen_queue_cursor], SIGUSR2);
        (glob_shdata->hydrogen_queue_cursor)++;
    }
}

// Is called in case of atoms lack. Waits for left oxygen atoms of hydrogen in oxygen queue and sends them signal to finish
// Then print message about atoms lack and finishes the process
static void oxygen_clear()
{
    int left_oxygen = glob_opts->NO - glob_shdata->oxygen_queue_cursor;
    for (int i = 0; i < left_oxygen; i++)
    {
        sem_wait(&glob_shdata->free_oxygen_counter);
        kill(glob_shdata->oxygen_queue[glob_shdata->oxygen_queue_cursor], SIGUSR2);
        (glob_shdata->oxygen_queue_cursor)++;
    }
    print_message(NOT_ENOUGH);
    oxygen_finish(0);
}

// Is called in case of enough hydrogen lack. Clears all left hydrogen and oxygen (that means that the process will be finished)
static void atom_lack()
{
    hydrogen_clear();
    oxygen_clear();
}

// Creates new H2O molecule. Also checks count of remaining atoms and call clear functions if atoms lack is detected
static void create_molecule()
{
    // Closing molecule_creating_sem, so other oxygen processes couldn't start creation
    sem_wait(&glob_shdata->molecule_creating_sem);

    // Decrementing free_oxygen_counter (to control count of remaining oxygen atoms), incrementing molecule_count and getting out of oxygen queue 
    sem_wait(&glob_shdata->free_oxygen_counter);
    (glob_shdata->molecule_count)++;
    (glob_shdata->oxygen_queue_cursor)++;

    // Control of left hydrogen count
    if (glob_opts->NH - glob_shdata->hydrogen_queue_cursor < 2)
        atom_lack();

    // Decrementing free_hydrogen_counter by two 
    // If enough atoms already are in hydrogen queue, then decrementing will be done immediately
    // Else, oxygen will sleep untill enaugh hydrogen will be in queue
    sem_wait(&glob_shdata->free_hydrogen_counter);
    sem_wait(&glob_shdata->free_hydrogen_counter);

    // Getting IDs of next two hydrogen atoms in hydrogen queue and getting them out of the queue
    // We have successfully decremented free_hydrogen_counter by two, so we can be sure that hydrogent are in queue,
    // so we don't need to get access to hydrogen_queue_sem
    pid_t hyd1, hyd2;
    hyd1 = glob_shdata->hydrogen_queue[glob_shdata->hydrogen_queue_cursor];
    hyd2 = glob_shdata->hydrogen_queue[glob_shdata->hydrogen_queue_cursor + 1];
    glob_shdata->hydrogen_queue_cursor += 2;

    // Printing about creation start and sending signals to hydrogen to they also print that message
    print_message(MOLECULE);
    kill(hyd1, SIGUSR1);
    kill(hyd2, SIGUSR1);

    // Waiting until hydrogens finish printing messages about creation start, then sleep for random time in range <0, TB>
    sem_wait(&glob_shdata->hydrogen_done_sem);
    sem_wait(&glob_shdata->hydrogen_done_sem);
    usleep((rand() % (glob_opts->TB + 1)) * MIKRO_IN_MILLI);

    // Printing about creation finish and sending signals to hydrogens to they also print that message
    print_message(MOLECULE_FINISH);
    kill(hyd1, SIGUSR1);
    kill(hyd2, SIGUSR1);

    // Waiting until hydrogens finish printing message about creation finish
    sem_wait(&glob_shdata->hydrogen_done_sem);
    sem_wait(&glob_shdata->hydrogen_done_sem);
    
    // We are going to read oxygen_in_queue_count, so we ask for access to oxygen_queue_sem, which is managing this variable
    sem_wait(&glob_shdata->oxygen_queue_sem);

    // Checking, if next oxygen is already in oxygen queue
    // If is, we send signal to that oxygen to it starts next molecule creation
    if (glob_shdata->oxygen_in_queue_count != glob_shdata->oxygen_queue_cursor)
        kill(glob_shdata->oxygen_queue[glob_shdata->oxygen_queue_cursor], SIGUSR1);
    // If is not and current oxygen was the last, we start cleaning left hydrogen
    else if (glob_shdata->oxygen_queue_cursor == glob_opts->NO)
        hydrogen_clear();
    
    // We don't need oxygen_in_queue_count anymore, so we return access to it
    sem_post(&glob_shdata->oxygen_queue_sem);

    // New molecule was created, so we can return access to molecule_creating_sem to other oxygens could start creation
    // Then process finishes
    sem_post(&glob_shdata->molecule_creating_sem);
    oxygen_finish(0);
}

// Sets handlers of signals
static void signal_set()
{
    // SIGUSR1 will work as command to new molecule cration start
    // While molecule is creating, oxygen process should ignore this signal
    struct sigaction act;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    act.sa_mask = set;
    act.sa_handler = create_molecule;
    sigaction(SIGUSR1, &act, 0);

    // SIGUSR2 will work as command to finish oxygen process
    // While process finishing both of signals will be ignored
    sigaddset(&set, SIGUSR2);
    act.sa_mask = set;
    act.sa_handler = oxygen_finish;
    sigaction(SIGUSR2, &act, 0);
}

// Body of oxygen process
void oxygen(const int id, shdata_t * const shdata, const opts_t opts)
{
    // These variables must be global to functions called by signal processing could use them
    glob_shdata = shdata;
    glob_id = id;
    glob_opts = &opts;

    // Printing messages about oxygen start, then sleep random time in range <0, TI>
    print_message(START);
    usleep((rand() % (opts.TI + 1)) * MIKRO_IN_MILLI);

    // Getting access to oxygen_queue and joining the queue
    sem_wait(&shdata->oxygen_queue_sem);
    print_message(QUEUE);
    join_queue(shdata, getpid(), OXY);

    // Setting signals for getting signals from other processes
    signal_set();

    // Returning access to oxygen_queue_sem and incrementing free_oxygen_counter (to control count of remaining oxygen atoms)
    sem_post(&shdata->oxygen_queue_sem);
    sem_post(&shdata->free_oxygen_counter);

    // Sending signal to next oxygen in oxygen queue to start creation
    // If next oxygen in queue has already started creation (or is waiting for previous creation end), then signal will be ignored
    // We also don't care about reading oxygen_queue_cursor outside of molecule_creating_sem because the only variants of it's value are:
    // 1.   Current oxygen is first oxygen joined the queue, cursor is automatically points to it, so current process send signal to itself,
    //      so current oxygen just will start molecule creation
    // 2.   Some process A has already started molecule creation, but hasn't changed oxygen_queue_cursor yet, so oxygen_queue_cursor points
    //      on process A and signal will be ignored
    // 3.   Some process A has already started molecule creation and has already changed oxygen_queue_cursor, so oxygen_queue_cursor points on
    //      correct next oxygen
    // 4.   Some process A has finished molecule craation (so oxygen_queue_cursor points on correct oxygen)
    // So we can see, that in any case signal sending is correct or ar least harmless
    kill(glob_shdata->oxygen_queue[shdata->oxygen_queue_cursor], SIGUSR1);

    // Process waits for signal to start creaton or to be finished in atoms lack case
    pause();
}