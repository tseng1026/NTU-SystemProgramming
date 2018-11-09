# CSIE 2210: System Programming HW#3

## <strong>Execution</strong>
- Platform: `linux (CSIE workstation)`
- Submitted File: `server.c`, `file_reader.c`, `mmap_read.c`, `mmap_write.c`, `Makefile`

1. Change the permission of all the file so that you can run it `chmod +x [filename]`.<br><br>
2. Type the command `make` to make the execution file of server.
3. If the command above doesn't work, try the command `gcc server.c -o server`, `gcc file_reader.c -o file_reader_slow -D slow`, `gcc file_reader.c -o file_reader_fast`, `gcc mmap_read.c -o mmap_read` and `gcc mmap_write.c -o mmap_write`.<br><br>
4. Place `server`, `file_reader`, `mmap_read` and `mmap_write` in the same directory.<br><br>
5. Type the command `./server [your port id] [logname]` to test if the server meet all the reuirements.

## <strong>New Function</strong>
1. `select`	- to do multiplexing IO.
2. `fork`	- to run a new process (not a new thread).
3. `pipe` or `FIFO` - to communicate with the process you have forked.
4. 	`mmap` - to record the used filename.

## <strong>Self Examination</strong>
1. `server` can successfully read and show data on web server.
2. I/O multiplexing.
3. Multiprocess.<br><br>
4. Return the right HTTP status code "`400 Bad Request`”, if `cgi_program` or `filename` has invalid name.
5. Return the right HTTP status code "`404 Not Found`”, if `cgi_program` or `filename` doesn't exist.<br><br>
6. Show the correct info on `http://your_ip:port/info`, including `number of died processes` and `running processes`.
7. Show the correct advanced info on `http://your_ip:port/info`, including `exit time` and the `opened filename`.