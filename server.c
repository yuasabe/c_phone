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
// #include "data_serialization.c"

typedef struct {
	int port;
	int s;
} server_params;

typedef struct {
	int sd;
	char ip[20];
	int port;
} user;

typedef struct {
	char *command;
	user *users;
	int sd;
} parse_command_params;

int online_users_count = 0;

void *parse_command(void *p) {
	parse_command_params *pcp = (parse_command_params*)p;
	// int i = 0;
	if (strncmp(pcp->command, "<call>", 6) == 0) {
		// while((text[i++] = command[6+i]) != '<');
		// text[i-1] = '\0';
		// strncpy(text, &command[6], 15);
		// return 0;
	}
	if (strncmp(pcp->command, "<list_users>", 12) == 0) {
		printf("Sending users list data\n");
		// user *ptr = pcp->users;
		char data[1024];
		int len = 0;
		int n = 0;
		for (int i = 0; i < online_users_count; i++) {
			printf("%s ", pcp->users[i].ip);
			printf("%d\n", pcp->users[i].port);
			// sprintf(data, "%s%d ", data, i);
			// strcat(data, pcp->users[i].ip);
			sprintf(data, "%s%d %s %d\n", data, i, pcp->users[i].ip, pcp->users[i].port);
			// n = send(pcp->sd, pcp->users[i].ip, sizeof(pcp->users[i].ip), MSG_NOSIGNAL);
			// if (n < 1) { perror("send"); exit(1); }
			// sprintf(data, "%s %d\n", data, pcp->users[i].port);
			// n = send(pcp->sd, data, sizeof(pcp->users[i].port), MSG_NOSIGNAL);
			// if (n < 1) { perror("send"); exit(1); }
			len += sizeof(pcp->users[i].ip);
			len += sizeof(pcp->users[i].port);
			len += 6;
		}
		n = send(pcp->sd, data, sizeof(data), MSG_NOSIGNAL);
		if (n < 1) { perror("send"); exit(1); }
		// data[len] = '\0';
		// int n = send(pcp->sd, data, len, MSG_NOSIGNAL);
		// printf("%d bytes send to user %d\n", n, pcp->sd);
		// if (n < 1) { perror("send"); exit(1); }
	}
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

void server_start_multi(void *p) {
	printf("Starting server. \n");
	server_params *sp = (server_params*)p;
	int port = sp->port;
	int client_socket[30], max_clients = 30, opt = 1;

	// online_users
	user *online_users = malloc(sizeof(user)*max_clients);

	fd_set readfds;
	for (int i = 0; i < max_clients; i++) {
		client_socket[i] = 0;
	}
	int ss = socket(PF_INET, SOCK_STREAM, 0);
	if (ss == -1) { perror("socket"); exit(1); }
	if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	int ret = bind(ss, (struct sockaddr *) &addr, sizeof(addr));
	if (ret != 0) { perror("bind"); exit(1); }
	ret = listen(ss, 10);
	if (ret != 0) { perror("listen"); exit(1); }

	int max_sd, sd;
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(ss, &readfds);
		max_sd = ss;
		for (int i = 0; i < max_clients; i++) {
			sd = client_socket[i];
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > max_sd)
				max_sd = sd;
		}

		int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		if ((activity < 0)) { perror("select");exit(1); }

		if (FD_ISSET(ss, &readfds)) {   
			struct sockaddr_in client_addr;
			socklen_t len = sizeof(struct sockaddr_in);

			int s = accept(ss, (struct sockaddr *) &client_addr, &len);
			if (s == -1) { perror("accept"); exit(1); }
			printf("Incoming connection: %d\n", s);
			for (int i = 0; i < max_clients; i++) {
				if( client_socket[i] == 0 ) {
					client_socket[i] = s;
					printf("Adding to list of sockets as %d\n" , i);
					break;
				}
			}
			online_users_count++;
			printf("Connected sd: \n");
			struct sockaddr_in addr;
			socklen_t len_sd = sizeof(struct sockaddr_in);
			for (int i = 0; i < max_clients; i++) {
				if (client_socket[i] != 0 ) {
					getpeername(client_socket[i], (struct sockaddr*)&addr, (socklen_t*)&len_sd);
					printf("[%d] %s:%d\n", client_socket[i], inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
					online_users[i].sd = client_socket[i];
					strcpy(online_users[i].ip, inet_ntoa(addr.sin_addr));
					// online_users[i].ip = inet_ntoa(addr.sin_addr);
					online_users[i].port = ntohs(addr.sin_port);
				}
			}
		} // end of new initial connection


		// process all clients
		int valread;
		char buffer[1024];
		for (int i = 0; i < max_clients; i++) {
			sd = client_socket[i];
			if(FD_ISSET(sd, &readfds)) {
				// if client disconnected
				if ((valread = read(sd, buffer, 1024)) == 0) {
					// getpeername(sd, (struct sockaddr*)&addr, (socklen_t*)&len);
					// printf("Host disconnected, %s:%d \n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
					printf("Host disconnected, %d\n", sd);
					close(sd);
					client_socket[i] = 0;
					printf("Connected sd: [");
					for (int i = 0; i < max_clients; i++) {
						if (client_socket[i] != 0 ) {
							printf("%d, ", client_socket[i]);
						}
					}
					printf("]\n");
					online_users_count--;
				} else {
					// on each client sd
					// pthread_t recv_play_tid;
					// pthread_create(&recv_play_tid, NULL, &recv_play, &sd);
					// pthread_t rec_send_tid;
					// pthread_create(&rec_send_tid, NULL, &rec_send, &sd);
					// pthread_join(rec_send_tid, NULL);
					printf("Command from sd:%d : %s \n", sd, buffer);
					// char text[50];
				
					parse_command_params *p = malloc(sizeof(parse_command_params));
					p->command = buffer;
					p->users = online_users;
					p->sd = sd;
					pthread_t client_command_tid;
					pthread_create(&client_command_tid, NULL, parse_command, p);
					// parse_command(buffer, sd, online_users);
					// printf("Command: %s\n", text);

					// send to all other sockets
					// for(int j = 0; j < max_clients; j++) {
					// 	if(j == i) continue;
					// 	sd_other = client_socket[j];
					// 	if(FD_ISSET(sd, &readfds)) {
					// 		send(sd_other, buffer, strlen(buffer), 0);
					// 	}
					// }
				}   
			}   
		}   
	}
}

// start server
void *server_start(void *p) {
	// pthread_detach(pthread_self());
	server_params *sp = (server_params*)p;
	int port = sp->port;

	int ss = socket(PF_INET, SOCK_STREAM, 0);
	if (ss == -1) { perror("socket"); exit(1); }
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	int ret = bind(ss, (struct sockaddr *) &addr, sizeof(addr));
	if (ret != 0) { perror("bind"); exit(1); }
	ret = listen(ss, 10);
	if (ret != 0) { perror("listen"); exit(1); }

	while (1) {
		printf("Listening on port %d\n", port);

		struct sockaddr_in client_addr;
		socklen_t len = sizeof(struct sockaddr_in);

		int s = accept(ss, (struct sockaddr *) &client_addr, &len);
		if (s == -1) { perror("accept"); exit(1); }
		printf("Incoming connection: %d\n", s);
		sp->s = s;

		pthread_t recv_play_tid;
		pthread_create(&recv_play_tid, NULL, &recv_play, &s);

		pthread_t rec_send_tid;
		pthread_create(&rec_send_tid, NULL, &rec_send, &s);

		pthread_join(rec_send_tid, NULL);
		close(s);
	}
}

int main(int argc, char **argv) {
	server_params *sp = malloc(sizeof(server_params));
	sp->port = 50000;
	server_start_multi(sp);

	return 0;

}
