# CSIE 2210: System Programming HW#1

## <strong>Execution</strong>
- Platform: `linux (CSIE workstation)`
- Submitted File: `server.c`, `Makefile`

1. Change the permission of all the file so that you can run it `chmod +x [filename]`.<br><br>
2. Type the command `make` to make the execution file of server.
3. If the command above doesn't work, trt the command `gcc server.c -o write_server` and `gcc server.c -D READ_SERVER -o read_server`.<br><br>
4. Place `write_server` and `read_server` in the same directory.
5. Type the command `./test.sh [read_server_port] [write_server_port]` to test if it meet all the reuirements.

## <strong>New Function</strong>
1. `select`	- to do multiplexing IO.
2. `flock`	- to check the lock of the specific file.

## <strong>Self Examination</strong>
1. `read server` returns the file content correctly.
2. `write server` returns the file content correctly.<br><br>
3. Two requests issued to read `read server`.
4. Two requests issued to read `write server`.<br><br>
5. Two requests to `read server` and `write server` at the same time.
6. Two requests to `write server` and `read server` at the same time.<br><br>
7. Multiple requests to `write server` and `read server` at the same time.