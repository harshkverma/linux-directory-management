#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>  // For the `getcwd()` function

#define PORT 50502
#define BUFFER_SIZE 1024

// Function prototypes
int initialize_server();
void process_client_request(int client_sock);
void transfer_file_to_client(const char *filename, int client_socket);
void remove_file(const char *filename, int client_socket);
void generate_tar_archive(int client_socket);
void display_files(int client_sock);

// Function to initialize the server and set up the listening socket
int initialize_server() {
    int server_fd, opt = 1;
    struct sockaddr_in address;

    // Create a socket for the server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("Error: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow reuse of the address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf("Error: Setsockopt failed\n");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 10) < 0) {
        printf("Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Stext server initialized and listening on port %d...\n", PORT);
    return server_fd;
}

// Function to handle client requests
void process_client_request(int client_sock) {
    char message[BUFFER_SIZE], command[16], filename[256];
    int bytes_read;

    // Read the incoming message from the client
    bytes_read = recv(client_sock, message, sizeof(message) - 1, 0);
    if (bytes_read <= 0) {
        printf("Error: Failed to read message\n");
        return;
    }
    message[bytes_read] = '\0';  // Null-terminate the message
    printf("Received message: %s\n", message);  // For debugging purposes

    // Parse the command and filename from the message
    sscanf(message, "%15s %255s", command, filename);
    printf("Parsed command: %s, filename: %s\n", command, filename);

    // Handle the command based on its type
    if (strcmp(command, "RETRIEVE") == 0) {
        transfer_file_to_client(filename, client_sock);
        printf("Successfully retrieved file: %s\n", filename);
    } else if (strcmp(command, "DELETE") == 0) {
        remove_file(filename, client_sock);
        printf("Successfully deleted file: %s\n", filename);
    } else if (strcmp(command, "dtar") == 0) {
        generate_tar_archive(client_sock);
        printf("Successfully created and sent tar archive\n");
    } else if (strcmp(command, "display") == 0) {
        display_files(client_sock);
        printf("Successfully displayed files\n");
    } else {
        char *error_message = "Unsupported command received.\n";
        send(client_sock, error_message, strlen(error_message), 0);
        printf("Error: Unsupported command received\n");
    }
}

// Function to transfer a file to the client
void transfer_file_to_client(const char *filename, int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        return;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/stext/%s", base_dir, filename);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        printf("Error: Failed to open file %s\n", filepath);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        int bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            printf("Error: Failed to send file data\n");
            break;
        }
        printf("Sent %d bytes from %s\n", bytes_sent, filepath);
    }

    fclose(file);
    printf("File transfer completed for %s\n", filepath);
}

// Function to remove a file from the server
void remove_file(const char *filename, int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        return;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/stext/%s", base_dir, filename);

    if (remove(filepath) == 0) {
        char *success_message = "File deleted successfully.\n";
        send(client_socket, success_message, strlen(success_message), 0);
        printf("File deleted successfully: %s\n", filepath);
    } else {
        printf("Error: Failed to delete file %s\n", filepath);
        char *error_message = "Failed to delete file.\n";
        send(client_socket, error_message, strlen(error_message), 0);
    }
}

// Function to create and send a tar archive of .txt files
void generate_tar_archive(int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        return;
    }

    // Command to create a tar archive of all .txt files in the stext directory
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "tar -cvf %s/stext/textfiles.tar -C %s/stext --exclude=textfiles.tar *.txt", base_dir, base_dir);
    system(command);

    // Send the tar file to the client
    transfer_file_to_client("textfiles.tar", client_socket);

    // Remove the tar file after sending it
    snprintf(command, sizeof(command), "%s/stext/textfiles.tar", base_dir);
    remove(command);
    printf("Tar file for .txt files created, sent, and removed successfully\n");
}

// Function to display the list of .txt files to the client
void display_files(int client_sock) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        printf("Error: getcwd() failed\n");
        return;
    }

    // Command to find and list all .txt files in the stext directory
    const char *file_extension = ".txt";
    char command[BUFFER_SIZE];
    FILE *pipe;
    char line[BUFFER_SIZE];

    snprintf(command, sizeof(command), "find %s/stext -type f -name '*%s' -exec basename {} \\;", base_dir, file_extension);
    pipe = popen(command, "r");
    if (!pipe) {
        printf("Error: Failed to list files\n");
        return;
    }

    while (fgets(line, sizeof(line), pipe)) {
        send(client_sock, line, strlen(line), 0);
        printf("Listed file: %s", line);
    }

    pclose(pipe);
    printf("File list sent successfully\n");
}

// Main function to start the server and handle incoming connections
int main() {
    int server_fd = initialize_server();
    printf("Stext server is now listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            printf("Error: Accept failed\n");
            continue;
        }

        int pid = fork();
        if (pid == 0) {
            close(server_fd);
            process_client_request(client_sock);
            exit(0);
        } else if (pid > 0) {
            close(client_sock);
        } else {
            printf("Error: Fork failed\n");
        }
    }

    return 0;
}
