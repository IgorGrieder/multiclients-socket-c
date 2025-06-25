#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define STR_LEN 11

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
  int server_socket;
  int client_socket_conn;
  struct sockaddr_in server_addr_ipv4;
  struct sockaddr_in6 server_addr_ipv6;
  void *addr_ptr;
  socklen_t addr_len;

  aviator_msg aviator_message;
  int current_players = 0;

  int client_wins = 0;
  int server_wins = 0;
  int is_IPv4 = 0;
  int port;
  int stop_game = 1;

  // Caso o numero de argumentos passados ao processo não seja condizente com
  // o necessário deve-se encerrar o programa
  if (argc != 3) {
    endWithErrorMessage("Insira corretamente a quantidade de argumentos para o "
                        "sevridor \n <Versão Protocolo IP> <Porta> ");
  }

  // Indicando o protocolo a ser utilizado no programa
  if (strcmp(argv[1], "v4") == 0) {
    is_IPv4 = 1;
  } else if (strcmp(argv[1], "v6") == 0) {
    is_IPv4 = 0;
  } else {
    endWithErrorMessage("Insira um protocolo válido (v4 ou v6)");
  }

  // Convertendo a porta a ser utilizada
  port = atoi(argv[2]);
  // Checando se a porta está no range de 16 bits
  if (port <= 0 || port > 65535) {
    endWithErrorMessage("Porta inválida");
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
      endWithErrorMessage("Erro ao criar socket IPv4 server side");
    }

    // Acoplando o socket
    int bindCheck =
        bind(server_socket, (struct sockaddr *)&server_addr_ipv4, addr_len);
    if (bindCheck < 0) {
      endWithErrorMessage("Erro ao executar o bind do socket IPv4");
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
      endWithErrorMessage("Erro ao criar socket IPv6 server side");
    }

    // Acoplando o socket
    int bindCheck =
        bind(server_socket, (struct sockaddr *)&server_addr_ipv6, addr_len);
    if (bindCheck < 0) {
      endWithErrorMessage("Erro ao executar o bind do socket IPv6");
    }
  }

  int checkListen = listen(server_socket, 1);
  if (checkListen < 0) {
    endWithErrorMessage("Erro ao escutar no servidor\n");
  }

  // Loop infinito para que caso o cliente feche a conexão o servidor já esteja
  // preparado para aceitar novas conexões
  while (1) {

    // Sempre aceitar uma nova conexão até o servidor falhar
    client_socket_conn = accept(server_socket, addr_ptr, &addr_len);
    if (client_socket_conn < 0) {
      endWithErrorMessage("Falha ao aceitar a conexão do cliente");
    }

    printf("Cliente conectado.\n");

    close(client_socket_conn);
  }

  // Fechando as conexões gerais
  close(client_socket_conn);
  close(server_socket);

  return EXIT_SUCCESS;
}

// Função para exibir erros genéricos e finalizar a execução do programa sem
// tratamentos
void endWithErrorMessage(const char *message) {
  perror(message);
  exit(EXIT_FAILURE);
}
