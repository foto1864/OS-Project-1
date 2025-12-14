# Operating Systems Project 1 

## 1. Overview

This project implements a multi-process dialog messaging system using System V shared memory, POSIX semaphores, and POSIX threads.

Multiple independent processes can join a dialog (identified by an integer ID) and exchange text messages in real time. Each process internally spawns two threads:

a reader thread, responsible for receiving and printing messages

a writer thread, responsible for reading user input and sending messages

All dialogs and messages are stored in shared memory, allowing unrelated processes (started from different terminals) to communicate without any parent–child relationship.

The system is designed with correctness, synchronization, and clean shutdown as first-class goals. When the last process exits, all shared resources are released, leaving no orphaned shared memory segments or semaphores behind.

## 2. High-Level Architecture

### 2.1 Processes and Dialogs

Each execution of ./dialog <dialog_id> starts one process
Processes with the same dialog ID participate in the same dialog

Up to:
32 dialogs total
16 processes per dialog
512 messages per dialog
256 characters per message

Each dialog behaves like a shared chat room. Messages are broadcasted to all active participants.

### 2.2 Threads per Process

Each process spawns two threads:

The responsibility of the the threads:
Reader thread: Waits for new messages and prints them
Writer thread: Reads user input and sends messages

These threads cooperate using a shared termination flag, ensuring clean and responsive shutdown without blocking on I/O.

### 2.3 Shared Memory Layout


All shared state lives inside a single shared_memory_t structure stored in System V shared memory.

shared_memory_t
├── total_processes
├── dialogs[32]
│   ├── dialog_t
│   │   ├── participants[16]
│   │   ├── slot semaphores[16]
│   │   └── messages[512]

No pointers are stored inside shared memory, only plain data. This ensures portability and correctness across processes.

## 3. Synchronization Design

### 3.1 Global Mutex (Inter-Process)

A named POSIX semaphore (/mutex) is used as a global mutex:

1. Protects all shared-memory modifications
2. Prevents race conditions between processes
3. Ensures one-time initialization and safe cleanup

Critical sections include:

1. shared memory initialization
2. joining/leaving dialogs
3. sending/receiving messages
4. cleanup operations

### 3.2 Per-Slot Semaphores (Notification)

Each dialog contains one semaphore per participant slot:

1. This seamphore is stored directly inside shared memory
2. It is initialized with pshared = 1
3. It is used by the reader thread to sleep efficiently
4. Writer threads call sem_post() to wake readers when messages arrive

This avoids busy-waiting for the CPU and scales cleanly with multiple participants.

### 3.3 Thread Coordination (Termination Flag)

Each process uses a shared termination flag: volatile sig_atomic_t terminate_flag;

Why this choice:

1. sig_atomic_t: guarantees atomic reads/writes
2. volatile: prevents compiler caching

Pointer-based sharing allows both threads to observe changes

This allows:
1. reader thread to request termination when it sees TERMINATE
2. writer thread to exit even if stdin is idle
3. no need for thread cancellation or signals

## 4. Message Lifecycle

### 4.1 Sending a Message

1. Writer thread reads input from stdin
2. Finds a free message slot
3. Writes message data into shared memory
4. Marks all participants as “unread”
5. Wakes all active participants via slot semaphores
6. Messages are broadcast, not point-to-point.

### 4.2 Receiving a Message

1. Reader thread waits on its slot semaphore
2. Scans messages for unread entries
3. Prints message content
4. Marks message as read for its slot
5. Checks for "TERMINATE"

### 4.3 Message Cleanup

After each receive pass:

1. messages are scanned - if all active participants have read a message, the slot is cleared
2. the message becomes reusable

This prevents unbounded growth and keeps memory usage stable.

## 5. Program Flow

### 5.1 Startup

1. Validate command-line arguments
2. Create/attach shared memory
3. Open global mutex semaphore
4. Initialize shared memory (once only)
5. Activate dialog if needed
6. Join dialog and get slot index
7. Spawn reader and writer threads

### 5.2 Runtime

1. Writer thread handles user input non-blockingly using poll()
2. Reader thread sleeps until notified
3. Messages are exchanged asynchronously
4. "TERMINATE" triggers coordinated shutdown

### 5.3 Shutdown

1. Threads terminate cooperatively
2. Process leaves its dialog slot
3. Global process counter is decremented

If this is the last process:

4. destroy all semaphores
5. unlink global mutex
6. detach and remove shared memory

This guarantees no leaked IPC resources.

## 6. Error Handling and Safety

1. Strict argument validation
2. Bounded string operations
3. Defensive checks on limits
4. Explicit exit codes
5. All critical sections protected
6. No undefined behavior from shared pointers

The implementation favors correctness and clarity over micro-optimizations.

## 7. Compilation and Execution

### 7.1 Compile

make
(or manually)
gcc -std=c11 -Wall -Wextra -pthread src/*.c -o dialog

### 7.2 Run

Open multiple terminals:

./dialog 0
./dialog 0
./dialog 0

All processes join dialog 0 and can exchange messages.

To terminate, type:

TERMINATE

Typed by any participant.

## 8. Design Decisions & Rationale

1. Shared memory instead of pipes: supports many-to-many communication
2. Semaphores instead of busy loops: efficient and scalable
3. Threads for I/O separation: avoids blocking
4. No pointers in shared memory: correctness across processes
5. Explicit cleanup logic: no IPC leaks

This design mirrors real OS-level IPC patterns while remaining understandable and robust.

## 9. Conclusion

This project demonstrates a complete, correct, and clean implementation of a multi-process communication system using classic UNIX primitives.

It combines process-level IPC, thread-level concurrency, and synchronization discipline into a cohesive whole — no hacks, no shortcuts, no undefined behavior.

It works.
It scales within its limits.
And when it dies, it cleans up after itself.
That’s the bar.