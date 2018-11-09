# CSIE 2210: System Programming HW#2

## <strong>Execution</strong>
- Platform: `linux (CSIE workstation)`
- Submitted File: `bidding_system.c`, `host.c`, `player.c`, `Makefile`

1. Change the permission of all the file so that you can run it `chmod +x [filename]`.<br><br>
2. Type the command `make` to make the execution file of server.
3. If the command above doesn't work, trt the command `gcc bidding_system.c -o bidding_system`, `gcc host.c -o host` and `gcc player.c -o player`.<br><br>
4. Place `bidding_system`, `host` and `player` in the same directory.<br><br>
5. Type the command `./bidding_system [host_num] [player_num]` to test if the bidding_system meet the reuirements.
6. Type the command `./host [host_id]` to test if the host meet the reuirements.
7. Type the command `./player [host_id] [player_index] [random_key]` to test if the player meet the reuirements.

## <strong>New Function</strong>
1. `select`	- to do multiplexing IO.
2. `fork`	- to run a new process (not a new thread).
3. `pipe` or `FIFO` - to communicate with the process you have forked.

## <strong>Self Examination</strong>
1. `bidding_system` works fine with the TA's code.
2. `bidding_system` schedules host effectively.
3. `bidding_system` executes host correctly.<br><br>
4. `host` works fine with the TA's code.<br><br>
5. `player` works fine with the TA's code.<br><br>
6. Completeness.
7. Produce executable files successfully.