/*B05902120 曾鈺婷*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define wrong() {printf("Something has gone wrong!\n"); exit(1);}

struct {
	char buf[1025];
	char filename[20];
	int file_fd;
} player, host;

int main(int argc, char** argv)
{
	if (argc != 4) wrong();

	sprintf(player.filename, "host%s_%s.FIFO", argv[1], argv[2]);
	sprintf(host.filename, "host%s.FIFO", argv[1]);
	player.file_fd = open(player.filename, O_RDONLY);
	host.file_fd = open(host.filename, O_WRONLY);
	if (player.file_fd == -1 || host.file_fd == -1) wrong();
	
	for (int i = 0; i < 10; i ++){
		int money[4];
		memset(player.buf, 0, sizeof(player.buf));
		read(player.file_fd, player.buf, 1024);
		sscanf(player.buf, "%d %d %d %d\n", &money[0], &money[1], &money[2], &money[3]);

		int bet = 0;
		if (i % 4 == argv[2][0] - 'A') bet = money[argv[2][0] - 'A'];
		sprintf(host.buf, "%s %s %d\n", argv[2], argv[3], bet);
		write(host.file_fd, host.buf, strlen(host.buf));
	}
	return 0;
}
