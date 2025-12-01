#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include "../common/protocol.h"

#pragma comment(lib, "ws2_32.lib")

void handle_request(SOCKET client_fd, dfs_request_t *request) {
    dfs_response_t response;
    memset(&response, 0, sizeof(response));
    
    // Prepend server_files/ to the path for security
    char full_path[MAX_PATH_LEN + 20];
    snprintf(full_path, sizeof(full_path), "server_files/%s", request->path);
    
    printf("Request type: %d, path: %s\n", request->type, request->path);
    
    switch (request->type) {
        case MSG_READ: {
            FILE *fp = fopen(full_path, "rb");
            if (fp == NULL) {
                printf("File not found: %s\n", full_path);
                response.status = STATUS_NOT_FOUND;
                response.data_len = 0;
            } else {
                size_t bytes_read = fread(response.data, 1, MAX_DATA_LEN, fp);
                response.data[bytes_read] = '\0';
                response.status = STATUS_OK;
                response.data_len = bytes_read;
                fclose(fp);
                printf("Read %zu bytes from %s\n", bytes_read, full_path);
            }
            break;
        }
            
        case MSG_WRITE: {
            FILE *fp = fopen(full_path, "wb");
            if (fp == NULL) {
                printf("Failed to open file for writing: %s\n", full_path);
                response.status = STATUS_ERROR;
                response.data_len = 0;
            } else {
                size_t bytes_written = fwrite(request->data, 1, request->data_len, fp);
                fclose(fp);
                response.status = STATUS_OK;
                response.data_len = 0;
                printf("Wrote %zu bytes to %s\n", bytes_written, full_path);
            }
            break;
        }
            
        case MSG_CREATE: {
            FILE *fp = fopen(full_path, "r");
            if (fp != NULL) {
                fclose(fp);
                printf("File already exists: %s\n", full_path);
                response.status = STATUS_ALREADY_EXISTS;
                response.data_len = 0;
            } else {
                fp = fopen(full_path, "w");
                if (fp == NULL) {
                    printf("Failed to create file: %s\n", full_path);
                    response.status = STATUS_ERROR;
                } else {
                    fclose(fp);
                    response.status = STATUS_OK;
                    printf("Created file: %s\n", full_path);
                }
                response.data_len = 0;
            }
            break;
        }
            
        case MSG_DELETE: {
            if (remove(full_path) == 0) {
                response.status = STATUS_OK;
                printf("Deleted file: %s\n", full_path);
            } else {
                response.status = STATUS_NOT_FOUND;
                printf("Failed to delete file: %s\n", full_path);
            }
            response.data_len = 0;
            break;
        }
            
        case MSG_LIST: {
            // TODO: Implement directory listing for Week 2
            response.status = STATUS_OK;
            strcpy(response.data, "LIST not implemented yet\n");
            response.data_len = strlen(response.data);
            break;
        }
            
        default:
            response.status = STATUS_ERROR;
            response.data_len = 0;
            break;
    }
    
    send(client_fd, (char*)&response, sizeof(response), 0);
}

DWORD WINAPI client_handler(LPVOID arg) {
    SOCKET client_fd = *(SOCKET*)arg;
    free(arg);
    
    dfs_request_t request;
    if (recv(client_fd, (char*)&request, sizeof(request), 0) > 0) {
        handle_request(client_fd, &request);
    }
    
    closesocket(client_fd);
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DFS_PORT);
    
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);
    
    printf("Server listening on port %d\n", DFS_PORT);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        HANDLE thread;
        SOCKET *client_fd_ptr = malloc(sizeof(SOCKET));
        *client_fd_ptr = client_fd;
        thread = CreateThread(NULL, 0, client_handler, client_fd_ptr, 0, NULL);
        CloseHandle(thread);
    }
    
    closesocket(server_fd);
    WSACleanup();
    return 0;
}