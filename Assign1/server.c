#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/file.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
	char hostname[512];		// server's hostname
	unsigned short port;	// port to listen
	int listen_fd;			// fd to wait for a new connection
} server;

typedef struct {
	char host[512];			// client's host
	int conn_fd;			// fd to talk with client
	char buf[512];			// data sent by/to client
	size_t buf_len;			// bytes used by buf
	char* filename;			// filename set in header, end with '\0'
	int header_done;		// used by handle_read to know if the header is read or not
} request;

// client and server communicate by reading from and writing to socket descriptors

server svr;					// server
request* requestP = NULL;	// point to a list of requests
int maxfd;					// size of open file descriptor table, size of request list

const char* accept_header = "ACCEPT\n";
const char* reject_header = "REJECT\n";

// forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error

int main(int argc, char** argv)
// argc for the number of the parameters
// argv for the content of the parameters
// argv[0] is the request (read or write); argv[1] is the port (4000 or 4001)		(?)
{
	int i, ret;

	struct sockaddr_in cliaddr;	// used by accept()
	int clilen;

	int conn_fd;			// fd for a new connection with clienetm, client's representation 
	int file_fd[1030];		// fd for file that we open for reading, file's representation
							// avoid being replaced by the new result account for the new conn_fd

	char buf[512];
	int buf_len;
	fd_set sel;				// main select solution
	fd_set tmp;				// temp select solution
	fd_set rtmp, wtmp;
	struct timeval time;	// timeout interval

	// parse args if the input is wrong
	if (argc != 2){
		fprintf(stderr, "usage: %s [port]\n", argv[0]);
		exit(1);
	}

	// initialize server
	// atoi change the char to int
	init_server((unsigned short) atoi(argv[1]));

	// get file descripter table size and initize request table
	maxfd = getdtablesize();
	requestP = (request*) malloc(sizeof(request) * maxfd);
	if (requestP == NULL)
		ERR_EXIT("out of memory allocating all requests");
	for (i = 0; i < maxfd; i ++)
		init_request(&requestP[i]);

	// connect the client and the server
	requestP[svr.listen_fd].conn_fd = svr.listen_fd;
	strcpy(requestP[svr.listen_fd].host, svr.hostname);

	// loop for handling connections
	fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

	// initialize select solution
	FD_ZERO(&sel);
	FD_SET(svr.listen_fd, &sel);

	// initialize the time
	time.tv_sec = 5;

	while (1){
		// TODO: Add IO multiplexing
		tmp = sel;
		int k = select(maxfd + 1, &tmp, NULL, NULL, &time);
		if (k == 0) continue;

		// the result of the select can be more than one
		for (int i = 0; i <= maxfd; i ++){
			// check new connection (if it is ready)
			// if the ready request_conn.fd is same as the one the server is going to listen_fd
			if (FD_ISSET(i, &tmp) && i == svr.listen_fd){
				clilen = sizeof(cliaddr);
				conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
				if (conn_fd < 0){
					if (errno == EINTR || errno == EAGAIN) continue;
					if (errno == ENFILE){
						(void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
						continue;
					}
					ERR_EXIT("accept")
				}
				// set the socket file descriptor of requestP
				// inet_ntoa change the int into char
				requestP[conn_fd].conn_fd = conn_fd;
				strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
				fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
				FD_SET(conn_fd, &sel);

				file_fd[conn_fd] = -1;
			}

			else if (FD_ISSET(i, &tmp)){

				#ifdef READ_SERVER				
				// file_fd[conn_fd] is from requestP[conn_fd].conn_fd
				ret = handle_read(&requestP[i]);					// read from the request

				if (ret < 0){
					fprintf(stderr, "bad request from %s\n", requestP[i].host);
					continue;
				}

				// requestP[conn_fd]->filename is guaranteed to be successfully set
				if (file_fd[i] == -1){
					// open the file here
					file_fd[i] = open(requestP[i].filename, O_RDONLY, 0);

					// TODO: Add lock
					int check = flock(file_fd[i], LOCK_SH | LOCK_NB);

					// TODO: check if the request should be rejected
					if (check == 0){
						// write "ACCEPT" and turn the file into read only mode
						write(requestP[i].conn_fd, accept_header, sizeof(accept_header));
						fprintf(stderr, "Opening file [%s]\n", requestP[i].filename);
						
						while (1){
							// record the status of reading
							ret = read(file_fd[i], buf, sizeof(buf));
							if (ret < 0){
								fprintf(stderr, "Error when reading file %s\n", requestP[i].filename);
								break;
							}
							else if (ret == 0){
								fprintf(stderr, "Done reading file [%s]\n", requestP[i].filename);
								flock(file_fd[i], LOCK_UN);
								break;
							}
							write(requestP[i].conn_fd, buf, ret);
						}
					}
					else {
						// write "REJECT"
						write(requestP[i].conn_fd, reject_header, sizeof(reject_header));
					}
				}
				#endif

				#ifndef READ_SERVER
				// file_fd[conn_fd] is from requestP[conn_fd].conn_fd
				ret = handle_read(&requestP[i]);					// read from the request						

				if (ret < 0){
					fprintf(stderr, "bad request from %s\n", requestP[i].host);
					continue;
				}

				// requestP[conn_fd]->filename is guaranteed to be successfully set
				if (file_fd[i] == -1){
					// open the file here
					file_fd[i] = open(requestP[i].filename, O_WRONLY | O_CREAT | O_TRUNC,
										S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

					// TODO: Add lock
					int check = flock(file_fd[i], LOCK_EX | LOCK_NB);

					// TODO: check if the request should be rejected
					if (check == 0){
						// write "ACCEPT" and turn the file into write only mode
						write(requestP[i].conn_fd, accept_header, sizeof(accept_header));
						fprintf(stderr, "Opening file [%s]\n", requestP[i].filename);
					}
					else {
						// write "REJECT"
						write(requestP[i].conn_fd, reject_header, sizeof(reject_header));
						if (file_fd[i] >= 0) close(file_fd[i]);
						close(requestP[i].conn_fd);
						free_request(&requestP[i]);
						FD_CLR(i, &sel);
						continue;
					}
				}
				if (ret > 0){
					write(file_fd[i], requestP[i].buf, requestP[i].buf_len);
					continue;
				}

				if (ret == 0){
					fprintf(stderr, "Done writing file [%s]\n", requestP[i].filename);
					flock(file_fd[i], LOCK_UN);
				}
				#endif

				if (file_fd[i] >= 0) close(file_fd[i]);
				close(requestP[i].conn_fd);
				free_request(&requestP[i]);
				FD_CLR(i, &sel);
			}
		}
	}

	free(requestP);
	return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);

static void init_request(request* reqP)
{
	reqP->conn_fd = -1;
	reqP->buf_len = 0;
	reqP->filename = NULL;
	reqP->header_done = 0;
}

static void free_request(request* reqP)
{
	if (reqP->filename != NULL) {
		free(reqP->filename);
		reqP->filename = NULL;
	}
	init_request(reqP);
}

// return 0: socket ended, request done
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0
// It's guaranteed that the header would be correctly set after the first read
// error code:
// -1: client connection error
static int handle_read(request* reqP)
{
	int r;
	char buf[512];

	// read in request from client
	r = read(reqP->conn_fd, buf, sizeof(buf));
	if (r < 0) return -1;
	if (r == 0) return 0;
	if (reqP->header_done == 0) {
		char* p1 = strstr(buf, "\015\012");
		int newline_len = 2;
		// be careful that in Windows, line ends with \015\012
		if (p1 == NULL){
			p1 = strstr(buf, "\012");
			newline_len = 1;
			if (p1 == NULL){
				// This would not happen in testing, but you can fix this if you want
				ERR_EXIT("header not complete in first read...");
			}
		}
		size_t len = p1 - buf + 1;
		reqP->filename = (char*)e_malloc(len);
		memmove(reqP->filename, buf, len);
		reqP->filename[len - 1] = '\0';
		p1 += newline_len;
		reqP->buf_len = r - (p1 - buf);
		memmove(reqP->buf, p1, reqP->buf_len);
		reqP->header_done = 1;
	}
	else {
		reqP->buf_len = r;
		memmove(reqP->buf, buf, r);
	}
	return 1;
}

static void init_server(unsigned short port)
{
	struct sockaddr_in servaddr;
	int tmp;

	gethostname(svr.hostname, sizeof(svr.hostname));
	svr.port = port;

	svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (svr.listen_fd < 0) ERR_EXIT("socket");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	tmp = 1;
	if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0)
		ERR_EXIT("setsockopt");

	if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) 
		ERR_EXIT("bind");

	if (listen(svr.listen_fd, 1024) < 0)
		ERR_EXIT("listen");
}

static void* e_malloc(size_t size)
{
	void* ptr;

	ptr = malloc(size);
	if (ptr == NULL) ERR_EXIT("out of memory");
	return ptr;
}