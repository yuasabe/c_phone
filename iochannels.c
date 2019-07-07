#include <stdio.h>
#include <gtk/gtk.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>


pthread_t server_tid;

GtkBox* makeWindow() {
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), box);

	GtkWidget *quit_button = gtk_button_new_with_label("Quit");
	g_signal_connect(quit_button, "clicked", G_CALLBACK(gtk_main_quit), NULL);
	gtk_box_pack_start(GTK_BOX(box), quit_button, FALSE, FALSE, 0);
	
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window);

	return GTK_BOX(box);
}

void addContent(GtkBox *parent, const char* string) {
	GtkWidget *label = gtk_label_new(string);
	gtk_box_pack_start(parent, label, FALSE, FALSE, 0);
	gtk_widget_show(label);
}

FILE *fp_play;
int i = 0;

void playSound(char data) {
	printf("playing sound: %d\n", i);
	int n = fwrite(&data, 1, 1, fp_play);
	if (n == -1) { perror("fwrite"); };
	i++;
}

gboolean onReadable(GIOChannel *source, GIOCondition condition, gpointer data) {
	gchar buf; 
	gsize bytes_read;
	GError *error = NULL;

	if (g_io_channel_read_chars(source, &buf, 1, &bytes_read, &error) == G_IO_STATUS_NORMAL) {
		// fprintf(stderr, "read failed. %s\n", error->message);
		// return FALSE;
		printf("playing sound: %d\n", i);
		int n = fwrite(&buf, 1, 1, fp_play);
		if (n == -1) { perror("fwrite");};
		i++;
	}

	// addContent((GtkBox *)data, buf);
	// playSound(buf);

	

	// g_free(buf);

	g_io_add_watch(source, G_IO_IN, onReadable, data);  // ここで設定し直さないと一度しか呼んでくれないっぽい。

	return FALSE;
}

int s;
GIOChannel *channel;
GtkBox *box;

void *server_start() {
	int port = 60000;

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

	printf("\n\nListening on port %d\n", port);

	struct sockaddr_in client_addr;
	socklen_t len = sizeof(struct sockaddr_in);

	s = accept(ss, (struct sockaddr *) &client_addr, &len);
	if (s == -1) { perror("accept"); exit(1); }
	printf("Incoming connection: %d\n", s);

	fp_play = popen("play -V0 -q -t raw -b 16 -c 1 -e s -r 44100 - ","w");

	channel = g_io_channel_unix_new(s);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_add_watch(channel, G_IO_IN, onReadable, box);

	pthread_cancel(server_tid);
}

void main() {
	gtk_init(NULL, NULL);

	box = makeWindow();

	// server_start(box);
	pthread_create(&server_tid, NULL, server_start, NULL);

	// channel = g_io_channel_unix_new(0);  // fdが0。つまりstdin。
	// g_io_add_watch(channel, G_IO_IN, onReadable, box);

	gtk_main();
}