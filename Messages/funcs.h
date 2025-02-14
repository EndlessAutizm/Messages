//
// Created by Ruben on 11/18/24.
//

#ifndef FUNCS_H
#define FUNCS_H


//bibliotecas
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>


//EXISTENCIAS MAXIMAS
#define MAX_USERS 10
#define MAX_TOPICS 20
#define MAX_TOPIC_PERSIS 5
#define MAX_TOTAL_PERSIS 100


//CARACTERES MAXIMOS
#define MAX_NOME_TOPICO 20
#define MAX_MSG 300
#define MAX_NAME_LENGTH 10
#define MAX_ARG MAX_NAME_LENGTH + MAX_MSG + MAX_NOME_TOPICO + 5

#define MSG_FICH getenv("MSG_FICH")

typedef struct {
    char comando[MAX_MSG];
    char arg1[MAX_MSG];
    char arg2[MAX_MSG];
    char arg3[MAX_MSG];
} prompt;

typedef struct {
    char remetente[MAX_NAME_LENGTH];
    char topico[MAX_NOME_TOPICO];
    int duracao;
    char mensagem[MAX_MSG];
} mensagem;


typedef struct {
    char username[MAX_NAME_LENGTH];
    char fifo[300];
    char topics[MAX_TOPICS][MAX_NOME_TOPICO];
    int topics_count;
} users;

typedef struct {
    char topic[MAX_NOME_TOPICO];
    int nPersistentes;
    int locked;
} topic;


// const char* Manager_Ligado = "/home/Ruben/Desktop/TPSO/fifos/manager_ligado";


//FUNCOES MANAGER
void *admin_thread();

void *feed_monitor_thread(void *arg);

void *decrease_msg_duration_thread();

void ResendMessage(mensagem msg, bool modopersistentes, const char *Vremetente);

void handle_fifo_comum_manager(const char *message);

void close_notify_feeds();

void ManageTopic(char *topic_nome, int locked_status, int nPersistentes);

int has_persistent_messages(const char *topic);

//Comandos Manager
void handle_users();

void handle_remove(prompt *cmd);

void handle_manager_topics();

void handle_show(const prompt *cmd);

void handle_lock(prompt *cmd);

void handle_unlock(prompt *cmd);

void help_manager();


//FUNCOES FEED

void *input_thread(void *arg);

void *receive_thread(void *arg);

void notify_manager(const char *username, int status);

void handle_fifo_feed(char *message);


//Comandos Feed
void handle_feed_topics();

void handle_msg(prompt *cmd, const char *nome_jogador);

void handle_subscribe(prompt *cmd, const char *nome_jogador);

void handle_unsubscribe(prompt *cmd, const char *nome_jogador);

void help_feed();


// FICHEIROS
void salvarMensagens();

void carregarMensagens();


#endif //FUNCS_H
