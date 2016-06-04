/*############################################################################*/
/* Midterm_Furkan_Erdol_131044065                                             */
/*                                                                            */
/* IntegralGen.c                                                              */
/* ______________________________                                             */
/* Written by Furkan Erdol on April 22, 2016                                  */
/* Description                                                                */
/*_______________                                                             */
/* Server                                                                     */
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
#include <math.h>
#include <sys/time.h>
#include <time.h>


#define SERVER_FIFO_NAME "IntegralFIFO"   /*Server fifo name*/
#define SERVER_LOG_NAME "IntegralReport.log" /*Server log name*/
#define BUFFER 1000                          /*BUFFER*/
#define NUMBER_OF_FUNCTIONS 6
#define SIGUSR3 SIGWINCH                  /*SIGURS3 for division by zero exception*/

/*Client message (calculate informations)*/
struct message {
    char fi[BUFFER]; /*First function name*/
    char fj[BUFFER]; /*Second function name*/
    double time_interval; /*Time interval*/
    char operator; /*Operator string*/
};


int client_pid_arr[BUFFER]; /*Client pids*/
char source_file_name[BUFFER]; /*Calculate process source name. Calculate process claculates integral and sends results helper process*/
char executable_file_name[BUFFER]; /*Executable name (Calculate process executable)*/
char redirection_file_name[BUFFER]; /*Temporary file to output redirection (Calculate exe error output)*/
int client_counter = 0; /*Client counter to server limit and sends SIGINT*/
time_t current_time;
static volatile server_pid; /*Server pid flag*/



/*
 * Helper process remains in contact with client process.
 * Helper process generates calculate process to calculations.
 * Helper process get results from calculate process and sends to client process
 */
void helper_process(pid_t client_pid, double resolution, double t0, char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]);

/*
 * Generates a source file to calculation (Calculation process)
 */
void create_source_file(struct message client_message, double resolution, int pipe_fd, double t0, char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]);

/*
 * Checks main arguments validity
 */
void check_arguments(int argc, char **argv);


/*
 * Unliks source file, executable file and temporaray file
 */
void unlink_files();

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_seconds(struct timeval t0, struct timeval t1);


/*
 * Reads funtions in functions txt (One time -when the server starts-)
 */
void read_operands(char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]);

/*
 * Signal handler for server process and helper process
 */
static void signal_handler(int signal_no);

/*
 * Signal handler for server process to max client limit
 */
static void max_client_handler(int signal_no);

/*
 * Signal handler for helper process to determine lost the connection between the helper process and the client
 */
static void sigpipe_handler(int signal_no);

void appeand_log(pid_t client_pid, double t0, struct message client_message, double resolution, time_t connection_time);


/*************************************** START SERVER ********************************************/
int main(int argc, char **argv) {

    int server_fifowr; /*File descriptor for server fifo (write)*/
    int server_fiford; /*File descriptor for server fifo (read)*/

    int client_fifowr; /*File descriptor for client fifo (write)*/
    int client_fiford; /*File descriptor for client fifo (read)*/

    pid_t client_pid;
    pid_t child_pid; /*Helper rocess pid*/

    double resolution; /*Resolution number*/
    double t0; /*to number*/

    int max_client; /*Max client number*/
    int status;

    struct sigaction act;
    struct sigaction act2;

    struct timeval start_time; /*Server start time*/
    struct timeval connection_time; /*For connection (Client) time)*/

    char client_fifo_name[BUFFER]; /*Client fifo name*/
    char functions_array[NUMBER_OF_FUNCTIONS][BUFFER];

    FILE *server_log_ptr;


    /*Checks main arguments*/
    check_arguments(argc, argv);

    /*Get server start time*/
    gettimeofday(&start_time, NULL);

    /*Get server pid*/
    server_pid = getpid();


    act.sa_handler = signal_handler;
    act.sa_flags = 0;

    /*Set signals*/
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)
            || (sigaction(SIGQUIT, &act, NULL) == -1)
            || (sigaction(SIGHUP, &act, NULL) == -1)) {
        perror("Failed to signal handler");
        exit(EXIT_FAILURE);
    }

    act2.sa_handler = max_client_handler;
    act2.sa_flags = 0;

    /*Set signal*/
    if ((sigemptyset(&act2.sa_mask) == -1) || (sigaction(SIGUSR1, &act2, NULL) == -1)) {
        perror("Failed to signal handler");
        exit(EXIT_FAILURE);
    }



    /*Set argument values*/
    max_client = atoi(argv[2]);
    resolution = atof(argv[1]);

    /*Read dunction*/
    read_operands(functions_array);


    /*Creates server fifo*/
    if (mkfifo(SERVER_FIFO_NAME, 0666) == -1) {
        fprintf(stderr, "\nIntegralGen Server already exists! Terminate the program...\n\n");
        exit(EXIT_FAILURE);
    }


    server_log_ptr = fopen(SERVER_LOG_NAME, "w+");

    fprintf(server_log_ptr, "******************* IntegralGen ********************\n");
    fprintf(server_log_ptr, "Server pid : %d\n", server_pid);
    fprintf(server_log_ptr, "****************************************************\n\n");

    fclose(server_log_ptr);

    fprintf(stderr, "\n***************** Server pid : %d *****************\n", server_pid);



    /*Server*/
    while (1) {


        server_fiford = open(SERVER_FIFO_NAME, O_RDWR);
        if (read(server_fiford, &client_pid, sizeof (pid_t)) < 0) {
            if (errno != EINTR) {
                perror("Read from server fifo");
                unlink(SERVER_FIFO_NAME);
                exit(EXIT_FAILURE);
            }

            continue;
        }




        if (client_counter >= max_client) {

            status = -1;

            sprintf(client_fifo_name, "Client%d", client_pid);

            client_fifowr = open(client_fifo_name, O_WRONLY);
            if (write(client_fifowr, &status, sizeof (int)) < 0) {
                perror("Write to server client fifo");
                unlink(SERVER_FIFO_NAME);
                exit(EXIT_FAILURE);
            }
            close(client_fifowr);


        } else {

            status = 1;

            sprintf(client_fifo_name, "Client%d", client_pid);

            client_fifowr = open(client_fifo_name, O_WRONLY);
            if (write(client_fifowr, &status, sizeof (int)) < 0) {
                perror("Write to server client fifo");
                unlink(SERVER_FIFO_NAME);
                exit(EXIT_FAILURE);
            }
            close(client_fifowr);

            /*Get conncetion time client to server*/
            gettimeofday(&connection_time, NULL);

            current_time = time(NULL);

            /*Calculate difference server start time and client connection time*/
            t0 = timedifference_seconds(start_time, connection_time);

            /*Save client pid*/
            client_pid_arr[client_counter++] = client_pid;

            fprintf(stderr, "\n##### Connection #####");
            fprintf(stderr, "\nClient pid           : %d", client_pid);
            fprintf(stderr, "\nConnection time (t0) : %f\n", t0);

            /*Helper process fork*/
            child_pid = fork();


            if (child_pid == -1) {
                perror("Failed to fork");
                unlink(SERVER_FIFO_NAME);
                exit(EXIT_FAILURE);

            } else if (child_pid == 0) { /*Helper process*/

                helper_process(client_pid, resolution, t0, functions_array);

                exit(EXIT_SUCCESS);

            } else {


            }

        }

    }

    return 0;
}
/*************************************** END SERVER ********************************************/

/*
 * Helper process remains in contact with client process.
 * Helper process generates calculate process to calculations.
 * Helper process get results from calculate process and sends to client process
 */
void helper_process(pid_t client_pid, double resolution, double t0, char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]) {


    int client_fifowr; /*File descriptor for client fifo (write)*/
    int client_fiford; /*File descriptor for client fifo (write)*/

    struct message client_message; /*Client message information*/
    struct sigaction act;

    char compile_arguments[BUFFER]; /*Compile arguments for execl*/
    char client_fifo_name[BUFFER]; /*Client fifo name*/

    double result; /*Result of integral*/

    pid_t this_pid; /*Helper process pid*/
    pid_t child_pid; /*Calculate process pid*/

    char a[BUFFER];

    int pipe_array[2]; /*Pipe connection between helper process and calculate process*/



    act.sa_handler = sigpipe_handler;
    act.sa_flags = 0;


    /*Set SIGPIPE signal*/
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGPIPE, &act, NULL) == -1)) {
        perror("Failed to signal handler");
        exit(EXIT_FAILURE);
    }

    /*Set client fifo name*/
    sprintf(client_fifo_name, "Client%d", client_pid);

    /*Open client fifo*/
    client_fiford = open(client_fifo_name, O_RDONLY);
    if (read(client_fiford, &client_message, sizeof (struct message)) < 0) {
        perror("Read from server client fifo");
        exit(EXIT_FAILURE);
    }
    close(client_fiford);

    appeand_log(client_pid, t0, client_message, resolution, current_time);

    /*Creates necessary files*/
    sprintf(source_file_name, "source%d.c", client_pid);
    sprintf(executable_file_name, "exe%d", client_pid);
    sprintf(redirection_file_name, "temp.txt");

    /*Set compile argument string*/
    sprintf(compile_arguments, "gcc -o %s %s -lm 2> %s", executable_file_name, source_file_name, redirection_file_name);

    /*Pipe connection between helper process and calculate process*/
    pipe(pipe_array);

    /*Creates calculate process source file*/
    create_source_file(client_message, resolution, pipe_array[1], t0, functions_array);


    /*Compile process fork*/
    child_pid = fork();

    if (child_pid == -1) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    } else if (child_pid == 0) { /*Compile process*/
        if (execl("/bin/sh", "/bin/sh", "-c", compile_arguments, NULL) < 0) {
            perror("execl");
            unlink(source_file_name);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);

    } else {
        wait(NULL);
    }


    /*Calculate process fork*/
    child_pid = fork();

    if (child_pid == -1) {
        perror("Failed to fork");
        unlink(source_file_name);
        exit(EXIT_FAILURE);
    } else if (child_pid == 0) { /*Calculate process*/

        /*If executable file does not exist. There are problems in compile*/
        if (execl(executable_file_name, executable_file_name, NULL) < 0) {

            fprintf(stderr, "\nInvalid function format! (Bad input)\n");
            fprintf(stderr, "\nCalculate process has been failed...\n\n");

            /*Sends signal for invalid function format*/
            kill(client_pid, SIGUSR2);

            unlink_files();
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);

    } else {



        client_fifowr = open(client_fifo_name, O_WRONLY);

        /*Reads and sends result of integral*/
        while (1) {

            /*Helper process -between- Calculate process*/
            read(pipe_array[0], &result, sizeof (double));

            /*Control divide by zero*/
            if (isinf(result) || isnan(result)) {

                fprintf(stderr, "\nDivide by zero error!\n");
                fprintf(stderr, "Calculate process has been failed...\n\n");

                /*Sends signal for divide by zero*/
                kill(client_pid, SIGUSR3);

                unlink_files();
                exit(EXIT_FAILURE);
            }

            /*Helper process -between- Client process*/
            if (write(client_fifowr, &result, sizeof (double)) < 0) {
                fprintf(stderr, "\nClient disconnected! Calculate process terminated...\n\n");
                unlink_files();
                exit(EXIT_FAILURE);
            }

            /* fprintf(stderr, "%f\n", result);*/

        }

        close(client_fifowr);

        wait(NULL);
    }


}

/*
 * Reads funtions in functions txt (One time -when the server starts-)
 */
void read_operands(char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]) {

    FILE *file_ptr;
    char file_name[BUFFER];

    int i = 0, j = 0;

    for (i = 0; i < 6; ++i) {

        sprintf(file_name, "f%d.txt", i + 1);

        file_ptr = fopen(file_name, "r");

        while (fscanf(file_ptr, "%c", &functions_array[i][j++]) != EOF);

        functions_array[i][j - 1] = '\0';

        fclose(file_ptr);

        j = 0;

    }


}

/*
 * Generates a source file to calculation (Calculation process)
 */
void create_source_file(struct message client_message, double resolution, int pipe_fd, double t0, char functions_array[NUMBER_OF_FUNCTIONS][BUFFER]) {

    FILE * source_ptr;

    char * fi;
    char * fj;

    fi = functions_array[(client_message.fi[1] - '0') - 1];
    fj = functions_array[(client_message.fj[1] - '0') - 1];

    source_ptr = fopen(source_file_name, "w+");

    fprintf(source_ptr, "#include <stdio.h>\n");
    fprintf(source_ptr, "#include <stdlib.h>\n");
    fprintf(source_ptr, "#include <unistd.h>\n");
    fprintf(source_ptr, "#include <fcntl.h>\n");
    fprintf(source_ptr, "#include <math.h>\n\n\n");

    fprintf(source_ptr, "int main(int argc, char **argv){\n\n");
    fprintf(source_ptr, "   int fifo_write;\n");
    fprintf(source_ptr, "   double result = 0;\n");
    fprintf(source_ptr, "   double t, k;\n\n");
    fprintf(source_ptr, "   for(t = %f, k=t; ; t+=%f){\n\n", t0, resolution * 0.001);
    fprintf(source_ptr, "       result += ((%s) %c (%s)) * %f;\n\n", fi, client_message.operator,fj, (resolution * 0.001));
    fprintf(source_ptr, "       if(t >= k + %f){\n", client_message.time_interval);
    fprintf(source_ptr, "       write(%d, &result, sizeof(double));\n\n", pipe_fd);
    fprintf(source_ptr, " k=t;\n\n} ");
    fprintf(source_ptr, "    }\n\n\n");
    fprintf(source_ptr, "   printf(%c%cf%c, result);\n\n\n", '"', '%', '"');
    fprintf(source_ptr, "   return 0;\n");
    fprintf(source_ptr, "}\n");

    fclose(source_ptr);

}

/*
 * Checks main arguments validity
 */
void check_arguments(int argc, char **argv) {

    int i;

    if (argc != 3) {

        fprintf(stderr, "\n#################### USAGE #########################\n");
        fprintf(stderr, "\nYou need enter 2 arguments >");
        fprintf(stderr, "\n<resolution>            : unit timer step in terms of miliseconds as a first argument");
        fprintf(stderr, "\n<max number of clients> : max number of clients which can be connected as a second argument");
        fprintf(stderr, "\n\n-Sample");
        fprintf(stderr, "\n./IntegralGen 1 20\n");

        exit(EXIT_FAILURE);
    }


    for (i = 0; i < strlen(argv[1]); ++i) {
        if (!isdigit(argv[1][i]) && argv[1][i] != '.' && argv[1][i] != '-') {
            fprintf(stderr, "Invalid argument!\n");
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < strlen(argv[2]); ++i) {
        if (!isdigit(argv[2][i]) && argv[2][i] != '.' && argv[2][i] != '-') {
            fprintf(stderr, "\nInvalid argument!\n\n");
            exit(EXIT_FAILURE);
        }
    }


}

/*
 * Signal handler for server process and helper process
 */
static void signal_handler(int signal_number) {

    int i;

    /*Waits all childs*/
    while (wait(NULL) != -1);


    if (getpid() == server_pid && signal_number == SIGINT) {

        /*Server process sends SIGINT signal to client process*/
        for (i = 0; i < client_counter; ++i) {
            kill(client_pid_arr[i], SIGUSR1);
        }

        fprintf(stderr, "\nIntegralGen (Server) : SIGINT signal detected! Terminate the program...\n\n");

        unlink(SERVER_FIFO_NAME);

    } else if (getpid() == server_pid && (signal_number == SIGQUIT || signal_number == SIGHUP)) {

        /*Server process sends SIGQUIT signal to client process*/
        for (i = 0; i < client_counter; ++i) {
            kill(client_pid_arr[i], SIGQUIT);
            fprintf(stderr, "+\n");
        }

        fprintf(stderr, "\nIntegralGen (Server) : Unexpected termination!\n\n");

        unlink(SERVER_FIFO_NAME);

    } else {
        unlink_files();
    }

    exit(EXIT_FAILURE);

}

/*
 * Signal handler for server process to max client limit
 */
static void max_client_handler(int signal_no) {

    sigset_t block_mask;

    --client_counter;

    /*Masking SIGUSER1 signal*/
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);
}

/*
 * Signal handler for helper process to determine lost the connection between the helper process and the client
 */
static void sigpipe_handler(int signal_number) {

    fprintf(stderr, "\nIntegralGen (Server) : Client not responding! Terminate the calculate process...\n");

    unlink_files();

    kill(server_pid, SIGUSR1);

    raise(SIGINT);

}

/*
 * Unliks source file, executable file and temporaray file
 */
void unlink_files() {

    unlink(source_file_name);
    unlink(executable_file_name);
    unlink(redirection_file_name);

}

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_seconds(struct timeval start, struct timeval finish) {

    return ( (finish.tv_sec - start.tv_sec) * 1000.0f + (finish.tv_usec - start.tv_usec) / 1000.0f) / 1000;

}

void appeand_log(pid_t client_pid, double t0, struct message client_message, double resolution, time_t connection_time) {

    FILE * server_log_ptr;

    server_log_ptr = fopen(SERVER_LOG_NAME, "a");

    fprintf(server_log_ptr, "****************************************************\n");
    fprintf(server_log_ptr, "Client pid                : %d                      \n", client_pid);
    fprintf(server_log_ptr, "Connection time to server : %s", ctime(&current_time));
    fprintf(server_log_ptr, "Function names            : %s and %s               \n", client_message.fi, client_message.fj);
    fprintf(server_log_ptr, "Operator                  : %c                      \n", client_message.operator);
    fprintf(server_log_ptr, "Resolution                : %f                      \n", resolution);
    fprintf(server_log_ptr, "Time interval             : %f                      \n", client_message.time_interval);
    fprintf(server_log_ptr, "t0                        : %f                      \n", t0);
    fprintf(server_log_ptr, "****************************************************\n");


    fclose(server_log_ptr);
}
