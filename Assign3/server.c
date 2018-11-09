/*b05902120 曾鈺婷*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define TIMEOUT_SEC 5		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE 1024		// timeout in seconds for wait for a connection 
#define NO_USE 0			// status of a http request
#define ERROR -1	
#define READING 1		
#define WRITING 2		
#define ERR_EXIT(a){perror(a); exit(1);}

typedef struct {
	char hostname[512];		// hostname
	unsigned short port;	// port to listen
	int listen_fd;			// fd to wait for a new connection
} http_server;

typedef struct {
	int run;
	pid_t pid;				// the result of fork

	int *pipe_status;
	int pipe_fd1[2];
	int pipe_fd2[2];
	int dup2_fd1[2];
	int dup2_fd2[2];

	int conn_fd;			// fd to talk with client
	int status;				// not used, error, reading (from client)
							// writing (to client)
	char file[MAXBUFSIZE];	// requested file
	char query[MAXBUFSIZE];	// requested query
	char host[MAXBUFSIZE];	// client host

	char progname[MAXBUFSIZE];
	char filename[MAXBUFSIZE];

	char* buf;				// data sent by/to client
	size_t buf_len;			// bytes used by buf
	size_t buf_size; 		// bytes allocated for buf
	size_t buf_idx; 		// offset for reading and writing
} http_request;

typedef struct {
	char last_time[100];
	char last_file[1025];
} Info;

static void add_to_buf(http_request *reqP, char* str, size_t len);
static void strdecode(char* to, char* from);
static int hexit(char c);
static char* get_request_line(http_request *reqP);
static void* e_malloc(size_t size);
static void* e_realloc(void* optr, size_t size);

static char* logfilenameP;	// log file name

// Forwards
//
static void init_http_server(http_server *svrP,  unsigned short port);
// initailize a http_request instance, exit for error

static void init_request(http_request* reqP);
// initailize a http_request instance

static void free_request(http_request* reqP);
// free resources used by a http_request instance

static int read_header_and_file(http_request* reqP, int *errP);
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: continue to it until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected

static void set_ndelay(int fd);
// Set NDELAY mode on a socket.

static volatile int sig = 0;
static void whensig(int signo){
	if (signo == SIGUSR1) sig = 1;
	return;
}

int main(int argc, char** argv)
{
	http_server server;				// http server
	http_request* requestP = NULL;	// pointer to http requests from client

	int maxfd;						// size of open file descriptor table

	struct sockaddr_in cliaddr; 	// used by accept()
	int clilen;

	int conn_fd;					// fd for a new connection with client
	int err;						// used by read_header_and_file()
	int i, ret, nwritten;

	int childnum = 0;
	char childyes[3] = {'0'};

	pid_t pid;
	fd_set sel;						// main select solution
	fd_set tmp;						// temp select solution
	struct timeval timeout;

	// parse args if the input is wrong
	if (argc != 3){
		(void)fprintf(stderr, "usage:  %s port# logfile\n", argv[0]);
		exit(1);
	}
	logfilenameP = argv[2];

	// initialize http server
	// atoi change the char to int
	init_http_server(&server, (unsigned short)atoi(argv[1]));

	// get file descripter table size and initize request table
	maxfd = getdtablesize();
	requestP = (http_request*) malloc(sizeof(http_request)* maxfd);
	if (requestP == (http_request*)0){
		fprintf(stderr, "out of memory allocating all http requests\n");
		exit(1);
	}
	for (i = 0; i < maxfd; i ++)
		init_request(&requestP[i]);

	// connect the client and the server
	requestP[server.listen_fd].conn_fd = server.listen_fd;
	requestP[server.listen_fd].status = READING;

	// loop for handling connections
	fprintf(stderr, "starting on %.80s, port %6d, fd %3d, maxconn %d, logfile %s...\n", server.hostname, server.port, server.listen_fd, maxfd, logfilenameP);

	// initialize select solution
	FD_ZERO(&sel);
	FD_SET(server.listen_fd, &sel);

	// initialize the time
	timeout.tv_sec = 5;

	while (1){
		tmp = sel;
		int m = select(maxfd + 1, &tmp, NULL, NULL, &timeout);
		if (m == 0) continue;

		// the result of the select can be more than one
		for (int i = 0; i <= maxfd; i ++){
			// check new connection (if it is ready)
			// if the ready request_conn.fd is same as the one the server is going to listen_fd
			if (FD_ISSET(i, &tmp) && i == server.listen_fd){
				clilen = sizeof(cliaddr);
				conn_fd = accept(server.listen_fd, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
				if (conn_fd < 0){
					if (errno == EINTR || errno == EAGAIN) continue; // try again 
					if (errno == ENFILE){
						(void)fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
						continue;
					}	
					ERR_EXIT("accept")
				}

				// set the socket file descriptor of requestP
				// inet_ntoa change the int into char
				requestP[conn_fd].conn_fd = conn_fd;
				requestP[conn_fd].status = READING;		
				strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
				set_ndelay(conn_fd);
				fprintf(stderr, "getting a new request... fd %3d from %s\n", conn_fd, requestP[conn_fd].host);
				FD_SET(conn_fd, &sel);

				while (1){
					ret = read_header_and_file(&requestP[conn_fd], &err);
					if (ret > 0) continue;
					else if (ret < 0){
						// error for reading http header or requested file
						fprintf(stderr, "error on fd %3d, code %d\n", requestP[conn_fd].conn_fd, err);
						requestP[conn_fd].status = ERROR;
						close(requestP[conn_fd].conn_fd);
						free_request(&requestP[conn_fd]);
						FD_CLR(conn_fd, &sel);
						break;
					}
					else if (ret == 0){
						// check the filename is valid
						int file_valid = 0, prog_valid = 0;

						memset(requestP[conn_fd].filename, '\0', sizeof(requestP[conn_fd].filename));
						memset(requestP[conn_fd].progname, '\0', sizeof(requestP[conn_fd].progname));
						strcpy(requestP[conn_fd].filename, requestP[conn_fd].query + 9);
						strcpy(requestP[conn_fd].progname, requestP[conn_fd].file);
						
						int file_fd = open(requestP[conn_fd].filename, O_RDWR);
						close(file_fd);
						if (file_fd < 0) file_valid = 1;
						for (int k = 0; k < strlen(requestP[conn_fd].filename); k ++)
							if (!(isalnum(requestP[conn_fd].filename[k]) || requestP[conn_fd].filename[k] == '_')){file_valid = 2; break;}

						struct stat sb;
						int r = stat(requestP[conn_fd].progname, &sb);
						if (r < 0) prog_valid = 1;
						for (int k = 0; k < strlen(requestP[conn_fd].progname); k ++)
							if (!(isalnum(requestP[conn_fd].progname[k]) || requestP[conn_fd].progname[k] == '_')){prog_valid = 2; break;}

						if (strcmp(requestP[conn_fd].progname, "info") == 0) file_valid = prog_valid = 0;

						int buflen;
						char buf[65536];
						char timebuf[100];
						time_t now;

						if (file_valid == 0 && prog_valid == 0){

							requestP[conn_fd].buf_len = 0;
							buflen = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
							add_to_buf(&requestP[conn_fd], buf, buflen);
						 	now = time((time_t*)0);
							(void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
							buflen = snprintf(buf, sizeof(buf), "Date: %s\015\012", timebuf);
							add_to_buf(&requestP[conn_fd], buf, buflen);

							signal(SIGUSR1, whensig);
							pipe(requestP[conn_fd].pipe_fd1);
							pipe(requestP[conn_fd].pipe_fd2);
							FD_SET(requestP[conn_fd].pipe_fd1[0], &sel);
							requestP[requestP[conn_fd].pipe_fd1[0]].conn_fd = conn_fd;
							if ((requestP[conn_fd].pid = fork()) == 0){
								requestP[conn_fd].dup2_fd1[1] = dup2(requestP[conn_fd].pipe_fd1[1], STDOUT_FILENO);
								requestP[conn_fd].dup2_fd2[0] = dup2(requestP[conn_fd].pipe_fd2[0], STDIN_FILENO);
								if (strcmp(requestP[conn_fd].progname, "info") == 0){
									kill(getppid(),SIGUSR1);
									execlp("./mmap_read", "./mmap_read", logfilenameP, childyes, (char*)0);
								}
								else {
									char execute[1025];
									write(requestP[conn_fd].pipe_fd2[1], requestP[conn_fd].filename, strlen(requestP[conn_fd].filename));
									sprintf(execute, "./%s", requestP[conn_fd].progname);
									execlp(execute, execute, logfilenameP, (char*)0);
								}
							}

							if (strcmp(requestP[conn_fd].progname, "info") != 0){
								strcpy(childyes, "1");
								requestP[conn_fd].run = 1;
							}
							else if (strcmp(requestP[conn_fd].progname, "info") == 0){
								while (!sig);
								sig = 0;
								char filecontent[65536];
								memset(filecontent, '\0', sizeof(filecontent));
								read(requestP[conn_fd].pipe_fd1[0], filecontent, 65536);

								char content[65536];
								int64_t length = (int64_t)(strlen(filecontent) + 29);
								for (int p = 0; p <= maxfd; p ++)
									if (requestP[p].run == 1){
										sprintf(content, "%d ", requestP[p].pid);
										length += (int64_t)strlen(content);
									}

								if (childnum == 0) sprintf(content, "No process died previously.\n");
								if (childnum == 1) sprintf(content, "%d process died previously.\n", childnum);
								if (childnum >= 2) sprintf(content, "%d processes died previously.\n", childnum);
								length += (int64_t)strlen(content);

								// buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)length);
								// add_to_buf(&requestP[conn_fd], buf, buflen);
								buflen = snprintf(buf, sizeof(buf), "Connection: close\015\012\015\012");
								add_to_buf(&requestP[conn_fd], buf, buflen);

								buflen = snprintf(buf, sizeof(buf), content);
								add_to_buf(&requestP[conn_fd], buf, buflen);

								sprintf(content, "PIDs of Running Processes: ");
								buflen = snprintf(buf, sizeof(buf), content);
								add_to_buf(&requestP[conn_fd], buf, buflen);

								for (int p = 0; p <= maxfd; p ++)
									if (requestP[p].run == 1){
										sprintf(content, "%d ", requestP[p].pid);
										buflen = snprintf(buf, sizeof(buf), content);
										add_to_buf(&requestP[conn_fd], buf, buflen);
									}

								sprintf(content, "\n");
								buflen = snprintf(buf, sizeof(buf), content);
								add_to_buf(&requestP[conn_fd], buf, buflen);

								buflen = snprintf(buf, sizeof(buf), filecontent);
								add_to_buf(&requestP[conn_fd], buf, buflen);

								// ready for writing
								fprintf(stderr, "writing (buf %p, idx %2d)%4d bytes to request fd %3d\n", 
													requestP[conn_fd].buf, (int)requestP[conn_fd].buf_idx,
														(int)requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd);
								
								nwritten = write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
								nwritten = write(1, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
								fprintf(stderr, "complete writing %4d bytes on fd %3d\n\n", nwritten, requestP[conn_fd].conn_fd);

								waitpid(requestP[conn_fd].pid, requestP[conn_fd].pipe_status, 0);
								close(requestP[conn_fd].conn_fd);
								close(requestP[conn_fd].pipe_fd1[0]);
								close(requestP[conn_fd].pipe_fd1[1]);
								close(requestP[conn_fd].pipe_fd2[0]);
								close(requestP[conn_fd].pipe_fd2[1]);
								free_request(&requestP[conn_fd]);
								FD_CLR(conn_fd, &sel);
								FD_CLR(requestP[conn_fd].pipe_fd1[0], &sel);
							}
							break;
						}

						else if (file_valid == 2 || prog_valid == 2){
							requestP[conn_fd].buf_len = 0;
							buflen = snprintf(buf, sizeof(buf), "HTTP/1.1 400 Bad Request\015\012Server: SP TOY\015\012");
							add_to_buf(&requestP[conn_fd], buf, buflen);
						 	now = time((time_t*)0);
							(void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
							buflen = snprintf(buf, sizeof(buf), "Date: %s\015\012", timebuf);
							add_to_buf(&requestP[conn_fd], buf, buflen);
							if (file_valid == 1 && prog_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)57);
							else if (file_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)35);
							else if (prog_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)38);
							add_to_buf(&requestP[conn_fd], buf, buflen);
							buflen = snprintf(buf, sizeof(buf), "Connection: close\015\012\015\012");
							add_to_buf(&requestP[conn_fd], buf, buflen);
							buflen = snprintf(buf, sizeof(buf), "400 Bad Request\n");
							add_to_buf(&requestP[conn_fd], buf, buflen);

							if (file_valid == 2){
								buflen = snprintf(buf, sizeof(buf), "Invalid file name!\n");
								add_to_buf(&requestP[conn_fd], buf, buflen);
							}
							if (prog_valid == 2){
								buflen = snprintf(buf, sizeof(buf), "Invalid cgi_program name!\n");
								add_to_buf(&requestP[conn_fd], buf, buflen);
							}

							nwritten = write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
							nwritten = write(1, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
							requestP[conn_fd].status = ERROR;
							close(requestP[conn_fd].conn_fd);
							free_request(&requestP[conn_fd]);
							FD_CLR(conn_fd, &sel);
							break;
						}

						else {
							if (file_valid == 1 && prog_valid != 1){
								childnum ++;
								strcpy(childyes, "2");
								pipe(requestP[conn_fd].pipe_fd1);
								pipe(requestP[conn_fd].pipe_fd2);
								requestP[requestP[conn_fd].pipe_fd1[0]].conn_fd = conn_fd;
								if ((requestP[conn_fd].pid = fork()) == 0){
									requestP[conn_fd].dup2_fd1[1] = dup2(requestP[conn_fd].pipe_fd1[1], STDOUT_FILENO);
									requestP[conn_fd].dup2_fd2[0] = dup2(requestP[conn_fd].pipe_fd2[0], STDIN_FILENO);
									char execute[1025];
									write(requestP[conn_fd].pipe_fd2[1], requestP[conn_fd].filename, strlen(requestP[conn_fd].filename));
									sprintf(execute, "./%s", requestP[conn_fd].progname);
									fprintf(stderr, "%s\n", requestP[conn_fd].progname);
									execlp(execute, execute, logfilenameP, (char*)0);
								}
								waitpid(requestP[conn_fd].pid, requestP[conn_fd].pipe_status, 0);
								close(requestP[conn_fd].pipe_fd1[0]);
								close(requestP[conn_fd].pipe_fd1[1]);
								close(requestP[conn_fd].pipe_fd2[0]);
								close(requestP[conn_fd].pipe_fd2[1]);
							}

							requestP[conn_fd].buf_len = 0;
							buflen = snprintf(buf, sizeof(buf), "HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
							add_to_buf(&requestP[conn_fd], buf, buflen);
						 	now = time((time_t*)0);
							(void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
							buflen = snprintf(buf, sizeof(buf), "Date: %s\015\012", timebuf);
							add_to_buf(&requestP[conn_fd], buf, buflen);
							if (file_valid == 1 && prog_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)49);
							else if (file_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)30);
							else if (prog_valid == 1) buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)33);
							add_to_buf(&requestP[conn_fd], buf, buflen);
							buflen = snprintf(buf, sizeof(buf), "Connection: close\015\012\015\012");
							add_to_buf(&requestP[conn_fd], buf, buflen);
							buflen = snprintf(buf, sizeof(buf), "404 Not Found\n");
							add_to_buf(&requestP[conn_fd], buf, buflen);

							if (file_valid == 1){
								buflen = snprintf(buf, sizeof(buf), "File not found!\n");
								add_to_buf(&requestP[conn_fd], buf, buflen);
							}
							if (prog_valid == 1){
								buflen = snprintf(buf, sizeof(buf), "Program not found!\n");
								add_to_buf(&requestP[conn_fd], buf, buflen);
							}
							
							nwritten = write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
							nwritten = write(1, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
							requestP[conn_fd].status = ERROR;
							close(requestP[conn_fd].conn_fd);
							free_request(&requestP[conn_fd]);
							FD_CLR(conn_fd, &sel);
							break;
						}
					}
				}
			}

			else if (FD_ISSET(i, &tmp)){
				conn_fd = requestP[i].conn_fd;
				int buflen;
				char buf[65536];
				char filecontent[65536];
				memset(filecontent, '\0', sizeof(filecontent));
				read(requestP[conn_fd].pipe_fd1[0], filecontent, 65536);

				buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)strlen(filecontent));
				add_to_buf(&requestP[conn_fd], buf, buflen);
				buflen = snprintf(buf, sizeof(buf), "Connection: close\015\012\015\012");
				add_to_buf(&requestP[conn_fd], buf, buflen);
				
				buflen = snprintf(buf, sizeof(buf), filecontent);
				add_to_buf(&requestP[conn_fd], buf, buflen);

				// ready for writing
				fprintf(stderr, "writing (buf %p, idx %2d)%4d bytes to request fd %3d\n", 
									requestP[conn_fd].buf, (int)requestP[conn_fd].buf_idx,
										(int)requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd);
				
				nwritten = write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
				nwritten = write(1, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
				fprintf(stderr, "complete writing %4d bytes on fd %3d\n\n", nwritten, requestP[conn_fd].conn_fd);

				childnum ++;
				requestP[conn_fd].run = 0;
				waitpid(requestP[conn_fd].pid, requestP[conn_fd].pipe_status, 0);
				close(requestP[conn_fd].conn_fd);
				close(requestP[conn_fd].pipe_fd1[0]);
				close(requestP[conn_fd].pipe_fd1[1]);
				close(requestP[conn_fd].pipe_fd2[0]);
				close(requestP[conn_fd].pipe_fd2[1]);
				free_request(&requestP[conn_fd]);
				FD_CLR(conn_fd, &sel);
				FD_CLR(requestP[conn_fd].pipe_fd1[0], &sel);
			}
		}
	}
	free(requestP);
	return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request(http_request* reqP){
	reqP->conn_fd = -1;
	reqP->status = 0;		// not used
	reqP->file[0] = (char)0;
	reqP->query[0] = (char)0;
	reqP->host[0] = (char)0;
	reqP->buf = NULL;
	reqP->buf_size = 0;
	reqP->buf_len = 0;
	reqP->buf_idx = 0;
}

static void free_request(http_request* reqP){
	if (reqP->buf != NULL){
		free(reqP->buf);
		reqP->buf = NULL;
	}
	init_request(reqP);
}

#define ERR_RET(error){*errP = error; return -1;}
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected
//
static int read_header_and_file(http_request* reqP, int *errP){
	// Request variables
	char* file = (char *)0;
	char* path = (char *)0;
	char* query = (char *)0;
	char* protocol = (char *)0;
	char* method_str = (char *)0;
	int r;
	char buf[65536];

	// Read in request from client
	while (1){
		r = read(reqP->conn_fd, buf, sizeof(buf));
		if (r < 0 && (errno == EINTR || errno == EAGAIN)) return 1;
		if (r <= 0) ERR_RET(1)
		add_to_buf(reqP, buf, r);
		if (strstr(reqP->buf, "\015\012\015\012") != (char*)0 || strstr(reqP->buf, "\012\012") != (char*)0) break;
	}
	// fprintf(stderr, "header: %s\n", reqP->buf);

	// Parse the first line of the request.
	method_str = get_request_line(reqP);
	if (method_str == (char*)0) ERR_RET(2)
	path = strpbrk(method_str, " \t\012\015");
	if (path == (char*)0) ERR_RET(2)
	*path++ = '\0';
	path += strspn(path, " \t\012\015");
	protocol = strpbrk(path, " \t\012\015");
	if (protocol == (char*)0) ERR_RET(2)
	*protocol++ = '\0';
	protocol += strspn(protocol, " \t\012\015");
	query = strchr(path, '?');
	if (query == (char*)0) query = "";
	else *query++ = '\0';

	if (strcasecmp(method_str, "GET") != 0) ERR_RET(3)
	else {
		strdecode(path, path);
		if (path[0] != '/') ERR_RET(4)
	else file = &(path[1]);
	}

	if (strlen(file) >= MAXBUFSIZE - 1) ERR_RET(4)
	if (strlen(query) >= MAXBUFSIZE - 1) ERR_RET(5)
	  
	strcpy(reqP->file, file);
	strcpy(reqP->query, query);

	/*
	char timebuf[100];
	int buflen;
	time_t now;

	int fd;
	struct stat sb;
	void *ptr;

	if (query[0] == (char)0){
		// for file request, read it in buf
		r = stat(reqP->file, &sb);
		if (r < 0)ERR_RET(6)

		fd = open(reqP->file, O_RDONLY);
		if (fd < 0)ERR_RET(7)

	reqP->buf_len = 0;

		buflen = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
		add_to_buf(reqP, buf, buflen);
	 	now = time((time_t*)0);
		(void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
		buflen = snprintf(buf, sizeof(buf), "Date: %s\015\012", timebuf);
		add_to_buf(reqP, buf, buflen);
	buflen = snprintf(
		buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)sb.st_size);
		add_to_buf(reqP, buf, buflen);
		buflen = snprintf(buf, sizeof(buf), "Connection: close\015\012\015\012");
		add_to_buf(reqP, buf, buflen);

	ptr = mmap(0, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == (void*)-1)ERR_RET(8)
		add_to_buf(reqP, ptr, sb.st_size);
	(void)munmap(ptr, sb.st_size);
	close(fd);
	// printf("%s\n", reqP->buf);
	// fflush(stdout);
	reqP->buf_idx = 0; // writing from offset 0
	return 0;
	}
	*/

	return 0;
}

static void add_to_buf(http_request *reqP, char* str, size_t len){ 
	char** bufP = &(reqP->buf);
	size_t* bufsizeP = &(reqP->buf_size);
	size_t* buflenP = &(reqP->buf_len);

	if (*bufsizeP == 0){
		*bufsizeP = len + 500;
		*buflenP = 0;
		*bufP = (char*)e_malloc(*bufsizeP);
	}
	else if (*buflenP + len >= *bufsizeP){
		*bufsizeP = *buflenP + len + 500;
		*bufP = (char*)e_realloc((void*)*bufP, *bufsizeP);
	}
	(void)memmove(&((*bufP)[*buflenP]), str, len);
	*buflenP += len;
	(*bufP)[*buflenP] = '\0';
}

static char* get_request_line(http_request *reqP){ 
	int begin;
	char c;

	char *bufP = reqP->buf;
	int buf_len = reqP->buf_len;

	for (begin = reqP->buf_idx ; reqP->buf_idx < buf_len; ++reqP->buf_idx){
		c = bufP[reqP->buf_idx];
		if (c == '\012' || c == '\015'){
			bufP[reqP->buf_idx] = '\0';
			++reqP->buf_idx;
			if (c == '\015' && reqP->buf_idx < buf_len && bufP[reqP->buf_idx] == '\012'){
				bufP[reqP->buf_idx] = '\0';
				++reqP->buf_idx;
			}
			return &(bufP[begin]);
		}
	}
	fprintf(stderr, "http request format error\n");
	exit(1);
}

static void init_http_server(http_server *svrP, unsigned short port){
	struct sockaddr_in servaddr;
	int tmp;

	gethostname(svrP->hostname, sizeof(svrP->hostname));
	svrP->port = port;
   
	svrP->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (svrP->listen_fd < 0) ERR_EXIT("socket")

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	tmp = 1;
	if (setsockopt(svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp))< 0) ERR_EXIT ("setsockopt ")
	if (bind(svrP->listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))< 0) ERR_EXIT("bind")
	if (listen(svrP->listen_fd, 1024)< 0) ERR_EXIT("listen")
}

// Set NDELAY mode on a socket.
static void set_ndelay(int fd){
	int flags, newflags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags != -1){
		newflags = flags | (int)O_NDELAY; // nonblocking mode
		if (newflags != flags) (void)fcntl(fd, F_SETFL, newflags);
	}
}   

static void strdecode(char* to, char* from){
	for (; *from != '\0'; ++to, ++from){
		if (from[0] == '%' && isxdigit(from[1])&& isxdigit(from[2])){
			*to = hexit(from[1])* 16 + hexit(from[2]);
			from += 2;
		}
		else {
			*to = *from;
		}
	}
	*to = '\0';
}

static int hexit(char c){
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;		   // shouldn't happen
}

static void* e_malloc(size_t size){
	void* ptr;

	ptr = malloc(size);
	if (ptr == (void*)0){
		(void)fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return ptr;
}

static void* e_realloc(void* optr, size_t size){
	void* ptr;

	ptr = realloc(optr, size);
	if (ptr == (void*)0){
		(void)fprintf(stderr, "out of memory\n");
		exit(1);
	}
	return ptr;
}
