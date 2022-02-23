#include "operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// max number of simultaneous active session
#define S (20)
char session_ids[S][40] = {0};

// pipe to read messages from clients
int rx;

int get_session_id() {
    for (int i = 0; i < S; i++) {
        if (session_ids[i][0] == '\0') {
            return i;
        }
    }
    return -1;
}

// handles TFS_mount requests from the client and sends back corresponding message(s)
void tfs_mount_handler() {
    // read client pipename and open the pipe
    char client_pipename[40] = {0};
    read(rx, &client_pipename, sizeof(client_pipename));
    int tx = open(client_pipename, O_WRONLY);
    
    // get available id (if none, send -1 as response)
    int id = get_session_id();
    if (id == -1) {
        write(tx, &id, sizeof(id));
    } else {
        // store the client_pipename in the index 'id' of the array
        strcpy(session_ids[id], client_pipename);
        // send session_id as response
        write(tx, &id, sizeof(id));
    }

    // close client pipe
    close(tx);
}

void tfs_unmount_handler() {
    // delete id from the session_ids
    int id;
    read(rx, &id, sizeof(id));
    session_ids[id][0] = '\0';
}

void tfs_open_handler() {
    // read client pipename and open the pipe
    int id;
    read(rx, &id, sizeof(id));
    char client_pipename[40] = {0};
    strcpy(client_pipename, session_ids[id]);
    int tx = open(client_pipename, O_WRONLY);

    // read client name file to open
    char filename[40] = {0};
    read(rx, filename, sizeof(filename));
    // read flags
    int flags;
    read(rx, &flags, sizeof(flags));

    // call 'tfs_open()'
    int res = tfs_open(filename, flags);

    // send the response to the client
    write(tx, &res, sizeof(res));

    // close client pipe
    close(tx);
}

void tfs_close_handler() {
    // read client pipename and open the pipe
    int id;
    read(rx, &id, sizeof(id));
    char client_pipename[40] = {0};
    strcpy(client_pipename, session_ids[id]);
    int tx = open(client_pipename, O_WRONLY);

    // read client fhandle
    int fhandle;
    read(rx, &fhandle, sizeof(fhandle));

    // call 'tfs_close()'
    int res = tfs_close(fhandle);

    // send the response to the client
    write(tx, &res, sizeof(res));

    // close client pipe
    close(tx);
}

void tfs_write_handler() {
    // read client pipename and open the pipe
    int id;
    read(rx, &id, sizeof(id));
    char client_pipename[40] = {0};
    strcpy(client_pipename, session_ids[id]);
    int tx = open(client_pipename, O_WRONLY);

    // read client fhandle
    int fhandle;
    read(rx, &fhandle, sizeof(fhandle));

    // read client message length
    size_t len;
    read(rx, &len, sizeof(len));

    //read client message to write
    void *buffer;
    read(rx, &buffer, sizeof(len));

    // call 'tfs_write()'
    ssize_t res = tfs_write(fhandle, buffer, len);

    // send the response to the client
    write(tx, &res, sizeof(res));

    // close client pipe
    close(tx);
}

void tfs_read_handler() {
    // read client pipename and open the pipe
    int id;
    read(rx, &id, sizeof(id));
    char client_pipename[40] = {0};
    strcpy(client_pipename, session_ids[id]);
    int tx = open(client_pipename, O_WRONLY);

    // read client fhandle
    int fhandle;
    read(rx, &fhandle, sizeof(fhandle));

    // read client message length
    size_t len;
    read(rx, &len, sizeof(len));

    //read client message to write
    void *buffer;
    read(rx, &buffer, sizeof(len));

    // call 'tfs_read()'
    ssize_t res = tfs_read(fhandle, buffer, len);

    // send the response to the client
    write(tx, &res, sizeof(res));

    // close client pipe
    close(tx);
}

void tfs_shutdown_after_all_closed_handler() {
    // read client pipename and open the pipe
    int id;
    read(rx, &id, sizeof(id));
    char client_pipename[40] = {0};
    strcpy(client_pipename, session_ids[id]);
    int tx = open(client_pipename, O_WRONLY);

    // call 'tfs_destroy_after_all_closed()'
    int res = tfs_destroy_after_all_closed();

    // send the response to the client
    write(tx, &res, sizeof(res));

    // close client pipe
    close(tx);
}


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    // remove pipe if it does not exist
    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }

    // create pipe
    if (mkfifo(pipename, 0777) != 0) {
        return -1;
    }

    // open pipe for reading (read requests by clients)
    rx = open(pipename, O_RDONLY);
    if (rx == -1) {
        return -1;
    }

    while (true) {
        int op_code;
        read(rx, &op_code, sizeof(op_code));
        
        if (op_code == TFS_OP_CODE_MOUNT) {
            tfs_mount_handler();
        } else if (op_code == TFS_OP_CODE_UNMOUNT) {
            tfs_unmount_handler();
        } else if (op_code == TFS_OP_CODE_OPEN) {
            tfs_open_handler();
        } else if (op_code == TFS_OP_CODE_CLOSE) {
            tfs_close_handler();
        } else if (op_code == TFS_OP_CODE_WRITE) {
            tfs_write_handler();
        } else if (op_code == TFS_OP_CODE_READ) {
            tfs_read_handler();
        } else if (op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {
            tfs_shutdown_after_all_closed_handler();
        }
    }

    close(rx);
    unlink(pipename);

    return 0;
}