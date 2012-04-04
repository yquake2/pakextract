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
	int file_length;
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

	if ((header.dir_length % 64) != 0)
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
read_directory(FILE *fd)
{
	int i;
	directory *head, *cur;

	head = NULL;

	/* Navigate to the directory */
	fseek(fd, header.dir_offset, SEEK_SET);

	for (i = 0; i < (header.dir_length / 64); i++)
	{
		cur = malloc(sizeof(directory));

		if (fread(cur, sizeof(directory) - sizeof(directory *), 1, fd) != 1)
		{
			perror("Could not read directory entry");
			return NULL;
		}

		cur->next = head;
		head = cur;
	}

	return head;
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
	char b;
	directory *old_d;
	FILE *out;
	int i;

	while (d != NULL)
	{
	    mktree(d->file_name);

        out = fopen(d->file_name, "w");
		if (out == NULL)
		{
			perror("Could open the outputfile");
			return;
		}

		fseek(fd, d->file_pos, SEEK_SET);

		for (i = 0; i < d->file_length; i++)
		{
			fread(&b, sizeof(char), 1, fd);
			fwrite(&b, sizeof(char), 1, out);
		}

		fclose(out);

		old_d = d;
		d = d->next;
		free(old_d);
	}
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
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s pakfile\n", argv[0]);
		exit(-1);
	}

	/* Open the pak file */
	fd = fopen(argv[1], "r");
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
	d = read_directory(fd);
	if (d == NULL)
	{
		fclose(fd);
		exit(-1);
	}

	/* And now extract the files */
	extract_files(fd, d);

	/* cleanup */
	fclose(fd);

	return 0;
}

