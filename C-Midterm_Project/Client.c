/*############################################################################*/
/* Midterm_Furkan_Erdol_131044065                                             */
/*                                                                            */
/* Client.c                                                                   */
/* ______________________________                                             */
/* Written by Furkan Erdol on April 22, 2016                                  */
/* Description                                                                */
/*_______________                                                             */
/* Client                                                                     */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/errno.h>
#include <time.h>
#include <sys/time.h>


#define SERVER_FIFO_NAME "IntegralFIFO"  /*Server fifo name*/
#define BUFFER 1000                         /*BUFFER*/
#define SIGUSR3 SIGWINCH                 /*SIGURS3 for division by zero exception*/

/*Client message (calculate informations)*/
struct message {
    char fi[BUFFER]; /*First function name*/
    char fj[BUFFER]; /*Second function name*/
    double time_interval; /*Time interval*/
    char operator; /*Operator string*/
};


time_t connection_time; /*Time client connect to server*/
char client_fifo_name[BUFFER]; /*Client fifo name*/
char first_function_name[BUFFER]; /*First function name*/
char second_function_name[BUFFER]; /*Second function name*/
char log_file_name[BUFFER]; /*Client report (log) file name*/
int status = -1; /*Check for server is full*/
FILE *log_file_pointer; /*File pointer to client report (log) file*/



/*
 * Checks main arguments validity
 */
void check_arguments(int argc, char **argv);

/*
 * Creates client report (log) file
 */
void create_log();

/*
 * Appends to report (log) file informantion about clients
 */
void append_log(int signal_no);

/*
 * Writes to console informantion about clients
 */
void write_consol(int signal_number);

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_seconds(struct timeval t0, struct timeval t1);

/*
 * Signal handler for client process
 */
static void signal_handler(int signal_number);

int main(int argc, char **argv) {


    int server_fifowr; /*File descriptor for server fifo (write)*/
    int server_fiford; /*File descriptor for server fifo (read)*/

    int client_fifowr; /*File descriptor for client fifo (write)*/
    int client_fiford; /*File descriptor for client fifo (read)*/

    struct message client_message; /*Client message information*/
    struct sigaction act;

    struct timeval start_time; /*Client start time*/
    struct timeval read_time; /*Time to read result from server*/

    double result; /*Result of integral*/

    pid_t this_pid; /*Client pid*/


    /*Checks main arguments*/
    check_arguments(argc, argv);

    this_pid = getpid();

    act.sa_handler = signal_handler;
    act.sa_flags = 0;

    /*Set signals*/
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)
            || (sigaction(SIGUSR1, &act, NULL) == -1) || (sigaction(SIGUSR2, &act, NULL) == -1)
            || (sigaction(SIGUSR3, &act, NULL) == -1) || (sigaction(SIGQUIT, &act, NULL) == -1)
            || (sigaction(SIGALRM, &act, NULL) == -1) || (sigaction(SIGHUP, &act, NULL) == -1)
            || (sigaction(SIGTERM, &act, NULL) == -1) || (sigaction(SIGPIPE, &act, NULL) == -1)) {
        perror("Failed to set signals");
        exit(EXIT_FAILURE);
    }


    /*Set function names*/
    sprintf(first_function_name, "%s", argv[1]);
    sprintf(second_function_name, "%s", argv[2]);


    sprintf(log_file_name, "%dReport.log", this_pid);
    sprintf(client_fifo_name, "Client%d", this_pid);

    /*Create client fifo*/
    unlink(client_fifo_name);
    if (mkfifo(client_fifo_name, 0666) == -1) {
        perror("Failed to create client fifo");
        exit(EXIT_FAILURE);
    }

    connection_time = time(NULL);

    create_log();

    /*Client sends its pid*/
    server_fifowr = open(SERVER_FIFO_NAME, O_WRONLY);
    if (write(server_fifowr, &this_pid, sizeof (pid_t)) < 0) {
        fprintf(stderr, "\nServer couldn't found! Terminate the program...\n\n");
        unlink(client_fifo_name);
        unlink(log_file_name);
        exit(EXIT_FAILURE);
    }
    close(server_fifowr);

    /*Reads server status*/
    client_fiford = open(client_fifo_name, O_RDONLY);
    if (read(client_fiford, &status, sizeof (int)) < 0) {
        perror("Read from server client fifo");
        unlink(client_fifo_name);
        exit(EXIT_FAILURE);
    }
    close(client_fiford);

    /*If server is full*/
    if (status == -1) {
        write_consol(-2);
        append_log(-2);

        unlink(client_fifo_name);
        exit(EXIT_FAILURE);
    }

    /*Calculate start time for client*/
    gettimeofday(&start_time, NULL);

    /*Set client message*/
    sprintf(client_message.fi, "%s", first_function_name);
    sprintf(client_message.fj, "%s", second_function_name);

    if (strcmp(argv[4], "+") == 0) {
        client_message.operator='+';
    } else if (strcmp(argv[4], "-") == 0) {
        client_message.operator='-';
    } else if (strcmp(argv[4], "/") == 0) {
        client_message.operator='/';
    } else {
        client_message.operator='*';
    }

    client_message.time_interval = atof(argv[3]);


    /*Client sends message*/
    client_fifowr = open(client_fifo_name, O_WRONLY);
    if (write(client_fifowr, &client_message, sizeof (struct message)) < 0) {
        perror("Write to server client fifo");
        exit(EXIT_FAILURE);
    }
    close(client_fifowr);




    /*Clients reads results of integral*/
    client_fiford = open(client_fifo_name, O_RDONLY);

    fprintf(log_file_pointer, "********************** RESULTS *********************\n");

    while (1) {

        if (read(client_fiford, &result, sizeof (double)) < 0) {
            write_consol(-1);
            append_log(-1);
            unlink(client_fifo_name);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "Result : %f\n", result);

        /*Read time*/
        gettimeofday(&read_time, NULL);

        fprintf(log_file_pointer, "Result : %f     #####     Elapsed time : %f\n", result, timedifference_seconds(start_time, read_time));

    }

    fclose(log_file_pointer);
    close(client_fiford);

    unlink(client_fifo_name);

    return 0;
}

/*
 * Appends to report (log) file informantion about clients
 */
void append_log(int signal_number) {

    fseek(log_file_pointer, 0, SEEK_SET);

    fprintf(log_file_pointer, "****************************************************\n");
    fprintf(log_file_pointer, "Client pid                : %d                      \n", getpid());
    if (status == -1) {
        fprintf(log_file_pointer, "Connection time to server : No connection       \n");
    } else {
        fprintf(log_file_pointer, "Connection time to server : %s", ctime(&connection_time));
    }
    fprintf(log_file_pointer, "Function names            : %s and %s               \n", first_function_name, second_function_name);

    if (signal_number == SIGINT) {
        fprintf(log_file_pointer, "\nSIGINT signal detected! Terminate the program...    \n");
    } else if (signal_number == SIGUSR1) {
        fprintf(log_file_pointer, "\nServer terminated! Terminate the program...         \n");
    } else if (signal_number == SIGUSR2) {
        fprintf(log_file_pointer, "\nInvalid input! Terminate the program...             \n");
    } else if (signal_number == SIGUSR3) {
        fprintf(log_file_pointer, "\nDivide by zero! Terminate the program...            \n");
    } else if ((signal_number == SIGQUIT) || (signal_number == SIGHUP) || (signal_number == SIGTERM)) {
        fprintf(log_file_pointer, "\nUnexpected termination!                             \n");
    } else if (signal_number == SIGALRM) {
        fprintf(log_file_pointer, "\nServer not responding! Terminate the program...     \n");
    } else if (signal_number == -2) {
        fprintf(log_file_pointer, "\nClient : Server is full! Terminate the program...   \n");
    } else {
        fprintf(log_file_pointer, "\nConnection was lost! Terminate the program...       \n");
    }

    fprintf(log_file_pointer, "****************************************************\n");


    fclose(log_file_pointer);

}

/*
 * Writes to console informantion about clients
 */
void write_consol(int signal_number) {


    if (signal_number == SIGINT) {
        fprintf(stderr, "\nClient : SIGINT signal detected! Terminate the program...    \n\n");
    } else if (signal_number == SIGUSR1) {
        fprintf(stderr, "\nClient : Server terminated! Terminate the program...         \n\n");
    } else if (signal_number == SIGUSR2) {
        fprintf(stderr, "\nClient : Invalid input! Terminate the program...             \n\n");
    } else if (signal_number == SIGUSR3) {
        fprintf(stderr, "\nClient : Divide by zero! Terminate the program...            \n\n");
    } else if ((signal_number == SIGQUIT) || (signal_number == SIGHUP) || (signal_number == SIGTERM)) {
        fprintf(stderr, "\nClient : Unexpected termination!                             \n\n");
    } else if (signal_number == SIGALRM) {
        fprintf(stderr, "\nClient : Server not responding! Terminate the program...     \n\n");
    } else if (signal_number == SIGPIPE) {
        fprintf(stderr, "\nClient : Server couldn't found! Terminate the program...     \n\n");
    } else if (signal_number == -2) {
        fprintf(stderr, "\nClient : Server is full! Terminate the program...            \n\n");
    } else {
        fprintf(stderr, "\nClient : Connection was lost! Terminate the program...       \n\n");
    }

}

/*
 * Creates client report (log) file
 */
void create_log() {

    int i;

    log_file_pointer = fopen(log_file_name, "w+");



    for (i = 1; i <= 364; ++i) {
        fprintf(log_file_pointer, " ");
        if (i % 52 == 0) {
            fprintf(log_file_pointer, "\n");
        }
    }
}

/*
 * Checks main arguments validity
 */
void check_arguments(int argc, char **argv) {

    int i;

    if (argc != 5) {

        fprintf(stderr, "\n#################### USAGE #########################\n");
        fprintf(stderr, "\nYou need enter 4 arguments <->");
        fprintf(stderr, "\n<fi>            : the name of the first operand as a first argument");
        fprintf(stderr, "\n<fj>            : the name of the second operand as a second argument");
        fprintf(stderr, "\n<time interval> : upper limit of integral operation in terms of seconds as a third argument");
        fprintf(stderr, "\n<operation>     : arithmetical operation as a fourth argument");
        fprintf(stderr, "\n\n-Sample");
        fprintf(stderr, "\n./Client f1 f2 5 +\n");

        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "f1") != 0 && strcmp(argv[1], "f2") != 0 && strcmp(argv[1], "f3") != 0
            && strcmp(argv[1], "f4") != 0 && strcmp(argv[1], "f5") != 0 && strcmp(argv[1], "f6") != 0) {
        fprintf(stderr, "Invalid argument!\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "f1") != 0 && strcmp(argv[2], "f2") != 0 && strcmp(argv[2], "f3") != 0
            && strcmp(argv[2], "f4") != 0 && strcmp(argv[2], "f5") != 0 && strcmp(argv[2], "f6") != 0) {
        fprintf(stderr, "Invalid argument!\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < strlen(argv[3]); ++i) {
        if (!isdigit(argv[3][i]) && argv[3][i] != '.' && argv[3][i] != '-') {
            fprintf(stderr, "Invalid argument!\n");
            exit(EXIT_FAILURE);
        }
    }


    if (strcmp(argv[4], "+") != 0 && strcmp(argv[4], "-") != 0 && strcmp(argv[4], "/") != 0 && strcmp(argv[4], "x") != 0) {
        fprintf(stderr, "Invalid argument!\n");
        exit(EXIT_FAILURE);
    }

}

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_seconds(struct timeval start, struct timeval finish) {

    return ( (finish.tv_sec - start.tv_sec) * 1000.0f + (finish.tv_usec - start.tv_usec) / 1000.0f) / 1000;

}

/*
 * Signal handler for client process
 */
static void signal_handler(int signal_number) {

    append_log(signal_number);
    write_consol(signal_number);

    unlink(client_fifo_name);


    exit(EXIT_FAILURE);
}
