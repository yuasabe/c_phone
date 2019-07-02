#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

void *server_start(void *p) {
	// Start server, listen on port
	// pthread_detach(pthread_self());
	int port = (int)*((int*)p);
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
		printf("\n\nListening on port %d\n", port);

		struct sockaddr_in client_addr;
		socklen_t len = sizeof(struct sockaddr_in);

		int s = accept(ss, (struct sockaddr *) &client_addr, &len);
		printf("%d\n",s);
		if (s == -1) { perror("accept"); exit(1); }
		printf("Incoming connection: %d\n", s);
		unsigned char data;

		FILE *fp_rec;
		fp_rec = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");

		while (fread(&data, 1, 1, fp_rec) > 0) {
			int n = send(s, &data, 1, MSG_NOSIGNAL);
			// if (n == NULL) {perror("send"); }
			if (n != 1) { perror("send"); break; }
		}
		pclose(fp_rec);
		close(s);
	}
	printf("ending server thread...\n");
}

int main() {

	// Start server, listen on port 60000
	int port = 60000;
	pthread_t server_tid;
	pthread_create(&server_tid, NULL, &server_start, &port);

	printf("main finishing...\n");

	// pthread_join(server_tid, NULL);
	return 0;
}