//
// Created by albert on 04.06.18.
//

#ifndef SIK_2_GUI_H
#define SIK_2_GUI_H
#include <vector>
#include <netinet/in.h>

using namespace std;

void initialize_socket(in_port_t port);

void handle_gui_events();

void display();
void handle_client_connect(int msg_sock);
ssize_t handle_client_key(int msg_sock);
void negotiate(int);
#endif //SIK_2_GUI_H
