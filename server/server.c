/*
 * miniDFS Server
 * Handles client connections and file operations
 * Supports concurrent clients using Windows threads
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include "../common/protocol.h"
#include <dirent.h>

#define MAX_LOCKS 100

typedef struct {
    char path[MAX_PATH_LEN];
    HANDLE mutex;
} file_lock_t;

file_lock_t file_locks[MAX_LOCKS];
int lock_count = 0;
HANDLE lock_table_mutex;

#pragma comment(lib, "ws2_32.lib")
/*
 * Handle a client request
 * Performs file operations based on request type
 * Sends response back to client
 */
void handle_request(SOCKET client_fd, dfs_request_t *request) {
    // create a response struct and set everything to 0
    dfs_response_t response;
    memset(&response, 0, sizeof(response));
    // build file path 
    char full_path[MAX_PATH_LEN + 20];
    snprintf(full_path, sizeof(full_path), "server_files/%s", request->path);
    printf("Request type: %d, path: %s\n", request->type, request->path);
    // switch on request type
    switch (request->type) {
        // handle read 
        case MSG_READ: {
    // get mutex for specific file         
    HANDLE lock = get_file_lock(full_path);
    if (lock) WaitForSingleObject(lock, INFINITE);
    // try to open file - should return file pointer 
    FILE *fp = fopen(full_path, "rb");
    // failed
    if (fp == NULL) {
        printf("File not found: %s\n", full_path);
        response.status = STATUS_NOT_FOUND;
        response.data_len = 0;
    // found file     
    } else {
        // read bytes (up to the max) and place into response buffer - returns bytes read 
        size_t bytes_read = fread(response.data, 1, MAX_DATA_LEN, fp);
        // null terminate 
        response.data[bytes_read] = '\0';
        response.status = STATUS_OK;
        response.data_len = bytes_read;
        fclose(fp);
        printf("Read %lu bytes from %s\n", (unsigned long)bytes_read, full_path);
    }
    // release lock so other threads can access the file 
    if (lock) ReleaseMutex(lock);
    break;
}
        // handle write     
        case MSG_WRITE: {
    // get mutex         
    HANDLE lock = get_file_lock(full_path);
    if (lock) WaitForSingleObject(lock, INFINITE);
    // open for writing - binary 
    FILE *fp = fopen(full_path, "wb");
    // failed
    if (fp == NULL) {
        printf("Failed to open file for writing: %s\n", full_path);
        response.status = STATUS_ERROR;
        response.data_len = 0;
    // found file     
    } else {
        // write data to the file 
        size_t bytes_written = fwrite(request->data, 1, request->data_len, fp);
        fclose(fp);
        response.status = STATUS_OK;
        // dont need to send anything back 
        response.data_len = 0;
        printf("Wrote %lu bytes to %s\n", (unsigned long)bytes_written, full_path);
    }
    // release lock 
    if (lock) ReleaseMutex(lock);
    break;
}
        // handle create     
        case MSG_CREATE: {
            // get lock 
            HANDLE lock = get_file_lock(full_path);
            if (lock) WaitForSingleObject(lock, INFINITE);
            FILE *fp = fopen(full_path, "r");
            // check if file exists already
            if (fp != NULL) {
                fclose(fp);
                printf("File already exists: %s\n", full_path);
                response.status = STATUS_ALREADY_EXISTS;
                response.data_len = 0;
            // doesnt exist:    
            } else {
                // create empty file w/ write mode
                fp = fopen(full_path, "w");
                // check if file operation failed: permissions, full disk, invalid path, etc 
                if (fp == NULL) {
                    printf("Failed to create file: %s\n", full_path);
                    response.status = STATUS_ERROR;
                } else {
                    // close file
                    fclose(fp);
                    response.status = STATUS_OK;
                    printf("Created file: %s\n", full_path);
                }
                // nothing to send back 
                response.data_len = 0;
            }
            if (lock) ReleaseMutex(lock);
            break;
        }
        // handle delete     
        case MSG_DELETE: {
            // get lock 
            HANDLE lock = get_file_lock(full_path);
            if (lock) WaitForSingleObject(lock, INFINITE);
            // delete file and return
            if (remove(full_path) == 0) {
                response.status = STATUS_OK;
                printf("Deleted file: %s\n", full_path);
            // not found cant delete     
            } else {
                response.status = STATUS_NOT_FOUND;
                printf("Failed to delete file: %s\n", full_path);
            }
            response.data_len = 0;
            // release lock 
            if (lock) ReleaseMutex(lock);
            break;
        }
            
        case MSG_LIST: {
            DIR *dir;
            struct dirent *entry;
            // open directory 
            dir = opendir("server_files");
            // check if opening failed 
            if (dir == NULL) {
                response.status = STATUS_ERROR;
                strcpy(response.data, "Failed to open directory");
                response.data_len = strlen(response.data);
            } else {
                response.status = STATUS_OK;
                response.data[0] = '\0';  
                // read each entry  
                while ((entry = readdir(dir)) != NULL) {
                    // skip special entries (current and parent)
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                    // add file names to response 
                    strcat(response.data, entry->d_name);
                    strcat(response.data, "\n");
                }
                // set response length and close 
                response.data_len = strlen(response.data);
                closedir(dir);
            }
            break;
        }
        // request types is not 1,2,3,4,5    
        default:
            response.status = STATUS_ERROR;
            response.data_len = 0;
            break;
    }
    // send response back to client 
    send(client_fd, (char*)&response, sizeof(response), 0);
}
/*
 * Thread function to handle a single client connection
 * Reads request, processes it, and closes connection
 */
DWORD WINAPI client_handler(LPVOID arg) {
    SOCKET client_fd = *(SOCKET*)arg;
    free(arg);
    dfs_request_t request;
    // read request from client 
    if (recv(client_fd, (char*)&request, sizeof(request), 0) > 0) {
        // process request
        handle_request(client_fd, &request);
    }
    // close connection 
    closesocket(client_fd);
    return 0;
}

void init_locks() {
    lock_table_mutex = CreateMutex(NULL, FALSE, NULL);
    lock_count = 0;
}

HANDLE get_file_lock(const char *path) {
    WaitForSingleObject(lock_table_mutex, INFINITE);
    
    for (int i = 0; i < lock_count; i++) {
        if (strcmp(file_locks[i].path, path) == 0) {
            HANDLE mutex = file_locks[i].mutex;
            ReleaseMutex(lock_table_mutex);
            return mutex;
        }
    }
    
    if (lock_count < MAX_LOCKS) {
        strncpy(file_locks[lock_count].path, path, MAX_PATH_LEN - 1);
        file_locks[lock_count].mutex = CreateMutex(NULL, FALSE, NULL);
        HANDLE mutex = file_locks[lock_count].mutex;
        lock_count++;
        ReleaseMutex(lock_table_mutex);
        return mutex;
    }
    
    ReleaseMutex(lock_table_mutex);
    return NULL;
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);
    init_locks();
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    // create a socket 
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // claim port 
    server_addr.sin_port = htons(DFS_PORT);
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // accept connections - queues up to 10 requests if busy 
    listen(server_fd, 10);
    
    printf("Server listening on port %d\n", DFS_PORT);
    // Main server loop - accept and handle client connections
    while (1) {
        // wait for client and create new socket using clients ip
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        HANDLE thread;
        // allocate memory for socket 
        SOCKET *client_fd_ptr = malloc(sizeof(SOCKET));
        *client_fd_ptr = client_fd;
        // create a new thread
        thread = CreateThread(NULL, 0, client_handler, client_fd_ptr, 0, NULL);
        CloseHandle(thread);
    }
    
    closesocket(server_fd);
    WSACleanup();
    return 0;
}