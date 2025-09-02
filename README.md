# Multi-Type File Server System in C

A modular and extensible **client-server file management system** implemented in **C using TCP sockets**, supporting type-specific file handling for `.c`, `.txt`, and `.pdf` files. This system enables file operations such as upload, download, delete, display, and tar archiving through a central controller and specialized sub-servers.

---

## Table of Contents

- [Introduction](#introduction)
- [System Architecture](#system-architecture)
- [Features](#features)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Running the System](#running-the-system)
- [Client Usage](#client-usage)
- [Testing Scenarios](#testing-scenarios)
- [Directory Structure](#directory-structure)
- [Known Limitations](#known-limitations)
- [Future Enhancements](#future-enhancements)

---

## Introduction

This project demonstrates a **file management system** using **multi-process socket programming** in C. The architecture separates file processing based on type — enabling more efficient, scalable, and maintainable solutions.

It is designed for:
- Operating Systems coursework
- Networking and systems-level programming portfolios
- Demonstrating file transfer, multiplexing, directory handling, and modular C design

---

## System Architecture

```text
┌───────────────┐
│   client24s   │ ←─── User interface (CLI)
└──────┬────────┘
       │ TCP Socket (127.0.0.1:50501)
       ▼
┌──────────────┐
│    Smain     │ ←── Handles `.c` files
└────┬────┬────┘
     │    │
     ▼    ▼
┌───────┐ ┌───────┐
│ Stext │ │ Spdf  │
│ .txt  │ │ .pdf  │
└───────┘ └───────┘
```

- Smain (port 50501): Central controller, handles `.c` files and delegates `.txt`/`.pdf` operations.
- Stext (port 50502): Handles `.txt` files.
- Spdf (port 50503): Handles `.pdf` files.

---


## Features

- Upload files to appropriate server based on extension
- Download individual files
- Delete files from specific file-type directories
- Display a combined list of all available files
- Download `.tar` archives of `.c`, `.txt`, or `.pdf` files
- Sub-server concurrency using `fork()`
- Auto-directory creation using `mkdir -p`
- Modular and extensible file type handling

---

## Installation

### Prerequisites

- GCC Compiler
- Linux/macOS terminal
- Permissions to create files and directories

### Build

Run the following commands to compile all components:

```bash
gcc client24s.c -o client24s
gcc Smain.c -o Smain
gcc Stext.c -o Stext
gcc Spdf.c -o Spdf
```

### Running the System

#### Step 1: Start the Sub-Servers

In separate terminals:
```bash
./Stext     # Terminal 1 (port 50502)
./Spdf      # Terminal 2 (port 50503)
```
#### Step 2: Start the Main Server

```bash
./Smain     # Terminal 3 (port 50501)
```

#### Step 3: Start the Client

```bash
./client24s # Terminal 4 (CLI interface)
```

## Client Usage
Once inside the client24s$ prompt, the following commands are supported:

| Command                                 | Description                                              |
|-----------------------------------------|----------------------------------------------------------|
| `ufile <filename> <destination_path>`   | Upload a file                                            |
| `dfile <filename>`                      | Download a file                                          |
| `rmfile <filename>`                     | Delete a file                                            |
| `dtar <.filetype>`                      | Download `.tar` archive of `.c`, `.txt`, or `.pdf` files |
| `display <pathname>`                    | Show list of all files                                   |
| `exit`                                  | Exit the client                                          |

## Testing Scenarios

1. Upload a File

```bash
ufile demo.txt /home/user/smain/stext
```

File is sent to the text sub-server (stext/).

2. Download a File

```bash
dfile sample.pdf
```

File is retrieved from the appropriate sub-server.

3. Delete a File

```bash
rmfile main.c
```
Removes the file from smain/.

4. Download Archive
```bash
dtar .txt
```
Downloads a .tar archive of all .txt files.

5. Display Files
```bash
display /home/user/smain
```
Displays all .c, .txt, and .pdf files across all servers.

## Directory Structure & Routing

| File Type | Handled By | Directory   |
|-----------|------------|-------------|
| .c        | Smain      | ./smain/    |
| .txt      | Stext      | ./stext/    |
| .pdf      | Spdf       | ./spdf/     |

- Directories are created dynamically.
- Relative paths from destination are preserved.

## Archive Management
When a client runs:

```bash
dtar .pdf
```

The system:
1. Identifies all .pdf files in the spdf/ directory
2. Creates a .tar archive (e.g., pdffiles.tar)
3. Sends the archive to the client
4. Deletes the archive from the server after transmission

Supported archive types: .c, .txt, .pdf

## Known Limitations
- No file overwrite detection or confirmation
- No SSL/TLS encryption (plaintext transmission)
- No user authentication or access control
- Smain is single-threaded (no concurrency support)
- Limited input validation for paths and filenames

## Future Enhancements
- Add multi-threading or fork() to Smain for concurrent client handling
- Secure the system with TLS/SSL
- Extend support to other file types (e.g., .docx, .jpg)
- Add user login and permissions system
- Expose REST API or build a web-based interface
- Add metadata tracking: timestamps, file sizes, owners
