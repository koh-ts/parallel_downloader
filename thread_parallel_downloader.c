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
#include <pthread.h>

#define BUF_LEN 1024
#define URL_LEN 256
#define MAX_FILES 10

struct arg {
	char* ch;
	int idx;
};

int f[MAX_FILES];
char c[MAX_FILES][BUF_LEN];
int cnt;

void progress(){ //display current progress
	static int pos = 0;
	static char mark[] = {'-','\\','|','/'};
    int k = 0;
	for(int i = 0; i < 1000000; ++i) {
        k = 0;
        clear();
        for(int j = 0; j < cnt; j++){
            mvprintw(3 * j, 0, "\r%s", c[j]);
            if(f[j] == 1) mvprintw(3 * j + 1, 0, "\r%c", mark[pos]);
            else{
                mvprintw(3 * j + 1, 0, "\r%s", "finishedddd!");
                k++;
            }
        }
		refresh();
		pos = (pos + 1) % sizeof(mark);
		usleep(100000);
        if(k == cnt) pthread_exit((void *) 0);
	}
}

int http_spliturl(char* url, char* host, char* path, char* filename, unsigned short* port){
    char *p, host_path[URL_LEN];
    if ( strlen(url) > URL_LEN-1 ) {
		endwin();
        perror("URL is too long\n");
		exit(EXIT_FAILURE);
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
		endwin();
        perror("URL has to be begun with http://\n");
        exit(EXIT_FAILURE);
    }
	return 0;
}

void child_task(void *tmp){
    pthread_detach(pthread_self());
	struct arg *info;
	info = tmp;
	char *url = info->ch;
	int id = info->idx;
	f[id] = 1;
	int fd, s, i = 0;
	long int diff = 0;
	char *p;
	char host[BUF_LEN], path[BUF_LEN], filename[BUF_LEN], host_path[BUF_LEN], send_buf[BUF_LEN];
	struct hostent *servhost;
	struct sockaddr_in server;
	unsigned short port = 80;
	int fuck;
	if(fuck = http_spliturl(url, host, path, filename, &port)){
		endwin();
		printf("fuck is %d\n", fuck);
		perror("failed to parse URL");
		exit(EXIT_FAILURE);
	}
	servhost = gethostbyname(host);
	if(servhost == NULL) {
		endwin();
		perror("failed to conversion to IP address.");
        exit(EXIT_FAILURE);
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	bcopy(servhost->h_addr, &server.sin_addr, servhost->h_length);
	server.sin_port = htons(port);
 
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		endwin();
		perror("failed to create a socket."); //check if socket is created
        exit(EXIT_FAILURE);
	}

	if ( connect(s, (struct sockaddr *)&server, sizeof(server)) == -1) {
		endwin();
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
		endwin();
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
				}
			} else write(fd, buf, read_size);
		} else break;
		i++;
	}
	close(fd);
	close(s);
    f[id] = 0;
	pthread_exit((void *) 0);
}


int main(int argc, char *argv[]){
    initscr();
    clear();
    if (argc != 2) {
		endwin();
		perror("Input error.\n");
        exit(EXIT_FAILURE);
	}

    FILE *fp;
	char buf[BUF_LEN];
 
	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		exit(EXIT_FAILURE);
	}
	cnt = 0;
	while (fgets(buf, BUF_LEN, fp) != NULL) {
    	cnt++;
  	}
	// cnt--;
	mvprintw(10, 0, "%d is cnt\n", cnt);
	refresh();
    for(int i = 0; i < cnt; i++){
        memset(&c[i], 0, sizeof(c[i]));
	}
    fseek(fp, 0L, SEEK_SET);
	for(int i = 0; i < cnt && fgets(c[i], sizeof(c[i]), fp) != NULL;i++){
        f[i] = 1;
    }
	fclose(fp);
    
    pthread_t thread[cnt], pro;
	struct arg tmp[cnt];
    pthread_create(&pro, NULL, (void *)progress, NULL);
	for(int i = 0; i < cnt; i++){
        tmp[i].ch = c[i];
		tmp[i].idx = i;
        sleep(1);
        if((pthread_create(&thread[i], NULL, (void *)child_task, (void *) &tmp[i]) != 0)){
			endwin();
			perror("pthread_create error");
			exit(EXIT_FAILURE);
		}
	}
	sleep(3);
	pthread_join(pro, NULL);
    endwin();
}