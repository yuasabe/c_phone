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

typedef struct {
	int id;
	char data[1024];
} command;

/* Server-Client Common Command ID List
1. Client-side Incoming Call
2. Request online users list
3. Request Call to user: sd

*/

int control_sd = 0;
int data_sd = 0;
int incoming_call = 0;

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

int serialize_command(command *c, char *content) {
	if (c->data != NULL) {
		sprintf(content, "%d %s", c->id, c->data);
	} else {
		sprintf(content, "%d", c->id);
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

int show_online_users() {
	command *c = malloc(sizeof(command));
	c->id = 2;
	char content[50];
	serialize_command(c, content);

	int n = send(control_sd, content, sizeof(content), MSG_NOSIGNAL);
	if (n < 0) { perror("send"); return -1; }
	
	char data[2048];
	n = recv(control_sd, data, sizeof(data), MSG_NOSIGNAL);
	if (n < 0) { perror("recv"); return -1; }
	printf("%s\n", data);

	free(c);
	return 0;
}

int call(int call_user_sd) {
	command *c = malloc(sizeof(command));
	c->id = 3;
	c->data[0] = call_user_sd + '0';
	char content[30];
	serialize_command(c, content);

	int n = send(control_sd, content, sizeof(content), MSG_NOSIGNAL);
	if (n < 0) { perror("send"); return -1; }
	
	char response[10];
	n = recv(control_sd, response, sizeof(response), MSG_NOSIGNAL);
	if (n < 0) { perror("recv"); return -1; }
	printf("Response: %s\n", response);

	// start call thread
	//vpthread_t call_tid;
	//pthread_create(&call_tid, NULL, &call_data_transmission, NULL);

	return 0;
}

void user_input_loop() {
	// connect to server, initiate control socket

	int user_input, user_input_call;

	while(1) {
		printf("What would you like to do? \n[1] Call \n[2] Join Call\n[3] See online users\n[4] Exit\n");
		scanf("%d", &user_input);
		switch(user_input) {
			case 1:
			printf("Who would you like to call?\n");
			int n = show_online_users();
			if (n < 0) { printf("list_user command send error\n"); exit(1); }
			scanf("%d", &user_input_call);
			n = call(user_input_call);
			if (n < 0) { printf("call error\n"); exit(1); }
			break;
			case 2:
			printf("Join call selected\n");
			break;
			case 3:
			printf("Showing online users: \n");
			//n = show_online_users(s);
			// if (n < 0) {printf("list_user command send error\n");exit(1);}
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
	// close(s);
	printf("socket closed\n");
}

void *control() {
	// initiate connection to server, recv signals from socket
	char *server_ip = "192.168.10.15";

	int s = socket(PF_INET, SOCK_STREAM, 0);
	if(s==-1) { perror("socket"); exit(1); }

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	int ip_ret = inet_aton(server_ip, &addr.sin_addr);
	if(ip_ret==0){ perror("inet_aton"); close(s); exit(1); }
	addr.sin_port = htons(50000);
	int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	if(ret!=0){ perror("connect"); close(s); exit(1); }

	char data[15];

	control_sd = s;
	printf("control_sd: %d\n", control_sd);

	int n;

	while(1) {
		n = recv(control_sd, data, 15, MSG_NOSIGNAL);
		if (n != 1) { break; }
		char *message = "inbound_call__";
		if (strcmp(data, message) == 0) {
			incoming_call = 1;
		}
	}

	return 0;

}

void *data_transmission() {
	int port = 50001;
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
		// printf("Listening on port %d\n", port);

		struct sockaddr_in client_addr;
		socklen_t len = sizeof(struct sockaddr_in);

		int s = accept(ss, (struct sockaddr *) &client_addr, &len);
		if (s == -1) { perror("accept"); exit(1); }
		// printf("Incoming connection: %d\n", s);
		data_sd = s;
		unsigned char data;
		int n;
		while(1) {
			n = recv(data_sd, &data, 1, MSG_NOSIGNAL);
			if (n != 1) { break; }
		}
		// close(control_sd);
	}
}

int main(int argc, char **argv) {
	// client_params *cp = malloc(sizeof(client_params));

	pthread_t data_tid;
	pthread_create(&data_tid, NULL, data_transmission, NULL);

	pthread_t control_tid;
	pthread_create(&control_tid, NULL, control, NULL);

	user_input_loop();

	return 0;
}
