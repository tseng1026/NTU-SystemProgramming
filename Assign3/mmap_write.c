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
	time_t current_time;
	char c_time_string[100];
	Info *p_map;

	char file[1025];
	sprintf(file, "%s", argv[1]);
	
	fd = open(file, O_RDWR | O_TRUNC | O_CREAT, 0777); 
	if(fd < 0){
		perror("open");
		exit(-1);
	}
	lseek(fd, sizeof(Info), SEEK_SET);
	write(fd, "", 1);

	p_map = (Info*) mmap(0, sizeof(Info), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	current_time = time(NULL);
	strcpy(c_time_string, ctime(&current_time));
	memcpy(p_map->last_time, &c_time_string, sizeof(c_time_string));
	p_map->last_time[strlen(p_map->last_time) - 1] = '\0';
	sprintf(p_map->last_file, "%s", argv[2]);
	munmap(p_map, sizeof(Info));
	
	close(fd);
	return 0;
}