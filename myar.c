/*
 * =====================================================================================
 *
 *       Filename:  myar.c
 *    Description:  Archiving utility based on "ar"
 *       Compiler:  gcc
 *         Author:  Ben Nejad (bn), benjamin.nejad@gmail.com
 *
 * =====================================================================================
 */
#define _BSD_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ar.h>
#include <ctype.h>
#include <utime.h>
#include <time.h>
#include <dirent.h>

#define STR_SIZE sizeof("rwxrwxrwx")
#define FP_SPECIAL 1 

#define HEADER_SIZE 61
#define HEADER_READ_SIZE 60
#define BLOCKSIZE 1024

char *file_perm_string(mode_t perm, int flags);

int main(int argc, char **argv)
{
	DIR *dp;

	char *archive;
	char *permstring;
	char read_buf[BLOCKSIZE], header[HEADER_SIZE], name[16], minibuf[1];
	static char timestr[100];

	unsigned long filesize;
	long temp;
	int i,x,c,flag, in_fd, out_fd, keep, num_read, spot_optind, del_read, match_read, total_written, exists = 0, num_written = 0;
	
	struct stat stat_buf;
	struct ar_hdr extractheader;
	struct utimbuf utimeStruct;
	struct tm *timestruct;
	struct dirent *entry;


	while ((c = getopt (argc, argv, "A:v:t:d:x:q:")) != -1)
	switch(c){
		case 'q':
			/* Open or create archive file */
			archive = optarg;
			umask(0);
			out_fd = open(archive, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
			if( out_fd < 0){
				if(errno == EEXIST){
					exists = 1;
					out_fd = open(archive, O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
					if(out_fd < 0){
						perror("Cannot open archive file.");
						exit(-1);
					}
					lseek(out_fd, 0, SEEK_END);
				}
			}

			/* Write header to archive if file newly created */
			if(exists != 1){
				if( (num_written = write(out_fd, ARMAG, SARMAG)) != SARMAG){
					perror("Error writing header to archive file.");
					exit(-1);
				}
			}
			
			for( ; optind < argc && *argv[optind] != '-'; optind++){
				memset(name, 0, 16);
				/* Reformat name and add trailing slash */
				for(x=0; x <16; x++){
					if(argv[optind][x] == '\0'){
						name[x] = '/';
						break;
					} else{
						name[x]=argv[optind][x];
					}
				}

				/* Open file */
				in_fd = open(argv[optind], O_RDONLY);
				if(in_fd < 0){
					perror("Unable to open one or more input files.");
					unlink(archive);
					exit(-1);
				}

				/* Get file stats */
				fstat(in_fd, &stat_buf);

				/* Format and write file header */
				snprintf(header, HEADER_SIZE, "%-16s%-12ld%-6d%-6d%-8o%-10ld%-2s", name,stat_buf.st_mtime,stat_buf.st_uid,stat_buf.st_gid,stat_buf.st_mode,stat_buf.st_size,ARFMAG);
				num_written = write(out_fd, header, HEADER_SIZE);
				if(num_written == -1){
					perror("Error writing file header.");
					unlink(archive);
					exit(-1);
				}

				

				/* Elimnates end line caused by snprintf */
				lseek(out_fd, -1, SEEK_CUR);

				/* Write File */
				while((num_read = read(in_fd, read_buf, BLOCKSIZE)) > 0) {
					num_written = write(out_fd, read_buf, num_read);

					if(num_read != num_written || num_written == -1){
						perror("Error writing files to archive.");
						unlink(archive);
						exit(-1);
					}
				}

				fstat(out_fd, &stat_buf);
				if(stat_buf.st_size % 2 != 0){
				num_written = write(out_fd, "\n", 1);
				}
			}		
			

			close(in_fd);
			close(out_fd);
			break;

		case 'x':
			archive = optarg;
			in_fd = open(archive, O_RDONLY);
			if(in_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}
			num_read = read(in_fd, read_buf, 8);

			if(strncmp(read_buf, ARMAG, 8) != 0){
				perror("Unknown archive format.");
				exit(-1);
			}

			spot_optind = optind; 
			while((num_read = read(in_fd, read_buf, 61)) > 0){
				flag =0;
				sscanf(read_buf, "%s%s%s%s%s%s%s", extractheader.ar_name, extractheader.ar_date, extractheader.ar_uid, extractheader.ar_gid, extractheader.ar_mode, extractheader.ar_size, extractheader.ar_fmag );
				filesize=strtoul(extractheader.ar_size, NULL, 10) -1;

				for(i=spot_optind ; i < argc && *argv[i] != '-'; i++){
					if(strncmp(extractheader.ar_name, argv[i], strlen(argv[i])-1) == 0){
						flag = 1;
						lseek(in_fd, -1, SEEK_CUR);
						
						out_fd = open(argv[i], O_WRONLY|O_CREAT, strtoul(extractheader.ar_mode, NULL, 8));
						if( out_fd < 0){
							perror("Cannot write extracted file.");
							exit(-1);
						}

						total_written = 0;
						while((match_read = read(in_fd, minibuf, 1)) > 0 && total_written <= filesize){
							num_written = write(out_fd, minibuf, match_read);
							if(num_written == -1){
								perror("Cannot write extracted file.");
								unlink(argv[i]);
								exit(-1);
							}
							total_written++;
						}
						utimeStruct.actime = strtoul(extractheader.ar_date, NULL, 10);
						utimeStruct.modtime = strtoul(extractheader.ar_date, NULL, 10);
						if(utime(argv[i], &utimeStruct) != 0){
							perror("Cannot write times to extracted file.");
							unlink(argv[i]);
							exit(-1);
						}

						printf("Wrote %d bytes to %s\n", total_written, argv[i]);
						close(out_fd);
						lseek(in_fd, -1, SEEK_CUR);
					}
				}
				if(flag == 0){
					lseek(in_fd, filesize, SEEK_CUR);
				}
				
			}

			close(in_fd);
			break;


		case 'd':
			/* Open Archive File */
			archive = optarg;
			in_fd = open(archive, O_RDONLY);
			if(in_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}

			/* Validate AR Magic String */
			num_read = read(in_fd, read_buf, 8);
			if(strncmp(read_buf, ARMAG, 8) != 0){
				perror("Unknown archive format.");
				exit(-1);
			}

			/* Unlink original archive */
			unlink(archive);

			/* Set umask to 0 to allow 666 permission */
			umask(0);
			/* Create new archive with same name */
			out_fd = open(archive, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
			if(out_fd < 0){
					perror("Unable to recreate archive file.");
					exit(-1);
			}

			/* Write archive magic string */
			num_written = write(out_fd, ARMAG, SARMAG);
			if( num_written != SARMAG){
					perror("Error writing archive header.");
					exit(-1);
			}

			/* Store option/argument index */
			spot_optind = optind; 
			

			while((num_read = read(in_fd, read_buf, HEADER_READ_SIZE)) == HEADER_READ_SIZE){
				/* Read header into struct */
				sscanf(read_buf, "%s%s%s%s%s%s%s", extractheader.ar_name, extractheader.ar_date, extractheader.ar_uid, extractheader.ar_gid, extractheader.ar_mode, extractheader.ar_size, extractheader.ar_fmag );

				/* Convert size string into decimal */
				filesize=strtoul(extractheader.ar_size, NULL, 10);

				
				keep = 1;
				/* Interates through each filename passed to delete */
				for(i=spot_optind ; i < argc && *argv[i] != '-'; i++){
					/* Check if the name extracted from the header does not match the file we are currenting iterating on */
					
					if(strncmp(extractheader.ar_name, argv[i], strlen(argv[i])-1) == 0){
						keep = 0;
						break;
					}
				}

				
				if(keep == 1){
					/* Write header */
					num_written = write(out_fd, read_buf, HEADER_READ_SIZE);
					if(num_written != HEADER_READ_SIZE){
						perror("Error retaining file.");
						exit(-1);
					}

					/* Write file */
					total_written = 0;
					while( (del_read = read(in_fd, minibuf, 1)) > 0 && total_written < filesize){
						num_written = write(out_fd, minibuf, del_read);
						if(num_written == -1){
								perror("Cannot write retained file.");
								exit(-1);
						}
						total_written++;
					}
					lseek(in_fd, -1, SEEK_CUR);
				} else{
					/* Seek ahead the amount of the filesize */
					lseek(in_fd, filesize, SEEK_CUR);
				}

				/* Determine if padding was used */
				if(filesize % 2 != 0)
					lseek(in_fd, 1, SEEK_CUR);
			}

			close(in_fd);
			close(out_fd);
			break;


		case 't':
			/* Open Archive File */
			archive = optarg;
			in_fd = open(archive, O_RDONLY);
			if(in_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}

			/* Validate AR Magic String */
			num_read = read(in_fd, read_buf, 8);
			if(strncmp(read_buf, ARMAG, 8) != 0){
				perror("Unknown archive format.");
				exit(-1);
			}

			while((num_read = read(in_fd, read_buf, HEADER_READ_SIZE)) == HEADER_READ_SIZE){
				/* Read header into struct */
				sscanf(read_buf, "%s%s%s%s%s%s%s", extractheader.ar_name, extractheader.ar_date, extractheader.ar_uid, extractheader.ar_gid, extractheader.ar_mode, extractheader.ar_size, extractheader.ar_fmag );

				/* Convert size string into decimal */
				filesize=strtoul(extractheader.ar_size, NULL, 10);

				/* Print file names */
				i = 0;
				while(extractheader.ar_name[i] != '/'){
					printf("%c", extractheader.ar_name[i]);
					i++;
				}
				printf("\n");

				/* Seek ahead the amount of the filesize */
				lseek(in_fd, filesize, SEEK_CUR);

				/* Determine if padding was used */
				if(filesize % 2 != 0)
					lseek(in_fd, 1, SEEK_CUR);
			}

			close(in_fd);
			break;


		case 'v':
			/* Open Archive File */
			archive = optarg;
			in_fd = open(archive, O_RDONLY);
			if(in_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}

			/* Validate AR Magic String */
			num_read = read(in_fd, read_buf, 8);
			if(strncmp(read_buf, ARMAG, 8) != 0){
				perror("Unknown archive format.");
				exit(-1);
			}

			while((num_read = read(in_fd, read_buf, HEADER_READ_SIZE)) == HEADER_READ_SIZE){
				/* Read header into struct */
				sscanf(read_buf, "%s%s%s%s%s%s%s", extractheader.ar_name, extractheader.ar_date, extractheader.ar_uid, extractheader.ar_gid, extractheader.ar_mode, extractheader.ar_size, extractheader.ar_fmag );

				/* Convert size string into decimal */
				filesize=strtoul(extractheader.ar_size, NULL, 10);

				/* Get and print permission string */
				permstring = file_perm_string(strtoul(extractheader.ar_mode, NULL, 8),0); 
				printf("%s ", permstring);

				/* Print uid/gid */
				printf("%s/%s ", extractheader.ar_uid,extractheader.ar_gid);

				/* Print size */
				printf("%6s ", extractheader.ar_size);

				/* Print date */
				temp = strtol(extractheader.ar_date, NULL, 10);
				timestruct = localtime(&temp);
				strftime(timestr, 100, "%b %d %R %Y ", timestruct);
				printf("%s", timestr);


				/* Print file names */
				i = 0;
				while(extractheader.ar_name[i] != '/'){
					printf("%c", extractheader.ar_name[i]);
					i++;
				}
				printf("\n");

				/* Seek ahead the amount of the filesize */
				lseek(in_fd, filesize, SEEK_CUR);

				/* Determine if padding was used */
				if(filesize % 2 != 0)
					lseek(in_fd, 1, SEEK_CUR);
			}

			close(in_fd);
			break;

		case 'A':
			/* Pre check */
			/* Open Archive File */
			archive = optarg;
			in_fd = open(archive, O_RDONLY);
			if(in_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}

			/* Validate AR Magic String */
			num_read = read(in_fd, read_buf, 8);
			if(strncmp(read_buf, ARMAG, 8) != 0){
				perror("Unknown archive format.");
				exit(-1);
			}

			close(in_fd);

			/* Open Archive File */
			archive = optarg;
			umask(0);
			out_fd = open(archive, O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
			
			if(out_fd < 0){
					perror("Unable to open archive file.");
					exit(-1);
			}

			lseek(out_fd, 0, SEEK_END);
			/* Open directory */
			if((dp = opendir(".")) == NULL){
				exit(-1);
			}

			/* Loop through directory */
			while((entry = readdir(dp)) != NULL){
				stat(entry->d_name, &stat_buf);
				if (S_ISDIR (stat_buf.st_mode))
					continue;
				if(strncmp(entry->d_name, ".", 1) == 0)
					continue;
				if(strncmp(entry->d_name, archive, strlen(archive)) == 0)
					continue;



				memset(name, 0, 16);
				/* Reformat name and add trailing slash */
				for(x=0; x <16; x++){
					if(entry->d_name[x] == '\0'){
						name[x] = '/';
						break;
					} else{
						name[x]=entry->d_name[x];
					}
				}

				/* Open file */
				in_fd = open(entry->d_name, O_RDONLY);
				if(in_fd < 0){
					perror("Unable to open one or more input files.");
					exit(-1);
				}

				/* Format and write file header */
				snprintf(header, HEADER_SIZE, "%-16s%-12ld%-6d%-6d%-8o%-10ld%-2s", name,stat_buf.st_mtime,stat_buf.st_uid,stat_buf.st_gid,stat_buf.st_mode,stat_buf.st_size,ARFMAG);
				num_written = write(out_fd, header, HEADER_SIZE);
				if(num_written == -1){
					perror("Error writing file header.");
					exit(-1);
				}

				/* Elimnates end line caused by snprintf */
				lseek(out_fd, -1, SEEK_CUR);

				/* Write File */
				while((num_read = read(in_fd, read_buf, BLOCKSIZE)) > 0) {
					num_written = write(out_fd, read_buf, num_read);

					if(num_read != num_written || num_written == -1){
						perror("Error writing files to archive.");
						exit(-1);
					}
				}

				if(stat_buf.st_size % 2 != 0){
				num_written = write(out_fd, "\n", 1);
				}				

				close(in_fd);
			}
			closedir(dp);
			close(out_fd);

			break;

		case '?':
			printf("Invalid command.");
			exit(-1);

		default:
			abort();
	}

	return 0;
}

char * /* Return ls(1)-style string for file permissions mask author: Kevin McGrath*/
file_perm_string(mode_t perm, int flags)
{
	static char str[STR_SIZE];
	snprintf(str, STR_SIZE, "%c%c%c%c%c%c%c%c%c",
	         (perm & S_IRUSR) ? 'r' : '-', (perm & S_IWUSR) ? 'w' : '-',
	         (perm & S_IXUSR) ?
	         (((perm & S_ISUID) && (flags & FP_SPECIAL)) ? 's' : 'x') :
	         (((perm & S_ISUID) && (flags & FP_SPECIAL)) ? 'S' : '-'),
	         (perm & S_IRGRP) ? 'r' : '-', (perm & S_IWGRP) ? 'w' : '-',
	         (perm & S_IXGRP) ?
	         (((perm & S_ISGID) && (flags & FP_SPECIAL)) ? 's' : 'x') :
	         (((perm & S_ISGID) && (flags & FP_SPECIAL)) ? 'S' : '-'),
	         (perm & S_IROTH) ? 'r' : '-', (perm & S_IWOTH) ? 'w' : '-',
	         (perm & S_IXOTH) ?
	         (((perm & S_ISVTX) && (flags & FP_SPECIAL)) ? 't' : 'x') :
	         (((perm & S_ISVTX) && (flags & FP_SPECIAL)) ? 'T' : '-'));
	return str;
}