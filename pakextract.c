/*-
 * Copyright (c) 2012 Yamagi Burmeister
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <assert.h>

#ifdef DAIKATANA
#define HDR_DIR_LEN 72
#else
#define HDR_DIR_LEN 64
#endif

/* Holds the pak header */
struct
{
	char signature[4];
	int dir_offset;
	int dir_length;
} header;

/* A directory entry */
struct directory_s
{
	char file_name[56];
	int file_pos;
	int file_length; // in case of is_compressed: size after decompression
#ifdef DAIKATANA
	int compressed_length; // size of compressed data in the pak
	int is_compressed; // 0: uncompressed, else compressed
#endif
	struct directory_s *next;
};
typedef struct directory_s directory;
 
/*
 * Creates a directory tree.
 *
 *  *s -> The path to create. The last
 *        part (the file itself) is
 *        ommitted
 */
void
mktree(char *s)
{
	char *dir;
	char *elements[28];
	char *path;
	char *token;
	int i, j;

	path = calloc(56, sizeof(char));
	dir = malloc(sizeof(char) * 56);

	strncpy(dir, s, sizeof(char) * 56);

	for (i = 0; (token = strsep(&dir, "/")) != NULL; i++)
	{
		elements[i] = token;
	}

	for (j = 0; j < i - 1; j++)
	{
		strcat(path, elements[j]);
		strcat(path, "/");

		mkdir(path, 0700);
	}

	free(path);
	free(dir);
}

/*
 * Reads the pak file header and
 * stores it into the global struct
 * "header".
 *
 *  *fd -> A file descriptor holding
 *         the pack to be read.
 */
int
read_header(FILE *fd)
{
    if (fread(&header, sizeof(header), 1, fd) != 1)
	{
		perror("Could not read the pak file header");
		return -1;
	}

	if (strncmp(header.signature, "PACK", 4))
	{
		fprintf(stderr, "Not a pak file\n");
		return -1;
	}

	if ((header.dir_length % HDR_DIR_LEN) != 0)
	{
		fprintf(stderr, "Corrupt pak file\n");
		return -1;
	}

	return 0;
}

/*
 * Reads the directory of a pak file
 * into a linked list and returns 
 * a pointer to the first element.
 *
 *  *fd -> a file descriptor holding
 *         holding the pak to be read
 */ 
directory *
read_directory(FILE *fd, int listOnly)
{
	int i;
	directory *head, *cur;

	head = NULL;

	/* Navigate to the directory */
	fseek(fd, header.dir_offset, SEEK_SET);

	for (i = 0; i < (header.dir_length / HDR_DIR_LEN); i++)
	{
		cur = malloc(sizeof(directory));

		if (fread(cur, HDR_DIR_LEN, 1, fd) != 1)
		{
			perror("Could not read directory entry");
			return NULL;
		}

		if(listOnly)
		{
			printf("%s (%d bytes", cur->file_name, cur->file_length);
		#ifdef DAIKATANA
			if(cur->is_compressed) printf(", %d compressed", cur->compressed_length);
		#endif
			printf(")\n");
		}
		
		cur->next = head;
		head = cur;
	}

	return head;
}

#ifdef DAIKATANA

static void extract_compressed(FILE* in, directory *d)
{
	FILE *out;
	int offset;
	int read;
	int written;
	int x;
	int num;
	unsigned char *in_buf;
	unsigned char *out_buf;

	if ((out = fopen(d->file_name, "w")) == NULL)
	{
		perror("Couldn't open outputfile");
		return;
	}

	if ((in_buf = malloc(d->compressed_length)) == NULL)
	{
		perror("Couldn't allocate memory");
		return;
	}

	if ((out_buf = calloc(1, d->file_length)) == NULL)
	{
		perror("Couldn't allocate memory");
		return;
	}

	fseek(in, d->file_pos, SEEK_SET);
	fread(in_buf, d->compressed_length, 1, in);

	read = 0;
	written = 0;

	while (read < d->compressed_length)
	{
		x = in_buf[read];
		++read;

		// x + 1 bytes of uncompressed data
		if (x < 64)
		{
			num = x + 1;
			memmove(out_buf + written, in_buf + read, num);

			read += num;
			written += num;

			continue;
		}
		// x - 62 zeros
		else if (x < 128)
		{
			num = x - 62;
			memset(out_buf + written, 0, num);

			written += num;

			continue;
		}
		// x - 126 times the next byte
		else if (x < 192)
		{
			num = x - 126;
			memset(out_buf + written, in_buf[read], num);

			++read;
			written += num;

			continue;
		}
		// Reference previously uncompressed data
		else if (x < 254)
		{
			num = x - 190;

			offset = (int)in_buf[read] + 2;
			++read;

			memmove(out_buf + written, (out_buf + written) - offset, num);
			written += num;
		}
		// Terminate
		else if (x == 255)
		{
			break;
		}
	}

	fwrite(out_buf, d->file_length, 1, out);
	fclose(out);

	free(in_buf);
	free(out_buf);
}
#endif

static void extract_raw(FILE* in, directory *d)
{
	FILE* out = fopen(d->file_name, "w");
	if (out == NULL)
	{
		perror("Could open the outputfile");
		return;
	}
	
	// just copy the data from the .pak to the output file (in chunks for speed)
	int bytes_left = d->file_length;
	char buf[2048];
	
	fseek(in, d->file_pos, SEEK_SET);
	
	while(bytes_left >= sizeof(buf))
	{
		fread(buf, sizeof(buf), 1, in);
		fwrite(buf, sizeof(buf), 1, out);
		bytes_left -= sizeof(buf);
	}
	if(bytes_left > 0)
	{
		fread(buf, bytes_left, 1, in);
		fwrite(buf, bytes_left, 1, out);
	}

	fclose(out);
}

/* 
 * Extract the files from a pak.
 *
 *  *d -> a pointer to the first element
 *        of the pak directory
 *
 *  *fd -> a file descriptor holding
 *         the pak to be extracted
 */
void
extract_files(FILE *fd, directory *d)
{
	directory *old_d;

	while (d != NULL)
	{
	    mktree(d->file_name);

#ifdef DAIKATANA
		if(d->is_compressed)
		{
			extract_compressed(fd, d);
		}
		else
#endif
		{
			extract_raw(fd, d);
		}

		old_d = d;
		d = d->next;
		free(old_d);
	}
}

static void printUsage(const char* argv0)
{
#ifdef DAIKATANA
	fprintf(stderr, "Extractor for Daikatana .pak files\n");
#else
	fprintf(stderr, "Extractor for Quake/Quake2 (and compatible) .pak files\n");
#endif
	fprintf(stderr, "Usage: %s [-l] pakfile\n", argv0);
	fprintf(stderr, "       -l don't extract, just list contents\n");
}

/*
 * A small programm to extract a Quake II pak file.
 * The pak file is given as the first an only 
 * argument.
 */
int
main(int argc, char *argv[])
{
	directory *d;
	FILE *fd;

	/* Correct usage? */
	if (argc < 2)
	{
		printUsage(argv[0]);
		exit(-1);
	}

	int listOnly = 0;

	if(strcmp(argv[1], "-l") == 0)
	{
		if(argc < 3)
		{
			printUsage(argv[0]);
			exit(-1);
		}
		listOnly = 1;
	}

	const char* filename = listOnly ? argv[2] : argv[1];

	/* Open the pak file */
	fd = fopen(filename, "r");
	if (fd == NULL)
	{
		perror("Could not open the pak file");
		exit(-1);
	}

	/* Read the header */
    if (read_header(fd) < 0)
	{
		fclose(fd);
		exit(-1);
	}

	/* Read the directory */
	d = read_directory(fd, listOnly);
	if (d == NULL)
	{
		fclose(fd);
		exit(-1);
	}

	if(!listOnly)
	{
		/* And now extract the files */
		extract_files(fd, d);
	}

	/* cleanup */
	fclose(fd);

	return 0;
}

