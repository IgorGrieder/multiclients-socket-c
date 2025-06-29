#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define STR_LEN 11
#define MAX_NICKNAME 13

// Hoisting de funções
void endWithErrorMessage(const char *message);

typedef struct {
  int32_t player_id;
  float value;
  char type[STR_LEN];
  float player_profit;
  float house_profit;
} aviator_msg;

int main(int argc, char *argv[]) {
  struct sockaddr_in client_addr_ipv4;
  struct sockaddr_in6 client_addr_ipv6;
  struct addrinfo criteria;
  struct addrinfo *response;

  char *server_IP = argv[1];
  char *server_port = argv[2];
  int client_socket;
  int is_IPv4 = 0;
  int stop_game = 1;
  aviator_msg aviator_message;

  // Checagens para inicio do cliente
  if (argc != 5) {
    endWithErrorMessage("Error: Invalid number of arguments");
  }

  if (strcmp(argv[3], "-nick") != 0) {
    endWithErrorMessage("Error: Expected '-nick' argument");
  }

  if (strlen(argv[4]) > MAX_NICKNAME) {
    endWithErrorMessage("Error: Nickname too long (max 13)");
  }

  // Comparando o argumento de versão do protocolo para definir qual tipo foi
  // selecionado
  memset(&criteria, 0, sizeof(criteria));
  criteria.ai_family = AF_UNSPEC;
  criteria.ai_socktype = SOCK_STREAM;

  // Resolve seguindo a versão de acordo com o IP e a porta, juntamente com um
  // modelo de resposta
  int err = getaddrinfo(server_IP, server_port, &criteria, &response);
  if (err < 0) {
    endWithErrorMessage("Erro ao identificar a versão do protocolo IP");
  }

  if (response->ai_family == AF_INET) {
    is_IPv4 = 1;
  } else if (response->ai_family == AF_INET6) {
    is_IPv4 = 0;
  }

  if (is_IPv4 == 1) {
    // Conectando ao servidor - Um loop infinito ate que a conexão com o
    // servidor seja aceita
    while (1) {
      // Socket IPv4
      memset(&client_addr_ipv4, 0, sizeof(client_addr_ipv4));
      client_addr_ipv4.sin_family = AF_INET;
      client_addr_ipv4.sin_port = htons(atoi(argv[2]));

      // Criando socket
      client_socket = socket(client_addr_ipv4.sin_family, SOCK_STREAM, 0);
      if (client_socket < 0) {
        endWithErrorMessage("Erro ao criar socket IPv4 client side");
      }

      // Utilizando biblioteca para convertar IP para binário
      int checkBinaryIP;
      checkBinaryIP = inet_pton(AF_INET, argv[1], &client_addr_ipv4.sin_addr);
      if (checkBinaryIP <= 0) {
        endWithErrorMessage("Endereço inválido");
      }

      int checkConnection;
      checkConnection =
          connect(client_socket, (struct sockaddr *)&client_addr_ipv4,
                  sizeof(client_addr_ipv4));
      if (checkConnection >= 0) {
        break;
      } else {
        close(client_socket);
      }
    }

  } else {
    // Conectando ao servidor - Um loop infinito ate que a conexão com o
    // servidor seja aceita
    while (1) {
      // Socket IPv6
      memset(&client_addr_ipv6, 0, sizeof(client_addr_ipv6));
      client_addr_ipv6.sin6_family = AF_INET6;
      client_addr_ipv6.sin6_port = htons(atoi(argv[2]));

      client_socket = socket(client_addr_ipv6.sin6_family, SOCK_STREAM, 0);
      if (client_socket < 0) {
        endWithErrorMessage("Erro ao criar socket IPv6 client side");
      }

      // Utilizando biblioteca para convertar IP para binário
      int checkBinaryIP;
      checkBinaryIP = inet_pton(AF_INET6, argv[1], &client_addr_ipv6.sin6_addr);
      if (checkBinaryIP <= 0) {
        endWithErrorMessage("Endereço inválido");
      }

      // Conectando ao servidor - Um loop infinito ate que a conexao seja aceita
      int checkConnection;
      checkConnection =
          connect(client_socket, (struct sockaddr *)&client_addr_ipv6,
                  sizeof(client_addr_ipv6));
      if (checkConnection == 0) {
        break;
      } else {
        close(client_socket);
      }
    }
  }

  printf("Conectado ao servidor.\n");

  // Loop sem fim de execução do jogo
  while (stop_game) {
    // Chamada blocking que armazena o retorno do server
    recv(client_socket, &game_message, sizeof(game_message), 0);

    switch (game_message.type) {
    case MSG_REQUEST:
      printf("%s", game_message.message);

      int action;
      scanf("%d", &action);

      // Enviando uma mensagem ao servidor com o type da mensagem e a ação do
      // usuário
      sendClientResponse(&game_message, action, MSG_RESPONSE, client_socket);
      break;
    case MSG_RESULT:
      printf("%s", game_message.message);

      break;
    case MSG_PLAY_AGAIN_REQUEST:
      printf("%s", game_message.message);

      int actionRequest;
      scanf("%d", &actionRequest);

      // Enviar resposta do usuário sobre querer jogar ou não
      sendClientResponse(&game_message, actionRequest, MSG_PLAY_AGAIN_RESPONSE,
                         client_socket);
      break;
    case MSG_ERROR:
      printf("%s", game_message.message);

      int decisionError;
      scanf("%d", &decisionError);

      // Enviar resposta do usuário sobre o erro reportado propagando type de
      // error, já que não terá distinção por parte do servidor nesse caso
      sendClientResponse(&game_message, decisionError, MSG_ERROR,
                         client_socket);
      break;
    case MSG_END:
      // Parando o jogo e exibindo mensagem do servidor do resultado
      stop_game = 0;
      printf("%s", game_message.message);

      // Liberando memória
      freeaddrinfo(response);
      close(client_socket);
      return EXIT_SUCCESS;
    }
  }
}

// Função para exibir erros genéricos e finalizar a execução do programa sem
// tratamentos
void endWithErrorMessage(const char *message) {
  perror(message);
  exit(EXIT_FAILURE);
}
