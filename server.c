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

#define MAX_CLIENTS 30
#define NUMTHREADS 20;

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
	int id;
	int sd[10];
	int sd_count;
	pthread_t thread;
	int is_active;
	int data_sd[10];
} group;

typedef struct {
	char *command;
	user *users;
	int sd;
} parse_command_params;

int online_users_count = 0;
int client_socket[MAX_CLIENTS];
pthread_t control_tid;
// int group_count = 0;
// int group_active[10] = {0,0,0,0,0,0,0,0,0,0};
group *groups;

// pthread_t call_threads[NUMTHREADS];
// int call_threads_count = 0;

// void *start_call(void *p) {

// }

void *call(void *p) {
	int group_index = *((int*)p);
	group g = groups[group_index];

	printf("call group thread for %d\n", group_index);

	// char *message = "hello world!";
	for (int i = 0; i < g.sd_count; i++) {
		printf("%d\n", g.sd[i]);
		struct sockaddr_in addr;
		socklen_t len_sd = sizeof(struct sockaddr_in);
		getpeername(g.sd[i], (struct sockaddr*)&addr, (socklen_t*)&len_sd);
		char ip_addr[20];
		strcpy(ip_addr, inet_ntoa(addr.sin_addr));
		printf("connecting to data socket on %s\n", ip_addr);

		int s = socket(PF_INET, SOCK_STREAM, 0);
		if(s==-1) { perror("socket"); exit(1); }

		addr.sin_family = AF_INET;
		int ip_ret = inet_aton(ip_addr, &addr.sin_addr);
		if(ip_ret==0){ perror("inet_aton"); close(s); exit(1); }
		addr.sin_port = htons(50001);
		int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
		if(ret!=0){ perror("connect"); close(s); exit(1); }

		//int n = send(s, message, sizeof(message), 0);
		//if (n < 0) { perror("send"); }

		g.data_sd[i] = s;
	}

	unsigned char data;
	int n;
	while(1) {
		for (int i = 0; i < g.sd_count; i++){
			n = recv(g.data_sd[i], &data, 1, 0);
			if (n < 1) {perror("recv"); break; }
			for (int j = 0; j < g.sd_count; j++) {
				if (i != j) {
					n = send(g.data_sd[j], &data, 1, 0);
					if (n < 1) {perror("send"); break; }
				}
			}
		}
		if (n < 1) { break; }
	}
}

void *parse_command(void *p) {
	parse_command_params *pcp = (parse_command_params*)p;

	if (strncmp(pcp->command, "3", 1) == 0) { // call request
		int j = 2;
		char call_to_sd_char[5];
		while(pcp->command[j] != '\0') {
			call_to_sd_char[j-2] = pcp->command[j];
			j++;
		}
		call_to_sd_char[j-2] = '\0';
		int call_users_index = atoi(call_to_sd_char);

		if (pcp->sd == pcp->users[call_users_index].sd) {
			char response[4] = "BAD";
			int n = send(pcp->sd, response, sizeof(response), MSG_NOSIGNAL);
			if (n < 1) { perror("send"); exit(1); }
			pthread_exit(NULL);
		}

		printf("Incoming call request from sd:%d to sd:%d\n", pcp->sd, pcp->users[call_users_index].sd);

		char response[3] = "OK";

		int n = send(pcp->sd, response, sizeof(response), MSG_NOSIGNAL);
		if (n < 1) { perror("send"); exit(1); }

		// 0. start new thread for this Group
		// 1. create new sockets with pcp->sd and pcp->users[call_users_index].sd
		// 2. create group and add these two users
		// 3. for each socket in group, read from socket and sent to every other
		// 4. 

		// update groups array
		for (int n = 0; n < 10; n++) {
			if (groups[n].is_active == 0) {
				groups[n].id = n;
				groups[n].sd[groups[n].sd_count] = pcp->sd;
				groups[n].sd_count++;
				groups[n].sd[groups[n].sd_count] = pcp->users[call_users_index].sd;
				groups[n].sd_count++;
				groups[n].is_active = 1;
				pthread_create(&(groups[n].thread), NULL, call, &n);
				printf("updated groups array and started group thread\n");
				break;
			} else {
				printf("groups full\n");
			}
		}
	}

	if (strncmp(pcp->command, "2", 1) == 0) {
		printf("Sending users list data\n");
		// user *ptr = pcp->users;
		char data[1024];
		int n = 0;
		for (int i = 0; i < online_users_count; i++) {
			printf("%s ", pcp->users[i].ip);
			printf("%d\n", pcp->users[i].port);
			sprintf(data, "%s%d %s %d\n", data, i, pcp->users[i].ip, pcp->users[i].port);
		}
		n = send(pcp->sd, data, sizeof(data), MSG_NOSIGNAL);
		if (n < 1) { perror("send"); exit(1); }
	}

	return 0;
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

user *update_online_users(user *online_users) {
	free(online_users);
	user *online_users_new = malloc(sizeof(user)*MAX_CLIENTS);
	struct sockaddr_in addr;
	socklen_t len_sd = sizeof(struct sockaddr_in);
	int i = 0;
	for (int j = 0; j < MAX_CLIENTS; j++) {
		if (client_socket[j] != 0 ) {
			getpeername(client_socket[j], (struct sockaddr*)&addr, (socklen_t*)&len_sd);
			printf("[%d] %s:%d\n", client_socket[j], inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			online_users[i].sd = client_socket[j];
			strcpy(online_users[i].ip, inet_ntoa(addr.sin_addr));
			online_users[i].port = ntohs(addr.sin_port);
			i++;
		}
	}
	return online_users_new;
}

void server_start() {
	printf("Starting server. \n");
	int port = 50000;
	int opt = 1;

	// online_users
	user *online_users = malloc(sizeof(user)*MAX_CLIENTS);

	// start thread for data transmission
	// pthread_t data_tid;
	// pthread_create(&data_tid, NULL, data_transmission, NULL);

	fd_set readfds;
	for (int i = 0; i < MAX_CLIENTS; i++) {
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
		for (int i = 0; i < MAX_CLIENTS; i++) {
			sd = client_socket[i];
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > max_sd)
				max_sd = sd;
		}

		int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		if ((activity < 0)) { perror("select");exit(1); }

		// new connection
		if (FD_ISSET(ss, &readfds)) {   
			struct sockaddr_in client_addr;
			socklen_t len = sizeof(struct sockaddr_in);

			int s = accept(ss, (struct sockaddr *) &client_addr, &len);
			if (s == -1) { perror("accept"); exit(1); }
			printf("Incoming connection: %d\n", s);
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if( client_socket[i] == 0 ) {
					client_socket[i] = s;
					printf("Adding to list of sockets as %d\n" , i);
					break;
				}
			}
			online_users_count++;
			online_users = update_online_users(online_users);
		} // end of new initial connection

		// process all clients
		int valread;
		char buffer[1024];
		for (int i = 0; i < MAX_CLIENTS; i++) {
			sd = client_socket[i];
			if(FD_ISSET(sd, &readfds)) {
				if ((valread = read(sd, buffer, 1024)) == 0) {
					// if client disconnected
					printf("Host disconnected, %d\n", sd);
					close(sd);
					client_socket[i] = 0;
					online_users_count--;
					online_users = update_online_users(online_users);
				} else {
					// constantly listen in for every client
					printf("Command from sd:%d : %s \n", sd, buffer);
					parse_command_params *p = malloc(sizeof(parse_command_params));
					p->command = buffer;
					p->users = online_users;
					p->sd = sd;
					pthread_create(&control_tid, NULL, parse_command, p);
					// parse_command(buffer, sd, online_users);
					// printf("Command: %s\n", text);
					// send command if user is being called.
					// char message[20] = "inbound_call___";
					// if (outbound_call_sd == sd) {
					// 	printf("send inbound call message to sd:%d\n", sd);
					// 	send(sd, message, strlen(message), 0);
					// }
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

int main(int argc, char **argv) {

	groups = malloc(sizeof(group)*10);

	server_start();

	return 0;

}
