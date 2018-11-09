all: server.c file_reader.c mmap_read.c mmap_write.c
	gcc server.c -o server
	gcc file_reader.c -o file_reader
	gcc file_reader.c -o file_reader_slow -D slow
	gcc file_reader.c -o file_reader_fast
	gcc mmap_read.c -o mmap_read
	gcc mmap_write.c -o mmap_write

clean:
	rm -f server file_reader file_reader_slow file_reader_fast mmap_read mmap_write info