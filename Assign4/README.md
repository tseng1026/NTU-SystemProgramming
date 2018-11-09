# CSIE 2210: System Programming HW#4

## <strong>Execution</strong>
- Platform: `linux (CSIE workstation)`
- Submitted File: `random_forest.c`,  `Makefile`, `submission.csv`, `ans.csv`

1. Change the permission of all the file so that you can run it `chmod +x [filename]`.<br><br>
2. Type the command `make` to make the execution file of server.
3. If the command above doesn't work, try the command `gcc random_forest.c -o random_forest -pthread`.<br><br>
4. Place `random_forest`, `data_dir` and `ans_csv` in the same directory.<br><br>
5. Type the command `./random_forest -data_data dir -output submission.csv -tree tree_number -thread thread_number` to test if the random forest meet all the reuirements.

## <strong>New Function</strong>
1. `thread`	- to run a new thread (under the same process).

## <strong>Self Examination</strong>
1. Successfully compile and execute the random_forest and get the result within 3 minutes with the accuracy higher than 80%.<br><br>
2. Thread using.
3. The number of the threads versus the used time.
4. The number of the threads versus the number of the used instructions.
5. The number of the trees versus the number of the used instructions.
6. Others.
