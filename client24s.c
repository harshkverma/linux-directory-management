#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>  // For basename()
#include <unistd.h>  // For getcwd()

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 50501
#define BUFFER_SIZE 1024

// Function declarations
void send_file(const char *filename, const char *destination_path);
void download_file(const char *filename);
void remove_file(const char *filename);
void download_tar_file(const char *filetype);
void display_files(const char *pathname);
int connect_to_server();

// Function to establish a connection to the server
int connect_to_server() {
    int sock;
    struct sockaddr_in server_addr;

    // Create a socket for communication with the server
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error: Socket creation failed\n");
        return -1;
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Connection to server failed\n");
        close(sock);
        return -1;
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
    return sock;
}

// Function to send a file to the server
void send_file(const char *filename, const char *destination_path) {
    int sock = connect_to_server();
    if (sock < 0) return;

    // Prepare and send the upload command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "ufile %s %s\n", filename, destination_path);
    send(sock, command, strlen(command), 0);

    // Open the file to be sent
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: File open failed\n");
        close(sock);
        return;
    }

    // Read from the file and send its content to the server
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(sock);
    printf("File transmission completed and socket closed.\n");
}

// Function to download a file from the server
void download_file(const char *filename) {
    int sock = connect_to_server();
    if (sock < 0) return;

    // Prepare and send the download command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "dfile %s\n", filename);
    send(sock, command, strlen(command), 0);

    char *base_filename = basename((char *)filename);

    // Get the current working directory
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        close(sock);
        return;
    }

    // Define the full path for the downloaded file
    char destination_path[512];
    snprintf(destination_path, sizeof(destination_path), "%s/%s", base_dir, base_filename);

    // Receive the file from the server
    char buffer[BUFFER_SIZE];
    int bytes_received;
    FILE *file = fopen(destination_path, "wb");
    if (!file) {
        printf("Error: File open failed\n");
        close(sock);
        return;
    }

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(sock);
    printf("File downloaded successfully to %s\n", destination_path);
}

// Function to remove a file on the server
void remove_file(const char *filename) {
    int sock = connect_to_server();
    if (sock < 0) return;

    // Prepare and send the remove command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "rmfile %s\n", filename);
    send(sock, command, strlen(command), 0);

    // Receive and print the server's response
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    }

    close(sock);
}

// Function to download a tar file containing specific file types from the server
void download_tar_file(const char *filetype) {
    int sock = connect_to_server();
    if (sock < 0) return;

    // Prepare and send the tar download command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "dtar %s\n", filetype);
    send(sock, command, strlen(command), 0);

    // Define the tar file name based on the file type
    char tar_filename[256];
    if (strcmp(filetype, ".c") == 0) {
        strcpy(tar_filename, "cfiles.tar");
    } else if (strcmp(filetype, ".txt") == 0) {
        strcpy(tar_filename, "text.tar");
    } else if (strcmp(filetype, ".pdf") == 0) {
        strcpy(tar_filename, "pdf.tar");
    } else {
        printf("Invalid file type for dtar command.\n");
        close(sock);
        return;
    }

    // Get the current working directory
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        close(sock);
        return;
    }

    // Define the full path for the downloaded tar file
    char destination_path[512];
    snprintf(destination_path, sizeof(destination_path), "%s/%s", base_dir, tar_filename);

    // Receive the tar file from the server
    char buffer[BUFFER_SIZE];
    int bytes_received;
    FILE *file = fopen(destination_path, "wb");
    if (!file) {
        printf("Error: File open failed\n");
        close(sock);
        return;
    }

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(sock);
    printf("Tar file downloaded successfully %s\n", destination_path);
}

// Function to display the list of files on the server
void display_files(const char *pathname) {
    int sock = connect_to_server();
    if (sock < 0) return;

    // Prepare and send the display command
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "display %s\n", pathname);
    send(sock, command, strlen(command), 0);

    // Receive the list of files
    char buffer[BUFFER_SIZE];
    int bytes_received;

    printf("Files in %s:\n", pathname);

    // Loop to receive the list until the server stops sending
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  // Null-terminate the buffer to safely print it as a string
        printf("%s", buffer);  // Print each chunk received
    }

    if (bytes_received < 0) {
        printf("Error: Receiving data from server failed\n");
    } else {
        printf("\nFile list received successfully.\n");
    }

    close(sock);
}

// Main function for client interaction
int main() {
    char input[BUFFER_SIZE];
    char command[16], filename[256], destination_path[256];

    printf("Connected to Smain server at %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Enter commands in the format:\n");
    printf("1. ufile filename destination_path\n");
    printf("2. dfile filename\n");
    printf("3. rmfile filename\n");
    printf("4. dtar filetype\n");
    printf("5. display pathname\n");
    printf("Type 'exit' to quit\n");

    // Main command loop
    while (1) {
        printf("client24s$ ");
        if (fgets(input, sizeof(input), stdin) == NULL) break; // Handle EOF or error

        if (sscanf(input, "%15s %255s %255s", command, filename, destination_path) == 3 && strcmp(command, "ufile") == 0) {
            send_file(filename, destination_path);
        } else if (sscanf(input, "%15s %255s", command, filename) == 2) {
            if (strcmp(command, "dfile") == 0) {
                download_file(filename);
            } else if (strcmp(command, "rmfile") == 0) {
                remove_file(filename);
            } else if (strcmp(command, "dtar") == 0) {
                download_tar_file(filename);
            } else if (strcmp(command, "display") == 0) {
                display_files(filename);
            } else {
                printf("Invalid command or format. Please use:\n");
                printf("ufile filename destination_path\n");
                printf("dfile filename\n");
                printf("rmfile filename\n");
                printf("dtar filetype\n");
                printf("display pathname\n");
            }
        } else if (strncmp(input, "exit", 4) == 0) {
            break;
        } else {
            printf("Invalid command or format. Please use:\n");
            printf("ufile filename destination_path\n");
            printf("dfile filename\n");
            printf("rmfile filename\n");
            printf("dtar filetype\n");
            printf("display pathname\n");
        }
    }

    return 0;
}
