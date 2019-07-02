#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
	// int port;
	int s;
	char *ip;
} client_params;

typedef struct {
	int sd;
	char *ip;
	int port;
} user;

int parse_command(char *command, char *text) {
	int i = 0;
	if (strncmp(command, "<call>", 6) == 0) {
		while((text[i++] = command[6+i]) != '<');
		text[i-1] = '\0';
		// strncpy(text, &command[6], 15);
		return 1;
	}
	return -1;
}

// receive data and play
void *recv_play(void *p) {
	int n;
	int s = *(int*)p;
	unsigned char content;
	FILE *fp_play;
	fp_play = popen("play -V0 -q -t raw -b 16 -c 1 -e s -r 44100 - ","w");
	while(1) {
		n = recv(s, &content, 1, MSG_NOSIGNAL);
		if (n == -1) { perror("recv"); break; };
		n = fwrite(&content, 1, 1, fp_play);
		if (n == -1) { perror("fwrite"); break; };
	}
	pclose(fp_play);
	return 0;
}

//  rec and send data
void *rec_send(void *p) {
	int n;
	int s = *(int*)p;
	unsigned char content;
	FILE *fp_rec;
	fp_rec = popen("rec -V0 -q -t raw -b 16 -c 1 -e s -r 44100 -", "r");
	while(1) {
		n = fread(&content, 1, 1, fp_rec);
    	if (n == 0) { break; }
		n = send(s, &content, 1, MSG_NOSIGNAL);
		if (n != 1) { perror("send"); break; }
	}
	pclose(fp_rec);
	return 0;
}

int show_online_users(int s) {
	char content[30] = "<list_users>";
	char data[1024];
	int n = send(s, content, sizeof(content), MSG_NOSIGNAL);
	if (n < 0) { perror("send"); return -1; }

	// while((n = recv(s, data, sizeof(data), MSG_NOSIGNAL)) != 0) {
	// 	if (data[0] == '\n') { break; }
	// 	printf("%s\n", data);
	// } 
	
	n = recv(s, data, sizeof(data), MSG_NOSIGNAL);
	if (n < 0) { perror("recv"); return -1; }
	printf("%s\n", data);

	return 0;
}

void client_start() {
	// connect to server, initiate control socket
	char *server_ip = "192.168.100.2";

	int s = socket(PF_INET, SOCK_STREAM, 0);
	if(s==-1) { perror("socket"); exit(1); }

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	int ip_ret = inet_aton(server_ip, &addr.sin_addr);
	if(ip_ret==0){ perror("inet_aton"); close(s); exit(1); }
	addr.sin_port = htons(50000);
	int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	if(ret!=0){ perror("connect"); close(s); exit(1); }

	int user_input;

	while(1) {
		printf("What would you like to do? \n[1] Call \n[2] Join Call\n[3] See online users\n[4] Exit\n");
		scanf("%d", &user_input);
		switch(user_input) {
			case 1:
			printf("Call selected\n");
			break;
			case 2:
			printf("Join call selected\n");
			break;
			case 3:
			printf("Showing online users: \n");
			int n = show_online_users(s);
			if (n < 0) {printf("list_user command send error\n");exit(1);}
			break;
			case 4:
			printf("Exiting... \n");
			break;
			default:
			printf("Incorrect input.\n");
		}
		if (user_input == 4) {
			break;
		}
	}

	// send Call command with ip_addr
	// printf("Calling %s\n", ip_addr);
	// int n;
	// char buff[30];
	// sprintf(buff, "<call>%s</call>", ip_addr);
	// n = send(s, buff, sizeof(buff), 0);
	// if (n < 0) { perror("send"); exit(1); }


	// pthread_t recv_play_tid;
	// pthread_create(&recv_play_tid, NULL, &recv_play, &s);

	// pthread_t rec_send_tid;
	// pthread_create(&rec_send_tid, NULL, &rec_send, &s);

	// pthread_join(rec_send_tid, NULL);
	close(s);
	printf("socket closed\n");
}

int main(int argc, char **argv) {
	// client_params *cp = malloc(sizeof(client_params));
	client_start();

	return 0;
}
