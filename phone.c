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

void cb_client_call(GtkWidget *widget);

// typedef struct {
// 	int i;
// 	GtkWidget *entry_client[4];
// } params;

int call_handler_id, end_call_handler_id;
int s;
pthread_t recv_play_tid, rec_send_tid;

// receive data and play
void *recv_play() {
	int n;
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
void *rec_send() {
	int n;
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

void show_error(gpointer window, char *error_message) {
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error_message);
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void *cb_end_call_and_destroy_dialog(GtkWidget *dialog) {
	gtk_widget_destroy(dialog);
	close(s);
}

void incoming_call_dialog(GtkWindow *parent, gchar *message) {
	GtkWidget *dialog, *label, *content_area, *end_call_button;
	GtkDialogFlags flags;
	gint response;

	// Create the widgets
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Message", parent, flags, "END CALL", GTK_RESPONSE_NONE, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	label = gtk_label_new(message);
	gtk_container_add(GTK_CONTAINER(content_area), label);

	// end_call_button = gtk_button_new_with_label("CALL");
	// gtk_container_add(GTK_CONTAINER(content_area), end_call_button);
	// end_call_handler_id = g_signal_connect(end_call_button, "clicked", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Ensure that the dialog box is destroyed when the user response
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Add the label, and show everything we've added
	gtk_container_add(GTK_CONTAINER(content_area), label);
	gtk_widget_show_all(dialog);
	printf("incoming_call_dialog displayed\n");

	// response = gtk_dialog_run(GTK_DIALOG(dialog));
	// if (response == GTK_RESPONSE_YES) {
	// 	printf("YES was pressed\n");
	// } else if (response == GTK_RESPONSE_NO) {
	// 	printf("NO was pressed\n");
	// }

	// gtk_widget_destroy(dialog);
}

void outbound_call_dialog(GtkWindow *parent, gchar *message) {
	GtkWidget *dialog, *label, *content_area, *end_call_button;
	GtkDialogFlags flags;
	gint response;

	// Create the widgets
	flags = GTK_DIALOG_DESTROY_WITH_PARENT;
	dialog = gtk_dialog_new_with_buttons("Message", parent, flags, "END CALL", GTK_RESPONSE_NONE, NULL);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	label = gtk_label_new(message);
	gtk_container_add(GTK_CONTAINER(content_area), label);

	// end_call_button = gtk_button_new_with_label("CALL");
	// gtk_container_add(GTK_CONTAINER(content_area), end_call_button);
	// end_call_handler_id = g_signal_connect(end_call_button, "clicked", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Ensure that the dialog box is destroyed when the user response
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(cb_end_call_and_destroy_dialog), dialog);

	// Add the label, and show everything we've added
	gtk_container_add(GTK_CONTAINER(content_area), label);
	gtk_widget_show_all(dialog);
	printf("outbound_call_dialog displayed\n");

	// response = gtk_dialog_run(GTK_DIALOG(dialog));
	// if (response == GTK_RESPONSE_YES) {
	// 	printf("YES was pressed\n");
	// } else if (response == GTK_RESPONSE_NO) {
	// 	printf("NO was pressed\n");
	// }

	// gtk_widget_destroy(dialog);
}

// typedef struct {
// 	int port;
// 	int s;
// } server_params;

void *server_start(void *p) {
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

		struct sockaddr_in client_addr;
		socklen_t len = sizeof(struct sockaddr_in);

		s = accept(ss, (struct sockaddr *) &client_addr, &len);
		if (s == -1) { perror("accept"); exit(1); }
		printf("Incoming connection: %d\n", s);
		// sp->s = s;

		// gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),1);
		incoming_call_dialog(GTK_WINDOW(window), "Incoming Call!");

		pthread_create(&recv_play_tid, NULL, recv_play, NULL);
		pthread_create(&rec_send_tid, NULL, rec_send, NULL);

		pthread_join(rec_send_tid, NULL);
		
		// unsigned char content;

		// FILE *fp_rec;
		// fp_rec = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");
		// FILE *fp_play;
		// fp_play = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ","w");

		// int n = 0;
		// while (1) {
		// 	n = recv(s, &content, 1, MSG_NOSIGNAL);
	 //    	if (n == -1) { perror("recv"); break; };
	 //    	n = fwrite(&content, 1, 1, fp_play);
	 //    	if (n == -1) { perror("fwrite"); break; };
	 //    	n = fread(&content, 1, 1, fp_rec);
	 //    	if (n == 0) break;
		// 	n = send(s, &content, 1, MSG_NOSIGNAL);
		// 	if (n != 1) { perror("send"); break; }
		// }
		// // gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),0);
		// // printf("set notebook page to client.\n");
		// pclose(fp_rec);
		// pclose(fp_play);
		// close(s);
	}
}

// Client: send recording data
void *client_call() {
	printf("Call thread : %d\n", s);

	pthread_create(&recv_play_tid, NULL, recv_play, NULL);
	pthread_create(&rec_send_tid, NULL, rec_send, NULL);

	pthread_join(rec_send_tid, NULL);

	
	// FILE *fp_rec;
	// fp_rec = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");
	// FILE *fp_play;
	// fp_play = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ","w");

	// unsigned char content;
	// int n = 0;
	// while (1) {
	// 	n = recv(s, &content, 1, MSG_NOSIGNAL);
 //    	if (n == -1) {perror("recv");break; };
 //    	n = fwrite(&content, 1, 1, fp_play);
 //    	if (n == -1) {perror("fwrite");break; };
 //    	n = fread(&content, 1, 1, fp_rec);
 //    	if (n == 0) break;
	// 	n = send(s, &content, 1, MSG_NOSIGNAL);
	// 	if (n != 1) { perror("send"); break; }
	// }
	// pclose(fp_rec);
	// pclose(fp_play);
	// close(s);
}

// Client: Call button pressed
// Create socket and initiate client_call on new thread
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
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, &client_call, &s);

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
			if (strncmp(ifa->ifa_name, "e",1 ) == 0) {
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

int main(int argc, char **argv) {

	// Get Hostname and IP address
	char *ip_addr = malloc(sizeof(char)*20);
	char *host_name = malloc(sizeof(char)*50);
	get_my_ip_address(ip_addr, host_name);

	// Start server, listen on port 60000
	// int port = 60000;
	// server_params *sp = malloc(sizeof(server_params));
	// sp->port = port;
	pthread_t server_tid;
	pthread_create(&server_tid, NULL, server_start, NULL);

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
	// gtk_notebook_set_current_page(GTK_NOTEBOOK(dialog.notebook), 0);

	// gdk_threads_enter();
	// Call and answer calls from the same interface
	// GtkWidget **entry_client = malloc(sizeof(GtkWidget*)*4);
	char bufferl[32];
	for(int i = 0; i < 2; i++) {
		if (i == 0) {
			// Client page
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

			// label = gtk_label_new("Port Number:");
			// gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
			// entry_client[1] = gtk_entry_new();
			// gtk_entry_set_text(GTK_ENTRY(entry_client[1]), "50000");
			// gtk_box_pack_start(GTK_BOX(hbox), entry_client[1], TRUE, TRUE, 0);

			
			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 10);
			button1 = gtk_button_new_with_label("CALL");
			gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 10);

			// entry_client[3] = button1;
			// entry_client[2] = window;
			call_handler_id = g_signal_connect(button1, "clicked", G_CALLBACK(cb_client_call), NULL);
			// entry_client[4] = call_handler_id;

			// timelabel = gtk_label_new(r);
			// gtk_box_pack_start(GTK_BOX(vbox), timelabel, TRUE, TRUE, 0);

			

			label = gtk_label_new(bufferl);
			gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
		}
		// else {
		// 	// Server setup page
		// 	// 1. Can start listening in for calls.
		// 	// 2. 
		// 	sprintf(bufferl, "Server");
		//     vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		// 	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		// 	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

		// 	label = gtk_label_new("Incoming Call... ");
		// 	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		// 	// gtk_box_pack_start(GTK_BOX(hbox), entry_client[0], TRUE, TRUE, 0);

		// 	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		// 	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
		// 	label = gtk_label_new("Connected to call.");
		// 	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		// 	// gtk_box_pack_start(GTK_BOX(hbox), entry_client[1], TRUE, TRUE, 0);
		// 	button_server_end = gtk_button_new_with_label("END CALL");
		// 	gtk_box_pack_start(GTK_BOX(vbox), button_server_end, TRUE, TRUE, 0);
		// 	// entry_client[2] = window;
		// 	g_signal_connect(button_server_end, "clicked", G_CALLBACK(server_end_call), sp);

		// 	// timelabel = gtk_label_new(r);
		// 	// gtk_box_pack_start(GTK_BOX(vbox), timelabel, TRUE, TRUE, 0);

		// 	label = gtk_label_new(bufferl);
		// 	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
		// }
	    
  	}

	// entry and label
	// dialog.hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	// gtk_box_pack_start(GTK_BOX(dialog.vbox), dialog.hbox, TRUE, TRUE, 0);
	// dialog.entry = gtk_entry_new();
	//gtk_box_pack_start(GTK_BOX(dialog.hbox), dialog.entry, TRUE, TRUE, 0);
	// dialog.label = gtk_label_new("blank");
	//gtk_box_pack_start(GTK_BOX(dialog.hbox), dialog.label, TRUE, TRUE, 0);
	// gtk_grid_attach(GTK_GRID(dialog.grid), dialog.entry, 0, 0, 3, 2);
	// gtk_grid_attach(GTK_GRID(dialog.grid), dialog.label, 3, 0, 3, 2);

	// enter button
	// dialog.hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	// gtk_box_pack_start(GTK_BOX(dialog.vbox), dialog.hbox, TRUE, TRUE, 0);
	// dialog.button1 = gtk_button_new_with_label("Enter");
	// gtk_box_pack_start(GTK_BOX(dialog.hbox), dialog.button1, TRUE, TRUE, 0);
	// g_signal_connect(dialog.button1, "clicked", G_CALLBACK(cb_button_clicked1), &dialog);
	// gtk_grid_attach(GTK_GRID(dialog.grid), dialog.button1, 0, 2, 6, 1);

	// quit button
	// vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	// hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	// gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	// gtk_box_pack_start(GTK_BOX(vbox), button2, TRUE, TRUE, 0);
	button2 = gtk_button_new_with_label("Quit");
	// gtk_widget_set_hexpand(button2, TRUE);
	// gtk_widget_set_halign(button2, GTK_ALIGN_END);
	// gtk_box_pack_start(GTK_BOX(dialog.hbox), dialog.button2, TRUE, TRUE, 0);
	g_signal_connect(button2, "clicked", G_CALLBACK(cb_button_clicked2), NULL);
	gtk_grid_attach(GTK_GRID(grid), button2, 0, 3, 1, 2);

	// dialog exit
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(window);
	gtk_main();
	// gdk_threads_leave();

	return 0;

}
