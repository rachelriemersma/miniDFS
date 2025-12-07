# miniDFS - Distributed File System

A simple client-server distributed file system for CS 310.

## How to Run

### Compile
```bash
gcc -Wall -o server.exe server/server.c -lws2_32
gcc -Wall -o client.exe client/client.c -lws2_32
```

### Run Server
```bash
server.exe
```

### Run Client (in another terminal)
```bash
client.exe 127.0.0.1 <command> <file> [data]
```

## Commands
- `read <file>` - Read a file
- `write <file> <data>` - Write data to a file
- `create <file>` - Create empty file
- `delete <file>` - Delete a file
