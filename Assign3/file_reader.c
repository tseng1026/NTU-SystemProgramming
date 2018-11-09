/*b05902120 曾鈺婷*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	char filename[1025];
	memset(filename, '\0', sizeof(filename));
	read(STDIN_FILENO, filename, 1024);
	int file_fd = open(filename, O_RDWR);

	char file[1025];
	sprintf(file, "%s", argv[1]);

	char buf[65536];
	memset(buf, 0, sizeof(buf));
	if (file_fd == -1){
		execlp("./mmap_write", "./mmap_write", file, filename, (char*)0);
		exit(1);
	}
	
	#ifdef slow
	sleep(5);
	#endif

	read(file_fd, buf, 65536);
	write(STDOUT_FILENO, buf, strlen(buf));

	execlp("./mmap_write", "./mmap_write", file, filename, (char*)0);
	return 0;
}