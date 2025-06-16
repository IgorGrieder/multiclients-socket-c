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

#define MSG_SIZE 256

typedef enum {
  MSG_REQUEST,
  MSG_RESPONSE,
  MSG_RESULT,
  MSG_PLAY_AGAIN_REQUEST,
  MSG_PLAY_AGAIN_RESPONSE,
  MSG_ERROR,
  MSG_END
} MessageType;

typedef struct {
  int type;
  int client_action;
  int server_action;
  int result;
  int client_wins;
  int server_wins;
  char message[MSG_SIZE];
} GameMessage;

// Hoisting de funções
void endWithErrorMessage(const char *message);
void showUserPlayOptions(GameMessage *msg);
void resetGame(int *server_wins, int *client_wins);
int randNum();
void showUserResult(GameMessage *game_message, int server_action,
                    int client_action, int client_socket_conn, char *actions[],
                    int result, int is_draw);
int checkGameWillContinue(GameMessage *game_message, int client_socket_conn);
void showUserEndGameMessage(GameMessage *game_message, int client_socket_conn,
                            int client_wins, int server_wins);
void resolveUserErrorInput(GameMessage *game_message, int client_socket_conn,
                           int is_error_play, int *client_act);

int main(int argc, char *argv[]) {
  int server_socket;
  int client_socket_conn;
  struct sockaddr_in server_addr_ipv4;
  struct sockaddr_in6 server_addr_ipv6;
  void *addr_ptr;
  socklen_t addr_len;

  GameMessage game_message;

  int client_wins = 0;
  int server_wins = 0;
  int is_IPv4 = 0;
  int port;
  int stop_game = 1;

  // Tabela do jogo [cliente][servidor]
  // Retornos 1: vitoria cliente, 0: empate, -1: vitoria servidor
  int win_conditions[5][5] = {
      // Nuclear Attack|Intercept Attack|Cyber Attack|Drone Strike|Bio Attack
      {-1, 0, 1, 1, 0}, // Nuclear Attack
      {1, -1, 0, 0, 1}, // Intercept Attack
      {0, 1, -1, 1, 0}, // Cyber Attack
      {0, 1, 0, -1, 1}, // Drone Strike
      {1, 0, 1, 0, -1}  // Bio Attack
  };

  char *actions[] = {"Nuclear Attack", "Intercept Attack", "Cyber Attack",
                     "Drone Strike", "Bio Attack"};

  srand(time(NULL));

  // rand() % 5
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
    printf("Servidor iniciado em modo %s na porta %d. Aguardando conexão...\n",
           is_IPv4 ? "IPv4" : "IPv6", port);

    stop_game = 1;

    // Sempre aceitar uma nova conexão até o servidor falhar
    client_socket_conn = accept(server_socket, addr_ptr, &addr_len);
    if (client_socket_conn < 0) {
      endWithErrorMessage("Falha ao aceitar a conexão do cliente");
    }

    printf("Cliente conectado.\n");

    while (stop_game) {
      // Enviar mensagem ao cliente mostrando as opções do jogo na primeira
      // iteração
      showUserPlayOptions(&game_message);

      send(client_socket_conn, &game_message, sizeof(game_message), 0);

      recv(client_socket_conn, &game_message, sizeof(game_message), 0);

      printf("Cliente escolheu %d\n", game_message.client_action);

      switch (game_message.type) {
      case MSG_RESPONSE: {
        int client_action = game_message.client_action;
        // Checando possível erro caso o usuário tenha inserido um valor > 4
        if (client_action > 4 || client_action < 0) {
          // Enviando uma mensagem de erro e pedindo para que o usuário envie
          // outro valor
          resolveUserErrorInput(&game_message, client_socket_conn, 1,
                                &client_action);
        }

        int server_action = randNum();
        printf("Servidor escolheu aleatoriamente %d.\n", server_action);
        int result = win_conditions[client_action][server_action];

        if (result == 1) {
          client_wins++;
        } else if (result == 0) {
          server_wins++;
        } else {
          printf("Jogo empatado.\n");
          showUserResult(&game_message, server_action, client_action,
                         client_socket_conn, actions, result, 1);
          continue;
        }

        printf("Placar atualizado: Cliente %d x %d Servidor.\n", client_wins,
               server_wins);

        showUserResult(&game_message, server_action, client_action,
                       client_socket_conn, actions, result, 0);

        // Após mostrar o resultado perguntar se o cliente deseja jogar outra
        // rodada
        // 1 -> Jogo não irá parar | 0 -> Jogo irá parar
        int is_going_to_continue =
            checkGameWillContinue(&game_message, client_socket_conn);

        // Checando a resposta para continuar jogando do cliente
        if (is_going_to_continue == 0) {
          stop_game = 0;

          showUserEndGameMessage(&game_message, client_socket_conn, client_wins,
                                 server_wins);
        } else {
          printf("Cliente deseja jogar novamente.\n");
        }
        break;
      }
      default:
        break;
      }
    }

    // Fechando a conexão atual para estabelecer outra e resetando o jogo
    close(client_socket_conn);
    resetGame(&server_wins, &client_wins);
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

// Função para exibir uma mensagem ao usuário pedindo um novo input
void resolveUserErrorInput(GameMessage *game_message, int client_socket_conn,
                           int is_error_play, int *client_act) {
  while (1) {
    if (is_error_play) {
      printf("Erro: opção inválida de jogada.\n");
    } else {
      printf("Erro: resposta inválida para jogar novamente.\n");
    }

    memset(game_message, 0, sizeof(*game_message));
    game_message->type = MSG_ERROR;
    snprintf(game_message->message, MSG_SIZE,
             "Por favor, selecione um valor de 0 a %d\n",
             is_error_play == 1 ? 4 : 1);
    send(client_socket_conn, game_message, sizeof(*game_message), 0);

    recv(client_socket_conn, game_message, sizeof(*game_message), 0);

    int client_action = game_message->client_action;
    *client_act = client_action;

    // Checagem se a mensagem deve ser enviada novamente ou não até que o
    // cliente envie uma mensagem processável
    if (is_error_play && (client_action <= 4 && client_action >= 0)) {
      break;
    } else if (!is_error_play && (client_action <= 1 && client_action >= 0)) {
      break;
    }
  }
}

// Função para exibir as jogadas para o usuario
void showUserPlayOptions(GameMessage *game_message) {
  printf("Apresentando as opções para o cliente.\n");
  memset(game_message, 0, sizeof(*game_message));
  game_message->type = MSG_REQUEST;
  snprintf(game_message->message, MSG_SIZE,
           "\nEscolha sua jogada:\n\n"
           "0 - Nuclear Attack\n"
           "1 - Intercept Attack\n"
           "2 - Cyber Attack\n"
           "3 - Drone Strike\n"
           "4 - Bio Attack\n");
}

// Função para resetar o jogo e aguardar novas conexões
void resetGame(int *server_wins, int *client_wins) {
  *server_wins = 0;
  *client_wins = 0;
  printf("Encerrando conexão.\n");
  printf("Cliente desconectado.\n");
}

// Função para gerar números aleatórios
int randNum() { return rand() % 5; }

// Função para mostrar o resultado da rodada ao usuário
void showUserResult(GameMessage *game_message, int server_action,
                    int client_action, int client_socket_conn, char *actions[],
                    int result, int is_draw) {
  memset(game_message, 0, sizeof(*game_message));
  game_message->type = MSG_RESULT;
  game_message->result = result;

  if (is_draw) {
    snprintf(game_message->message, MSG_SIZE,
             "\nVocê escolheu: %s\n"
             "Servidor escolheu: %s\n"
             "Resultado: Empate!\n",
             actions[client_action], actions[server_action]);
  } else {
    snprintf(game_message->message, MSG_SIZE,
             "\nVocê escolheu: %s\n"
             "Servidor escolheu: %s\n"
             "Resultado: %s\n",
             actions[client_action], actions[server_action],
             result == 1 ? "Vitória!" : "Derrota!");
  }
  send(client_socket_conn, game_message, sizeof(*game_message), 0);
}

// Função para confirmar se o cliente irá continuar jogando ou não
int checkGameWillContinue(GameMessage *game_message, int client_socket_conn) {
  printf("Perguntando se o cliente deseja jogar novamente.\n");

  memset(game_message, 0, sizeof(*game_message));
  game_message->type = MSG_PLAY_AGAIN_REQUEST;
  snprintf(game_message->message, MSG_SIZE,
           "\nDeseja jogar novamente?\n"
           "1 - Sim\n"
           "0 - Não\n");
  send(client_socket_conn, game_message, sizeof(*game_message), 0);

  // Chamada blocking de recv
  recv(client_socket_conn, game_message, sizeof(*game_message), 0);

  int action = game_message->client_action;

  if (action < 0 || action > 1) {
    resolveUserErrorInput(game_message, client_socket_conn, 0, &action);
  }

  return action;
}

// Função para mostrar ao usuário o resultado final do jogo
void showUserEndGameMessage(GameMessage *game_message, int client_socket_conn,
                            int client_wins, int server_wins) {
  printf("Cliente não deseja jogar novamente.\n");
  memset(game_message, 0, sizeof(*game_message));
  game_message->type = MSG_END;
  snprintf(game_message->message, MSG_SIZE,
           "\nFim de jogo!\n"
           "Placar final: Você %d x %d Servidor\n"
           "Obrigado por jogar!\n",
           client_wins, server_wins);
  printf("Enviando placar final.\n");
  send(client_socket_conn, game_message, sizeof(*game_message), 0);
}
