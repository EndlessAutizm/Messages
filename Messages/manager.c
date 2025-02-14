//FICHEIRO MANAGER.C

#include "funcs.h"

const char *Manager_Ligado = "./fifos/manager_ligado";


int running = 1;

int numUtilizadores = 0;
users Users[MAX_USERS]; //feeds existentes

mensagem Persistentes[MAX_TOTAL_PERSIS]; //Mensagens Persistentes
int numPersistentes = 0;

topic topics[MAX_TOPICS]; //topicos existentes
int numTopics = 0;



int main() {
    if (access(Manager_Ligado, F_OK) == 0) {
        fprintf(stderr, "O Programa Manager já está em execucao\n");
        exit(1);
    }

    // Remove a pasta dos fifos
    if (system("rm -rf ./fifos") == -1) {
        perror("Erro ao remover o diretório de FIFOs");
        exit(EXIT_FAILURE);
    }

    // adiciona a pasta dos fifos
    if (system("mkdir ./fifos") == -1) {
        perror("Erro ao recriar o diretório de FIFOs");
        exit(EXIT_FAILURE);
    }

    // printf("MSG_FICH recebido: %s\n", MSG_FICH);
    printf("\tBem vindo ao Manager!\n");

    if (access(MSG_FICH, F_OK) == 0) {
        carregarMensagens();
    }
    if (numPersistentes != 0) {
        printf("Mensagens carregadas: %d\n", numPersistentes);
    }


    if (mkfifo(Manager_Ligado, 0666) == -1) {
        perror("mkfifo");
     }

    // Abrir o FIFO para leitura e escrita
    int fifoManager = open(Manager_Ligado, O_RDWR);
    if (fifoManager == -1) {
        perror("open");
        exit(1);
    }

    // Criar threads
    pthread_t admin_tid, feed_tid, decrease_duration_tid;

    if (pthread_create(&admin_tid, NULL, admin_thread, NULL) != 0) {
        perror("Failed to create admin thread");
        close(fifoManager);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&feed_tid, NULL, feed_monitor_thread, &fifoManager) != 0) {
        perror("Failed to create feed monitor thread");
        close(fifoManager);
        pthread_cancel(admin_tid);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&decrease_duration_tid, NULL, decrease_msg_duration_thread, NULL) != 0) {
        perror("Failed to create decrease duration thread");
        close(fifoManager);
        exit(EXIT_FAILURE);
    }

    // Aguardar threads
    pthread_join(admin_tid, NULL);
    pthread_cancel(feed_tid);
    pthread_cancel(decrease_duration_tid);


    salvarMensagens();


    close(fifoManager);
    unlink(Manager_Ligado);

    // Remove a pasta de fifos
    if (system("rm -rf ./fifos") == -1) {
        perror("Erro ao remover o diretório de FIFOs");
        exit(EXIT_FAILURE);
    }

    printf("============= Fim do programa Manager =============\n");
    return 0;
}


void *admin_thread() {
    char input[256];
    prompt cmd;


    while (running) {
        printf("> "); // Mostrar o prompt

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            continue;
        }

        // Remover o /0
        input[strcspn(input, "\n")] = '\0';

        // Separar o comando e os argumentos
        memset(&cmd, 0, sizeof(prompt));
        int num_args = sscanf(input, "%s %s %[^\n]", cmd.comando, cmd.arg1, cmd.arg2);

        if (num_args > 0) {
            if (strcmp(cmd.comando, "close") == 0 || strcmp(cmd.comando, "CLOSE") == 0) {
                running = 0;
                if (numUtilizadores > 0) { close_notify_feeds(); }
                break;
            } else if (strcmp(cmd.comando, "users") == 0) {
                handle_users();
            } else if (strcmp(cmd.comando, "remove") == 0) {
                if (num_args == 2) {
                    handle_remove(&cmd);
                } else {
                    printf("Usage: remove <username>\n");
                }
            } else if (strcmp(cmd.comando, "topics") == 0) {
                handle_manager_topics();
            } else if (strcmp(cmd.comando, "show") == 0) {
                if (num_args == 2) {
                    handle_show(&cmd);
                } else {
                    printf("Usage: show <topico>\n");
                }
            } else if (strcmp(cmd.comando, "lock") == 0) {
                if (num_args == 2) {
                    handle_lock(&cmd);
                } else {
                    printf("Usage: lock <topico>\n");
                }
            } else if (strcmp(cmd.comando, "unlock") == 0) {
                if (num_args == 2) {
                    handle_unlock(&cmd);
                } else {
                    printf("Usage: unlock <topico>\n");
                }
            } else if (strcmp(cmd.comando, "help") == 0) {
                help_manager();
            } else {
                printf("Unknown command: %s\n", cmd.comando);
            }
        }
    }


    return NULL;
}


// Thread para monitorar fifo comun
void *feed_monitor_thread(void *arg) {
    int fifoManager = *((int *) arg);
    char message[MAX_MSG];

    while (running) {
        ssize_t bytes_read = read(fifoManager, message, sizeof(message));
        if (bytes_read > 0) {
            message[bytes_read] = '\0';
            // printf("received message: %s\n", message);
            handle_fifo_comum_manager(message);
        } else if (bytes_read == -1) {
            perror("Error reading from manager FIFO");
        }
    }
    return NULL;
}

// Thread para diminuir a duração das mensagens persistentes
void *decrease_msg_duration_thread() {
    while (running) {
        sleep(1); // 1 segundo

        // Iterar sobre as mensagens persistentes e diminuir a duração
        for (int i = 0; i < numPersistentes; i++) {
            if (Persistentes[i].duracao > 0) {
                Persistentes[i].duracao--; 
            }


            if (Persistentes[i].duracao <= 0) {
                //expirou
                //guardar o nome
                char temp[MAX_NOME_TOPICO];
                strcpy(temp, Persistentes[i].topico);

                //remove a que expirou
                for (int j = i; j < numPersistentes; j++) {
                    Persistentes[j] = Persistentes[j + 1];
                }
                numPersistentes--;
                printf("Eliminei uma mensagem persistente de [%s]\n", temp);

                //diminui o N de persistentes no topico que estava
                for (int j = 0; j < numTopics; j++) {
                    if (strcmp(temp, topics[j].topic) == 0) {
                        topics[j].nPersistentes--;
                        // printf("%s Ficou com %d mensagens persistentes\n", temp, numPersistentes);
                        ManageTopic(topics[j].topic, topics[j].locked, topics[j].nPersistentes);
                    }
                }

                i--; // Para verificar novamente a posição após a remoção
            }
        }
    }
    return NULL;
}


// Lidar com mensagens dos feeds no fifo comun
void handle_fifo_comum_manager(const char *message) {
    prompt cmd;
    if (strncmp(message, "novofeed ", 9) == 0) {
        const char *username = message + 9; // Obtém o nome do utilizador

        // Verifica se há espaço para mais utilizadores
        if (numUtilizadores < MAX_USERS) {
            // Preenche as informações do utilizador
            strncpy(Users[numUtilizadores].username, username, MAX_NAME_LENGTH - 1);
            Users[numUtilizadores].username[MAX_NAME_LENGTH - 1] = '\0'; // Garante o null terminator

            // Cria o nome do FIFO associado ao utilizador
            snprintf(Users[numUtilizadores].fifo, sizeof(Users[numUtilizadores].fifo), "./fifos/fifo_%s", username);

            printf("Novo utilizador: %s\n", Users[numUtilizadores].username);

            // Notificar o novo feed sobre os tópicos existentes
            int feed_fd = open(Users[numUtilizadores].fifo, O_WRONLY);
            if (feed_fd == -1) {
                perror("Erro ao abrir o FIFO para notificar tópicos existentes");
            } else {
                char notify_message[300] = {0};
                snprintf(notify_message, sizeof(notify_message), "knowtopics %d", numTopics);
                for (int i = 0; i < numTopics; i++) {
                    char topic_details[100];
                    snprintf(topic_details, sizeof(topic_details), " %s %d %d",
                             topics[i].topic, topics[i].nPersistentes, topics[i].locked);
                    strcat(notify_message, topic_details);
                }

                if (write(feed_fd, notify_message, strlen(notify_message)) == -1) {
                    perror("Erro ao enviar tópicos existentes para o feed");
                }
                close(feed_fd);
            }

            numUtilizadores++;
        } else {
            printf("Limite de utilizadores atingido. Não foi possível adicionar: %s\n", username);

            // Notifica o feed que não pode ser adicionado
            char fifo_path[100];
            snprintf(fifo_path, sizeof(fifo_path), "./fifos/fifo_%s", username);
            int feed_fd = open(fifo_path, O_WRONLY);
            if (feed_fd == -1) {
                perror("Erro ao abrir o FIFO para notificar limite atingido");
            } else {
                const char *error_message = "maxusers";
                if (write(feed_fd, error_message, strlen(error_message)) == -1) {
                    perror("Erro ao enviar mensagem de limite atingido para o feed");
                }
                close(feed_fd);
            }
        }
    }else if (strncmp(message, "exit ", 5) == 0) {
        // O CLIENTE DEU EXIT
        const char *username = message + 5; // Obtém o nome do utilizador a ser removido
        int i, found = 0;
        char subscribedTopics[MAX_TOPICS][MAX_NOME_TOPICO];
        int topicCount = 0; // Contador para tópicos subscritos

        // Procura o utilizador na lista
        for (i = 0; i < numUtilizadores; i++) {
            if (strcmp(Users[i].username, username) == 0) {
                found = 1;

                // Armazena os tópicos subscritos do utilizador
                for (int k = 0; k < Users[i].topics_count; k++) {
                    strcpy(subscribedTopics[topicCount], Users[i].topics[k]);
                    topicCount++;
                }
                break;
            }
        }

        if (found) {
            // Remove o utilizador deslocando os outros
            for (int j = i; j < numUtilizadores ; j++) {
                Users[j] = Users[j + 1];
            }
            numUtilizadores--;

            // Gerencia os tópicos subscritos pelo utilizador
            for (int t = 0; t < topicCount; t++) {
                // Chama ManageTopic para cada tópico subscrito pelo utilizador
                ManageTopic(subscribedTopics[t], -1, 0); // -1 não altera o status de bloqueio
            }

            printf("Utilizador %s saiu da sessão!\n", username);
        } else {
            printf("Utilizador %s não encontrado.\n", username);
        }
    }else if (strncmp(message, "msg ", 4) == 0) {
        // O CLIENTE MANDOU MSG

        mensagem msg;
        // TOPICO ARG1
        // DURACAO ARG2
        // MENSAGEM ARG3

        sscanf(message, "msg %s %s %d %[^\n]", msg.remetente, msg.topico, &msg.duracao, msg.mensagem);

        // Verificar se é persistente
        if (msg.duracao > 0) {
            if (numPersistentes < MAX_TOTAL_PERSIS) {
                int topicIndex = -1;
                for (int i = 0; i < numTopics; i++) {
                    if (strcmp(topics[i].topic, msg.topico) == 0) {
                        topicIndex = i;
                        break;
                    }
                }

                if (topicIndex != -1 && topics[topicIndex].nPersistentes < MAX_TOPIC_PERSIS) {
                    // Guardar a mensagem como persistente
                    Persistentes[numPersistentes] = msg;
                    numPersistentes++;
                    topics[topicIndex].nPersistentes++;

                    // Atualizar os feeds
                    ManageTopic(topics[topicIndex].topic, topics[topicIndex].locked, topics[topicIndex].nPersistentes);
                    printf("O tópico [%s] tem uma nova mensagem persistente\n", topics[topicIndex].topic);
                } else if (topicIndex != -1) {
                    printf("O tópico [%s] atingiu o limite de mensagens persistentes.\n", topics[topicIndex].topic);
                    msg.duracao = 0; // Considera a mensagem normal
                } else {
                    printf("Erro: Tópico [%s] não encontrado.\n", msg.topico);
                }
            } else {
                printf("Atingiu o limite global de mensagens persistentes. A mensagem foi considerada normal.\n");
                msg.duracao = 0; // Considera a mensagem normal
            }
        }

        printf("[%s] -> %s  - %s \n", msg.topico, msg.mensagem, msg.remetente);
        // Reenviar a mensagem para os clientes subscritos
        // usleep(250000); // delay para dar tempo de atualizar nPersistentes
        ResendMessage(msg, false, msg.remetente);
    }else if (strncmp(message, "subscribe ", 10) == 0) {
        // O CLIENTE SUBSCRIBE
        int found = 0;
        sscanf(message, "subscribe %s %[^\n]", cmd.comando, cmd.arg1);

        // Verifica se o tópico já existe
        for (int i = 0; i < numTopics; i++) {
            if (strcmp(topics[i].topic, cmd.arg1) == 0) {
                found = 1;
                break;
            }
        }

        // Adiciona o tópico à lista se não foi encontrado
        if (!found) {
            if (numTopics < MAX_TOPICS) {
                strcpy(topics[numTopics].topic, cmd.arg1);
                topics[numTopics].nPersistentes = 0;
                topics[numTopics].locked = 0;
                numTopics++;
            } else {
                printf("Erro: Limite de tópicos atingido. Não é possível adicionar [%s].\n", cmd.arg1);
            }
        }

        // Associa o tópico ao utilizador
        for (int i = 0; i < numUtilizadores; i++) {
            if (strcmp(Users[i].username, cmd.comando) == 0) {
                // Adiciona o tópico ao utilizador
                if (Users[i].topics_count < MAX_TOPICS) {
                    strcpy(Users[i].topics[Users[i].topics_count], cmd.arg1);
                    Users[i].topics_count++;
                } else {
                    printf("Erro: Limite de tópicos para o utilizador '%s' atingido.\n", Users[i].username);
                }
                break;
            }
        }

        //avisar ou outros feeds de um potencial novo topico
        for (int k = 0; k < numUtilizadores; k++) {
            if (strcmp(Users[k].username, cmd.comando) != 0) {
                int feed_fd = open(Users[k].fifo, O_WRONLY);
                if (feed_fd == -1) {
                    printf("Não foi possível notificar o feed do utilizador: %s\n", Users[k].username);
                } else {
                    // Criar mensagem para os outros feeds
                    char notify_message[350];
                    snprintf(notify_message, sizeof(notify_message), "novo topic %s", cmd.arg1);

                    // Escrever a notificação no FIFO
                    if (write(feed_fd, notify_message, strlen(notify_message)) == -1) {
                        perror("Erro ao escrever no FIFO para outros feeds");
                    }
                    // Fechar o FIFO
                    close(feed_fd);
                }
            }
        }

        //Reenvia mensagens persistentes para quem deu sub ao topic
        char Vremetente[MAX_NAME_LENGTH];
        for (int k = 0; k < numPersistentes; k++) {
            if (strcmp(Persistentes[k].topico, cmd.arg1) == 0) {
                strcpy(Vremetente, Persistentes[k].remetente);
                strcpy(Persistentes[k].remetente, cmd.comando);
                ResendMessage(Persistentes[k], true, Vremetente);
                strcpy(Persistentes[k].remetente, Vremetente); // Restaura o remetente original
                sleep(1);
            }
        }
    }else if (strncmp(message, "unsubscribe ", 12) == 0) {
        // O CLIENTE UNSUBSCRIBE

        sscanf(message, "unsubscribe %s %[^\n]", cmd.comando, cmd.arg1);

        // Localiza o utilizador
        for (int i = 0; i < numUtilizadores; i++) {
            if (strcmp(Users[i].username, cmd.comando) == 0) {
                // Encontrar a posição do tópico no utilizador
                int topic_index = -1;
                for (int j = 0; j < Users[i].topics_count; j++) {
                    if (strcmp(Users[i].topics[j], cmd.arg1) == 0) {
                        topic_index = j;
                        break;
                    }
                }

                if (topic_index == -1) {
                    printf("Erro: O utilizador '%s' não está subscrito ao tópico [%s].\n", cmd.comando, cmd.arg1);
                    break;
                }

                // Remove o tópico da lista do utilizador
                for (int j = topic_index; j < Users[i].topics_count ; j++) {
                    strcpy(Users[i].topics[j], Users[i].topics[j + 1]);
                }
                Users[i].topics_count--;

                // Gerencia o tópico para verificar remoção ou atualizações
                int global_topic_index = -1;
                for (int t = 0; t < numTopics; t++) {
                    if (strcmp(topics[t].topic, cmd.arg1) == 0) {
                        global_topic_index = t;
                        break;
                    }
                }

                if (global_topic_index != -1) {
                    ManageTopic(topics[global_topic_index].topic,
                                topics[global_topic_index].locked,
                                topics[global_topic_index].nPersistentes);
                } else {
                    printf("Erro: O tópico [%s] não foi encontrado na lista global.\n", cmd.arg1);
                }

                break;
            }
        }
    }else {
        printf("Outra mensagem lida: %s\n", message);
    }
}


void ResendMessage(mensagem msg, bool modopersistentes, const char *Vremetente) {
    char reenviar[MAX_ARG];

    if (modopersistentes) {
        // Para mensagens persistentes, apenas envia ao remetente especificado
        for (int i = 0; i < numUtilizadores; i++) {
            if (strcmp(Users[i].username, msg.remetente) == 0) {
                // Abre o FIFO do remetente da mensagem
                int fd = open(Users[i].fifo, O_WRONLY);
                if (fd == -1) {
                    perror("Erro ao abrir o FIFO do utilizador");
                    return; // Encerra pois só enviamos a este utilizador
                }

                // Prepara a mensagem para envio
                snprintf(reenviar, sizeof(reenviar), "msg %s %s %s", Vremetente, msg.topico, msg.mensagem);


                // Envia a mensagem para o FIFO
                if (write(fd, reenviar, strlen(reenviar) + 1) == -1) {
                    perror("Erro ao enviar mensagem para o FIFO");
                }

                close(fd); // Fecha o FIFO
                return; // Apenas envia para um utilizador
            }
        }
    } else {
        // Para mensagens normais, envia para todos os subscritores, exceto o próprio remetente
        for (int i = 0; i < numUtilizadores; i++) {
            bool isSubscribed = false;

            // Verifica se o utilizador está subscrito ao tópico da mensagem
            for (int j = 0; j < Users[i].topics_count; j++) {
                if (strcmp(Users[i].topics[j], msg.topico) == 0) {
                    isSubscribed = true;
                    break;
                }
            }

            if (!isSubscribed) {
                continue; // Se não estiver subscrito, passa para o próximo utilizador
            }

            // Não reenvia para o próprio remetente
            if (strcmp(Users[i].username, msg.remetente) == 0) {
                continue;
            }

            // Abre o FIFO do utilizador
            int fd = open(Users[i].fifo, O_WRONLY);
            if (fd == -1) {
                perror("Erro ao abrir o FIFO do utilizador");
                continue;
            }

            // Prepara a mensagem para envio
            snprintf(reenviar, sizeof(reenviar), "msg %s %s %s", msg.remetente, msg.topico, msg.mensagem);


            // Envia a mensagem para o FIFO
            if (write(fd, reenviar, strlen(reenviar) + 1) == -1) {
                perror("Erro ao enviar mensagem para o FIFO");
            }

            close(fd); // Fecha o FIFO
        }
    }
}


//avisar os clientes do close no fifo deles
void close_notify_feeds() {
    for (int i = 0; i < numUtilizadores; i++) {
        // Abrir o FIFO do utilizador
        int fd = open(Users[i].fifo, O_WRONLY);
        if (fd == -1) {
            perror("Erro ao abrir o FIFO para o utilizador");
            printf("Não foi possível notificar o utilizador: %s\n", Users[i].username);
            continue;
        }

        // Escrever a mensagem de encerramento no FIFO
        const char *close_message = "close";
        if (write(fd, close_message, strlen(close_message)) == -1) {
            perror("Erro ao escrever no FIFO");
        }

        // Fechar o FIFO
        close(fd);
    }
}



// Implementação dos comandos de consola
void handle_users() {
    printf("Usuários ativos:\n");
    printf("---------------------------------------------------------\n");
    printf("| %-25s | %-25s |\n", "Nome do Usuário", "Topicos Subscritos");
    printf("---------------------------------------------------------\n");

    for (int i = 0; i < numUtilizadores; i++) {
        printf("| %-25s | %-25d |\n", Users[i].username, Users[i].topics_count);
    }

    printf("---------------------------------------------------------\n");
}


void handle_remove(prompt *cmd) {
    int found = 0;
    char subscribedTopics[MAX_TOPICS][MAX_NOME_TOPICO];
    int topicCount = 0; // Contador para tópicos subscritos

    for (int i = 0; i < numUtilizadores; i++) {
        if (strcmp(Users[i].username, cmd->arg1) == 0) {
            found = 1;

            // Armazena os tópicos subscritos do utilizador antes de removê-lo
            for (int k = 0; k < Users[i].topics_count; k++) {
                strcpy(subscribedTopics[topicCount], Users[i].topics[k]);
                topicCount++;
            }

            // Abrir o FIFO do usuário para notificar a remoção
            int fd = open(Users[i].fifo, O_WRONLY);
            if (fd == -1) {
                perror("Erro ao abrir o FIFO para o utilizador");
                printf("Não foi possível notificar o utilizador: %s\n", Users[i].username);
            } else {
                // Criar a mensagem de remoção
                char remove_message[350];
                snprintf(remove_message, sizeof(remove_message), "remove %s", cmd->arg1);

                // Escrever a mensagem de encerramento no FIFO
                if (write(fd, remove_message, strlen(remove_message)) == -1) {
                    perror("Erro ao escrever no FIFO");
                }

                // Fechar o FIFO
                close(fd);
            }

            // Remover o usuário do array
            for (int j = i; j < numUtilizadores ; j++) {
                Users[j] = Users[j + 1];
            }
            numUtilizadores--;

            // Verificar os tópicos subscritos e usar ManageTopic
            for (int t = 0; t < topicCount; t++) {
                int global_topic_index = -1;
                for (int u = 0; u < numTopics; u++) {
                    if (strcmp(topics[u].topic, subscribedTopics[t]) == 0) {
                        global_topic_index = u;
                        break;
                    }
                }

                if (global_topic_index != -1) {
                    ManageTopic(topics[global_topic_index].topic, topics[global_topic_index].locked, topics[global_topic_index].nPersistentes);
                } else {
                    printf("Erro: O tópico [%s] não foi encontrado na lista global.\n", subscribedTopics[t]);
                }
            }

            printf("Usuário %s removido com sucesso.\n", cmd->arg1);

            // Notificar todos os outros feeds sobre a remoção
            for (int k = 0; k < numUtilizadores; k++) {
                int feed_fd = open(Users[k].fifo, O_WRONLY);
                if (feed_fd == -1) {
                    perror("Erro ao abrir FIFO para notificar outros feeds");
                    printf("Não foi possível notificar o feed do utilizador: %s\n", Users[k].username);
                } else {
                    // Criar mensagem para os outros feeds
                    char notify_message[350];
                    snprintf(notify_message, sizeof(notify_message), "notify remove %s", cmd->arg1);

                    // Escrever a notificação no FIFO
                    if (write(feed_fd, notify_message, strlen(notify_message)) == -1) {
                        perror("Erro ao escrever no FIFO para outros feeds");
                    }

                    // Fechar o FIFO
                    close(feed_fd);
                }
            }

            break; // Já encontramos e removemos o usuário, saímos do loop
        }
    }

    if (!found) {
        printf("Não existe o utilizador %s\n", cmd->arg1);
    }
}




void handle_manager_topics() {

    printf("Lista de tópicos existentes:\n");
    printf("---------------------------------------------------------\n");
    printf("| %-25s | %-15s | %-8s|\n", "Nome do Tópico", "Msgs Persistentes", "Status");
    printf("---------------------------------------------------------\n");

    for (int i = 0; i < numTopics; i++) {
        const char *estado = topics[i].locked ? "[Locked]" : "Unlocked";
        printf("| %-25s | %-15d | %-8s |\n", topics[i].topic, topics[i].nPersistentes, estado);
    }

    printf("---------------------------------------------------------\n");
}


void handle_show(const prompt *cmd) {
    const char *topico = cmd->arg1;

    // Verifica se o tópico existe
    int existe = 0;
    for (int i = 0; i < numTopics; i++) {
        if (strcmp(topics[i].topic, topico) == 0) {
            existe = 1;
            break;
        }
    }

    if (!existe) {
        printf("Erro: O tópico [%s] não existe.\n", topico);
        return;
    }

    // Exibe as mensagens persistentes do tópico
    printf("Mensagens persistentes do tópico [%s]:\n", topico);
    printf("---------------------------------------------------------------\n");

    int encontrouMensagens = 0;
    for (int i = 0; i < numPersistentes; i++) {
        if (strcmp(Persistentes[i].topico, topico) == 0) {
            encontrouMensagens = 1;
            printf("Mensagem: %s\n", Persistentes[i].mensagem);
            printf("Remetente: %s\n", Persistentes[i].remetente);
            printf("Duração: %d segundos restantes\n", Persistentes[i].duracao);
            printf("---------------------------------------------------------------\n");
        }
    }

    if (!encontrouMensagens) {
        printf("Nenhuma mensagem persistente encontrada.\n");
    }
}


void handle_lock(prompt *cmd) {
    int found = 0, pos = 0;
    for (int i = 0; i < numTopics; i++) {
        if (strcmp(topics[i].topic, cmd->arg1) == 0) {
            found = 1;
            pos = i;
            break;
        }
    }
    if (found) {
        if (topics[pos].locked) {
            printf("O tópico [%s] já está bloqueado.\n", topics[pos].topic);
            return;
        }
        topics[pos].locked = 1;

        ManageTopic(topics[pos].topic, 1, topics[pos].nPersistentes);

        printf("Tópico [%s] bloqueado.\n", topics[pos].topic);
    } else {
        printf("O tópico [%s] não existe.\n", cmd->arg1);
    }
}

void handle_unlock(prompt *cmd) {
    int found = 0, pos = -1;
    for (int i = 0; i < numTopics; i++) {
        if (strcmp(topics[i].topic, cmd->arg1) == 0) {
            found = 1;
            pos = i;
            break;
        }
    }
    if (found) {
        if (!topics[pos].locked) {
            printf("O tópico [%s] já está desbloqueado.\n", topics[pos].topic);
            return;
        }
        topics[pos].locked = 0;

        ManageTopic(topics[pos].topic, 0, topics[pos].nPersistentes);

        printf("Tópico [%s] desbloqueado.\n", topics[pos].topic);
    } else {
        printf("O tópico [%s] não existe.\n", cmd->arg1);
    }
}


void ManageTopic(char *topic_nome, int locked_status, int nPersistentes) {
    char notify_message[300];
    char topic_name[MAX_NOME_TOPICO];

    // Copia o nome do tópico para evitar problemas com o ponteiro
    strncpy(topic_name, topic_nome, MAX_NOME_TOPICO);

    // Verifica subscrições e mensagens persistentes
    if (nPersistentes <= 0) {
        int has_subscribers = 0;
        for (int k = 0; k < numUtilizadores; k++) {
            for (int l = 0; l < Users[k].topics_count; l++) {
                if (strcmp(Users[k].topics[l], topic_name) == 0) {
                    has_subscribers = 1;
                    break;
                }
            }
            if (has_subscribers) {
                break;
            }
        }

        if (!has_subscribers) {
            if (has_persistent_messages(topic_name) == 0) {
                int global_topic_index = -1;
                for (int k = 0; k < numTopics; k++) {
                    if (strcmp(topics[k].topic, topic_name) == 0) {
                        global_topic_index = k;
                        break;
                    }
                }

                if (global_topic_index != -1) {
                    for (int i = global_topic_index; i < numTopics ; i++) {
                        topics[i] = topics[i + 1];
                    }
                    numTopics--;
                    printf("Tópico [%s] removido do manager, pois não há subs nem mensagens.\n", topic_name);

                    // Notificar os feeds sobre a remoção do tópico
                    snprintf(notify_message, sizeof(notify_message), "removetopic %s", topic_name);
                    for (int k = 0; k < numUtilizadores; k++) {
                        int feed_fd = open(Users[k].fifo, O_WRONLY);
                        if (feed_fd == -1) {
                            printf("Não foi possível notificar o feed do utilizador: %s\n", Users[k].username);
                        } else {
                            if (write(feed_fd, notify_message, strlen(notify_message)) == -1) {
                                perror("Erro ao escrever no FIFO para outros feeds");
                            }
                            close(feed_fd);
                        }
                        sleep(1);
                    }
                }
                return; // Encerra a função após remoção
            }
        }
    }

    // Atualiza o status do tópico (bloqueado/desbloqueado) e envia notificações
    snprintf(notify_message, sizeof(notify_message), "updatelocked %s %d %d", topic_name, locked_status, nPersistentes);
    // printf("Eu enviei: %s\n", notify_message);

    for (int i = 0; i < numUtilizadores; i++) {
        int feed_fd = open(Users[i].fifo, O_WRONLY);
        if (feed_fd == -1) {
            perror("Erro ao abrir o FIFO para notificar sobre atualização de tópico");
            continue; // Tenta o próximo feed
        }

        if (write(feed_fd, notify_message, strlen(notify_message)) == -1) {
            perror("Erro ao enviar mensagem de atualização de tópico para o feed");
        }

        close(feed_fd);
    }
    sleep(1);
}



int has_persistent_messages(const char *topic) {
    for (int i = 0; i < numPersistentes; i++) {
        if (strcmp(Persistentes[i].topico, topic) == 0) {
            return 1; // Encontrou uma mensagem persistente para o tópico
        }
    }
    return 0; // Não há mensagens persistentes para o tópico
}

void carregarMensagens() {
    FILE *arquivo = fopen(MSG_FICH, "r");
    if (arquivo == NULL) {
        printf("Nenhum arquivo de mensagens encontrado.\n");
        return;
    }

    int i = 0;
    char line[1024];
    while (fgets(line, sizeof(line), arquivo)) {
        // Remover o caractere de nova linha
        line[strcspn(line, "\n")] = 0;

        if (sscanf(line, "%s %s %d %[^\n]",
                   Persistentes[i].topico,
                   Persistentes[i].remetente,
                   &Persistentes[i].duracao,
                   Persistentes[i].mensagem) == 4) {
            // Atualizar a lista de tópicos
            int j;
            for (j = 0; j < numTopics; j++) {
                if (strcmp(Persistentes[i].topico, topics[j].topic) == 0) {
                    topics[j].nPersistentes++;
                    break;
                }
            }

            // Se o tópico não foi encontrado, adicioná-lo
            if (j == numTopics) {
                strcpy(topics[numTopics].topic, Persistentes[i].topico);
                topics[numTopics].nPersistentes = 1;
                topics[numTopics].locked = 0;
                numTopics++;
            }

            i++;
                   }
    }

    numPersistentes = i;
    fclose(arquivo);
}



void salvarMensagens() {
    // Se não houver mensagens persistentes, limpa ou remove o arquivo
    if (numPersistentes <= 0) {
        remove(MSG_FICH);
        // printf("Arquivo de mensagens persistentes removido por estar vazio.\n");
        return;
    }

    FILE *arquivo = fopen(MSG_FICH, "w");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo para escrita.\n");
        return;
    }

    // Escreve as mensagens persistentes no formato especificado
    for (int i = 0; i < numPersistentes; i++) {
        fprintf(arquivo, "%s %s %d %s\n",
                Persistentes[i].topico,
                Persistentes[i].remetente,
                Persistentes[i].duracao,
                Persistentes[i].mensagem);
    }

    fclose(arquivo);
    printf("Mensagens persistentes salvas com sucesso.\n");
}

