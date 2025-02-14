//
// FICHEIRO FEED.C
//


#include "funcs.h"


int running = 1;
const char *nome_jogador;

char subedtopic[MAX_TOPICS][MAX_NOME_TOPICO]; //topicos subs pelo cliente
int numsubTopics = 0;

topic topics[MAX_TOPICS];
int numTopics = 0;

const char *Manager_Ligado = "./fifos/manager_ligado";
const char *feed_fifo_base = "./fifos/fifo_";



int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nome_jogador>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    nome_jogador = argv[1];

    if (strlen(nome_jogador) >= MAX_NAME_LENGTH) {
        fprintf(stderr, "Erro: O nome do jogador deve ter menos de %d caracteres.\n", MAX_NAME_LENGTH);
        exit(EXIT_FAILURE);
    }

    if (access(Manager_Ligado, F_OK) != 0) {
        fprintf(stderr, "O Programa Manager não está em execucao\n");
        exit(1);
    }

    if (access(Manager_Ligado, F_OK) != 0) {
        fprintf(stderr, "O Programa Manager não está em execucao\n");
        exit(1);
    }


    char caminho_feed_fifo[200];

    snprintf(caminho_feed_fifo, 200, "%s%s", feed_fifo_base, nome_jogador);
    //printf("Caminho do FIFO para o jogador %s: %s\n", nome_jogador, caminho_feed_fifo);

    if (access(caminho_feed_fifo, F_OK) == 0) {
        fprintf(stderr, "Ja existe um cliente com esse nome!\n");
        exit(1);
    }

    if (mkfifo(caminho_feed_fifo, 0666) == -1) {
        perror("mkfifo");
    }

    int fifo_feed = open(caminho_feed_fifo, O_RDWR);
    if (fifo_feed == -1) {
        perror("nao deu para abrir");
        exit(1);
    }

    notify_manager(nome_jogador, 1);


    // Criar threads
    pthread_t input_tid = 0, receive_tid;

    if (pthread_create(&receive_tid, NULL, receive_thread, &fifo_feed) != 0) {
        perror("Failed to create receive thread");
        pthread_cancel(input_tid);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&input_tid, NULL, input_thread, (void *) nome_jogador) != 0) {
        perror("Failed to create input thread");
        pthread_cancel(receive_tid);
        exit(EXIT_FAILURE);
    }


    // Aguardar threads
    pthread_join(input_tid, NULL);
    pthread_cancel(receive_tid);


    close(fifo_feed);
    unlink(caminho_feed_fifo);

    printf("============= Fim do programa Feed =============\n");
    return 0;
}

// Thread responsável por capturar entrada do utilizador
void *input_thread(void *arg) {
    printf("\tBem vindo %s!. Digita 'help' para lista de comandos.\n", nome_jogador);

    const char *nome_jogador = (const char *) arg;
    char input[MAX_NAME_LENGTH + MAX_NOME_TOPICO + MAX_MSG];
    prompt cmd;

    while (running) {
        printf("> "); // Mostrar o prompt

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
        }

        // Remover o caractere de nova linha
        input[strcspn(input, "\n")] = '\0';

        // Separar o comando e os argumentos
        memset(&cmd, 0, sizeof(prompt));
        int num_args = sscanf(input, "%s %s %s %[^\n]", cmd.comando, cmd.arg1, cmd.arg2, cmd.arg3);

        if (num_args > 0) {
            if (strcmp(cmd.comando, "exit") == 0) {
                notify_manager(nome_jogador, 0);
                running = 0;
                break;
            } else if (strcmp(cmd.comando, "msg") == 0) {
                //msg <topico> <duração> <mensagem>
                if (num_args == 4) {
                    handle_msg(&cmd, nome_jogador);
                } else {
                    printf("Usage: msg <topic> <duration> <message>\n");
                }
            } else if (strcmp(cmd.comando, "subscribe") == 0) {
                //subscribe
                if (num_args == 2) {
                    handle_subscribe(&cmd, nome_jogador);
                } else {
                    printf("Usage: subscribe <topic>\n");
                }
            } else if (strcmp(cmd.comando, "unsubscribe") == 0) {
                //unsubscribe
                if (num_args == 2) {
                    handle_unsubscribe(&cmd, nome_jogador);
                } else {
                    printf("Usage: unsubscribe <topic>\n");
                }
            } else if (strcmp(cmd.comando, "topics") == 0) {
                handle_feed_topics(&cmd);
            } else if (strcmp(cmd.comando, "help") == 0) {
                help_feed();
            } else {
                printf("Unknown command: %s\n", cmd.comando);
            }
        }
    }

    return NULL;
}


// Thread responsável por receber mensagens
void *receive_thread(void *arg) {
    int fifofeed = *((int *) arg);
    char message[MAX_MSG];

    while (running) {
        // Ler a mensagem do FIFO
        ssize_t bytes_read = read(fifofeed, message, sizeof(message));
        if (bytes_read > 0) {
            message[bytes_read] = '\0'; // Garantir que é uma string terminada em '\0'
            // printf("Recebi algo: %s\n", message);
            handle_fifo_feed(message);
        } else if (bytes_read == -1) {
            perror("Error reading from manager FIFO");
        }

        usleep(250000);
    }


    return NULL;
}


void handle_fifo_feed(char *message) {
    if (strcmp(message, "close") == 0) {
        // manager deu close
        printf("\nO manager encerrou. Clique Enter para sair\n");
        running = 0;
    } else if (strncmp(message, "remove ", 7) == 0) {
        // manager deu remove
        const char *nome = message + 7;

        if (strcmp(nome, nome_jogador) == 0) {
            printf("\nO manager removeu este usuário. Clique Enter para sair\n");
            running = 0;
        }
    } else if (strncmp(message, "maxusers", 8) == 0) {
        // limite de users

        printf("\nLimite de usuarios atingido. Clique Enter para sair\n");
        running = 0;
    } else if (strncmp(message, "knowtopics ", 11) == 0) {
        // Remove o prefixo "knowtopics" da mensagem

        char *topics_list = message + 11;
        int nrecebidos;

        // Extrai o número de tópicos
        sscanf(topics_list, "%d", &nrecebidos);
        topics_list = strchr(topics_list, ' ') + 1; // Avança para os tópicos

        for (int i = 0; i < nrecebidos; i++) {
            char topic_name[MAX_NOME_TOPICO];
            int nPersistentes, locked;

            // Extrai as informações de cada tópico
            if (sscanf(topics_list, "%s %d %d", topic_name, &nPersistentes, &locked) == 3) {
                if (numTopics < MAX_TOPICS) {
                    strcpy(topics[numTopics].topic, topic_name);
                    topics[numTopics].nPersistentes = nPersistentes;
                    topics[numTopics].locked = locked;

                    numTopics++;
                } else {
                    printf("Aviso: Limite de tópicos alcançado. Não foi possível armazenar o tópico [%s]'.\n",
                           topic_name);
                }

                // Avança para o próximo tópico
                topics_list = strchr(topics_list, ' ') + 1; // Salta o nome do tópico
                topics_list = strchr(topics_list, ' ') + 1; // Salta nPersistentes
                topics_list = strchr(topics_list, ' ') + 1; // Salta locked
            } else {
                printf("Erro: Formato inválido na mensagem recebida.\n");
                break;
            }
        }
    } else if (strncmp(message, "notify remove ", 14) == 0) {
        //remoção de outro usuário
        const char *nome_removido = message + 14;

        printf("Notificação: O usuário '%s' foi removido da plataforma.\n", nome_removido);
    } else if (strncmp(message, "novo topic ", 11) == 0) {
        //potencial novo topico
        const char *nome_topico = message + 11;
        int sabe = 0;
        int esta_sub = 0;

        // Verifica se o cliente já está subscrito ao tópico
        for (int i = 0; i < numsubTopics; i++) {
            if (strcmp(nome_topico, subedtopic[i]) == 0) {
                esta_sub = 1;
                break;
            }
        }
        // Verifica se o cliente sabe do tópico
        for (int i = 0; i < numTopics; i++) {
            if (strcmp(nome_topico, topics[i].topic) == 0) {
                sabe = 1;
                break;
            }
        }

        if (sabe == 0) {
            // Copia o novo tópico
            strcpy(topics[numTopics].topic, nome_topico);
            topics[numTopics].topic[strlen(nome_topico) + 1] = '\0'; // Garante terminação
            numTopics++;
        }
        if (esta_sub) {
            printf("Um utilizador juntou se a [%s]\n", nome_topico);
        }
    } else if (strncmp(message, "removetopic ", 12) == 0) {
        // Tópico removido do manager
        const char *nome_topico = message + 12;

        for (int i = 0; i < numsubTopics; i++) {
            // Remove o tópico dos subscritos
            if (strcmp(nome_topico, subedtopic[i]) == 0) {
                for (int j = i; j < numsubTopics; j++) {
                    strncpy(subedtopic[j], subedtopic[j + 1], MAX_NOME_TOPICO);
                }
                numsubTopics--;
                break;
            }
        }

        for (int i = 0; i < numTopics; i++) {
            // Remove o tópico dos conhecidos
            if (strcmp(nome_topico, topics[i].topic) == 0) {

                for (int j = i; j < numTopics; j++) {

                    topics[j] = topics[j + 1];

                }
                numTopics--;

                printf("O topico [%s] deixou de existir.\n", nome_topico);
            }
        }
    } else if (strncmp(message, "updatelocked ", 13) == 0) {
        char topic_name[MAX_NOME_TOPICO];
        int locked_status, nPersistentes;

        // Extrai o nome do tópico e o estado de bloqueio da mensagem
        sscanf(message + 13, "%s %d %d", topic_name, &locked_status, &nPersistentes);

        // Procura o tópico na lista local e atualiza o estado

        for (int i = 0; i < numTopics; i++) {
            if (strcmp(topics[i].topic, topic_name) == 0) {
                if (locked_status != topics[i].locked) {
                    topics[i].locked = locked_status;
                    printf("O Tópico [%s] foi %s\n", topic_name, locked_status ? "bloqueado" : "desbloqueado");
                }
                if (nPersistentes != topics[i].nPersistentes) {
                    topics[i].nPersistentes = nPersistentes;
                }

                break;
            }
        }
    } else if (strncmp(message, "msg ", 4) == 0) {
        //O CLIENTE RECEBEU MSG
        mensagem msg;


        sscanf(message, "msg %s %s %[^\n]", msg.remetente, msg.topico, msg.mensagem);

        printf("[%s] -> %s  - %s \n", msg.topico, msg.mensagem, msg.remetente);
    } else {
        printf("Mensagem recebida no fifo: %s\n", message);
    }
}


void notify_manager(const char *username, int status) {
    if (status == 1) {
        int fifoManager = open(Manager_Ligado, O_WRONLY);
        if (fifoManager == -1) {
            perror("Failed to open manager FIFO");
            exit(EXIT_FAILURE);
        }

        char message[MAX_MSG];
        snprintf(message, sizeof(message), "novofeed %s", username);

        if (write(fifoManager, message, strlen(message) + 1) == -1) {
            perror("Failed to write to manager FIFO");
        }

        close(fifoManager);
    }
    if (status == 0) {
        int fifoManager = open(Manager_Ligado, O_WRONLY);
        if (fifoManager == -1) {
            perror("Failed to open manager FIFO");
            exit(EXIT_FAILURE);
        }

        char message[MAX_MSG];
        snprintf(message, sizeof(message), "exit %s", username);

        if (write(fifoManager, message, strlen(message) + 1) == -1) {
            perror("Failed to write to manager FIFO");
        }

        close(fifoManager);
    }
}

void handle_msg(prompt *cmd, const char *nome_jogador) {
    //TOPICO ARG1
    //DURACAO ARG2
    // MENSAGEM ARG3

    if (strlen(cmd->arg1) > MAX_NOME_TOPICO) {
        printf("Erro: O nome do tópico é muito longo. Máximo permitido: %d\n", MAX_NOME_TOPICO);
        return;
    }


    if (strlen(cmd->arg3) > MAX_MSG) {
        printf("Erro: A mensagem é muito longa. Máximo permitido: %d\n", MAX_MSG);
        return;
    }

    //verifica se esta sub
    int encontrado = 0, status = 0;
    for (int i = 0; i < numsubTopics; i++) {
        if (strcmp(cmd->arg1, subedtopic[i]) == 0) {
            encontrado = 1;
            break;
        }
    }
    if (!encontrado) {
        printf("Nao se encontra subscrito a este topico, utiliza 'subscribe <topico>'\n");
        return;
    }

    //verifica se esta lock
    for (int i = 0; i < numTopics; i++) {
        if (strcmp(cmd->arg1, topics[i].topic) == 0 && topics[i].locked == 0) {
            status = 1;
            break;
        }
    }

    if (status) {
        // Abrir o FIFO para o manager
        int fifoManager = open(Manager_Ligado, O_WRONLY);
        if (fifoManager == -1) {
            perror("Failed to open manager FIFO");
            return; // Apenas retorna para continuar a execução
        }


        char message[500];
        snprintf(message, sizeof(message), "msg %s %s %s %s", nome_jogador, cmd->arg1, cmd->arg2, cmd->arg3);

        // Escrever no FIFO
        if (write(fifoManager, message, strlen(message) + 1) == -1) {
            perror("Failed to write to manager FIFO");
            close(fifoManager);
            return; // Retorna para evitar mensagens duplicadas
        }

        close(fifoManager);

        printf("[%s] -> %s  - %s \n", cmd->arg1, cmd->arg3, nome_jogador);
    } else {
        printf("O topico %s está bloqueado\n", cmd->arg1);
    }
}


void handle_subscribe(prompt *cmd, const char *nome_jogador) {
    int esta_sub = 0;
    int sabe = 0;

    // Verifica se o cliente já está subscrito ao tópico
    for (int i = 0; i < numsubTopics; i++) {
        if (strcmp(cmd->arg1, subedtopic[i]) == 0) {
            esta_sub = 1;
            break;
        }
    }
    // Verifica se o cliente sabe do tópico
    for (int i = 0; i < numTopics; i++) {
        if (strcmp(cmd->arg1, topics[i].topic) == 0) {
            sabe = 1;
            break;
        }
    }

    if (!esta_sub) {
        // Verifica se dá para adicionar
        if (strlen(cmd->arg1) + 1 > MAX_NOME_TOPICO) {
            printf("Nome de topico demasiado grande\n");
            return;
        }
        if (numTopics >= MAX_TOPICS) {
            printf("Erro: Limite de tópicos (%d) atingido. Não é possível subscrever a '%s'.\n", MAX_TOPICS, cmd->arg1);
            return;
        }

        // Copia o novo subbed tópico para as listas
        strcpy(subedtopic[numsubTopics], cmd->arg1);
        subedtopic[numsubTopics][strlen(cmd->arg1) + 1] = '\0'; // Garante terminação
        numsubTopics++;

        if (!sabe) {
            strcpy(topics[numTopics].topic, cmd->arg1);
            topics[numTopics].topic[strlen(cmd->arg1) + 1] = '\0'; // Garante terminação
            numTopics++;
        }

        int fifoManager = open(Manager_Ligado, O_WRONLY);
        if (fifoManager == -1) {
            perror("Failed to open manager FIFO");
            return; // Apenas retorna para continuar a execução
        }

        printf("Bem vindo ao topico [%s]\n", cmd->arg1);
        // Montar a mensagem
        char message[MAX_MSG];
        snprintf(message, sizeof(message), "subscribe %s %s", nome_jogador, cmd->arg1);

        // Escrever no FIFO
        if (write(fifoManager, message, strlen(message) + 1) == -1) {
            perror("Failed to write to manager FIFO");
            close(fifoManager);
            return; // Retorna para evitar mensagens duplicadas
        }

        close(fifoManager);
    } else {
        printf("Já se encontra subscrito a [%s].\n", cmd->arg1);
    }
}


void handle_unsubscribe(prompt *cmd, const char *nome_jogador) {
    int encontrado = -1;

    // Verifica se o cliente está subscrito ao tópico
    for (int i = 0; i < numsubTopics; i++) {
        if (strcmp(cmd->arg1, subedtopic[i]) == 0) {
            encontrado = i; // Salva o índice
            break;
        }
    }

    if (encontrado == -1) {
        printf("Erro: Não está subscrito ao tópico [%s].\n", cmd->arg1);
        return;
    }

    // Remove o tópico deslocando os restantes
    for (int i = encontrado; i < numsubTopics; i++) {
        strcpy(subedtopic[i], subedtopic[i + 1]);
    }
    numsubTopics--; // Atualiza o número de tópicos

    // Envia mensagem para o manager via FIFO
    int fifoManager = open(Manager_Ligado, O_WRONLY);
    if (fifoManager == -1) {
        perror("Failed to open manager FIFO");
        return;
    }

    char message[MAX_MSG];
    snprintf(message, sizeof(message), "unsubscribe %s %s", nome_jogador, cmd->arg1);

    if (write(fifoManager, message, strlen(message) + 1) == -1) {
        perror("Failed to write to manager FIFO");
        close(fifoManager);
        return;
    }

    close(fifoManager);

    printf("Saiu do tópico [%s].\n", cmd->arg1);
}


void handle_feed_topics() {
    // Caso não existam tópicos no manager
    if (numTopics == 0) {
        printf("Nenhum tópico disponível no manager.\n");
        return;
    }

    printf("Lista de tópicos:\n");
    printf("---------------------------------------------------------\n");
    printf("| %-25s | %-15s | %-8s |\n", "Nome do Tópico", "Msgs Persistentes", "Status");

    for (int i = 0; i < numTopics; i++) {
        int is_subscribed = 0;

        // Verifica se o tópico atual está na lista de tópicos subscritos
        for (int j = 0; j < numsubTopics; j++) {
            if (strcmp(topics[i].topic, subedtopic[j]) == 0) {
                is_subscribed = 1;
                break;
            }
        }

        // Exibe o tópico com ou sem a marcação de subscrição
        if (is_subscribed && topics[i].locked == 1) {
            printf("| %-25s | %-15d | %-8s |\n", topics[i].topic, topics[i].nPersistentes, "[SUBSCRITO] [LOCKED]");
        } else if (is_subscribed && topics[i].locked == 0) {
            printf("| %-25s | %-15d | %-8s |\n", topics[i].topic, topics[i].nPersistentes, "[SUBSCRITO]");
        } else if (!is_subscribed && topics[i].locked == 1) {
            printf("| %-25s | %-15d | %-8s |\n", topics[i].topic, topics[i].nPersistentes, "LOCKED");
        } else {
            printf("| %-25s | %-15d | %-8s |\n", topics[i].topic, topics[i].nPersistentes, "");
        }
    }
    printf("---------------------------------------------------------\n");
}

