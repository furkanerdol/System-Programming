/*############################################################################*/
/* Final_Furkan_Erdol_131044065                                               */
/*                                                                            */
/* client.c                                                                   */
/* ______________________________                                             */
/* Written by Furkan Erdol on May 29, 2016                                    */
/* Description                                                                */
/*_______________                                                             */
/* Client                                                                     */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/errno.h>

/*Message defines*/
#define TRANSFER_COMPLETE '0'
#define CLIENTS_INFORMATION '1'
#define SEND_FILE '2'
#define SERVER_FILE_LIST '3'
#define DEAD_MESSAGE '5'
#define RECEIVE_FILE '7'
#define CLIENT_DISCONNECT '8'
#define MESSAGE_INITIALIZE 'F';

/*Maximum file name length*/
#define MAX_FILE_LENGHT 96

/*Some color*/
#define RED_TEXT(x) "\033[31;1m" x "\033[0m"
#define GREEN_TEXT(x) "\033[32;1m" x "\033[0m"
#define BLUE_TEXT(x) "\033[34;1m" x "\033[0m"

/*Send file information for send file thread*/
struct SendFileInformation {
    char file_name[MAX_FILE_LENGHT];
    int client_id;
};


static volatile main_pid; /*Main pid*/ 
static volatile read_file_thread; /*Read file thread self id*/
static volatile flag = 0; /*Process flag to finish complete current process*/
int client_counter = 0; /*Client counter*/
int server_socket_fd; /*Server socket file descriptor*/

pthread_t waiting_thread;
pthread_t receive_file_thread;
pthread_t send_file_thread;

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; /*Client mutex to synchronization*/


/*
 * For listLocal request
 */
void listLocal();

/*
 * For listServer request
 */
void listServer();

/*
 * For lsClients request
 */
void lsClients(int this_client_id);

/*
 * For help request
 */
void help();

/*
 * Send file thread function
 */ 
void* sendFile(void *send_file_information);

/*
 * Thread function of waits server message 
 */
void* wait_for_read(void *data);

/*
 * read file thread fucntion
 */ 
void* read_file(void *data);

/*
 * Signal handler for client process
 */
static void signal_handler(int signal_number);

/*
 * Is directory function
 */
int is_directory(const char *path);



int main(int argc, char** argv) {

    struct sockaddr_in server_addr;
    struct hostent *server;

    pid_t client_pid;
    int client_id;

    char stdin_array[MAX_FILE_LENGHT];
    char client_id_string[MAX_FILE_LENGHT];

    char file_name[MAX_FILE_LENGHT];
    
    const char s[2] = " ";
    char *token;

    struct sigaction act;

    struct SendFileInformation * send_information; /*Information data for send file thread*/

    char* ip_address;
    int port_number;

    int i; /*For loops*/
    int send_client_id = 0;


    if (argc != 3) {

        fprintf(stderr, "\n###################### USAGE ##################################\n");
        fprintf(stderr, "\nYou need enter 2 arguments >");
        fprintf(stderr, "\n<ip address>            : ip address for client");
        fprintf(stderr, "\n<port number>           : port number to connect server process");
        fprintf(stderr, "\n\n-Sample");
        fprintf(stderr, "\n./client 127.0.0.1 1071\n");

        exit(EXIT_FAILURE);
    }

    /*Gets main arguments*/
    ip_address = argv[1];
    port_number = atoi(argv[2]);


    /*Set signal handler*/
    act.sa_handler = signal_handler;
    act.sa_flags = 0;

    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)  || (sigaction(SIGUSR1, &act, NULL) == -1) 
                                          || (sigaction(SIGPIPE, &act, NULL) == -1) || (sigaction(SIGQUIT, &act, NULL) == -1)
                                          || (sigaction(SIGHUP, &act, NULL) == -1)  || (sigaction(SIGTERM, &act, NULL) == -1)) {
        perror("Signal set error!");
        exit(EXIT_FAILURE);
    }

    /*Get process pid*/
    client_pid = getpid();
    main_pid = getpid();

    /*Socket setup*/
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_fd < 0) {
        perror("Create socket error!");
        exit(EXIT_FAILURE);
    }

    /*Memory initialize*/
    memset(&server_addr, 0, sizeof (struct sockaddr_in));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    inet_pton(AF_INET, ip_address, &server_addr.sin_addr.s_addr);


    if (connect(server_socket_fd, &server_addr, sizeof (server_addr)) < 0) {
        fprintf(stderr, RED_TEXT("\nClient : ") "Server is not available! Please check your connection and try again.\n\n");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    write(server_socket_fd, &client_pid, sizeof (pid_t));
    read(server_socket_fd, &client_id, sizeof (int));

    fprintf(stderr, "\n############## Client pid : %d ##################\n", getpid());
    fprintf(stderr, "\nConnection to the server : Successful");
    fprintf(stderr, "\nClient id                : %d\n", client_id);


    if (pthread_create(&waiting_thread, NULL, wait_for_read, NULL) != 0) {
        perror("Error in pthread_create!");
        exit(EXIT_FAILURE);
    }


    while (1) {

        /*Commend line*/
        fprintf(stderr, BLUE_TEXT("\nFileShareServer") RED_TEXT("/client") "$ ");

        /*Get commend*/
        fgets(stdin_array, 98, stdin);

        pthread_mutex_lock(&client_mutex);


        /*Determine process type*/
        if (strcmp(stdin_array, "listLocal\n") == 0) {
            listLocal();
        } else if (strcmp(stdin_array, "listServer\n") == 0) {
            listServer();
        } else if (strcmp(stdin_array, "lsClients\n") == 0) {
            lsClients(client_id);
        } else if (strncmp(stdin_array, "sendFile", 8) == 0) {

            token = strtok(stdin_array, s);

            i = 0;
            while (token != NULL) {

                if (i == 1) {
                    strcpy(file_name, token);
                } else if (i == 2) {
                    strcpy(client_id_string, token);
                    send_client_id = atoi(token);

                }

                token = strtok(NULL, s);

                ++i;
            }

            if (i == 2) {
                send_client_id = -1;
                file_name[strlen(file_name) - 1] = '\0';
            } else if (i != 3) {
                fprintf(stderr, "Invalid command..!");
                pthread_mutex_unlock(&client_mutex);
                continue;
            } else if (send_client_id == 0) {
                if (strncmp(client_id_string, " ", 1) != 0) {
                    fprintf(stderr, "Invalid command..!");
                    pthread_mutex_unlock(&client_mutex);
                    continue;
                }
            }

            if (access(file_name, 0) == -1) {
                fprintf(stderr, RED_TEXT("\nClient : ") "No such file..! Please try again...\n");
                pthread_mutex_unlock(&client_mutex);
                continue;
            }

            if (send_client_id == client_id) {
                fprintf(stderr, RED_TEXT("\nClient : ") "Cannot send the file yourself..!\n");
                pthread_mutex_unlock(&client_mutex);
                continue;
            }


            /*Allocate memory*/
            send_information = (struct SendFileInformation *) calloc(1, sizeof (struct SendFileInformation));

            sprintf(send_information->file_name, "%s", file_name);
            send_information->client_id = send_client_id;

            /*Send file thread start*/
            if (pthread_create(&send_file_thread, NULL, sendFile, send_information) != 0) {
                perror("Error in pthread_create!");
                exit(EXIT_FAILURE);
            }

            /*Send file thread finish*/
            if (pthread_join(send_file_thread, NULL) != 0) {
                fprintf(stderr, "Some error in wait thread process!");
                exit(EXIT_FAILURE);
            }


        } else if (strcmp(stdin_array, "help\n") == 0) {
            help();
        } else {

            if (strcmp(stdin_array, "\n") == 0) {
                pthread_mutex_unlock(&client_mutex);
                continue;
            }

            fprintf(stderr, "Invalid command..!");
        }

        pthread_mutex_unlock(&client_mutex);

    }

    return 0;
}



/*
 * For listLocal request
 */
void listLocal() {

    DIR *directory_pointer;
    struct dirent *dirent_pointer;
    char sub_directory_path[256];

    char directory_path[256];
    int file_counter = 0;

    /*Get current directory*/
    getcwd(directory_path, sizeof (directory_path));

    /*Open directory*/
    directory_pointer = opendir(directory_path);

    if (directory_pointer == NULL) {

        fprintf(stderr, "\nDirectory couldn't open..!\n");

    } else {

        fprintf(stderr, "\n##### Local Files #####\n");

        while ((dirent_pointer = readdir(directory_pointer)) != NULL) {

            /*Skip for current directory and parent directory*/
            if (strcmp(dirent_pointer->d_name, ".") != 0 && strcmp(dirent_pointer->d_name, "..") != 0 && dirent_pointer->d_name[strlen(dirent_pointer->d_name) - 1] != '~') {

                /*Sub directory name ( sub_directory_path )*/
                sprintf(sub_directory_path, "%s/%s", directory_path, dirent_pointer->d_name);

                if (is_directory(sub_directory_path) == 0) {

                    ++file_counter;
                    fprintf(stderr, "\n%d - %s", file_counter, dirent_pointer->d_name);

                }
            }
        }

        fprintf(stderr, "\n");

        closedir(directory_pointer);
    }
}

/*
 * For listServer request
 */
void listServer() {

    char message[2];
    int read_result;
    int i;

    message[0] = SERVER_FILE_LIST;
    write(server_socket_fd, &message, 2 * sizeof (char));

    fprintf(stderr, "\n##### Server Files #####\n");

    for (i = 0; ; ++i) {

        if (message[0] == TRANSFER_COMPLETE) {
            break;
        }

        read_result = read(server_socket_fd, &message, 2 * sizeof (char));

        if (read_result == 0) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "Connection to the server was disconnected! Terminate the program...\n\n");
            exit(EXIT_FAILURE);
        }


        write(1, &message[1], sizeof (char));
    }
    message[1] = '\n';
    write(1, &message[1], sizeof (char));
}



void lsClients(int this_id) {

    pid_t client_id;
    char message[2];
    int read_result;
    int i;


    message[0] = CLIENTS_INFORMATION;
    write(server_socket_fd, &message, 2 * sizeof (char));

    client_id = 0;

    fprintf(stderr, "\n##### CLIENTS ##### (currently connected to the server)\n");


    for (i = 0; client_id != -1; ++i) {
        read_result = read(server_socket_fd, &client_id, sizeof (int));

        if (read_result == 0) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "Connection to the server was disconnected! Terminate the program...\n\n");
            exit(EXIT_FAILURE);
        }

        if (read_result == -1) {
            continue;
        }
        if (client_id != -1 && client_id != -2) {
            if (client_id == this_id) {
                fprintf(stderr, RED_TEXT("\nClient id: %d"), client_id);
            } else {
                fprintf(stderr, "\nClient id: %d", client_id);
            }

        }

    }

    fprintf(stderr, "\n");

}

/*
 * For help request
 */
void help() {

    fprintf(stderr, "\n##### Available Commends #####\n\n");

    fprintf(stderr, "- listLocal  : To list the local files\n");
    fprintf(stderr, "- listServer : To list the files in the current scope of the server-client\n");
    fprintf(stderr, "- lsClients  : Lists the clients currently connected to the server\n");
    fprintf(stderr, "- sendFile   : Send the file from local directory to the client with client id clientid\n");
    fprintf(stderr, "\n##### Usages #####\n\n");
    fprintf(stderr, "- $ listLocal\n");
    fprintf(stderr, "- $ listServer\n");
    fprintf(stderr, "- $ listClients\n");
    fprintf(stderr, "- $ sendFile file.txt 1071 | $ sendFile file.txt\n");

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
 * Send file thread function
 */ 
void* sendFile(void *data) {

    struct SendFileInformation * info = (struct SendFileInformation *) data;

    int file_descriptor;
    char message[2];
    int target_client_id;
    int client_status;
    int i;

    char* file_name = info->file_name;
    int send_client_id = info->client_id;

    sigset_t block_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &block_mask, NULL);

    flag = 1;

    message[0] = SEND_FILE;

    write(server_socket_fd, &message, 2 * sizeof (char));

    if (send_client_id == -1) {
        target_client_id = 0;
        write(server_socket_fd, &target_client_id, sizeof (int));
    } else {
        target_client_id = send_client_id;
        write(server_socket_fd, &target_client_id, sizeof (int));
    }

    read(server_socket_fd, &client_status, sizeof (int));

    if (client_status == -1) {
        fprintf(stderr, RED_TEXT("\nClient : ") "No such client! Please try again...\n");
        flag = 0;
        pthread_exit(NULL);
    }

    fprintf(stderr, RED_TEXT("\nClient : ") "File sending process was started...");
    fprintf(stderr, "\nFile name         : %s", file_name);
    if (send_client_id == -1) {
        fprintf(stderr, "\nTarget location   : Server");
    } else {
        fprintf(stderr, "\nTarget location   : Client %d", send_client_id);
    }


    for (i = 0; file_name[i] != '\0'; ++i) {
        write(server_socket_fd, &file_name[i], sizeof (char));
    }

    write(server_socket_fd, &file_name[i], sizeof (char));


    file_descriptor = open(file_name, O_RDONLY);

    if (file_descriptor == -1) {
        fprintf(stderr, RED_TEXT("\n\nClient : ") "File could not open! Terminate the program...\n\n");
        exit(EXIT_FAILURE);
    }

    while (read(file_descriptor, &message[1], sizeof (char)) > 0) {

            write(server_socket_fd, &message, 2 * sizeof (char));

    }

    message[0] = TRANSFER_COMPLETE;
    write(server_socket_fd, &message, sizeof (char));

    close(file_descriptor);

    fprintf(stderr, RED_TEXT("\nClient : ") "File transfer is successful\n");

    /*Free allocate memory*/
    free(data);

    flag = 0;

    return 0;

}

/*
 * Thread function of waits server message 
 */
void * wait_for_read(void *data) {

    char message[2];
    int read_result;

    sigset_t block_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &block_mask, NULL);


    message[0] = MESSAGE_INITIALIZE;

    read_file_thread = pthread_self();


    while (1) {


        pthread_mutex_lock(&client_mutex);

        read_result = recv(server_socket_fd, &message, 2 * sizeof (char), MSG_DONTWAIT);

        if (read_result == -1) {
            pthread_mutex_unlock(&client_mutex);
            continue;
        }

        if (message[0] == DEAD_MESSAGE) {
            kill(main_pid, SIGUSR1);
            pthread_exit(NULL);
        }

        if (read_result == 0) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "Connection to the server was disconnected! Terminate the program...\n\n");
            exit(EXIT_FAILURE);
        }


        if (message[0] == RECEIVE_FILE) {


            if (pthread_create(&receive_file_thread, NULL, read_file, NULL) != 0) {
                perror("Error in pthread_create!");
                exit(EXIT_FAILURE);
            }


            if (pthread_join(receive_file_thread, NULL) != 0) {
                perror("Some error in wait thread process!");
                exit(EXIT_FAILURE);
            }


            fprintf(stderr, BLUE_TEXT("\n\nFileShareServer") RED_TEXT("/client") "$ ");

        }

        pthread_mutex_unlock(&client_mutex);

        message[0] = MESSAGE_INITIALIZE;

    }

}

/*
 * read file thread fucntion
 */ 
void* read_file(void *data) {

    char message[2];
    char file_name[MAX_FILE_LENGHT];
    char temp_file_name[MAX_FILE_LENGHT];

    int file_descriptor;
    int open_flags;
    
    int counter = 1;
    int read_result;
    int i;

    mode_t file_permssions;


    fprintf(stderr, RED_TEXT("\n\nClient : ") "File receive process was started...");


    flag = 1;

    for (i = 0;; ++i) {
        read(server_socket_fd, &file_name[i], sizeof (char));
        if (file_name[i] == '\0') {
            break;
        }
    }


    open_flags = O_CREAT | O_WRONLY | O_TRUNC;
    /*File permission*/
    file_permssions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;



    fprintf(stderr, "\nFile name       : %s", file_name);

    sprintf(temp_file_name, "%s", file_name);

    while (access(temp_file_name, 0) != -1) {
        sprintf(temp_file_name, "%s(%d)", file_name, counter);
        ++counter;
    }


    sprintf(file_name, "%s", temp_file_name);

    file_descriptor = open(file_name, open_flags, file_permssions);

    if (file_descriptor == -1) {
        perror("New file could not open!");
        exit(EXIT_FAILURE);
    }

    while (1) {

        read_result = read(server_socket_fd, &message, 2 * sizeof (char));

        if (read_result == 0) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "File transfer failed! Connection to the server was disconnected! Terminate the program...\n\n");
            unlink(file_name);
            exit(EXIT_FAILURE);
        }


        if (message[0] == TRANSFER_COMPLETE) {
            break;
        }

        if (message[0] == DEAD_MESSAGE) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "File transfer failed! Connection to the server was disconnected! Terminate the program...\n\n");
            unlink(file_name);
            exit(EXIT_FAILURE);
        }

        if (message[0] == CLIENT_DISCONNECT) {
            fprintf(stderr, RED_TEXT("\n\nClient : ") "There was a problem while receiving files! File transfer failed..!");
            unlink(file_name);
            pthread_exit(NULL);
        }

        write(file_descriptor, &message[1], sizeof (char));

    }

    close(file_descriptor);


    fprintf(stderr, "\nName of saved   : %s", file_name);


    fprintf(stderr, RED_TEXT("\nClient : ") "File transfer is successful");

    flag = 0;

    return 0;

}

/*
 * Signal handler for client process
 */
static void signal_handler(int signal_number) {


    char message[2];

    sigset_t block_mask;

    if (signal_number == SIGINT) {

        sigemptyset(&block_mask);
        sigaddset(&block_mask, SIGINT);
        pthread_sigmask(SIG_BLOCK, &block_mask, NULL);

        while (flag);
    }


    if (signal_number == SIGUSR1 || signal_number == SIGINT) {

        pthread_cancel(waiting_thread);
        pthread_join(waiting_thread, NULL);

    }

    if (signal_number == SIGINT) {
        fprintf(stderr, RED_TEXT("\n\nClient : ") "SIGINT signal detected! Terminate the program...\n\n");

        message[0] = DEAD_MESSAGE;
        write(server_socket_fd, &message, 2 * sizeof (char));

        close(server_socket_fd);
        exit(EXIT_FAILURE);

    }

    if (signal_number == SIGUSR1) {
        fprintf(stderr, RED_TEXT("\n\nClient : ") "Server terminated! Terminate the program...\n\n");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    if (signal_number == SIGPIPE) {
        fprintf(stderr, RED_TEXT("\n\nClient : ") "File transfer failed! Connection to the server was disconnected! Terminate the program...\n\n");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    if(signal_number == SIGQUIT || signal_number == SIGTERM || signal_number == SIGHUP){
        fprintf(stderr, RED_TEXT("\n\nClient : ") "Unexpected termination...\n\n" );
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }
    

}



