#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <ncurses.h>

#define BUF_LEN 1024
#define URL_LEN 256

void sigchld_handler(int x){
    int chld;
    chld = waitpid(-1, NULL, WNOHANG);
    if (chld == -1)
		perror("wait error");
		exit(EXIT_FAILURE);
    signal(SIGCHLD, sigchld_handler);
}

int http_spliturl(char* url, char* host, char* path, char* filename, unsigned short* port){
    char *p, host_path[URL_LEN];
    if ( strlen(url) > URL_LEN-1 ) {
        fprintf(stderr,"URLが長すぎます.\n"); return 1;
    }
    if ( strstr(url, "http://") && sscanf(url, "http://%s", host_path) ) {
        if ( (p = strchr(host_path,'/')) != NULL ) {
            strcpy(path,p); *p = '\0'; strcpy(host,host_path);
        } else {
            strcpy(host,host_path);
        }

		p = strrchr(path, '/');
		if(p != NULL) {
			strcpy(filename, ++p);
		} else {
			strcpy(filename, path);
		}

        if ( (p = strchr(host,':')) != NULL ) {
            *port = atoi(p+1);
            *p = '\0';
        }
        if ( *port <= 0 ) *port = 80;
    } else {
        fprintf(stderr,"URLは [http://host/path] の形式で指定してください.\n");
        return 1;
    }
    return 0;
}

void child_task(char* url, int cnt){
	int fd, s, i = 0;
	long int diff = 0;
	char *p;
	char host[BUF_LEN], path[BUF_LEN], filename[BUF_LEN], host_path[BUF_LEN], send_buf[BUF_LEN];
	struct hostent *servhost;
	struct sockaddr_in server;
	unsigned short port = 80;

	if(http_spliturl(url, host, path, filename, &port)){
		perror("failed to parse URL");
		exit(EXIT_FAILURE);
	}

	// printf("\nhost = %s\n", host);
	// printf("path = %s\n", path);
	// printf("filename = %s\n", filename);

	servhost = gethostbyname(host);
	if(servhost == NULL) {
		perror("failed to conversion to IP address.");
        exit(EXIT_FAILURE);
	}
	// printf("IP Address %d.%d.%d.%d\n", (u_char)servhost->h_addr[0], (u_char)servhost->h_addr[1], (u_char)servhost->h_addr[2], (u_char)servhost->h_addr[3]);
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	bcopy(servhost->h_addr, &server.sin_addr, servhost->h_length);
	server.sin_port = htons(port);
 
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("failed to create a socket."); //check if socket is created
        exit(EXIT_FAILURE);
	}

	if ( connect(s, (struct sockaddr *)&server, sizeof(server)) == -1) {
		perror("failed to connect."); //check connection
        exit(EXIT_FAILURE);
	}

	sprintf(send_buf, "GET %s HTTP/1.0\r\n", path);
	write(s, send_buf, strlen(send_buf));

	sprintf(send_buf, "Host: %s:%d\r\n",  host, port);
	write(s, send_buf, strlen(send_buf));

	sprintf(send_buf, "\r\n");
	write(s, send_buf, strlen(send_buf));

	fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	if (fd < 0) {
		perror("open error\n");
		exit(EXIT_FAILURE);
	}

	p = NULL;
	while(1) {
		char buf[BUF_LEN];
		int read_size;
		read_size = read(s, buf, BUF_LEN);
		if (read_size > 0) {
			if( i == 0) {
				if(strstr(buf, "200") && strstr(buf, "OK")) {
					p = strstr(buf, "\r\n\r\n");
					if ( p != NULL) {
						diff = p - buf;
						p += 4;
						write(fd, p, read_size - diff - 4);
					}
				} else {
					break;
					printf("http error\n");
				}
			} else write(fd, buf, read_size);
		} else break;
		i++;
	}
	
	close(fd);
	close(s);
}

int main(int argc, char *argv[]){
	initscr();
	if (argc != 2) {
		perror("Input error.\n");
        exit(EXIT_FAILURE);
	}

	FILE *fp;
	int cnt = 0, pid;
	char buf[BUF_LEN];
 
	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		printf("%s file not open!\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, BUF_LEN, fp) != NULL) {
    	cnt++;
  	}
	char c[cnt][BUF_LEN];
	fseek(fp, 0L, SEEK_SET);
	for(int i = 0; i < cnt && fgets(c[i], sizeof(c[i]), fp) != NULL;i++){
		mvprintw(i, 0, "list[%d]=%s", i, c[i]);
		sleep(1);
		refresh();
	}
	fclose(fp);
	sleep(3);
	endwin();

	signal(SIGCHLD, sigchld_handler);
	for (int j = 0; j < cnt; ++j) {
		pid = fork();
		if (pid == 0) {
			child_task(c[j], cnt);
			_exit(0);
		}
  	}
	wait(NULL);
	exit(0);
	// endwin();
}