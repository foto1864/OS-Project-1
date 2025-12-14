#ifndef UTILS_H
#define UTILS_H

/*  utils.h
    The utils.h file contains declaration of structs and function prototypes
    along with the inclusion of many libraries that are used in the project.
*/ 

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

/*  We define some constants that signify the maximum capacities of the messaging system.
    Each message can be up to 256 characters long, each dialog can contain up to 512 messages in total.
    The max amount of dialog that can occur simultaneously is 32 and each dialog can have up to 16
    participating processes at once. Under that are some error codes.
*/

#define MAX_MSG_SIZE 256
#define MAX_MSGS_PER_DIALOG 512
#define MAX_DIALOG_PARTICIPANTS 16
#define MAX_DIALOGS 32

#define LIMIT_REACHED 10

/*  A general comment is that we have avoided the use of booleans inside the code as ints are easier to work with.
    Since our memory is not limited to the point that we need to optimize 1 byte instead of 4, we have gone with int.
*/

/*  The message_t struct represents a single message inside a dialog.
    Since the struct is stored inside shared memory we don't use pointers, so all fields are
    just plain data types. Below is the logic behind the picking of all data types:
    message_exists: marks whether the specified slot message is in use (could have been a boolean).
    dialog_id: the dialog in which the message is present
    sender_slot: the index of the sender inside the dialog
    msg_text: the actual text being carried by the message.
    has_been_read_by: marks the index of the participants slot that has seen the message (could have also been a boolean) 

*/

typedef struct {
    int message_exists;
    int dialog_id;
    int sender_slot;
    pid_t sender_pid;
    char msg_text[MAX_MSG_SIZE];
    int has_been_read_by[MAX_DIALOG_PARTICIPANTS];
} message_t;

/*  The dialog_t struct represents a single dialog between processes:
    is_active: indicates whether the specified dialog_slot is used.
    dialog_id: unique id for the dialog
    dialog_participants: the number of active participants (processes) in a dialog
    slots_used: marks which participant slots are occupied.
    pids: the process ids of all the processes that are taking part in the dialog
    slot_sem: one semaphore per participant slot
    sem_initialized: initializer for the semafores, ensures semaphores are initialized only once
    messages: the messages contained in th dialog.
*/
typedef struct {
    int is_active;
    int dialog_id;
    int dialog_participants;
    int slot_used[MAX_DIALOG_PARTICIPANTS];
    pid_t pids[MAX_DIALOG_PARTICIPANTS];
    sem_t slot_sem[MAX_DIALOG_PARTICIPANTS];
    int sem_initialized;
    message_t messages[MAX_MSGS_PER_DIALOG];
} dialog_t;

/*  The shared_memory_t struct is the top level structure which gets saved in shared memory
    and it is accessed by all processes.
    is_initialized: ensures initialization happens only once
    total_processes: the number of processes present in shared memory
    dialogs: array of all dialogs in the system
*/
typedef struct {
    int is_initialized;
    int total_processes;
    dialog_t dialogs[MAX_DIALOGS];
} shared_memory_t;


/*  Global mutex semaphore. It is used to protect critical sections of the code that modify
    shared memory. It is shared by all processes and prevents race conditions between them.
*/
extern sem_t *mutex;


////////////////////////// FUNCTIONS //////////////////////////

/*  Checks whether the command line arguments given to the program are valid. 
    It returns the dialog_id on success and it exits the program on failure. 
*/
int check_argument_validity(int argc, char* argv[]);

/*  Creates a shared memory segment if one does not already exist and attaches it
    to the process address space. 
    Input: the parameter that receives the shared memory ID
    Output: a pointer to the now attached shared_memory_t structre.
*/
shared_memory_t* create_and_attach_shared_mem(int *out_shm_id);

/*  Initializes the shared memory contents. Sets all initial values for all dialogs
    and messages and marks the shared memory as initialized. Should be called inside 
    a mutex because it must be called only once, even if many processes start simultaneously.
*/
void init_shared_memory(shared_memory_t *shared_mem);

/*  Assings the calling proccess to a free slot in the dialog they selected.    
    It returns the slot_index assigned to the process on success, or a negative
    value if the dialog is full.
*/
int join_dialog(dialog_t *dialog, pid_t pid);

/*  Removes a process from a dialog. This happens when a "TERMINATE" message is received
    or when a process exits a dialog unexpectedly. This function frees the participant slot, 
    it updates the metadata of the dialog, and it might trigger a cleanup if the dialog
    becomes empty after the process' departure.
*/
void leave_dialog(shared_memory_t *shared_mem, dialog_t *dialog, int my_slot);

/*  Performs cleanup after all processes have exited the program. It destroys the semaphores
    and it detaches and removes the shared memory segment from the system.
*/
void cleanup_if_last_process(shared_memory_t *shared_mem, int shm_id);


#endif
