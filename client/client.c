/*
 * miniDFS Client
 * Command-line tool for accessing remote file system
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include "../common/protocol.h"

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[]) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;
    // check command line args 
    if (argc < 4) {
        printf("Usage: %s <server_ip> <command> <path> [data]\n", argv[0]);
        printf("Commands: read, write, create, delete, list\n");
        return 1;
    }
    // parse command line args
    char *server_ip = argv[1];
    char *command = argv[2];
    char *path = argv[3];
    // initialize winsock 
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    // create socket 
    sock = socket(AF_INET, SOCK_STREAM, 0);
    // set up server adrress
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DFS_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    // connect to server 
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("connect failed\n");
        return 1;
    }
    // build request 
    dfs_request_t request;
    memset(&request, 0, sizeof(request));
    strncpy(request.path, path, MAX_PATH_LEN - 1);
    // figure out request type 
    if (strcmp(command, "read") == 0) {
        request.type = MSG_READ;
    } else if (strcmp(command, "write") == 0) {
        request.type = MSG_WRITE;
        if (argc > 4) {
            strncpy(request.data, argv[4], MAX_DATA_LEN - 1);
            request.data_len = strlen(request.data);
        }
    } else if (strcmp(command, "create") == 0) {
        request.type = MSG_CREATE;
    } else if (strcmp(command, "delete") == 0) {
        request.type = MSG_DELETE;
    } else if (strcmp(command, "list") == 0) {
        request.type = MSG_LIST;
    } else {
        printf("Unknown command\n");
        return 1;
    }
    // send to server 
    send(sock, (char*)&request, sizeof(request), 0);
    // recieve response 
    dfs_response_t response;
    recv(sock, (char*)&response, sizeof(response), 0);
    // result 
    if (response.status == STATUS_OK) {
        if (response.data_len > 0) {
            printf("%.*s\n", response.data_len, response.data);
        } else {
            printf("Success\n");
        }
    } else {
        printf("Error: status %d\n", response.status);
    }
    // close socket 
    closesocket(sock);
    WSACleanup();
    return 0;
}