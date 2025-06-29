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

typedef enum {
  WAIT,
  BET,
  FlIGHT,
} GameStates;

// Variáveis globais para acompanhamento de estados
char nickname[MAX_NICKNAME];
int client_running = 1;
int client_socket;
int current_game_phase = WAIT; // 0: waiting, 1: betting phase, 2: flight phase
int has_bet_this_round = 0;
float current_bet = 0.0;

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
    endWithErrorMessage("Error trying to identify the IP protocol");
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
        endWithErrorMessage("Error creating ipv4 socket");
      }

      // Utilizando biblioteca para convertar IP para binário
      int checkBinaryIP;
      checkBinaryIP = inet_pton(AF_INET, argv[1], &client_addr_ipv4.sin_addr);
      if (checkBinaryIP <= 0) {
        endWithErrorMessage("Invalid address");
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
        endWithErrorMessage("Error creating ipv4 socket");
      }

      // Utilizando biblioteca para convertar IP para binário
      int checkBinaryIP;
      checkBinaryIP = inet_pton(AF_INET6, argv[1], &client_addr_ipv6.sin6_addr);
      if (checkBinaryIP <= 0) {
        endWithErrorMessage("Invalid address");
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

  printf("Conected\n");

  // Loop sem fim de execução do jogo
  while (client_running) {
    ssize_t bytes_received =
        recv(client_socket, &aviator_message, sizeof(aviator_msg), 0);

    if (bytes_received <= 0) {
      // Connection lost
      break;
    }

    // Process different message types
    if (strcmp(aviator_message.type, "start") == 0) {
      current_game_phase = BET;
      has_bet_this_round = 0;
      current_bet = 0.0;

      printf("Rodada aberta! Digite o valor da aposta ou digite [Q] para sair "
             "(%.0f segundos restantes):\n",
             aviator_message.value);
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "closed") == 0) {
      current_game_phase = FlIGHT;
      printf("Apostas encerradas! Não é mais possível apostar nesta rodada.\n");

      if (has_bet_this_round) {
        printf("Digite [C] para sacar.\n");
      }
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "multiplier") == 0) {
      printf("Multiplicador atual: %.2fx\n", aviator_message.value);
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "explode") == 0) {
      current_game_phase = WAIT;
      printf("Aviãozinho explodiu em: %.2fx\n", aviator_message.value);
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "payout") == 0) {
      printf("Você sacou em %.2fx e ganhou R$ %.2f!\n",
             aviator_message.value / current_bet, aviator_message.value);
      printf("Profit atual: R$ %.2f\n", aviator_message.player_profit);
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "profit") == 0) {
      if (aviator_message.player_id != 0) {
        // Individual profit update
        if (has_bet_this_round && current_game_phase == WAIT) {
          // This means we lost our bet
          printf("Você perdeu R$ %.2f. Tente novamente na próxima rodada! "
                 "Aviãozinho tá pagando :)\n",
                 current_bet);
        }
        printf("Profit atual: R$ %.2f\n", aviator_message.player_profit);
      } else {
        // House profit update
        printf("Profit da casa: R$ %.2f\n", aviator_message.house_profit);
      }
      fflush(stdout);

    } else if (strcmp(aviator_message.type, "bye") == 0) {
      printf("O servidor caiu, mas sua esperança pode continuar de pé. Até "
             "breve!\n");
      client_running = 0;
      break;
    }
  }
}

// Função para exibir erros genéricos e finalizar a execução do programa sem
// tratamentos
void endWithErrorMessage(const char *message) {
  perror(message);
  exit(EXIT_FAILURE);
}

// Função para indicar o servidor que o cliente não irá mais jogar
// aviator_msg message;
void shutdown_client() {
  aviator_msg aviator_message;
  printf("\nAposte com responsabilidade. A plataforma é nova e tá com horário "
         "bugado. Volte logo, %s.\n",
         nickname);

  // Send bye message to server
  memset(&aviator_message, 0, sizeof(aviator_msg));
  strcpy(aviator_message.type, "bye");
  send(client_socket, &aviator_message, sizeof(aviator_msg), 0);

  client_running = 0;
  close(client_socket);
  exit(0);
}
