
#include "funcs.h"




void help_manager() {
    printf("Available commands:\n");
    printf("\tusers                  List all users currently using the platform.\n");
    printf("\tremove <username>      Remove a user from the platform.\n");
    printf("\ttopics                 List all topics on the platform, including the number of persistent messages.\n");
    printf("\tshow <topic>           Display all persistent messages for a specific topic.\n");
    printf("\tlock <topic>           Block new messages from being sent to the specified topic.\n");
    printf("\tunlock <topic>         Allow new messages to be sent to the specified topic.\n");
    printf("\tclose                  Shut down the platform and terminate the manager program.\n");
    printf("\thelp                   Show this help message.\n");
}



void help_feed() {
    printf("Available commands:\n");
    printf("\tmsg <topic> <message>   Send a message to a topic.\n");
    printf("\tsubscribe <topic>       Subscribe to a topic.\n");
    printf("\tunsubscribe <topic>     Unsubscribe from a topic.\n");
    printf("\texit                    Exit the feed program.\n");
    printf("\thelp                    Show this help message.\n");
}



/*
void showSubscribedTopics() {
    printf("=== Tópicos Subscritos (%d) ===\n", numsubTopics);
    if (numsubTopics == 0) {
        printf("Nenhum tópico subscrito.\n");
        return;
    }
    for (int i = 0; i < numsubTopics; i++) {
        printf("[%d] %s\n", i + 1, subedtopic[i]);
    }
}
void showTopics() {
    printf("=== Tópicos no Sistema (%d) ===\n", numTopics);

    for (int i = 0; i < 5; i++) {
        printf("[%d] Tópico: %s | Bloqueado: %s | Persistentes: %d\n",
               i + 1,
               topics[i].topic,
               topics[i].locked ? "Sim" : "Não",
               topics[i].nPersistentes);
    }
}

*/
