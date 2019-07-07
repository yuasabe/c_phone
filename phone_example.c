/*Ubuntuでのコンパイル方法
  sudo apt-get install libgtk-3-dev     //(GTKをインストール)
  gcc phone.c $(pkg-config --cflags --libs gtk+-3.0)
  ./a.out
*/

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <err.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <string.h>

//エラー処理用関数
void die(char* s)
{
  perror(s);
  exit(1);
}

GtkWidget *notebook;
GtkWidget *window;
gint pos = 2;
gboolean show_t = TRUE;
gboolean show_b = TRUE;

typedef struct _Entrybuf {
  char buf1[256];
  int buf2;
} Entrybuf;

//プログラムを終了する
static void cb_close_clicked(GtkWidget *closebutton, gpointer user_data)
{
  gtk_main_quit();
}

//ポート番号が10000~65535じゃないときにダイアログを表示する関数
static void show_dialog(void)
{
  GtkWidget* dialog;
  dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Port Number must be 10000~65535");
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

//Clientの関数
void *cb_client_call_clicked(void* _eb)
{
  const gchar *text1;
  const gchar *text2;
  Entrybuf eb = *(Entrybuf*)_eb;
  //sscanf(strtol(text2, NULL, 10), "%d", &buf2);
  char command[512];

  int n_rec, n_recv, w_send, w_play;
  int N = 16;
  unsigned char data_rec[N];
  unsigned char data[N];
  for(int i = 0; i < N; i++) {
    data_rec[i] = 0;
    data[i] = 0;
  }
  
  int s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) die("socket");
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  if (inet_aton(eb.buf1, &addr.sin_addr) == 0) exit(1);
  addr.sin_port = htons(eb.buf2);
  int ret = connect(s, (struct sockaddr*)&addr, sizeof(addr));
  if (ret == -1) die("socket");
  FILE* rec_p;
  if((rec_p = popen("rec -t raw -b 16 -c 1 -e s -r 44100 - ", "r")) == NULL) exit(1);

  FILE* play_p;

  play_p = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ","w");
  
  while(1) {
    n_recv = recv(s, data, N, 0);
    if (n_recv == -1) die("read");
    w_play = fwrite(data, 1, N, play_p);
    if (w_play == -1) die("fwrite");
    n_rec = fread(data_rec, 1, N, rec_p);
    if (n_rec == 0) break;
    w_send = send(s, data_rec, N, 0);
    if (w_send == -1) die("send");
  }
  /*pclose(play_p);
  close(s);
  pclose(pp);*/
}

//clientのCALLボタンが押されると起動する関数
static void cb_client_thread_call(GtkWidget *button, gpointer *entrydata)
{
  pthread_t thread_id;
  int status;
  char buf1[256];
  int buf2;
  const gchar *text1;
  const gchar *text2;
  text1 = gtk_entry_get_text(GTK_ENTRY((char*)entrydata[0]));
  //sprintf(buf1, "%s", text1);
  text2 = gtk_entry_get_text(GTK_ENTRY((char*)entrydata[1]));
  buf2 = (int)strtol(text2, NULL, 10);
  Entrybuf eb;
  sprintf(eb.buf1, "%s", text1);
  eb.buf2 = buf2;
  if (eb.buf2 < 10000 || eb.buf2 > 65535) {
    show_dialog();
    return;
  }
  status = pthread_create(&thread_id, NULL, &cb_client_call_clicked, &eb);
   if (status == -1) {
    perror("pthread_create");
  }
}

//Serverの関数
void *cb_server_call_clicked(void* _buf)
{
  const gchar *text;
  //sscanf(text, "%d", &buf);
  char command[512];
  int buf = *(int*)_buf;

  int n_rec, n_recv, w_send, w_play;
  int N = 16;
  unsigned char data_rec[N];
  unsigned char data[N];

  for (int i = 0; i < N; i++) {
    data_rec[i] = 0;
    data[i] = 0;
  }
  
  int ss = socket(PF_INET, SOCK_STREAM, 0);
  if (ss == -1) die("socket");
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(buf);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind(ss, (struct sockaddr*)&addr, sizeof(addr));

  listen(ss,10);

  struct sockaddr_in client_addr;
  socklen_t len = sizeof(struct sockaddr_in);
  int s = accept(ss, (struct sockaddr*)&client_addr, &len);
  close(ss);
  FILE* rec_p;
  if((rec_p = popen("rec -t raw -b 16 -c 1 -e s -r 44100 - ", "r")) == NULL) exit(1);

  FILE* play_p;
  play_p = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ","w");
  
  while(1) {
    n_rec = fread(data_rec, 1, N, rec_p);
    if (n_rec == 0) break;
    w_send = send(s, data_rec, N, 0);
    if (w_send == -1) die("send");
    n_recv = recv(s, data, N, 0);
    if (n_recv == -1) die("recv");
    w_play = fwrite(data, 1, N, play_p);
    if (w_play == -1) die("fwrite");
  }
  /*pclose(play_p);
  close(s);
  pclose(pp);*/
}

//ServerのCALLボタンが押されると動作する関数
static void cb_server_thread_call(GtkWidget *button, gpointer entrydata)
{
  pthread_t thread_id;
  int status, buf;
  const gchar* text;
  text = gtk_entry_get_text(GTK_ENTRY((char*)entrydata));
  buf = (int)strtol(text, NULL, 10);
  if (buf < 10000 || buf > 65535) {
    show_dialog();
    return;
  }
  /*謎　こうしないと動くかどうか運ゲーになる
    サーバだけじゃなくクライアントで使ってもAndroidと接続するとエラーになるから
    こいつ以外が原因だと思われる（？）*/
  for(int i = 0; i < 3; i++) {
    status = pthread_create(&thread_id, NULL, &cb_server_call_clicked, &buf);
    if (status == -1) {
      perror("pthread_create");
    }
  }
}
//IPアドレスの取得
const char* GetMyIpAddr(const char* device_name)
{
  int s = socket(AF_INET, SOCK_STREAM, 0);

  struct ifreq ifr;
  ifr.ifr_addr.sa_family = AF_INET;
  strcpy(ifr.ifr_name, device_name);
  ioctl(s, SIOCGIFADDR, &ifr);
  close(s);

  struct sockaddr_in addr;
  memcpy(&addr, &ifr.ifr_ifru.ifru_addr, sizeof(struct sockaddr_in));
  return inet_ntoa(addr.sin_addr);
}

//ほぼGUIの処理のみ　IPアドレスの値だけ何もやってないから直しておく
int main(int argc, char** argv)
{
  GtkWidget *entry_client[2];
  GtkWidget *entry_server;
  GtkWidget *label;
  GtkWidget *timelabel;
  GtkWidget *closebutton;
  GtkWidget *buttons;
  GtkWidget *buttonc;
  GtkWidget *grid;
  GtkWidget *vbox;
  GtkWidget *hbox;
  char bufferl[32];
  //const char* r = GetMyIpAddr("eth0");
  char r[256];
  snprintf(r, 256, "IP:%s", GetMyIpAddr("wlp3s0"));
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "i1i2i3Phone");
  //今回はウィジェットを多く使うので、指定サイズより大きくなる
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);

  grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(window), grid);

  notebook = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK (notebook), GTK_POS_TOP);
  gtk_grid_attach(GTK_GRID(grid), notebook, 0, 0, 1, 2);
  gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
  
  int i;
  //Pageの作成・追加
  for(i=0; i < 2; i++){
    if (i == 0) {
      sprintf(bufferl, "Client");
      vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

      label = gtk_label_new("IP Address:");
      gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
      entry_client[0] = gtk_entry_new();
      gtk_box_pack_start(GTK_BOX(hbox), entry_client[0], TRUE, TRUE, 0);

      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
      
      label = gtk_label_new("Port Number:");
      gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
      entry_client[1] = gtk_entry_new();
      gtk_box_pack_start(GTK_BOX(hbox), entry_client[1], TRUE, TRUE, 0);
      buttonc = gtk_button_new_with_label("CALL");
      gtk_box_pack_start(GTK_BOX(hbox), buttonc, TRUE, TRUE, 0);
      g_signal_connect(buttonc, "clicked", G_CALLBACK(cb_client_thread_call), entry_client);

      timelabel = gtk_label_new(r);
      gtk_box_pack_start(GTK_BOX(vbox), timelabel, TRUE, TRUE, 0);

      label = gtk_label_new(bufferl);
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    }
    else {
      sprintf(bufferl, "Server");
      vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

      label = gtk_label_new("Port Number:");
      gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);
      entry_server = gtk_entry_new();
      gtk_box_pack_start(GTK_BOX(hbox), entry_server, TRUE, TRUE, 5);
      buttons = gtk_button_new_with_label("CALL");
      gtk_box_pack_start(GTK_BOX(hbox), buttons, TRUE, TRUE, 5);
      g_signal_connect(buttons, "clicked", G_CALLBACK(cb_server_thread_call), entry_server);
      label = gtk_label_new(r);
      gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

      label = gtk_label_new(bufferl);
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    }
  }
  
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

  
  closebutton = gtk_button_new_with_label ("close");
  g_signal_connect(GTK_BUTTON(closebutton), "clicked", G_CALLBACK(cb_close_clicked), NULL);
  gtk_grid_attach(GTK_GRID(grid), closebutton, 0,5,1,1);

  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(window);

  gtk_main();

  return 0;
}
