#include "tecnicofs_client_api.h"
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

int session_id = -1;
char pipename[40]={0};
int rx, tx;

/*
 * Establishes a session with a TecnicoFS server.
 * Input:
 * - client_pipe_path: pathname of a named pipe that will be used for
 *   the client to receive responses. This named pipe will be created (via
 * 	 mkfifo) inside tfs_mount.
 * - server_pipe_path: pathname of the named pipe where the server is listening
 *   for client requests
 * When successful, the new session's identifier (session_id) was
 * saved internally by the client; also, the client process has
 * successfully opened both named pipes (one for reading, the other one for
 * writing, respectively).
 *
 * Returns 0 if successful, -1 otherwise.
 */
int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    // remove pipe if it does not exist
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }

    strcpy(pipename,client_pipe_path);

    // create pipe
    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }


    // open client pipe for reading
    rx = open(client_pipe_path, O_RDONLY);
    if (rx == -1) {
        return -1;
    }

    // open server pipe for writing
    tx = open(server_pipe_path, O_WRONLY);
    if (tx == -1) {
        return -1;
    }

    // send message to server requesting session_id
    int op_code = TFS_OP_CODE_MOUNT;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &client_pipe_path, sizeof(client_pipe_path));

    // read server response with the session_id
    read(rx, &session_id, sizeof(session_id));
    if (session_id == -1){
        return -1;
    }
    return 0;
}

/*
 * Ends the currently active session.
 * After notifying the server, both named pipes are closed by the client,
 * the client named pipe is deleted (via unlink) and the client's session_id is
 * set to none.
 *
 * Returns 0 if successful, -1 otherwise.
 */
int tfs_unmount() {
    if (session_id == -1) {
        return -1;
    }

    int op_code = TFS_OP_CODE_UNMOUNT;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));

    close(rx);
    close(tx);

    unlink(pipename);

    session_id = -1;
    return 0;
}


/*
 * Opens a file
 * Input:
 *  - name: absolute path name
 *  - flags: can be a combination (with bitwise or) of the following flags:
 *    - append mode (TFS_O_APPEND)
 *    - truncate file contents (TFS_O_TRUNC)
 *    - create file if it does not exist (TFS_O_CREAT)
 */
int tfs_open(char const *name, int flags) {
    int op_code = TFS_OP_CODE_OPEN;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));
    write(tx, name, sizeof(name));
    write(tx, &flags, sizeof(flags));

    // read server response
    int res;
    read(rx, &res, sizeof(res));
    if (res == -1){
        return -1;
    }

    return 0;
}


/* Closes a file
 * Input:
 * 	- file handle (obtained from a previous call to tfs_open)
 * Returns 0 if successful, -1 otherwise.
 */
int tfs_close(int fhandle) {
    int op_code = TFS_OP_CODE_CLOSE;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));
    write(tx, &fhandle, sizeof(fhandle));

    // read server response
    int res;
    read(rx, &res, sizeof(res));
    if (res == -1){
        return -1;
    }

    return 0;
}


/* Writes to an open file, starting at the current offset
 * Input:
 * 	- file handle (obtained from a previous call to tfs_open)
 * 	- buffer containing the contents to write
 * 	- length of the contents (in bytes)
 *
 * Returns the number of bytes that were written (can be lower than
 * 'len' if the maximum file size is exceeded), or -1 in case of error.
 */
ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    int op_code = TFS_OP_CODE_WRITE;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));
    write(tx, &fhandle, sizeof(fhandle));
    write(tx, &len, sizeof(len));
    write(tx, buffer, sizeof(len));

    // read server response
    ssize_t written;
    read(rx, &written, sizeof(written));
    if (written == -1){
        return -1;
    }

    return written;
}


/* Reads from an open file, starting at the current offset
 * * Input:
 * 	- file handle (obtained from a previous call to tfs_open)
 * 	- destination buffer
 * 	- length of the buffer
 *
 * Returns the number of bytes that were copied from the file to the buffer
 * (can be lower than 'len' if the file size was reached), or -1 in case of
 * error.
 */
ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    int op_code = TFS_OP_CODE_READ;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));
    write(tx, &fhandle, sizeof(fhandle));
    write(tx, &len, sizeof(len));
    write(tx, buffer, sizeof(len));

    // read server response
    ssize_t res;
    read(rx, &res, sizeof(res));
    if (res == -1){
        return -1;
    }

    return res;
}


/*
 * Orders TecnicoFS server to wait until no file is open and then shutdown
 * Returns 0 if successful, -1 otherwise.
 */
int tfs_shutdown_after_all_closed() {
    int op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    write(tx, &op_code, sizeof(op_code));
    write(tx, &session_id, sizeof(session_id));

    // read server response
    int res;
    read(rx, &res, sizeof(res));
    if (res == -1){
        return -1;
    }

    return 0;
}
