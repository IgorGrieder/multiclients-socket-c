#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define STR_LEN 11
#define PLAYERS_MAX 10
#define NICKNAME 13
#define RESP_LEN 256

typedef struct {
  int32_t player_id;
  float value;
  char type[STR_LEN];
  float player_profit;
  float house_profit;
} aviator_msg;

typedef struct {
  int socket_conn;
  int player_id;
  char nickname[NICKNAME + 1];
  float current_bet;
  float profit;
  int has_bet;
  int has_cashed_out;
  int active;
  pthread_t client_thread;
} client_info;

// Variáveis globais para acompanhamento de estados
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
client_info clients[PLAYERS_MAX];
float house_profit = 0.0;
int server_running = 1;
int current_players = 0;
int is_bet_phase = 0;
int is_flight_phase = 0;
float mult = 1;
float explosion = 0;

// Hoisting de funções
void endWithErrorMessage(const char *message);
void *handle_client(void *arg);
void *handle_game(void *arg);
void close_all_connections(int server_socket);
float game_explosion(float total_bet);
void send_all_message(aviator_msg *message);

int main(int argc, char *argv[]) {
  int server_socket;
  int client_socket_conn;
  struct sockaddr_in server_addr_ipv4;
  struct sockaddr_in6 server_addr_ipv6;
  void *addr_ptr;
  socklen_t addr_len;
  pthread_t game_thread;
  aviator_msg aviator_message;
  int user_id = 1;
  int is_IPv4 = 0;
  int port;

  // Caso o numero de argumentos passados ao processo não seja condizente com
  // o necessário deve-se encerrar o programa
  if (argc != 3) {
    endWithErrorMessage("Invalid number of arguments");
  }

  // Indicando o protocolo a ser utilizado no programa
  if (strcmp(argv[1], "v4") == 0) {
    is_IPv4 = 1;
  } else if (strcmp(argv[1], "v6") == 0) {
    is_IPv4 = 0;
  } else {
    endWithErrorMessage("Please choose an ip protocol(v4 or v6)");
  }

  // Convertendo a porta a ser utilizada
  port = atoi(argv[2]);
  // Checando se a porta está no range de 16 bits
  if (port <= 0 || port > 65535) {
    endWithErrorMessage("Invalid port");
  }

  if (is_IPv4 == 1) {
    // Socket IPv4
    memset(&server_addr_ipv4, 0, sizeof(server_addr_ipv4));
    server_addr_ipv4.sin_family = AF_INET;
    server_addr_ipv4.sin_port = htons(port);
    server_addr_ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_len = sizeof(server_addr_ipv4);
    addr_ptr = &server_addr_ipv4;

    // Criando socket
    server_socket = socket(server_addr_ipv4.sin_family, SOCK_STREAM, 0);
    if (server_socket < 0) {
      endWithErrorMessage("Error creating ipv4 socket");
    }

    // Acoplando o socket
    int bindCheck =
        bind(server_socket, (struct sockaddr *)&server_addr_ipv4, addr_len);
    if (bindCheck < 0) {
      endWithErrorMessage("Error binding ipv4 socket");
    }

  } else {
    // Socket IPv6
    memset(&server_addr_ipv6, 0, sizeof(server_addr_ipv6));
    server_addr_ipv6.sin6_family = AF_INET6;
    server_addr_ipv6.sin6_port = htons(port);
    server_addr_ipv6.sin6_addr = in6addr_any;
    addr_len = sizeof(server_addr_ipv6);
    addr_ptr = &server_addr_ipv6;

    // Criando socket
    server_socket = socket(server_addr_ipv6.sin6_family, SOCK_STREAM, 0);
    if (server_socket < 0) {
      endWithErrorMessage("Error creating ipv6 socket");
    }

    // Acoplando o socket
    int bindCheck =
        bind(server_socket, (struct sockaddr *)&server_addr_ipv6, addr_len);
    if (bindCheck < 0) {
      endWithErrorMessage("Error binding ipv6 socket");
    }
  }

  int checkListen = listen(server_socket, 1);
  if (checkListen < 0) {
    endWithErrorMessage("Error while listening in the socket");
  }

  // Separando a execução do jogo em outra thread, mantendo a main thread de
  // sem locks
  pthread_create(&game_thread, NULL, handle_game, NULL);

  while (server_running) {

    client_socket_conn = accept(server_socket, addr_ptr, &addr_len);
    if (client_socket_conn < 0) {
      endWithErrorMessage("Failed to acccept client socket connection");
    }

    // Procedimento para checar se o limite de jogadores foi ultrapassado
    pthread_mutex_lock(&lock);
    int available_idx = -1;
    for (int i = 0; i < PLAYERS_MAX; i++) {
      if (!clients[i].active) {
        available_idx = i;
        break;
      }
    }

    if (available_idx != -1) {
      clients[available_idx].socket_conn = client_socket_conn;
      clients[available_idx].player_id = current_players++;
      clients[available_idx].profit = 0;
      clients[available_idx].current_bet = 0;
      clients[available_idx].has_bet = 0;
      clients[available_idx].has_cashed_out = 0;
      clients[available_idx].active = 1;

      // Invocação da função do jogo, sem bloquear a main thread
      pthread_create(&clients[available_idx].client_thread, NULL, handle_client,
                     &clients[available_idx]);
      printf("Cliente conectado.\n");

    } else {
      // Fechando a conexão por falta de espaço no jogo
      memset(&aviator_message, 0, sizeof(aviator_msg));
      snprintf(aviator_message.type, STR_LEN, "bye");

      printf("Limites de conexões ultrapassado.\n");
      close(client_socket_conn);
    }
    pthread_mutex_unlock(&lock);
  }

  // Fechando as conexões gerais
  close_all_connections(server_socket);

  return EXIT_SUCCESS;
}

// Função de handler para a execução do jogo ser em uma outra thread
void *handle_game(void *arg) {
  // TO-DO
  return NULL;
}

// Função de handler para conexões de clientes
void *handle_client(void *arg) {
  client_info *client = (client_info *)arg;
  return NULL;
}

// Função para fechar todas as conexões
void close_all_connections(int server_socket) {
  // TO-DO
}

// Função para exibir erros genéricos e finalizar a execução do programa sem
// tratamentos
void endWithErrorMessage(const char *message) {
  perror(message);
  exit(EXIT_FAILURE);
}

float game_explosion(float total_bet) {
  if (current_players == 0)
    return 2.0;

  float k = 100.0;
  float alpha = 0.5;

  float explosion = 1.0 + pow((current_players + total_bet / k), alpha);
  return explosion;
}

// Função para enviar uma mensagem para todos os jogadores disponíveis
// atualmente
void send_all_message(aviator_msg *message) {
  // Acredito necessitar de um lock para não ocorrer de um cliente deixar de
  // estar ativo no instante em que a mensagem estaria sendo direcionada para
  // ele
  pthread_mutex_lock(&lock);
  for (int i = 0; i < PLAYERS_MAX; i++) {
    if (clients[i].active) {
      send(clients[i].socket_conn, message, sizeof(aviator_msg), 0);
    }
  }
  pthread_mutex_unlock(&lock);
}
