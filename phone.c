/*
gcc client_send.c -o client_send $(pkg-config --cflags --libs gtk+-3.0)
*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <pthread.h>
#include <time.h>

GtkWidget *window;
GtkWidget *button1;
GtkWidget *button2;
GtkWidget *label;
GtkWidget *grid;
GtkWidget *vbox;
GtkWidget *hbox;
GtkWidget *notebook;
GtkWidget *button_server_end;
GtkWidget *ip_addr_entry;
GtkWidget *dialog;

void cb_client_call(GtkWidget *widget);

int call_handler_id, end_call_handler_id;
int s;
int show_incoming_dialog = 0;
int was_connected = 0;
int socket_OK = 0;
time_t seconds = 0;
pthread_t recv_play_tid, rec_send_tid, client_call_tid, server_tid;
pthread_mutex_t mutex ;

// void sigpipe_handler() {
// 	printf("SIGPIPE caught\n");
// 	socket_OK = 0;
// }

// void call_ended() {
// 	gtk_widget_destroy(dialog);
// 	pthread_cancel(rec_send_tid);
// 	pthread_cancel(recv_play_tid);
// 	close(s);
// }

// receive data and play
void *recv_play() {
	int n;
	unsigned char content;
	FILE *fp_play;
	fp_play = popen("play -V0 -q -t raw -b 16 -c 1 -e s -r 44100 - ","w");
	while(1) {
		n = recv(s, &content, 1, 0);
		if (n == -1) { perror("recv"); break; };
		seconds = time(NULL);
		// if (n == 0) { printf("Call disconnected\n"); call_ended(); }
		n = fwrite(&content, 1, 1, fp_play);
		if (n == -1) { perror("fwrite"); break; };
	}
	pclose(fp_play);
	return 0;
}
//  rec and send data
void *rec_send() {
	int n;
	unsigned char content;
	FILE *fp_rec;
	fp_rec = popen("rec -V0 -q -t raw -b 16 -c 1 -e s -r 44100 -", "r");
	while(1) {
		n = fread(&content, 1, 1, fp_rec);
    	if (n == 0) { break; }
		n = send(s, &content, 1, 0);
		if (n != 1) { perror("send"); break; }
	}
	pclose(fp_rec);
	return 0;
}

void show_error(gpointer window, char *error_message) {
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error_message);
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

gboolean cb_end_call_and_destroy_dialog() {
	printf("cb_end_call_and_destroy_dialog started\n");
	gtk_widget_destroy(dialog);
	pthread_cancel(rec_send_tid);
	pthread_cancel(recv_play_tid);
	close(s);
	s = -1;
	printf("s: %d\n", s);
	return G_SOURCE_REMOVE;
}

gboolean incoming_call_dialog() {
	GtkWidget *label, *content_area;
	GtkDialogFlags flags;
	gint response;

	seconds = time(NULL);

	// Create the widgets
	flags = GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL;
	dialog = gtk_dialog_new_with_buttons("Message", GTK_WINDOW(window), flags, "END CALL", 1, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	label = gtk_label_new("Incoming Call!!");

	// Ensure that the dialog box is destroyed when the user response
	// g_signal_connect_swapped(dialog, "response", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Add the label, and show everything we've added
	gtk_container_add(GTK_CONTAINER(content_area), label);
	gtk_widget_show_all(dialog);

	was_connected = 1;
	
	printf("incoming_call_dialog displayed\n");

	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == 1) {
		printf("Close dialog\n");
		gdk_threads_add_idle(cb_end_call_and_destroy_dialog, NULL);
	}
	
	return G_SOURCE_REMOVE; // spend like three hours on this line
}

void outbound_call_dialog(GtkWindow *parent, gchar *message) {
	GtkWidget *label, *content_area, *end_call_button;
	GtkDialogFlags flags;
	gint response;

	seconds = time(NULL);

	// Create the widgets
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Message", parent, flags, "END CALL", 1, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	label = gtk_label_new(message);

	// Ensure that the dialog box is destroyed when the user response
	// g_signal_connect_swapped(dialog, "response", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Add the label, and show everything we've added
	gtk_container_add(GTK_CONTAINER(content_area), label);
	gtk_widget_show_all(dialog);

	was_connected = 1;

	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == 1) {
		printf("Close dialog\n");
		gtk_widget_destroy(dialog);
		pthread_cancel(rec_send_tid);
		pthread_cancel(recv_play_tid);
		int n = close(s);
		if (n != 0) { perror("close"); }
		s = -1;
		printf("s: %d\n", s);
	}
}

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

	while (1) {
		printf("\n\nListening on port %d\n", port);

		// s = -1;
		struct sockaddr_in client_addr;
		socklen_t len = sizeof(struct sockaddr_in);

		s = accept(ss, (struct sockaddr *) &client_addr, &len);
		if (s == -1) { perror("accept"); exit(1); }
		printf("Incoming connection: %d\n", s);

		gdk_threads_add_idle(incoming_call_dialog, NULL);

		pthread_create(&recv_play_tid, NULL, recv_play, NULL);
		pthread_create(&rec_send_tid, NULL, rec_send, NULL);
	}
}

// Client: Call button pressed
void cb_client_call(GtkWidget *widget) {
	const gchar *text1;
	char ip_addr[20];
	int port;

	text1 = gtk_entry_get_text(GTK_ENTRY(ip_addr_entry));
	sprintf(ip_addr, "%s", text1);
	port = 60000;
	printf("Connecting to %s:%d\n", text1, port);

	s = socket(PF_INET, SOCK_STREAM, 0);
	if(s==-1){perror("socket");exit(1);}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	int ip_ret = inet_aton(ip_addr, &addr.sin_addr);
	if(ip_ret==0){
		perror("inet_aton");
		char *error_message = "IPアドレスが正しくありません。";
		show_error(window, error_message);
		close(s);
		return;
	}
	addr.sin_port = htons(port);
	int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	if(ret!=0){
		perror("connect");
		char *error_message = "指定されたサーバへ接続できませんでした。";
		show_error(window, error_message);
		close(s);
		return;
	}
	
	printf("socket : %d\n", s);

	pthread_create(&recv_play_tid, NULL, recv_play, NULL);
	pthread_create(&rec_send_tid, NULL, rec_send, NULL);

	outbound_call_dialog(GTK_WINDOW(window), "Calling!");
}


// Quit button pressed
static void cb_button_clicked2(GtkWidget *button2, gpointer data) {
	gtk_main_quit();
}

void get_my_ip_address(char *ip_addr, char *host_name) {
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		family = ifa->ifa_addr->sa_family;

		if (family == AF_INET) {
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0) {
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				exit(EXIT_FAILURE);
			}
			if (strncmp(ifa->ifa_name, "e",1 ) == 0 || strncmp(ifa->ifa_name, "w",1 ) == 0) {
				strcpy(ip_addr, host);
			}
			printf("<Interface>: %s \t <Address> %s\n", ifa->ifa_name, host);
		}
	}
	char hostbuffer[256];
	// char *IPbuffer;
	struct hostent *host_entry;

	int i = gethostname(hostbuffer, sizeof(hostbuffer));
	if (i==-1) {perror("gethostname"); exit(1); }
	host_entry = gethostbyname(hostbuffer);
	if (host_entry==NULL) {perror("gethostbyname"); exit(1); }
	printf("Hostname: %s\n", hostbuffer);
	// IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
	// if (IPbuffer==NULL) {perror("inet_ntoa"); exit(1); }
	// strcpy(ip_addr, IPbuffer);
	strcpy(host_name, hostbuffer);
}

gboolean check_if_call_ended() {
	// int error = 0;
	// socklen_t len = sizeof (error);
	// int retval = getsockopt (s, SOL_SOCKET, SO_ERROR, &error, &len);
	// char data = 0x1;
	// int n = fcntl(s, &data, 1, 0);
	
	
	time_t now;
	now = time(NULL);
	printf("seconds= %ld, was_connected = %d\n", now - seconds, was_connected);
	if (now - seconds > 2 && was_connected == 1) {
		printf("Socket closed\n");
		gtk_widget_destroy(dialog);
		pthread_cancel(rec_send_tid);
		pthread_cancel(recv_play_tid);
		was_connected = 0;
	}
	return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv) {

	// Get Hostname and IP address
	char *ip_addr = malloc(sizeof(char)*20);
	char *host_name = malloc(sizeof(char)*50);
	get_my_ip_address(ip_addr, host_name);

	// pthread_mutex_init(&mutex, NULL);

	// Start server, listen on port 60000
	pthread_create(&server_tid, NULL, server_start, NULL);
	// server_start();

	// signal(SIGPIPE, sigpipe_handler);

	seconds = time(NULL);
	g_timeout_add_seconds(1, check_if_call_ended, NULL);

	gtk_init(&argc, &argv);

	// window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),300,200);
	gtk_window_set_title(GTK_WINDOW(window), "Phone");
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(window), grid);

	// notebook
	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_grid_attach(GTK_GRID(grid), notebook, 0, 0, 1, 2);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);

	char bufferl[32];
	for(int i = 0; i < 2; i++) {
		if (i == 0) {
		    sprintf(bufferl, "Client");
		    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 10);

			label = gtk_label_new("IP Address:");
			gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 10);
			ip_addr_entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(ip_addr_entry), "192.168.10.13");
			gtk_box_pack_start(GTK_BOX(hbox), ip_addr_entry, TRUE, TRUE, 10);

			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
			label = gtk_label_new("My IP Address: ");
			gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
			label = gtk_label_new(ip_addr);
			gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
			label = gtk_label_new("My Hostname: ");
			gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
			label = gtk_label_new(host_name);
			gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
			
			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 10);
			button1 = gtk_button_new_with_label("CALL");
			gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 10);

			call_handler_id = g_signal_connect(button1, "clicked", G_CALLBACK(cb_client_call), NULL);
			label = gtk_label_new(bufferl);
			gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
		}	    
  	}

	// quit button
	button2 = gtk_button_new_with_label("Quit");
	g_signal_connect(button2, "clicked", G_CALLBACK(cb_button_clicked2), NULL);
	gtk_grid_attach(GTK_GRID(grid), button2, 0, 3, 1, 2);

	// dialog exit
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(window);
	gtk_main();

	return 0;

}
