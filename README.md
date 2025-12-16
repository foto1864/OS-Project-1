# Operating Systems Project 1 - sdi2200207

## 1. Overview

This project implements a multi-process dialog messaging system using:

- System V shared memory
- POSIX semaphores
- POSIX threads

Each execution of the program starts an independent process.
Processes that use the same dialog ID end up in the same dialog and can exchange messages in real time.

Inside every process, two threads run in parallel:

- a reader thread (receives and prints messages)
- a writer thread (reads user input and sends messages)

All dialogs, participants, and messages live in shared memory, so processes don’t need any parent–child relationship or pipes to talk to each other.

The main goal is simple:
correct synchronization, no races, and clean shutdown.
When the last process exits, nothing is left behind.

## 2. High-Level Architecture

### 2.1 Processes and Dialogs

Each execution of ./dialog <dialog_id> starts one process

Processes with the same dialog_id participate in the same dialog

System limits:

- Maximum dialogs: 32
- Maximum processes per dialog: 16
- Maximum messages per dialog: 512
- Maximum message length: 256 characters

Each dialog behaves like a shared chat room.
Messages are broadcast to all active participants.

### 2.2 Threads per Process

Each process spawns two threads:

1. A reader thread that waits for new messages and prints unread messages
2. A writer thread that reads input from stdin and sends messages to the dialog

Both threads share a termination flag so they can shut down together without blocking forever on I/O.

### 2.3 Shared Memory Layout

All shared state lives inside a single shared_memory_t structure stored in System V shared memory.

shared_memory_t
├── total_processes
├── dialogs[32]
│   ├── dialog_t
│   │   ├── participants[16]
│   │   ├── slot semaphores[16]
│   │   └── messages[512]


Important rule:
No pointers inside shared memory.
Only plain data. No surprises when multiple processes attach.

## 3. Synchronization Design

### 3.1 Global Mutex (Inter-Process)

A named POSIX semaphore (/mutex) is used as a global mutex. This semaphore protects:

1. shared memory initialization
2. joining and leaving dialogs
3. sending and receiving messages
4. cleanup when processes exit

Only one process modifies shared memory at a time.
No races. No half-written state.

### 3.2 Per-Slot Semaphores (Notifications)

Each dialog contains one semaphore per participant slot.

Key points:

- stored directly inside shared memory
- initialized with pshared = 1
- reader thread blocks on it
- writer threads sem_post() when messages arrive

This lets reader threads sleep properly instead of burning CPU in loops.

### 3.3 Thread Coordination (Termination Flag)

Each process uses a shared termination flag:

volatile sig_atomic_t terminate_flag;

Why this works:

- sig_atomic_t means atomic reads/writes
- volatile means the compiler doesn’t get clever trying to optimize
- both threads see the same memory

This allows:

- reader thread to request shutdown when it sees TERMINATE
- writer thread to exit even if stdin is idle
- no thread cancellation, no signals, no hacks

## 4. Message Lifecycle

### 4.1 Sending a Message

- writer thread reads input from stdin
- finds a free message slot
- writes the message into shared memory
- marks the message as unread for all participants
- wakes all active slots using semaphores

Messages are broadcasted, not point-to-point.

### 4.2 Receiving a Message

- reader thread waits on its slot semaphore
- scans messages for unread entries
- prints message contents
- marks message as read for its slot
- checks for the string TERMINATE

### 4.3 Message Cleanup

After each receive pass:

- messages are checked
- if all active participants have read a message, the slot is cleared
- the slot becomes reusable
- Memory usage stays bounded.
- No message pile-up.

## 5. Program Flow

### 5.1 Startup

- validate command-line arguments
- create and attach shared memory
- open global mutex semaphore
- initialize shared memory (only once)
- activate dialog if needed
- join dialog and acquire slot index
- spawn reader and writer threads

### 5.2 Runtime

- writer thread handles input using poll()
- reader thread blocks on semaphores
- messages flow asynchronously
- typing TERMINATE starts shutdown

### 5.3 Shutdown

- threads exit cooperatively
- process releases its dialog slot
- global process counter is decremented

If this is the last process:

- destroy all per-slot semaphores
- unlink the global mutex
- detach and remove shared memory

No leaked IPC objects.
System left clean.

## 6. Error Handling and Safety

- strict argument validation
- bounded string operations
- hard limits enforced
- explicit exit codes
- all shared access protected
- no pointers crossing process boundaries

## 7. Compilation and Execution

### 7.1 Compile

make

or manually:

gcc -std=c11 -Wall -Wextra -pthread src/*.c -o dialog

### 7.2 Run

Open multiple terminals:

./dialog 0
./dialog 0
./dialog 0

All processes join dialog 0.

To terminate the dialog, type: TERMINATE from any participant.

## 8. Design Choices (Why It’s Built This Way)

- shared memory instead of pipes so that we establish many-to-many communication
- semaphores instead of busy waiting so that we achieve no CPU abuse
- threads split I/O so that there is no blocking
- no pointers in shared memory so that we can have predictable behavior for the program
- explicit cleanup so that no IPC trash is left behind

## 9. Conclusion

This project is a full implementation of a multi-process messaging system using classic UNIX tools.

- Processes, threads, shared memory, and semaphores all working together.
- No undefined behavior.
- No silent races.
- No abandoned resources.