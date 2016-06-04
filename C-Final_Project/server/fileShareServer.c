/*############################################################################*/
/* Final_Furkan_Erdol_131044065                                               */
/*                                                                            */
/* fileShareServer                                                            */
/* ______________________________                                             */
/* Written by Furkan Erdol on May 29, 2016                                    */
/* Description                                                                */
/*_______________                                                             */
/* File Share Server                                                          */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/errno.h>

/*Maximum client number and maximum file name length*/
#define MAX_CLIENT 128
#define MAX_FILE_LENGHT 96

/*Message defines*/
#define TRANSFER_COMPLETE '0'
#define CLIENTS_INFORMATION '1'
#define RECEIVE_FILE '2'
#define SERVER_FILE_LIST '3'
#define DEAD_MESSAGE '5'
#define SEND_FILE '7'
#define CLIENT_DISCONNECT '8'
#define MESSAGE_INITIALIZE 'F'

/*Some color*/
#define BLUE_TEXT(x) "\033[34;1m" x "\033[0m"
#define MAGENTA_TEXT(x) "\033[35;1m" x "\033[0m"

/*Client information for helper server thread*/
struct ClientInformation {
    pid_t client_pid;
    int client_socket_fd;
    int client_id;
};

int socket_fd; /*Server socket file decriptor*/
int client_counter = 0; /*Client counter*/

/*The following arrays are paralel to each other*/
pid_t client_array[MAX_CLIENT]; /*Client array*/
pthread_t client_thread_array[MAX_CLIENT]; /*Thread array*/
pthread_mutex_t client_mutex_arr[MAX_CLIENT]; /*Mutex array*/
int client_socket_array[MAX_CLIENT]; /*Socket array*/

struct ClientInformation client_informaiton_array[MAX_CLIENT]; /*Client information structure array*/

struct timeval start_time; /*timeval structure*/


/*
 * Signal handler for server
 */
static void signal_handler(int signal_number);

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_milliseconds(struct timeval start, struct timeval finish);


/*
 * Helper process remains in contact with client process.
 */
void* helper_server(void* client_data);


/*
 * Determine given client id is connected
 */
int client_is_connect(int client_id);


/*
 * Sends to client file names in server's local directory 
 */
void send_server_local_list(int client_socket);


/*
 * Is directory function
 */
int is_directory(const char* path);


/*
 * Finds empty index in client array 
 */
int find_empty_index(pid_t* client_pid_array);


/*************************************** START SERVER ********************************************/
int main(int argc, char** argv) {

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    unsigned int client_len;

    int time_difference;

    int new_socket_fd;

    int client_id;
    pid_t server_pid;

    struct timeval connection_time; /*To get connection time clients*/

    pid_t client_pid; /*Client pid*/

    struct sigaction act;

    int socket_option = 1;

    int port_number;

    int client_index; /*Client index to client array*/

    /*Get start time to server*/
    gettimeofday(&start_time, NULL);


    if (argc != 2) {

        fprintf(stderr, "\n#################### USAGE #########################\n");
        fprintf(stderr, "\nYou need enter 1 argument >");
        fprintf(stderr, "\n<port number>            : port number for server process");
        fprintf(stderr, "\n\n-Sample");
        fprintf(stderr, "\n./fileShareServer 1995\n");

        exit(EXIT_FAILURE);
    }

    /*Get main argument*/
    port_number = atoi(argv[1]);

    /*Set signal handler*/
    act.sa_handler = signal_handler;
    act.sa_flags = 0;

    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1) || (sigaction(SIGPIPE, &act, NULL) == -1)) {
        perror("Signal set error!");
        exit(EXIT_FAILURE);
    }

    /*Get server pid*/
    server_pid = getpid();

    client_array[0] = -1;

    /*Socket setup*/
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        perror("Create socket error!");
        exit(EXIT_FAILURE);
    }

    /*Set socket option to reusable*/
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof (int));

    /*Memory initialize*/
    memset(&server_addr, 0, sizeof (struct sockaddr_in));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);


    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof (struct sockaddr_in)) < 0) {
        fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "This port is used!\n\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    listen(socket_fd, 5);


    fprintf(stderr, "\n############## Server pid : %d ##############\n", getpid());


    while (1) {

        client_len = sizeof (client_addr);
        /*Waits new client*/
        new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_len);

        if (new_socket_fd == -1) {
            continue;
        }

        read(new_socket_fd, &client_pid, sizeof (pid_t));

        client_index = find_empty_index(client_array);

        if (client_array[client_index] == -1) {
            client_array[client_index + 1] = -1;
        }

        client_array[client_index] = client_pid;
        client_id = client_index + 1;
        client_socket_array[client_index] = new_socket_fd;

        ++client_counter;

        write(new_socket_fd, &client_id, sizeof (int));

        /*Get connection time*/
        gettimeofday(&connection_time, NULL);

        time_difference = (int) timedifference_milliseconds(start_time, connection_time);

        fprintf(stderr, "\n##### Connection #####");
        fprintf(stderr, "\nClient id            : %d", client_id);
        fprintf(stderr, "\nConnection time      : %d ms (After server starts)\n", time_difference);


        client_informaiton_array[client_index].client_pid = client_pid;
        client_informaiton_array[client_index].client_id = client_id;
        client_informaiton_array[client_index].client_socket_fd = new_socket_fd;

        /*Create helper server*/
        if (pthread_create(&client_thread_array[client_index], NULL, helper_server, &client_informaiton_array[client_index]) != 0) {
            perror("Error in pthread_create!");
            exit(EXIT_FAILURE);
        }

        /*Initialize client mutex*/
        if (pthread_mutex_init(&client_mutex_arr[client_index], NULL) != 0) {
            perror("Mutex init errror!");
            exit(EXIT_FAILURE);
        }

    }



    return 0;
}
/*************************************** END SERVER ********************************************/


/*
 * Helper process remains in contact with client process.
 */
void * helper_server(void * client_data) {

    struct ClientInformation *data = (struct ClientInformation*) client_data;

    char message[2];

    char file_name_receive[MAX_FILE_LENGHT];
    char file_name_send[MAX_FILE_LENGHT];
    char temp_file_name[MAX_FILE_LENGHT];
    char old_file_name[MAX_FILE_LENGHT];

    int open_flags;
    int file_descriptor;
    struct timeval current_time;
    int time_difference;

    mode_t file_permissions;
    int temp = -1;
    int client_index;
    int read_result;
    int dead = -2;
    int counter = 1;
    int client_id;
    int read_result_two;
    int client_status;
    int i;

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block_mask, NULL);

    while (1) {

        message[0] = MESSAGE_INITIALIZE;

        read_result = read(data->client_socket_fd, &message, 2 * sizeof (char));


        pthread_mutex_lock(&client_mutex_arr[data->client_id - 1]);

        if (message[0] == CLIENTS_INFORMATION) {

            for (i = 0; client_array[i] != -1; ++i) {

                if (client_array[i] == -2) {
                    write(data->client_socket_fd, &dead, sizeof (int));
                } else {
                    client_id = i + 1;
                    write(data->client_socket_fd, &client_id, sizeof (int));
                }

            }

            write(data->client_socket_fd, &temp, sizeof (int));

        } else if (message[0] == RECEIVE_FILE) {

            read(data->client_socket_fd, &client_index, sizeof (int));

            client_status = client_is_connect(client_index);

            write(data->client_socket_fd, &client_status, sizeof (int));

            if (client_status == -1) {
                pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
                continue;
            }


            gettimeofday(&current_time, NULL);
            time_difference = (int) timedifference_milliseconds(start_time, current_time);
            fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "File retrieval process was started... " MAGENTA_TEXT("(%d ms)"), time_difference);


            client_index -= 1;


            if (client_index == -1) {

                for (i = 0;; ++i) {
                    read(data->client_socket_fd, &file_name_receive[i], sizeof (char));
                    if (file_name_receive[i] == '\0') {
                        break;
                    }
                }

                fprintf(stderr, "\nFile name            : %s", file_name_receive);
                fprintf(stderr, "\nTarget location      : Server");


                sprintf(old_file_name, "%s", file_name_receive);
                sprintf(temp_file_name, "%s", file_name_receive);

                while (access(temp_file_name, 0) != -1) {
                    sprintf(temp_file_name, "%s(%d)", file_name_receive, counter);
                    ++counter;
                }

                sprintf(file_name_receive, "%s", temp_file_name);


                open_flags = O_CREAT | O_WRONLY | O_TRUNC;
                file_permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;


                file_descriptor = open(file_name_receive, open_flags, file_permissions);

                if (file_descriptor == -1) {
                    perror("New file could not open!");
                    client_array[data->client_id - 1] = -2;
                    pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
                    pthread_exit(NULL);
                }

                while (1) {

                    read(data->client_socket_fd, &message, 2 * sizeof (char));

                    if (message[0] == TRANSFER_COMPLETE) {
                        break;
                    }

                    write(file_descriptor, &message[1], sizeof (char));
                }


                close(file_descriptor);

                fprintf(stderr, "\nName of saved        : %s", file_name_receive);

                gettimeofday(&current_time, NULL);
                time_difference = (int) timedifference_milliseconds(start_time, current_time);
                fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "File transfer is successful " MAGENTA_TEXT("(%d ms)\n"), time_difference);


            } else {

                pthread_mutex_lock(&client_mutex_arr[client_index]);

                message[0] = SEND_FILE;

                send(client_socket_array[client_index], &message, 2 * sizeof (char), 0);


                for (i = 0;; ++i) {
                    read(data->client_socket_fd, &file_name_send[i], sizeof (char));
                    if (file_name_send[i] == '\0') {
                        break;
                    }
                }

                fprintf(stderr, "\nFile name            : %s", file_name_send);

                fprintf(stderr, "\nTarget location      : Client %d", (client_index + 1));


                for (i = 0; file_name_send[i] != '\0'; ++i) {
                    write(client_socket_array[client_index], &file_name_send[i], sizeof (char));
                }

                write(client_socket_array[client_index], &file_name_send[i], sizeof (char));


                while (1) {

                    read_result = read(data->client_socket_fd, &message, 2 * sizeof (char));

                    if (message[0] == TRANSFER_COMPLETE) {
                        break;
                    }

                    if (read_result == 0) {
                        fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "Disconnected from client! File transfer process quits\n");
                        client_array[data->client_id - 1] = -2;
                        message[0] = CLIENT_DISCONNECT;
                        write(client_socket_array[client_index], &message, 2 * sizeof (char));
                        pthread_mutex_unlock(&client_mutex_arr[client_index]);
                        pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
                        pthread_exit(NULL);

                    }

                    write(client_socket_array[client_index], &message, 2 * sizeof (char));

                }


                write(client_socket_array[client_index], &message, 2 * sizeof (char));

                gettimeofday(&current_time, NULL);
                time_difference = (int) timedifference_milliseconds(start_time, current_time);
                fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "File transfer is successful " MAGENTA_TEXT("(%d ms)\n"), time_difference);

                pthread_mutex_unlock(&client_mutex_arr[client_index]);


            }


        } else if (message[0] == SERVER_FILE_LIST) {


            send_server_local_list(data->client_socket_fd);


        } else if (message[0] == DEAD_MESSAGE) {
            fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "Client %d was terminated!\n", data->client_id);
            client_array[data->client_id - 1] = -2;
            pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
            pthread_exit(NULL);

        } else if (message[0] == MESSAGE_INITIALIZE) {

        } else {

            read_result_two = recv(data->client_socket_fd, &message, 2 * sizeof (char), MSG_DONTWAIT);

            if (read_result_two == 0) {
                fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "Client %d was disconnected!\n", data->client_id);

            } else {
                fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "There was a problem during the exchange of data with the client %d! Disconnecting from server...\n", data->client_id);

            }

            client_array[data->client_id - 1] = -2;
            close(client_socket_array[data->client_id - 1]);
            pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
            pthread_exit(NULL);

        }

        if (read_result == 0) { /*BURAYA BAK*/
            fprintf(stderr, BLUE_TEXT("\nFileShareServer : ") "Client %d disconnected!\n", data->client_id);
            client_array[data->client_id - 1] = -2;
            pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);
            pthread_exit(NULL);
        }


        pthread_mutex_unlock(&client_mutex_arr[data->client_id - 1]);

    }

}

/*
 * Determine given client id is connected
 */
int client_is_connect(int client_id) {

    if (client_id > client_counter) {
        return -1;
    } else {

        if (client_array[client_id - 1] == -2) {
            return -1;
        } else {
            return 0;
        }
    }

}


/*
 * Sends to client file names in server's local directory 
 */
void send_server_local_list(int client_socket) {


    DIR *directory_pointer;
    struct dirent *dirent_pointer;
    char sub_directory_path[256];
    char directory_path[256];

    char temp[256];
    char message[2];

    int file_counter = 0;
    int i;

    getcwd(directory_path, sizeof (directory_path));

    message[0] = MESSAGE_INITIALIZE;

    /*Open directory*/
    directory_pointer = opendir(directory_path);

    if (directory_pointer == NULL) {

        fprintf(stderr, "\nDirectory couldn't open..!\n");

    } else {

        while ((dirent_pointer = readdir(directory_pointer)) != NULL) {

            /*Skip for current directory and parent directory*/
            if (strcmp(dirent_pointer->d_name, ".") != 0 && strcmp(dirent_pointer->d_name, "..") != 0 && dirent_pointer->d_name[strlen(dirent_pointer->d_name) - 1] != '~') {

                /*Sub directory name ( sub_directory_path )*/
                sprintf(sub_directory_path, "%s/%s", directory_path, dirent_pointer->d_name);

                if (is_directory(sub_directory_path) == 0) {

                    ++file_counter;
                    sprintf(temp, "\n%d - %s", file_counter, dirent_pointer->d_name);

                    for (i = 0;; ++i) {
                        message[1] = temp[i];
                        write(client_socket, &message, 2 * sizeof (char));

                        if (temp[i] == '\0') {
                            break;
                        }
                    }
                }
            }
        }

        message[0] = TRANSFER_COMPLETE;
        write(client_socket, &message, 2 * sizeof (char));

        closedir(directory_pointer);
    }
}

/*
 * Is directory function
 */
int is_directory(const char *path) {
    struct stat temp_stat;

    /* Get info */
    if (stat(path, &temp_stat) == -1)
        return 0;
    else
        return S_ISDIR(temp_stat.st_mode); /* Returns directory's info */
}

/*
 * Finds empty index in client array 
 */
int find_empty_index(pid_t * client_array) {

    int i;

    for (i = 0; client_array[i] != -1; ++i) {

        if (client_array[i] == -2) {
            return i;
        }
    }

    return i;

}

/*
 * Calculates different two times (Use timeval structure)
 */
double timedifference_milliseconds(struct timeval start, struct timeval finish) {

    return ( (finish.tv_sec - start.tv_sec) * 1000.0f + (finish.tv_usec - start.tv_usec) / 1000.0f);

}

/*
 * Signal handler for server
 */
static void signal_handler(int signal_number) {

    char message[2];
    int i;


    if (signal_number == SIGINT) {


        for (i = 0; client_array[i] != -1; ++i) {

            if (pthread_self() != client_thread_array[i]) {
                pthread_cancel(client_thread_array[i]);
                pthread_join(client_thread_array[i], NULL);

            }
        }


        message[0] = DEAD_MESSAGE;

        for (i = 0; client_array[i] != -1; ++i) {
            write(client_socket_array[i], &message, 2 * sizeof (char));
            close(client_socket_array[i]);
            pthread_mutex_destroy(&client_mutex_arr[i]);
        }


        fprintf(stderr, BLUE_TEXT("\n\nFileShareServer :") " SIGINT signal detected! Terminate the program...\n\n");

        close(socket_fd);

        exit(EXIT_FAILURE);

    }

}
