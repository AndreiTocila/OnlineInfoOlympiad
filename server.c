#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>

#define handle_error(text)  \
    {                       \
        perror(text);       \
        exit(EXIT_FAILURE); \
    }
#define print_error_and_continue(text) \
    {                                  \
        perror(text);                  \
        continue;                      \
    }

//portul la care vom astepta conexiuni
#define PORT 3852

//strucura pentru thread
typedef struct threadINFO
{
    pthread_t thread_id;
    int client_id;
} threadINFO;

int current_connection = 0, maximum_connection, nr_prob_disp, nr_prob_comp, *comp_prob, *problems_time;
double avg_time;

static void *thread_function(void *arg);
void raspunde(void *arg);
void send_information(int fd, char text[]);
void recv_information(int fd, char raspuns[]);
char command_type(char comanda[]);
void send_problem(int fd, int numar_problema);
void set_config();
int search_info(char *info, char *text);
int search_array(int x[], int nr_elemente, int element);
void write_result(int id_client, int nr_problema, int punctaj);
void write_all_results_0(int id_client, int nr_problema);
void send_results_file(int fd);
int get_source_file(int fd, int nr_problema);
int compile (int fd,int nr_problema);
int run_program (int fd, int nr_problema, int nr_test);
int compute_points(double avg, int corecte, struct timeval timer,int problem_time);


//model TCP luat de pe site-ul materiei: https://profs.info.uaic.ro/~computernetworks/files/NetEx/S12/ServerConcThread/servTcpConcTh2.c

int main()
{
    //configurarea sesiunii
    set_config();
    //declaratii
    struct sockaddr_in server;
    struct sockaddr_in from;
    int socket_server;
    int id;
    id = 0;
    pthread_t th[100];

    //creare socket
    if ((socket_server = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        handle_error("[server]Eroare la socket().");

    //structura server
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    //bind socket_server la server
    if (bind(socket_server, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
        handle_error("[server] Eroare la bind().");

    //serverul asculta sa vada daca un client incearca sa se conecteze
    if (listen(socket_server, 4) == -1)
        handle_error("[server] Eroare la listen().")

            while (1)
        {
            int client;
            threadINFO *td;
            int lenght = sizeof(from);

            printf("[server]Asteptam la portul %d...\n", PORT);
            fflush(stdout);

            if ((client = accept(socket_server, (struct sockaddr *)&from, &lenght)) < 0)
                print_error_and_continue("[server] Eroare la accept()");

            td = (struct threadINFO *)malloc(sizeof(struct threadINFO));

            td->thread_id = id++;
            td->client_id = client;
            current_connection++;
            pthread_create(&th[id], NULL, &thread_function, td);
        }
}

static void *thread_function(void *arg)
{
    //struct threadINFO td_function;
    //td_function= *((struct threadINFO*)arg);
    pthread_detach(pthread_self());
    raspunde((struct threadINFO *)arg);
    close((intptr_t)arg);
    return (NULL);
};

void raspunde(void *arg)
{
    int numar_problema, remain_problems;
    remain_problems = nr_prob_comp;
    struct threadINFO td_raspuns;
    td_raspuns = *((struct threadINFO *)arg);

    //wait_until_all_connected
    if (maximum_connection == 1)
        send_information(td_raspuns.client_id, "Sa incepem!!\n\n\0");
    else
        send_information(td_raspuns.client_id, "Se asteapta sa se atinga numarul necesar de participanti\n\n\0");

    while (current_connection % maximum_connection != 0)
        continue;

    //send information about the competition
    send_information(td_raspuns.client_id, "Bine ati venit! Sa incepem cu cateva informatii.\n1.Fiecare problema are un timp alocat precizat la primirea problemei, daca problema nu este incarcata la timp punctajul automat este 0.\n2.Fisierele de upload vor avea formatul problema*nr_problemei la care sunteti*.c\n3.Toate datele citite de la utilizator in probleme vor fi preluate de la linia de comanda, altfel 0 puncte.\n4.Veti afisa doar raspunsul, fara mesaje inutile, altfel 0 puncte.\n\n\0");

    //send possible commands
    send_information(td_raspuns.client_id, "ready - pentru a continua/primi problema\nexit - pentru a parasii competitia\n\n\0");

    //read the first command: ready/exit
    int ok = 0;
    char comanda[50];
    while (ok == 0)
    {
        memset(comanda, 0, 50);
        recv_information(td_raspuns.client_id, comanda);

        switch (command_type(comanda))
        {
        case 'r':
        {
            ok = 1;
            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                handle_error("[server] Eroare la write() catre server");
            break;
        }
        case 'e':
        {
            ok = 2;
            write_all_results_0(td_raspuns.client_id, nr_prob_comp - remain_problems + 1);
            send_results_file(td_raspuns.client_id);
            send_information(td_raspuns.client_id, "Ati primit un fisier cu puntajul pentru fiecare problema. Multumim pentru participare.\n");
            break;
        }
        default:
        {
            ok = 0;
            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                handle_error("[server] Eroare la write() catre server");
            send_information(td_raspuns.client_id, "Comanda incorecta. Incercati una dintre comenzile ready/exit\n\n\0");
            break;
        }
        }
    }

    if (ok == 1)
    {
        bool not_exit;
        not_exit = 1;
        while (remain_problems != 0 && not_exit)
        {
            ok = 0;
            remain_problems--;
            numar_problema = (rand() % nr_prob_disp) + 1;
            if (comp_prob[nr_prob_comp - remain_problems] == 0)
            {
                while (search_array(comp_prob, nr_prob_comp - remain_problems - 1, numar_problema) == 0)
                {
                    numar_problema = (rand() % nr_prob_disp) + 1;
                }
                if (comp_prob[nr_prob_comp - remain_problems] == 0)
                    comp_prob[nr_prob_comp - remain_problems] = numar_problema;
                else
                    numar_problema = comp_prob[nr_prob_comp - remain_problems];
            }
            else
                numar_problema = comp_prob[nr_prob_comp - remain_problems];
            send_problem(td_raspuns.client_id, numar_problema);

            while (ok == 0)
            {
                int problem_time = problems_time[numar_problema]*60;

                if (write(td_raspuns.client_id, &problem_time, sizeof(int)) < 0)
                    handle_error("[server] Eroare la write()");

                struct timeval tv, timer;
                int return_sel;
                fd_set rfds;
                //timer
                timer.tv_sec = 0;
                timer.tv_usec = 0;

                //select declaration
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                FD_SET(td_raspuns.client_id, &rfds);

                return_sel = select(1, &rfds, NULL, NULL, &tv);
                if (return_sel == -1)
                    handle_error("[server] Eroare la select()");

                timer.tv_sec++;
                while (return_sel == 0 && timer.tv_sec <= problem_time)
                {
                    tv.tv_sec = 1;
                    tv.tv_usec = 0;
                    FD_SET(td_raspuns.client_id, &rfds);
                    return_sel = select(td_raspuns.client_id + 1, &rfds, NULL, NULL, &tv);
                    if (return_sel == -1)
                        handle_error("[server] Eroare la select()");

                    timer.tv_sec++;
                }

                if (timer.tv_sec <= problem_time)
                {

                    memset(comanda, 0, 50);
                    recv_information(td_raspuns.client_id, comanda);

                    switch (command_type(comanda))
                    {
                    case 's':
                    {
                        if (remain_problems == 0)
                        {
                            ok = 2;
                            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                                handle_error("[server] Eroare la write() catre client");
                            write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                            not_exit = 1;

                            send_information(td_raspuns.client_id, "\nAcesta a fost ultima problema.\nFolositi comanda exit pentru a inchide aplicatia si pentru a primi fisierul cu rezultate. \n\n");
                        }
                        else
                        {
                            ok = 1;
                            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                                handle_error("[server] Eroare la write() catre client");
                            write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                            not_exit = 1;
                        }
                        break;
                    }
                    case 'u':
                    {
                        int done;
                        int corecte;
                        int points;
                        if (remain_problems == 0)
                        {
                            ok = 2;
                            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                                handle_error("[server] Eroare la write() catre server");
                            not_exit = 1;
                            done=get_source_file(td_raspuns.client_id, nr_prob_comp - remain_problems);
                            if (compile(td_raspuns.client_id,nr_prob_comp - remain_problems)==1){
                                corecte=run_program(td_raspuns.client_id,nr_prob_comp - remain_problems,numar_problema);
                                points=compute_points(avg_time,corecte,timer,problem_time);
                                write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, points);
                            }
                            else
                                write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                            send_information(td_raspuns.client_id, "\nAcesta a fost ultima problema.\nFolositi comanda exit pentru a inchide aplicatia si pentru a primi fisierul cu rezultate. \n\n");
                        }
                        else
                        {
                            ok = 1;
                            if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                                handle_error("[server] Eroare la write() catre server");
                            done=get_source_file(td_raspuns.client_id, nr_prob_comp - remain_problems);
                            if (compile(td_raspuns.client_id,nr_prob_comp - remain_problems)==1){
                                corecte=run_program(td_raspuns.client_id,nr_prob_comp - remain_problems,numar_problema);
                                points=compute_points(avg_time,corecte,timer,problem_time);
                                write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, points);
                            }
                            else
                                write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                            not_exit = 1;
                        }
                        break;
                    }
                    case 'c':
                    {
                        ok = 0;
                        if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                            handle_error("[server] Eroare la write() catre server");
                        send_information(td_raspuns.client_id, "Trebuie sa incarcati un fisier .c.\n\n\0");
                        break;
                    }
                    case 'e':
                    {
                        ok = 1;
                        write_all_results_0(td_raspuns.client_id, nr_prob_comp - remain_problems);
                        send_results_file(td_raspuns.client_id);
                        send_information(td_raspuns.client_id, "Ati primit un fisier cu puntajul pentru fiecare problema. Multumim pentru participare.\n");
                        not_exit = 0;
                        break;
                    }
                    default:
                    {
                        ok = 0;
                        if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                            handle_error("[server] Eroare la write() catre server");
                        send_information(td_raspuns.client_id, "Comanda incorecta. Incercati una dintre comenzile upload/skip/exit\n\n\0");
                        break;
                    }
                    }
                }
                else
                {
                    if (remain_problems == 0)
                    {
                        ok = 2;
                        if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                            handle_error("[server] Eroare la write() catre server");
                        write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                        send_information(td_raspuns.client_id, "\nAcesta a fost ultima problema.\nFolositi comanda exit pentru a inchide aplicatia si pentru a primi fisierul cu rezultate. \n\n");
                    }
                    else
                    {
                        ok = 1;
                        if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                            handle_error("[server] Eroare la write() catre server");
                        write_result(td_raspuns.client_id, nr_prob_comp - remain_problems, 0);
                        send_information(td_raspuns.client_id, "\nTimpul alocat problemei s-a scurs!!!\n\n");
                    }
                }
            }
        }
        if (not_exit == 1)
        {
            ok = 0;
            while (ok == 0)
            {
                memset(comanda, 0, 50);
                recv_information(td_raspuns.client_id, comanda);

                switch (command_type(comanda))
                {
                case 'e':
                {
                    ok = 1;
                    send_results_file(td_raspuns.client_id);
                    send_information(td_raspuns.client_id, "\nMultumim pentru participare. Ati primit si fisierul cu rezultatele.\n");
                    break;
                }

                default:
                {
                    ok = 0;
                    if (write(td_raspuns.client_id, &ok, sizeof(int)) < 0)
                        handle_error("[server] Eroare la write() catre server");
                    send_information(td_raspuns.client_id, "Va rog sa folositi comanda exit pentru a inchide aplicatia si pentru a primi fisierul cu rezultate.\n\n\0");
                    break;
                }
                }
            }
        }
    }
}
//for sending problems
void send_problem(int fd, int numar_problema)
{
    char nr[3];
    sprintf(nr, "%d", numar_problema);
    char path_problema[40];
    char path_teste[40];
    memset(path_problema, 0, 40);
    memset(path_teste, 0, 40);

    strcpy(path_problema, "./Probleme/Problema\0");
    strcpy(path_teste, "./Probleme/TesteProblema\0");

    strcat(path_problema, nr);
    strcat(path_problema, ".txt");
    path_problema[strlen(path_problema)] = '\0';

    strcat(path_teste, nr);
    strcat(path_teste, ".txt");
    path_teste[strlen(path_teste)] = '\0';

    int fd_problema;
    fd_problema = open(path_problema, O_RDWR);
    if (fd_problema == -1)
        handle_error("[server]Eroare la deschidere fisier problema");

    int fd_teste;
    fd_teste = open(path_teste, O_RDWR);
    if (fd_teste == -1)
        handle_error("[server]Eroare la deschidere fisier teste");

    int rc, total_caractere = 0;
    //problema
    char problema[2000];
    while (1)
    {
        rc = read(fd_problema, problema, sizeof(problema));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier problema");

        total_caractere += rc;
    }

    lseek(fd_problema, 0, SEEK_SET);

    if (write(fd, &total_caractere, sizeof(int)) < 0)
        handle_error("[server][send]]Eroare la write()");

    while (1)
    {
        rc = read(fd_problema, problema, sizeof(problema));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier problema");

        write(fd, problema, rc);
    }

    //teste
    total_caractere = 0;
    char teste[2000];
    while (1)
    {
        rc = read(fd_teste, teste, sizeof(teste));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier teste");

        total_caractere += rc;
    }

    lseek(fd_teste, 0, SEEK_SET);

    if (write(fd, &total_caractere, sizeof(int)) < 0)
        handle_error("[server][send]]Eroare la write()");

    while (1)
    {
        rc = read(fd_teste, teste, sizeof(teste));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier problema");

        write(fd, teste, rc);
    }
}

//for sending information
void send_information(int fd, char text[])
{
    int nr_caractere = strlen(text);
    if (write(fd, &nr_caractere, sizeof(int)) < 0)
        handle_error("[server][send]]Eroare la write()");

    if (write(fd, text, nr_caractere) < 0)
        handle_error("[server][send]Eroare la write()");
}

//for receiving information
void recv_information(int fd, char raspuns[])
{

    int nr_caractere;
    if (read(fd, &nr_caractere, sizeof(int)) < 0)
        handle_error("[server][recv] Eroare la read()");

    if (read(fd, raspuns, nr_caractere) < 0)
        handle_error("[server][recv] Eroare la read() de la server");

    raspuns[nr_caractere] = '\0';
}

//for commanda type
char command_type(char comanda[])
{
    if (strcmp(comanda, "ready") == 0)
        return 'r';
    else if (strcmp(comanda, "exit") == 0)
        return 'e';
    else if (strcmp(comanda, "skip") == 0)
        return 's';
    else if (strstr(comanda, "upload") != 0)
    {
        char verificare_comanda[] = "upload ";
        int i;
        for (i = 0; i < 7; i++)
            if (verificare_comanda[i] != comanda[i])
                return 'g';
        if (comanda[strlen(comanda) - 2] != '.' && comanda[strlen(comanda) - 1] != 'c')
            return 'c';
        return 'u';
    }
}

//commands behavior

//for configuration
void set_config()
{
    int fd_config;
    fd_config = open("config.txt", O_RDONLY);

    if (fd_config == -1)
        handle_error("Eroare la deschiderea fisierului pt configurare");

    int rc_config;
    char text[1000];
    rc_config = read(fd_config, text, 1000);

    if (rc_config == -1)
        handle_error("Eroare la citirea din config.txt");

    maximum_connection = search_info(text, "Nr participanti:");
    nr_prob_disp = search_info(text, "Nr probleme disponibile:");
    nr_prob_comp = search_info(text, "Nr probleme competitie:");

    comp_prob = (int *)malloc((nr_prob_disp + 1) * sizeof(int));
    memset(comp_prob, 0, (nr_prob_disp + 1) * sizeof(int));
    problems_time = (int *)malloc((nr_prob_disp + 1) * sizeof(int));
    memset(problems_time, 0, (nr_prob_disp + 1) * sizeof(int));
    int nr = 1;
    char time_prob[100];
    while (nr <= nr_prob_disp)
    {
        memset(time_prob, 0, 100);

        strcpy(time_prob, "Timp Problema \0");

        time_prob[strlen(time_prob)] = nr + '0';
        strcat(time_prob, ":");
        time_prob[strlen(time_prob)] = '\0';

        problems_time[nr] = search_info(text, time_prob);
        nr++;
    }
}

int search_info(char *info, char *text)
{
    char answer[5];
    char *p;
    int i;
    p = strstr(info, text);
    p = p + strlen(text);
    if (p != NULL)
    {
        i = 0;
        while (p[i] != '\n')
        {
            answer[i] = p[i];
            i++;
        }
        answer[i] = '\0';
    }
    return atoi(answer);
}

//for searching in an array
int search_array(int x[], int nr_elemente, int element)
{
    int i;
    for (i = 1; i <= nr_elemente; i++)
        if (x[i] == element)
            return 0;
    return 1;
}

//for results file
void write_result(int id_client, int nr_problema, int punctaj)
{
    char problema[30];
    memset(problema, 0, 30);
    strcpy(problema, "Problema ");
    problema[strlen(problema)] = nr_problema + '0';
    strcat(problema, ": ");
    char pctj[4];
    sprintf(pctj, "%d", punctaj);
    strcat(problema, pctj);
    problema[strlen(problema)] = '\n';
    problema[strlen(problema)] = '\0';

    int fd;
    char path[40];
    memset(path, 0, 40);
    strcat(path, "Results_");
    char code[30];
    sprintf(code, "%d", id_client);
    strcat(path, code);
    strcat(path, ".txt");
    path[strlen(path)] = '\0';

    fd = open(path, O_CREAT | O_APPEND | O_RDWR, 0777);
    if (fd == -1)
        handle_error("[server] Eroare la open() pentru fisierul de rezultate");

    if (write(fd, problema, strlen(problema)) < 0)
        handle_error("[server] Eroare la write() in fisierul de rezultate");

    close(fd);
}

void write_all_results_0(int id_client, int nr_problema)
{
    char problema[30];
    int fd;
    char path[40];
    memset(path, 0, 40);
    strcat(path, "Results_");
    char code[30];
    sprintf(code, "%d", id_client);
    strcat(path, code);
    strcat(path, ".txt");
    path[strlen(path)] = '\0';

    fd = open(path, O_CREAT | O_APPEND | O_RDWR, 0777);
    if (fd == -1)
        handle_error("[server] Eroare la open() pentru fisierul de rezultate");

    while (nr_problema <= nr_prob_comp)
    {
        memset(problema, 0, 30);
        strcpy(problema, "Problema ");
        problema[strlen(problema)] = nr_problema + '0';
        strcat(problema, ": ");
        problema[strlen(problema)] = '0';
        problema[strlen(problema)] = '\n';
        problema[strlen(problema)] = '\0';

        nr_problema++;

        if (write(fd, problema, strlen(problema)) < 0)
            handle_error("[server] Eroare la write() in fisierul de rezultate");
    }
    close(fd);
}

void send_results_file(int fd)
{
    char path[40];
    memset(path, 0, 40);
    strcat(path, "Results_");
    char code[30];
    sprintf(code, "%d", fd);
    strcat(path, code);
    strcat(path, ".txt");
    path[strlen(path)] = '\0';

    int fd_results;
    fd_results = open(path, O_RDWR);
    if (fd_results == -1)
        handle_error("[server]Eroare la deschidere fisier rezultate");

    int total_caractere = 0, rc;
    char results[2000];
    char teste[2000];
    while (1)
    {
        rc = read(fd_results, teste, sizeof(teste));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier rezultate");

        total_caractere += rc;
    }

    lseek(fd_results, 0, SEEK_SET);

    if (write(fd, &total_caractere, sizeof(int)) < 0)
        handle_error("[server][send]]Eroare la write()");

    while (1)
    {
        rc = read(fd_results, teste, sizeof(teste));

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[server]Eroare la read() din fisier problema");

        write(fd, teste, rc);
    }

    close(fd_results);
    remove(path);
}

//for source file 

int get_source_file(int fd, int nr_problema)
{
    int fd_rezolvare, nr_caractere, current, rc;
    char path[100];
    memset(path, 0, 100);
    strcpy(path, "./Rezolvari/Problema");
    char nr[4];
    sprintf(nr, "%d", nr_problema);
    strcat(path, nr);
    memset(nr, 0, 4);
    sprintf(nr, "%d", fd);
    strcat(path, "_");
    strcat(path, nr);
    strcat(path, ".c");
    path[strlen(path)] = '\0';

    fd_rezolvare = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0777);

    if (fd == -1)
        handle_error("[client] Eroare la open() fisier teste");

    if (read(fd, &nr_caractere, sizeof(int)) < 0)
        handle_error("[client] Eroare la read() de la server");

    char rezolvari[2000];
    current = 0;
    while (current < nr_caractere)
    {
        rc = read(fd, rezolvari, nr_caractere);

        if (rc == 0)
            break;

        if (rc < 0)
            handle_error("[client] Eroare la read() de la server");

        rezolvari[nr_caractere] = '\0';
        write(fd_rezolvare, rezolvari, rc);

        current += rc;
    }
    return 1;
}

int compile (int fd,int nr_problema)
{
    char path[100];
    memset(path, 0, 100);
    strcpy(path, "./Rezolvari/Problema");
    char nr[4];
    memset(nr,0,4);
    sprintf(nr, "%d", nr_problema);
    strcat(path, nr);
    memset(nr, 0, 4);
    sprintf(nr, "%d", fd);
    strcat(path, "_");
    strcat(path, nr);
    path[strlen(path)] = '\0';

    char execute[500];
    memset(execute,0,500);
    strcpy(execute,"./Rezolvari/compile.sh ");
    strcat(execute,path);

    if (system(execute)!=0)
        {
        strcat(path,".c");
        remove(path);
        return 0;
        }
    else
        return 1;
}

int run_program (int fd, int nr_problema, int nr_test)
{
    avg_time=0.0;

    int corecte=0;
    char path[100];
    memset(path, 0, 100);
    strcpy(path, "./Rezolvari/Problema");
    char nr[4];
    memset(nr,0,4);
    sprintf(nr, "%d", nr_problema);
    strcat(path, nr);
    memset(nr, 0, 4);
    sprintf(nr, "%d", fd);
    strcat(path, "_");
    strcat(path, nr);
    strcat(path," ");
    path[strlen(path)] = '\0';

    char execute[1000];
    char path_teste[100];
    char path_results[100];
    char path_time[100];
    char index[2];
    int fd_teste,rc_teste,fd_results,rc_results, fd_time,rc_time;
    char teste[1000];
    char output[100];
    char results[1000];
    char time[10];
    for (int i=1;i<=4;i++){
        memset(path_teste,0,100);
        memset(index,0,2);
        strcpy(path_teste,"./Teste/TPB");
        memset(nr,0,4);
        sprintf(nr, "%d", nr_test); 
        strcat(path_teste, nr);
        sprintf(index,"%d",i);
        strcat(path_teste,"_");
        strcat(path_teste,index);
        strcat(path_teste,".txt");
        path_teste[strlen(path_teste)]='\0';

        memset(execute,0,1000);
        strcpy(execute,"./Rezolvari/run.sh ");
        strcat(execute,path);

        fd_teste = open(path_teste, O_RDWR);
        if (fd_teste == -1)
            handle_error("[server]Eroare la deschidere fisier verificare/teste");

        memset(teste,0,1000);
        rc_teste = read(fd_teste, teste, sizeof(teste));

        if (rc_teste < 0)
            handle_error("[server]Eroare la read() din fisier verificare/teste");
        
        int j=0;
        while(teste[j]!='\n')
        {
            j++;
        }
        j++;
        memset(output,0,100);
        strcpy(output,teste+j);

        teste[j-1]='\0';
        strcat(execute,teste);

        system(execute);

        memset(path_results,0,100);
        strcpy(path_results,"./Rezolvari/Problema");
        memset(nr,0,4);
        sprintf(nr, "%d", nr_problema);
        strcat(path_results, nr);
        memset(nr, 0, 4);
        sprintf(nr, "%d", fd);
        strcat(path_results, "_");
        strcat(path_results, nr);
        strcat(path_results,"_res.txt");

        fd_results=open(path_results,O_RDWR);
        if (fd_results == -1)
            handle_error("[server]Eroare la deschidere fisier rezultate rulare");

        rc_results = read(fd_results, results, sizeof(results));

        if (rc_results < 0)
            handle_error("[server]Eroare la read() din fisier rezultate rulare");
    
        int i=0,done=0;
        while(i<strlen(output) && !done)
        {
            if(output[i]!=results[i])
                done=1;
            i++;
        }
        if (done==0)
            corecte++;

        memset(path_time,0,100);
        strcpy(path_time,"./Rezolvari/Problema");
        memset(nr,0,4);
        sprintf(nr, "%d", nr_problema);
        strcat(path_time, nr);
        memset(nr, 0, 4);
        sprintf(nr, "%d", fd);
        strcat(path_time, "_");
        strcat(path_time, nr);
        strcat(path_time,"_time.txt");

        fd_time = open(path_time, O_RDWR);
        if (fd_time == -1)
            handle_error("[server]Eroare la deschidere fisier time");

        memset(time,0,10);
        rc_time = read(fd_time, time, sizeof(time));

        if (rc_time < 0)
            handle_error("[server]Eroare la read() din fisier time");

        time[strlen(time)-1]='\0';
        int index=0;
        while(time[index]!=',')
            index++;
        time[index]='.';
        avg_time = avg_time + strtod(time,NULL);

        remove(path_time);
        remove(path_results);
        close(fd_results);
        close(fd_teste);
    }
     path[strlen(path)-1]='\0';
     remove(path);
     strcat(path,".c");
     remove(path);
     avg_time = avg_time/4.0;
     return corecte;

        
}

// for score

int compute_points(double avg, int corecte, struct timeval timer, int problem_time)
{
    int points=0;
    points = 20*corecte;

    if (timer.tv_sec<= problem_time/2)
        points+=10;
    else
        points+=5;
    
    if (avg_time<=0.1)
        points+=10;

    return points;
}