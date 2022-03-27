#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define handle_error(text) \
  {                        \
    perror(text);          \
    exit(EXIT_FAILURE);    \
  }

int port, nr_problema;

void read_info(int sd);
void write_info(int sd, char text[]);
void get_problem(int sd);
void get_results(int sd);
void send_source_file(int sd, int numar_problema);

int read_first_command(int sd, char comanda[]);
int read_command(int sd, char comanda[]);
void read_last_exit(int sd, char comanda[]);
int out_of_time(int sd);

int main(int argc, char *argv[])
{
  int sd, nr_caractere;
  char comanda[50];
  struct sockaddr_in server;

  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  /* stabilim portul */
  port = atoi(argv[2]);

  /* cream socketul */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    handle_error("Eroare la socket()\n");

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons(port);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  read_info(sd);
  read_info(sd);
  read_info(sd);

  int comanda_corecta;
  int continua = 1;
  //0 - comanda incorecta
  //1 - comanda corecta
  //2 - ultima comanda

  continua = read_first_command(sd, comanda);

  if (continua != 0)
  {
    struct timeval tv, timer;
    int return_sel, problem_time;
    fd_set rfds;

    while (continua != 0)
    {
      if (read(sd, &problem_time, sizeof(int)) < 0)
        handle_error("[client] Eroare la read() de la server");

      //timer
      timer.tv_sec = 0;
      timer.tv_usec = 0;

      //select declaration
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      FD_SET(0, &rfds);

      return_sel = select(1, &rfds, NULL, NULL, &tv);
      if (return_sel == -1)
        handle_error("[client] Eroare la select()");

      timer.tv_sec++;

      while (return_sel == 0 && timer.tv_sec <= problem_time)
      {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_SET(0, &rfds);

        return_sel = select(1, &rfds, NULL, NULL, &tv);
        if (return_sel == -1)
          handle_error("[client] Eroare la select()");

        timer.tv_sec++;
      }

      if (timer.tv_sec <= problem_time)
      {
        continua = read_command(sd, comanda);
      }

      else
      {
        continua=out_of_time(sd);
      }
    }
  }
  /* inchidem conexiunea, am terminat */
  close(sd);
}

int read_first_command(int sd, char comanda[])
{
  int comanda_corecta;
  while (1)
  {
    memset(comanda, 0, 50);
    if (read(0, comanda, 50) < 0)
      handle_error("[client] Eroare la read() de la tastatura");
    comanda[strlen(comanda) - 1] = '\0';

    write_info(sd, comanda);

    if (strcmp(comanda, "exit") == 0)
    {
      get_results(sd);
      read_info(sd);
      close(sd);
      return 0;
    }

    if (read(sd, &comanda_corecta, sizeof(int)) < 0)
      handle_error("[client] Eroare la read() de la server");

    if (comanda_corecta == 1)
    {
      get_problem(sd);
      return 2;
    }
    else
      read_info(sd);
  }
}

int read_command(int sd, char comanda[])
{
  int comanda_corecta;
  memset(comanda, 0, 50);
  if (read(0, comanda, 50) < 0)
    handle_error("[client] Eroare la read() de la tastatura");
  comanda[strlen(comanda) - 1] = '\0';

  write_info(sd, comanda);

  if (strcmp(comanda, "exit") == 0)
  {
    get_results(sd);
    read_info(sd);
    close(sd);
    return 0;
  }

  if (read(sd, &comanda_corecta, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server");

  if (comanda_corecta == 1)
  {
    if (strstr(comanda, "upload") != 0)
      send_source_file(sd, nr_problema);
    get_problem(sd);
    return 1;
  }
  else if (comanda_corecta == 0)
  {
    read_info(sd);
    return 1;
  }
  else if (comanda_corecta == 2)
  {
    if (strstr(comanda, "upload") != 0)
      send_source_file(sd, nr_problema);
    read_info(sd);

    read_last_exit(sd, comanda);

    return 0;
  }
}

void read_last_exit(int sd, char comanda[])
{
  int comanda_corecta;
  while (1)
  {
    memset(comanda, 0, 50);
    if (read(0, comanda, 50) < 0)
      handle_error("[client] Eroare la read() de la tastatura");
    comanda[strlen(comanda) - 1] = '\0';

    write_info(sd, comanda);

    if (strcmp(comanda, "exit") == 0)
    {
      get_results(sd);
      read_info(sd);
      close(sd);
      break;
    }

    if (read(sd, &comanda_corecta, sizeof(int)) < 0)
      handle_error("[client] Eroare la read() de la server");

    if (comanda_corecta == 0)
      read_info(sd);
  }
}

int out_of_time(int sd)
{
  int next;
  char comanda[50];
  if (read(sd, &next, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server");

  if (next == 1)
  {
    read_info(sd);
    get_problem(sd);
    return 1;
  }
  else
  {
    read_info(sd);
    read_last_exit(sd, comanda);
    return 0;
  }
}

void get_results(int sd)
{
  int current = 0, fd, nr_caractere, rc;
  char results[200];
  char path[30];

  memset(path, 0, 30);
  strcpy(path, "Results");
  strcat(path, ".txt");
  path[strlen(path)] = '\0';
  remove(path);

  fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0777);

  if (fd == -1)
    handle_error("[client] Eroare la open() fisier rezultate");

  if (read(sd, &nr_caractere, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server fisierul cu rezultate");

  while (current < nr_caractere)
  {
    rc = read(sd, results, nr_caractere);

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[client] Eroare la read() de la server fisierul cu rezultate");

    results[nr_caractere] = '\0';
    write(fd, results, rc);

    current += rc;
  }
}
void get_problem(int sd)
{
  nr_problema++;
  char text[2000];
  int current = 0, rc, nr_caractere;
  if (read(sd, &nr_caractere, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server");
  printf("\n");
  while (current < nr_caractere)
  {
    rc = read(sd, text, nr_caractere);

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[client] Eroare la read() de la server");

    text[nr_caractere] = '\0';
    printf("%s", text);

    current += rc;
  }

  int fd;
  char path[25];
  memset(path, 0, 100);
  strcpy(path, "Teste_Problema_");
  char nr[4];
  sprintf(nr, "%d", nr_problema);
  strcat(path, nr);
  strcat(path, ".txt");
  path[strlen(path)] = '\0';
  remove(path);

  fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0777);

  if (fd == -1)
    handle_error("[client] Eroare la open() fisier teste");

  if (read(sd, &nr_caractere, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server");

  char teste[2000];
  current = 0;
  while (current < nr_caractere)
  {
    rc = read(sd, teste, nr_caractere);

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[client] Eroare la read() de la server");

    teste[nr_caractere] = '\0';
    write(fd, teste, rc);

    current += rc;
  }
}

void read_info(int sd)
{
  char text[2000];
  int nr_caractere;
  if (read(sd, &nr_caractere, sizeof(int)) < 0)
    handle_error("[client] Eroare la read() de la server");

  int current = 0;
  int rc;
  while (current < nr_caractere)
  {
    rc = read(sd, text, nr_caractere);

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[client] Eroare la read() de la server");

    text[nr_caractere] = '\0';
    printf("%s", text);

    current += rc;
  }
}

void write_info(int sd, char text[])
{
  int nr_caractere = strlen(text);
  if (write(sd, &nr_caractere, sizeof(int)) < 0)
    handle_error("[client] Eroare la write() spre server");

  if (write(sd, text, nr_caractere) < 0)
    handle_error("[client] Eroare la write() spre server");
}

void send_source_file(int sd, int numar_problema)
{
  char nr[3];
  sprintf(nr, "%d", numar_problema);
  char name[50];
  memset(name, 0, 50);

  strcpy(name, "problema\0");
  strcat(name, nr);
  strcat(name, ".c");

  int fd_sursa;
  fd_sursa = open(name, O_CREAT | O_RDWR, 0777);
  if (fd_sursa == -1)
    handle_error("[server]Eroare la deschidere fisier problema");

  int total_caractere = 0, rc;
  char sursa[2000];
  while (1)
  {
    rc = read(fd_sursa, sursa, sizeof(sursa));

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[server]Eroare la read() din fisier teste");

    total_caractere += rc;
  }

  lseek(fd_sursa, 0, SEEK_SET);

  if (write(sd, &total_caractere, sizeof(int)) < 0)
    handle_error("[server][send]]Eroare la write()");

  while (1)
  {
    rc = read(fd_sursa, name, sizeof(name));

    if (rc == 0)
      break;

    if (rc < 0)
      handle_error("[server]Eroare la read() din fisier problema");

    write(sd, name, rc);
  }
}
