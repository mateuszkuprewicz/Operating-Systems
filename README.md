# Operating Systems Learning Projects

This repository contains a collection of small C programs that I wrote as exercises while learning about Operating Systems. These projects cover various fundamental concepts such as file system navigation, memory mapping, inter-process communication, and network programming.

## Projects Overview

Here is a breakdown of the programs included in this repository and what they do:

1.  **Directory Scan**
    * This program demonstrates file system traversal. It scans a directory (and potentially subdirectories) to list its contents, showing how to interact with directory structures and file metadata at the system call level.
    * **Key Concepts:** Directory structures, file permissions, POSIX directory APIs (`opendir`, `readdir`, `stat`).
2.  **Processes and Signals**
    * This program explores process creation, management, and inter-process communication (IPC) using signals. It illustrates how an operating system spawns processes, manages their execution flow, and allows them to communicate asynchronously via signals.
    * **Key Concepts:** Spawning child processes (`fork`, `exec`), reaping zombies (`waitpid`), registering signal handlers (`sigaction`, `kill`) and setting a signal mask (`sigprocmask`)
3.  **MMap (Memory Mapping)**
    * An exercise in memory management using the `mmap` system call. This program shows how to map a file or device into memory, allowing file contents to be accessed directly via memory pointers, which is a key concept in OS memory architecture.
    * **Key Concepts:** Virtual memory, file I/O optimization, `mmap()`, `munmap()`, multiple processes synchronisation, `pthread_mutex_t`, `pthread_mutexattr_setpshared`
4.  **Multi-thread Datagram Server**
    * A network programming project focusing on UDP (Datagram) communication and concurrency. This server uses multiple threads to handle incoming datagrams simultaneously, demonstrating concurrent execution and synchronization in a networking context.
    * **Key Concepts:** Connectionless protocols (UDP), threading (`pthread_create`), thread synchronization (mutexes/condition variables), and socket datagrams.
5.  **Tcp Linux Server**
    * A classic TCP server implementation. It demonstrates reliable, connection-oriented network communication using sockets. This includes handling client connections, reading/writing streams of data, and managing network file descriptors.
    * **Key Concepts:** Socket binding/listening, accepting connections (`socket`, `bind`, `listen`, `accept`), and stream data transfer.

## How to Build and Run

These projects are built using `make`. You will need a Unix-like environment (like macOS or Linux) with a C compiler (like `gcc` or `clang`) and `make` installed.

1.  **Navigate to a project directory:**
    Open your terminal and use the `cd` command to enter the directory of the project you want to run. For example:
    ```bash
    cd "Directory Scan"
    ```

2.  **Compile the code:**
    Each project contains a `Makefile`. To compile the program, simply type:
    ```bash
    make all
    ```
    This will execute the instructions in the Makefile and generate the executable file.

3.  **Run the executable:**
    After compiling, you can run the generated program. The name of the executable might vary depending on the project (check the Makefile or the output of the `make` command), but typically you run it like this:
    ```bash
    ./<executable_name>
    ```
    *(Note: You may need to provide specific arguments depending on the program's requirements. Review the `main.c` or any provided text files `Task.txt` in the project folder for specific usage instructions.)*

4.  **Clean up (Optional):**
    To remove the compiled executable and any object files, you can usually run:
    ```bash
    make clean
    ```
