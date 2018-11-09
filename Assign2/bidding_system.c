/*B05902120 曾鈺婷*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>

#define swap(a, b) {int temp = a; a = b; b = temp;}
#define wrong() {printf("Something has gone wrong!\n"); exit(1);}

struct {
	char buf[1025];
	int pipe_fd1[2], dup_fd1[2];		// for host to read
	int pipe_fd2[2], dup_fd2[2];		// for host to write
} host[12];

struct {
	int rank;
	int score;
} player[20];

int main(int argc, char** argv)
{
	if (argc != 3) wrong();

	int host_num = atoi(argv[1]);
	int player_num = atoi(argv[2]);

	char ending[100];
	strcpy(ending, "-1 -1 -1 -1\n");

	int count = 0, which = 0, where = 0;
	char identification[5000][50] = {0};
	for (int a = 1; a <= player_num; a ++)
		for (int b = a + 1; b <= player_num; b ++)
			for (int c = b + 1; c <= player_num; c ++)
				for (int d = c + 1; d <= player_num; d ++){
					sprintf(identification[count ++], "%d %d %d %d\n", a, b, c, d);
				}

	fd_set sel, tmp;
	FD_ZERO(&sel);

	pid_t pid[12] = {0};
	for (int i = 0; i < host_num; i ++){
		if (pipe(host[i].pipe_fd1) == -1) wrong();
		if (pipe(host[i].pipe_fd2) == -1) wrong();
		FD_SET(host[i].pipe_fd2[0], &sel);

		if ((pid[i] = fork()) == 0){
			host[i].dup_fd1[0] = dup2(host[i].pipe_fd1[0], STDIN_FILENO);
			host[i].dup_fd2[1] = dup2(host[i].pipe_fd2[1], STDOUT_FILENO);
			if (host[i].dup_fd1[0] == -1) wrong();
			if (host[i].dup_fd2[1] == -1) wrong();
			char host_id[10];
			sprintf(host_id, "%d", i + 1);
			execlp("./host", "./host", host_id, (char*)0);
		}
		close(host[i].pipe_fd1[0]);
		close(host[i].pipe_fd2[1]);
	}

	for (int i = 0; i < host_num && which < count; i ++){
		write(host[i].pipe_fd1[1], identification[which], strlen(identification[which]));
		which ++;
	}

	int maxfd = getdtablesize();
	struct timeval time; time.tv_sec = 5;
	while (1){
		tmp = sel;
		int lalala = select(maxfd + 1, &tmp, NULL, NULL, &time);

		for (int r = 0; r < host_num && lalala > 0; r ++){
			int player_id[4], player_rank[4];
			if (FD_ISSET(host[r].pipe_fd2[0], &tmp)){
				if (count == where && count == which) break;

				memset(host[r].buf, 0, sizeof(host[r].buf));
				read(host[r].pipe_fd2[0], host[r].buf, 1024);
				where ++;

				int player_id[4], player_rank[4];
				sscanf(host[r].buf, "%d %d\n%d %d\n%d %d\n%d %d\n", &player_id[0], &player_rank[0], &player_id[1], &player_rank[1], &player_id[2], &player_rank[2], &player_id[3], &player_rank[3]);
				player[player_id[0] - 1].score += 4 - player_rank[0];
				player[player_id[1] - 1].score += 4 - player_rank[1];
				player[player_id[2] - 1].score += 4 - player_rank[2];
				player[player_id[3] - 1].score += 4 - player_rank[3];

				if (which < count){
					write(host[r].pipe_fd1[1], identification[which], strlen(identification[which]));
					which ++;
				}
				else {
					write(host[r].pipe_fd1[1], ending, strlen(ending));
					FD_CLR(host[r].pipe_fd2[0], &sel);
					wait(NULL);
				}
			}
		}
		if (where == count && which == count) break;
	}

	int max[20][2] = {0};
	for (int m = 0; m < player_num; m ++){
		max[m][0] = m;
		max[m][1] = player[m].score;
	}

	for (int m = 0; m < player_num; m ++)
		for (int n = m + 1; n < player_num; n ++)
			if (max[m][1] < max[n][1]){
				swap(max[m][0], max[n][0]);
				swap(max[m][1], max[n][1]);
			}

	player[max[0][0]].rank = 1;
	for (int m = 1; m < player_num; m ++){
		if (max[m][1] == max[m - 1][1]) player[max[m][0]].rank = player[max[m - 1][0]].rank;
		else {player[max[m][0]].rank = m + 1;}
	}

	char buffer[100];
	for (int m = 0; m < player_num; m ++){
		sprintf(buffer, "%d %d\n", m + 1, player[m].rank);
		write(STDOUT_FILENO, buffer, strlen(buffer));
	}
	return 0;
}