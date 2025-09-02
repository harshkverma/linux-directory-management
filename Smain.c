#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>  // For handling file paths
#include <unistd.h>  // For directory operations

// Define constants for server communication
#define PORT 50501
#define BUFFER_SIZE 1024
#define STEXT_IP "127.0.0.1"
#define STEXT_PORT 50502
#define SPDF_IP "127.0.0.1"
#define SPDF_PORT 50503

// Function declarations for handling different commands
void process_upload_file(const char *filename, const char *destination_path, int client_socket);
void process_download_file(const char *filename, int client_socket);
void process_remove_file(const char *filename, int client_socket);
void process_archive_request(const char *filetype, int client_socket);
void transmit_file_to_client(const char *filepath, int client_socket);
void fetch_file_from_sub_server(const char *filename, const char *server_ip, int server_port, int client_socket, const char *operation);
void fetch_archive_from_sub_server(const char *filetype, const char *server_ip, int server_port, int client_socket);
void process_display_request(const char *pathname, int client_socket);
void retrieve_and_combine_file_lists(const char *filetype, const char *server_ip, int server_port, int output_fd);
void combine_and_send_file_list(const char *pathname, int client_socket);

int initialize_server_socket(int port);

// Set up a server socket and listen for incoming connections
int initialize_server_socket(int port) {
    int server_fd, opt = 1;
    struct sockaddr_in address;

    // Create a socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    // Assign the socket to the specified port
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Socket binding failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server initialized and listening on port %d\n", port);
    return server_fd;
}

// Handle the uploading of a file to the server
void process_upload_file(const char *filename, const char *destination_path, int client_socket) {
    char base_dir[BUFFER_SIZE];
    
    // Get the current working directory
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        perror("Failed to get current directory");
        return;
    }

    printf("Processing upload: filename=%s, destination=%s\n", filename, destination_path);

    char *ext = strrchr(filename, '.');
    char target_dir[BUFFER_SIZE];

    // Determine the appropriate target directory based on file extension
    if (ext) {
        if (strcmp(ext, ".txt") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/stext", base_dir);
        } else if (strcmp(ext, ".pdf") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/spdf", base_dir);
        } else if (strcmp(ext, ".c") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/smain", base_dir);
        } else {
            const char *error_message = "Unsupported file type.\n";
            send(client_socket, error_message, strlen(error_message), 0);
            fprintf(stderr, "Unsupported file type\n");
            return;
        }
    } else {
        const char *error_message = "File has no extension.\n";
        send(client_socket, error_message, strlen(error_message), 0);
        fprintf(stderr, "File has no extension\n");
        return;
    }

    // Extract relative sub-directory from destination path
    const char *sub_dir = destination_path + strlen("/home/{{user}}/smain");
    if (*sub_dir == '/') sub_dir++;

    // Create the final path for the file
    char final_destination[BUFFER_SIZE];
    snprintf(final_destination, sizeof(final_destination), "%s/%s", target_dir, sub_dir);

    // Ensure the necessary directories exist
    char command[512];
    snprintf(command, sizeof(command), "mkdir -p %s", final_destination);
    system(command);

    // Construct the full file path for storage
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", final_destination, filename);

    // Open the file for writing
    FILE *file = fopen(full_path, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }

    // Receive the file content from the client and save it
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("File '%s' successfully saved at '%s'\n", filename, full_path);
}

// Handle downloading a file from the server
void process_download_file(const char *filename, int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        perror("Failed to get current directory");
        return;
    }

    char *ext = strrchr(filename, '.');
    char target_dir[BUFFER_SIZE];
    char filepath[BUFFER_SIZE];

    // Determine the appropriate directory based on file extension
    if (ext) {
        if (strcmp(ext, ".c") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/smain", base_dir);
        } else if (strcmp(ext, ".txt") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/stext", base_dir);
        } else if (strcmp(ext, ".pdf") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/spdf", base_dir);
        } else {
            const char *error_message = "Unsupported file type.\n";
            send(client_socket, error_message, strlen(error_message), 0);
            fprintf(stderr, "Unsupported file type\n");
            return;
        }

        // Extract the relative path and construct the full file path
        const char *relative_path = filename + strlen("/home/{{user}}/smain");
        if (*relative_path == '/') relative_path++;
        snprintf(filepath, sizeof(filepath), "%s/%s", target_dir, relative_path);

        // Check if the file exists before attempting to send
        if (access(filepath, F_OK) != 0) {
            const char *error_message = "File not found.\n";
            send(client_socket, error_message, strlen(error_message), 0);
            fprintf(stderr, "File not found at '%s'\n", filepath);
            return;
        }

        // Send the file to the client
        printf("Sending file: %s\n", filepath);
        transmit_file_to_client(filepath, client_socket);
        printf("File transfer completed for '%s'\n", filepath);
    } else {
        const char *error_message = "File has no extension.\n";
        send(client_socket, error_message, strlen(error_message), 0);
        fprintf(stderr, "File has no extension\n");
    }
}

// Handle file deletion on the server
void process_remove_file(const char *filename, int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        perror("Failed to get current directory");
        return;
    }

    char target_dir[BUFFER_SIZE];
    char filepath[BUFFER_SIZE];

    char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcmp(ext, ".c") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/smain", base_dir);
        } else if (strcmp(ext, ".txt") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/stext", base_dir);
        } else if (strcmp(ext, ".pdf") == 0) {
            snprintf(target_dir, sizeof(target_dir), "%s/spdf", base_dir);
        } else {
            const char *error_message = "Unsupported file type.\n";
            send(client_socket, error_message, strlen(error_message), 0);
            fprintf(stderr, "Unsupported file type\n");
            return;
        }

        // Extract the relative path and construct the full file path
        const char *relative_path = filename + strlen("/home/{{user}}/smain");
        if (*relative_path == '/') relative_path++;
        snprintf(filepath, sizeof(filepath), "%s/%s", target_dir, relative_path);

        // Attempt to delete the file
        if (remove(filepath) == 0) {
            const char *success_message = "File deleted successfully.\n";
            send(client_socket, success_message, strlen(success_message), 0);
            printf("File '%s' deleted successfully\n", filepath);
        } else {
            perror("Failed to delete file");
            const char *error_message = "Failed to delete file.\n";
            send(client_socket, error_message, strlen(error_message), 0);
        }
    } else {
        const char *error_message = "File has no extension.\n";
        send(client_socket, error_message, strlen(error_message), 0);
        fprintf(stderr, "File has no extension\n");
    }
}

// Handle requests to create and transmit tar files of specified file types
void process_archive_request(const char *filetype, int client_socket) {
    char base_dir[BUFFER_SIZE];
    if (getcwd(base_dir, sizeof(base_dir)) == NULL) {
        perror("Failed to get current directory");
        return;
    }

    char tar_path[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    // Create tar file based on the specified file type
    if (strcmp(filetype, ".txt") == 0) {
        snprintf(tar_path, sizeof(tar_path), "%s/stext/textfiles.tar", base_dir);
        snprintf(command, sizeof(command), "find %s/stext -type f -name '*.txt' -print0 | tar --null -cvf %s --files-from=-", base_dir, tar_path);
    } else if (strcmp(filetype, ".pdf") == 0) {
        snprintf(tar_path, sizeof(tar_path), "%s/spdf/pdffiles.tar", base_dir);
        snprintf(command, sizeof(command), "find %s/spdf -type f -name '*.pdf' -print0 | tar --null -cvf %s --files-from=-", base_dir, tar_path);
    } else if (strcmp(filetype, ".c") == 0) {
        snprintf(tar_path, sizeof(tar_path), "%s/smain/cfiles.tar", base_dir);
        snprintf(command, sizeof(command), "find %s/smain -type f -name '*.c' -print0 | tar --null -cvf %s --files-from=-", base_dir, tar_path);
    } else {
        const char *error_message = "Unsupported file type for archive creation.\n";
        send(client_socket, error_message, strlen(error_message), 0);
        fprintf(stderr, "Unsupported file type for archive creation\n");
        return;
    }

    printf("Running command: %s\n", command);
    int result = system(command);

    // Check if the tar file was successfully created and send it
    if (result == 0 && access(tar_path, F_OK) == 0) {
        transmit_file_to_client(tar_path, client_socket);
        sleep(1);  // Ensure the file is not in use before deleting
        if (remove(tar_path) == 0) {
            printf("Successfully removed tar file '%s'\n", tar_path);
        } else {
            perror("Failed to remove tar file");
        }
        printf("Successfully created and sent tar file for %s files\n", filetype);
    } else {
        perror("Failed to create tar file");
        const char *error_message = "Failed to create tar file.\n";
        send(client_socket, error_message, strlen(error_message), 0);
    }
}

// Send a file to the client over the socket
void transmit_file_to_client(const char *filepath, int client_socket) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
    printf("File '%s' successfully transmitted\n", filepath);
}

// Retrieve and send a file from a sub-server
void fetch_file_from_sub_server(const char *filename, const char *server_ip, int server_port, int client_socket, const char *operation) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(server_port) };
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Failed to connect to sub-server");
        close(sock);
        return;
    }

    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s %s", operation, basename((char *)filename));
    send(sock, message, strlen(message), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        send(client_socket, buffer, bytes_received, 0);
    }

    close(sock);
}

// Retrieve and send an archive file from a sub-server
void fetch_archive_from_sub_server(const char *filetype, const char *server_ip, int server_port, int client_socket) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(server_port) };
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Failed to connect to sub-server");
        close(sock);
        return;
    }

    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "dtar %s", filetype);
    send(sock, message, strlen(message), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_socket, buffer, bytes_received, 0);
    }

    close(sock);
}

// Handle file list display requests by combining file lists from multiple servers
void process_display_request(const char *pathname, int client_socket) {
    FILE *temp_file = fopen("/tmp/file_list.txt", "w");
    if (!temp_file) {
        perror("Failed to create temporary file for file list");
        return;
    }

    combine_and_send_file_list(pathname, fileno(temp_file));

    fclose(temp_file);

    temp_file = fopen("/tmp/file_list.txt", "r");
    if (!temp_file) {
        perror("Failed to open temporary file for reading");
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), temp_file)) {
        send(client_socket, line, strlen(line), 0);
    }

    fclose(temp_file);
    remove("/tmp/file_list.txt");
    printf("File list successfully sent\n");
}

// Fetch file lists from sub-servers and combine them into a single list
void retrieve_and_combine_file_lists(const char *filetype, const char *server_ip, int server_port, int output_fd) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {.sin_family = AF_INET, .sin_port = htons(server_port)};
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Failed to connect to sub-server");
        close(sock);
        return;
    }

    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "display %s", filetype);
    send(sock, message, strlen(message), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        write(output_fd, buffer, bytes_received);
    }

    close(sock);
}

// Combine file lists from different servers and send them to the client
void combine_and_send_file_list(const char *pathname, int client_socket) {
    char command[BUFFER_SIZE];
    FILE *temp_file = fopen("/tmp/file_list.txt", "w+");

    if (!temp_file) {
        perror("Failed to create temporary file for file list");
        return;
    }

    // List .c files and append to the temp file
    snprintf(command, sizeof(command), "find %s -type f -name '*.c' -exec basename {} \\;", pathname);
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("Failed to list .c files");
        fclose(temp_file);
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), pipe)) {
        fprintf(temp_file, "%s", line);
    }
    pclose(pipe);

    // Append .pdf and .txt file lists from sub-servers
    fseek(temp_file, 0, SEEK_END);
    retrieve_and_combine_file_lists("pdf", SPDF_IP, SPDF_PORT, fileno(temp_file));
    fseek(temp_file, 0, SEEK_END);
    retrieve_and_combine_file_lists("txt", STEXT_IP, STEXT_PORT, fileno(temp_file));

    // Send the combined list to the client
    rewind(temp_file);
    while (fgets(line, sizeof(line), temp_file)) {
        if (send(client_socket, line, strlen(line), 0) == -1) {
            break;
        }
    }

    fclose(temp_file);
    printf("File list sent to client. Closing connection.\n");
    close(client_socket);
}

// Main function to run the server
int main() {
    int server_fd = initialize_server_socket(PORT);

    while (1) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int client_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_sock < 0) {
            perror("Failed to accept connection");
            continue;
        }

        char buffer[BUFFER_SIZE];
        int bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            char command[16], filename[BUFFER_SIZE], destination_path[BUFFER_SIZE];
            printf("Received command: %s\n", buffer);

            // Parse the command and execute the appropriate handler
            if (sscanf(buffer, "%15s %1023s %1023s", command, filename, destination_path) == 3) {
                if (strcmp(command, "ufile") == 0) {
                    process_upload_file(filename, destination_path, client_sock);
                } else {
                    printf("Unsupported command: %s\n", command);
                }
            } else if (sscanf(buffer, "%15s %1023s", command, filename) == 2) {
                if (strcmp(command, "dfile") == 0) {
                    process_download_file(filename, client_sock);
                } else if (strcmp(command, "rmfile") == 0) {
                    process_remove_file(filename, client_sock);
                } else if (strcmp(command, "dtar") == 0) {
                    process_archive_request(filename, client_sock);
                } else if (strcmp(command, "display") == 0) {
                    process_display_request(filename, client_sock);
                } else {
                    printf("Unsupported command: %s\n", command);
                }
            } else {
                fprintf(stderr, "Failed to parse command: %s\n", buffer);
            }
        } else {
            printf("No data received or client disconnected.\n");
        }

        close(client_sock);
    }

    return 0;
}
