# Variáveis
CC = gcc                  # Compilador
CFLAGS = -Wall -Wextra -Wno-format-truncation    # Flags do compilador
OBJS = funcs.o            # Arquivos objetos reutilizáveis

# Compilação dos executáveis
all: manager feed

manager: manager.o $(OBJS)
	$(CC) -o manager manager.o $(OBJS)
	
broker: manager

feed: feed.o $(OBJS)
	$(CC) -o feed feed.o $(OBJS)

# Compilação dos arquivos .o (objetos)
manager.o: manager.c funcs.h
	$(CC) $(CFLAGS) -c manager.c

feed.o: feed.c funcs.h
	$(CC) $(CFLAGS) -c feed.c

funcs.o: funcs.c funcs.h
	$(CC) $(CFLAGS) -c funcs.c
	
# Exportar variável de ambiente MSG_FICH
MSG_FICH := ./MENSAGENS.txt
export MSG_FICH
#@echo $(MSG_FICH)
 
clean:
	rm -f *.o manager feed