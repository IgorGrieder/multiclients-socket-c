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
float house_profit = 0;
int server_running = 1;
int is_bet_phase = 0;
int is_flight_phase = 0;
float mult = 1;
float explosion = 0;
float countdown = 10;

// Hoisting de funções
void endWithErrorMessage(const char *message);
void *handle_client(void *arg);
void *handle_game(void *arg);
void close_all_connections(int server_socket);
float game_explosion(int num_of_players, float total_bet);
void send_all_message(aviator_msg *message);
void start_new_game();
void remove_client(int player_id);
void reset_past_play(int *active_players, float *total_bet);
void calculate_end_game();

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
      clients[available_idx].player_id = user_id;
      clients[available_idx].profit = 0;
      clients[available_idx].current_bet = 0;
      clients[available_idx].has_bet = 0;
      clients[available_idx].has_cashed_out = 0;
      clients[available_idx].active = 1;

      // Invocação da função do jogo, sem bloquear a thread de conexões
      pthread_create(&clients[available_idx].client_thread, NULL, handle_client,
                     &clients[available_idx]);
      printf("Client conected.\n");

      user_id++;
    } else {
      // Fechando a conexão por falta de espaço no jogo
      memset(&aviator_message, 0, sizeof(aviator_msg));
      snprintf(aviator_message.type, STR_LEN, "bye");

      printf("Max number of players reached.\n");
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
  aviator_msg aviator_message;

  while (server_running) {
    // Aguardar pelo menos um cliente se conectar para de fato a partida
    // iniciar
    int client_num = 0;
    while (!client_num) {
      pthread_mutex_lock(&lock);
      for (int i = 0; i < PLAYERS_MAX; i++) {
        if (clients[i].active) {
          break;
        }
      }
      pthread_mutex_unlock(&lock);
    }

    // Partida irá começar
    start_new_game();

    float total_bet = 0;
    int active_players = 0;
    reset_past_play(&active_players, &total_bet);

    // Fechando as apostas e comunicando aos clientes
    memset(&aviator_message, 0, sizeof(aviator_msg));
    strcpy(aviator_message.type, "closed");
    send_all_message(&aviator_message);

    // Considerando oficialmente o começo da fase de voo
    is_bet_phase = 0;
    is_flight_phase = 1;

    float explosion_limit = game_explosion(active_players, total_bet);

    while (mult < explosion_limit) {
      memset(&aviator_message, 0, sizeof(aviator_msg));
      strcpy(aviator_message.type, "multiplier");
      aviator_message.value = mult;
      send_all_message(&aviator_message);

      // TO-DO adicionar log aqui do multiplicador
      usleep(100000);
      mult += 0.01;
    }

    // Informando aos clientes a explosão do avião
    is_flight_phase = 0;
    memset(&aviator_message, 0, sizeof(aviator_msg));
    strcpy(aviator_message.type, "explode");
    aviator_message.value = explosion_limit;
    send_all_message(&aviator_message);
    // TO-DO log da explosao deve ser feito

    calculate_end_game();
  }
  return NULL;
}

// Função para fazer todos os cálculos referentes ao fim da rodada
void calculate_end_game() {
  aviator_msg aviator_message;

  // Processar perdas dos jogadores que não sacaram
  pthread_mutex_lock(&lock);
  for (int i = 0; i < PLAYERS_MAX; i++) {
    // Caso o jogador tenha feito uma aposta e não tenha realizado cashout
    // deverá ser calculado a sua perda e o lucro da casa
    if (clients[i].active && clients[i].has_bet && !clients[i].has_cashed_out) {
      clients[i].profit -= clients[i].current_bet;
      house_profit += clients[i].current_bet;

      memset(&aviator_message, 0, sizeof(aviator_msg));
      strcpy(aviator_message.type, "profit");
      aviator_message.player_id = clients[i].player_id,
      aviator_message.value = clients[i].profit,
      aviator_message.player_profit = clients[i].profit,
      aviator_message.house_profit = house_profit;

      // TO-DO Log do profit
      send(clients[i].socket_conn, &aviator_message, sizeof(aviator_msg), 0);
    }
  }
  pthread_mutex_unlock(&lock);
}

// Função de handler para conexões de clientes
void *handle_client(void *arg) {
  client_info *client = (client_info *)arg;
  aviator_msg aviator_message;

  while (client->active && server_running) {
    // Enviar ao cliente que a rodada começou caso ele não possua apostas
    // ainda
    if (is_bet_phase && !client->has_bet) {
      memset(&aviator_message, 0, sizeof(aviator_msg));
      aviator_message.value = countdown;
      strcpy(aviator_message.type, "start");

      send(client->socket_conn, &aviator_message, sizeof(aviator_msg), 0);
    }

    // Esperando resposta para apostas do cliente
    recv(client->socket_conn, &aviator_message, sizeof(aviator_msg), 0);

    if (strcmp(aviator_message.type, "bet") == 0 && is_bet_phase) {
      // Checando caso o cliente já tenha feito uma aposta na rodada
      if (client->has_bet) {
        continue;
      }

      client->current_bet = aviator_message.value;
      client->has_bet = 1;
      client->has_cashed_out = 0;

      // TO-DO Logar aposta do cliente
    } else if (strcmp(aviator_message.type, "cashout") == 0 &&
               is_flight_phase) {
      // Checando se o cliente já não realizou um cashout
      if (client->has_cashed_out) {
        continue;
      }

      client->has_cashed_out = 1;
      // Calculando o ganho pelo cliente
      float payout = client->current_bet * mult;
      float transaction_balance = payout - client->current_bet;

      client->profit += transaction_balance;
      house_profit -= transaction_balance;

      // To-DO log no servidor do cashout

      memset(&aviator_message, 0, sizeof(aviator_msg));
      strcpy(aviator_message.type, "payout");
      aviator_message.value = payout;
      aviator_message.player_id = client->player_id;
      aviator_message.player_profit = client->profit;
      aviator_message.house_profit = house_profit;
      send(client->socket_conn, &aviator_message, sizeof(aviator_msg), 0);

      // TO-DO Log do payout no servidor

      // Enviar profit atualizado do jogador
      strcpy(aviator_message.type, "profit");
      send(client->socket_conn, &aviator_message, sizeof(aviator_msg), 0);

      // TO-DO log profit servdior
    } else if (strcmp(aviator_message.type, "bye") == 0) {
      remove_client(client->player_id);
      break;
    }
  }

  return NULL;
}

// Função para remover um client do jogo, utilizando a flag de active
void remove_client(int player_id) {
  pthread_mutex_lock(&lock);
  for (int i = 0; i < PLAYERS_MAX; i++) {
    if (clients[i].player_id == player_id) {
      // TO-DO log de evento de remocao

      clients[i].active = 0;
      clients[i].player_id = 0;
      clients[i].profit = 0;
      clients[i].current_bet = 0;
      clients[i].has_bet = 0;
      clients[i].has_cashed_out = 0;
      close(clients[i].socket_conn);

      break;
    }
  }
  pthread_mutex_unlock(&lock);
}

void reset_past_play(int *active_players, float *total_bet) {
  pthread_mutex_lock(&lock);
  for (int i = 0; i < PLAYERS_MAX; i++) {
    if (clients[i].active) {
      clients[i].has_bet = 0;
      clients[i].has_cashed_out = 0;
      clients[i].current_bet = 0;
      (*active_players)++;
      *total_bet += clients[i].current_bet;
    }
  }
  pthread_mutex_unlock(&lock);
}

// Função para preparar o inicio de um novo jogo
void start_new_game() {
  is_bet_phase = 1;
  is_flight_phase = 0;
  countdown = 10;

  // TO-DO colocar um log para o servidor
  while (countdown > 0) {
    // Esperar 1 segundo para a nova iteração
    sleep(1);
    countdown--;
  }

  // TO-DO colocar log para inicio da partida
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

float game_explosion(int num_of_players, float total_bet) {
  float constant = 0.01;
  float gamma = 0.5;

  return pow((1.0 + num_of_players + total_bet * constant), gamma);
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
