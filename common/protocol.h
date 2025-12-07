#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
// Server configuration
#define MAX_PATH_LEN 256
#define MAX_DATA_LEN 4096
#define DFS_PORT 9999
// Message types for client requests
typedef enum {
    MSG_READ = 1,
    MSG_WRITE = 2,
    MSG_CREATE = 3,
    MSG_DELETE = 4,
    MSG_LIST = 5,
    MSG_RESPONSE = 6
} msg_type_t;
// Status codes for server responses
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_NOT_FOUND = 2,
    STATUS_PERMISSION_DENIED = 3,
    STATUS_ALREADY_EXISTS = 4
} status_t;
// Client request message structure
typedef struct {
    msg_type_t type;
    uint32_t data_len;
    char path[MAX_PATH_LEN];
    char data[MAX_DATA_LEN];
} dfs_request_t;
// Server response message structure
typedef struct {
    status_t status;
    uint32_t data_len;
    char data[MAX_DATA_LEN];
} dfs_response_t;

#endif