/*B05902120 曾鈺婷*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#define swap(a, b) {int temp = a; a = b; b = temp;}
#define wrong() {printf("Something has gone wrong!\n"); exit(1);}
#define retur() {printf("Something has gone wrong!\n"); return 0;}

struct {
	char buf[1025];
	char filename[20];
	int file_fd;
} host;

struct {
	int id, rank;
	int bet, money;
	
	char buf[1025];
	char filename[20];
	int file_fd;
} player[4];

int main(int argc, char** argv)
{
	if (argc != 2) wrong();
	sprintf(host.filename, "host%s.FIFO", argv[1]);
	sprintf(player[0].filename, "host%s_A.FIFO", argv[1]);
	sprintf(player[1].filename, "host%s_B.FIFO", argv[1]);
	sprintf(player[2].filename, "host%s_C.FIFO", argv[1]);
	sprintf(player[3].filename, "host%s_D.FIFO", argv[1]);

	mkfifo(host.filename, 0777);
	mkfifo(player[0].filename, 0777);
	mkfifo(player[1].filename, 0777);
	mkfifo(player[2].filename, 0777);
	mkfifo(player[3].filename, 0777);
	
	host.file_fd = open(host.filename, O_RDWR);
	player[0].file_fd = open(player[0].filename, O_RDWR);
	player[1].file_fd = open(player[1].filename, O_RDWR);
	player[2].file_fd = open(player[2].filename, O_RDWR);
	player[3].file_fd = open(player[3].filename, O_RDWR);
	
	if (host.file_fd == -1) wrong();
	if (player[0].file_fd == -1) wrong();
	if (player[1].file_fd == -1) wrong();
	if (player[2].file_fd == -1) wrong();
	if (player[3].file_fd == -1) wrong();

	while (1){
		player[0].id = player[0].rank = player[0].bet = player[0].money = 0;
		player[1].id = player[1].rank = player[1].bet = player[1].money = 0;
		player[2].id = player[2].rank = player[2].bet = player[2].money = 0;
		player[3].id = player[3].rank = player[3].bet = player[3].money = 0;

		char identification[50];
		memset(identification, 0, sizeof(identification));
		read(STDIN_FILENO, identification, 50);
		sscanf(identification, "%d %d %d %d\n", &player[0].id, &player[1].id, &player[2].id, &player[3].id);
		if (strcmp(identification, "-1 -1 -1 -1\n") == 0) break;

		pid_t pid[4] = {0};
		if ((pid[0] = fork()) == 0) execlp("./player", "./player", argv[1], "A", "1", (char*)0);
		if ((pid[1] = fork()) == 0) execlp("./player", "./player", argv[1], "B", "2", (char*)0);
		if ((pid[2] = fork()) == 0) execlp("./player", "./player", argv[1], "C", "3", (char*)0);
		if ((pid[3] = fork()) == 0) execlp("./player", "./player", argv[1], "D", "4", (char*)0);

		sprintf(player[0].buf, "1000 1000 1000 1000\n");
		sprintf(player[1].buf, "1000 1000 1000 1000\n");
		sprintf(player[2].buf, "1000 1000 1000 1000\n");
		sprintf(player[3].buf, "1000 1000 1000 1000\n");

		for (int i = 0; i < 10; i ++){
			write(player[0].file_fd, player[0].buf, strlen(player[0].buf));
			write(player[1].file_fd, player[1].buf, strlen(player[1].buf));
			write(player[2].file_fd, player[2].buf, strlen(player[2].buf));
			write(player[3].file_fd, player[3].buf, strlen(player[3].buf));

			int where[1025] = {0}, check[4] = {0}, max[4][2] = {0};
			while (1){
				if (check[0] + check[1] + check[2] + check[3] == 4) break;

				int key, bet;
				char index;
				memset(host.buf, 0, sizeof(host.buf));
				read(host.file_fd, host.buf, 1024);

				int count = 1;
				for (int k = 0; k < strlen(host.buf); k ++)
					if (host.buf[k] == '\n') where[count] = k + 1, count ++;

				for (int k = 0; k < count - 1; k ++){
					sscanf(&host.buf[where[k]], "%c %d %d\n", &index, &key, &bet);
					check[index - 'A'] = 1;
					player[index - 'A'].bet = bet;
					
					max[index - 'A'][0] = index - 'A';
					max[index - 'A'][1] = player[index - 'A'].bet;
				}
			}

			for (int m = 0; m < 4; m ++)
				for (int n = m + 1; n < 4; n ++)
					if (max[m][1] == max[n][1]) max[m][1] = max[n][1] = 0;

			for (int m = 0; m < 4; m ++)
				for (int n = m + 1; n < 4; n ++)
					if (max[m][1] < max[n][1]){
						swap(max[m][0], max[n][0]);
						swap(max[m][1], max[n][1]);
					}

			int alpha[4] = {0};
			if (max[0][1] == 0) alpha[max[0][0]] = 0;
			else alpha[max[0][0]] = max[0][1], player[max[0][0]].rank ++;

			player[max[0][0]].money += 1000 - alpha[max[0][0]];
			player[max[1][0]].money += 1000 - alpha[max[1][0]];
			player[max[2][0]].money += 1000 - alpha[max[2][0]];
			player[max[3][0]].money += 1000 - alpha[max[3][0]];

			sprintf(player[0].buf, "%d %d %d %d\n", player[0].money, player[1].money, player[2].money, player[3].money);
			sprintf(player[1].buf, "%d %d %d %d\n", player[0].money, player[1].money, player[2].money, player[3].money);
			sprintf(player[2].buf, "%d %d %d %d\n", player[0].money, player[1].money, player[2].money, player[3].money);
			sprintf(player[3].buf, "%d %d %d %d\n", player[0].money, player[1].money, player[2].money, player[3].money);
		}

		int max[4][2] = {0};
		for (int m = 0; m < 4; m ++){
			max[m][0] = m;
			max[m][1] = player[m].rank;
		}

		for (int m = 0; m < 4; m ++)
			for (int n = m + 1; n < 4; n ++)
				if (max[m][1] < max[n][1]){
					swap(max[m][0], max[n][0]);
					swap(max[m][1], max[n][1]);
				}

		player[max[0][0]].rank = 1;
		for (int m = 1; m < 4; m ++){
			if (max[m][1] == max[m - 1][1]) player[max[m][0]].rank = player[max[m - 1][0]].rank;
			else {player[max[m][0]].rank = m + 1;}
		}
		
		char buffer[100];
		sprintf(buffer, "%d %d\n%d %d\n%d %d\n%d %d\n", player[0].id, player[0].rank, player[1].id, player[1].rank, player[2].id, player[2].rank, player[3].id, player[3].rank);
		write(STDOUT_FILENO, buffer, strlen(buffer));
		
		wait(NULL); wait(NULL);
		wait(NULL); wait(NULL);
	}

	unlink(host.filename);
	unlink(player[0].filename);
	unlink(player[1].filename);
	unlink(player[2].filename);
	unlink(player[3].filename);

	return 0;
}