#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

struct params {
	int i;
};

void *fun(void *_p) {
	// sleep(1);
	printf("thread started\n");
	struct params *p = (struct params*)_p;
	printf("thread: %d\n", p->i);
	return NULL;
}

int main() {
	pthread_t thread_id;
	struct params p;
	p.i = 20;
	printf("first: %d\n", p.i);
	int status = pthread_create(&thread_id, NULL, fun, &p);
	if (status != 0) {
		perror("pthread_create");
	}
	pthread_join(thread_id, NULL);

	printf("status %d: end\n", status);
	return 0;
}