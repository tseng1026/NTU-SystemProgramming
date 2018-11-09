/*b05902120 曾鈺婷*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

typedef struct {
	char last_time[100];
	char last_file[1025];
} Info;

int main(int argc, char** argv) 
{
	int fd;
	Info *p_map;

	char file[1025];
	sprintf(file, "%s", argv[1]);

	fd = open(file, O_RDWR);
	p_map = (Info*) mmap(0, sizeof(Info), PROT_READ, MAP_SHARED, fd, 0);

	if (argv[2][0] == '1'){
		char last_exit[2048];
		sprintf(last_exit, "Last Exit CGI: %s, Filename: %s\n", p_map->last_time, p_map->last_file);
		write(STDOUT_FILENO, last_exit, strlen(last_exit));
	}

	if (argv[2][0] == '0') {
		char last_exit[2048];
		sprintf(last_exit, "Last Exit CGI: NULL, Filename: NULL\n");
		write(STDOUT_FILENO, last_exit, strlen(last_exit));
	}
	
	if (argv[2][0] == '2') {
		char last_exit[2048];
		sprintf(last_exit, "Last Exit CGI: %s, Filename: NOT FOUND\n", p_map->last_time);
		write(STDOUT_FILENO, last_exit, strlen(last_exit));
	}

	close(fd);
	return 0;
}