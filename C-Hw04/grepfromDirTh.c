/*############################################################################*/
/* HW04_Furkan_Erdol_131044065                                                */
/*                                                                            */
/* grepfromDirTh.c                                                            */
/* ______________________________                                             */
/* Written by Furkan Erdol on April 24, 2016                                  */
/* Description                                                                */
/*_______________                                                             */
/* Grep From Directory with Threads                                           */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/errno.h>
#include <pthread.h>

#define BUFFER 512
#define LOG_FILE "gfD.log"

/*Necessary grep information for thread process*/
struct GrepData {
    char file_path[BUFFER];
    char grep_name[BUFFER];
    int pipe_arr[2];
};

char fifo_name_array[BUFFER][BUFFER];
int fifo_name_counter = 0;
static volatile sig_atomic_t main_pid_flag;


/* Takes a file name as an input parameter. It returns number of character in file. */
int numberOfFileSize(const char *file_name);

/* Takes two file names and one word as a string. */
/* Finds number of target string in input file and writes to output file it's coordinates */
int grepFromFile(const char *input_file_name, const char *output_file_name, const char *target_string);

/* If path is not a directory returns 0 otherwise returns directory's st_mod number */
int isdirectory(const char *path);

/* To go in all subdirectories and returns total grep number as recursively */
int grepfromDir(char *directory_path, const char *grep_name);

/* Finds number of directory and file */
void numberOfDirectoryAndFile(const char *directory_path, int *idrec_counter, int *ifile_counter);

/* Create fifos */
void create_fifos(const char *directory_path, char charArr[BUFFER][BUFFER]);

/*SIGINT handler*/
static void signal_handler(int signal_number);

/*Grep from file function for thread process*/
void * grep_from_file_th(void *grep_data);



int main(int argc, char** argv) {


    int totalGrep = 0;
    struct sigaction act;

    act.sa_handler = signal_handler;
    act.sa_flags = 0;

    main_pid_flag = getpid();


    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
        perror("Failed to SIGINT handler");
        return 1;
    }


    if (argc != 3) {

        printf("\n#################### USAGE #########################\n");
        printf("\n-You need enter the directory name as first argument");
        printf("\n-You need enter the word name who you want search in directory as second argument");
        printf("\n\n-Sample");
        printf("\n./grepfromDir sample targetWord\n");

    } else {

        totalGrep = grepfromDir(argv[1], argv[2]);

        fprintf(stdout, "\nNumber of '%s' in '%s' = %d\n\n", argv[2], argv[1], totalGrep);

    }


    return 0;
}



int grepfromDir(char *directory_path, const char *grep_name) {

    int temp_grep = 0;
    int total_grep_dir = 0;
    int totalGrep = 0;

    int idirec_counter = 0; /*Number of directory*/
    int ifile_counter = 0; /*Number of file*/

    char fifo_name[BUFFER];
    char fifo_name_arr[BUFFER][BUFFER];

    DIR *directory_pointer;
    struct dirent *dirent_pointer;
    char sub_directory_path[BUFFER];

    int fifowr; /*File descriptor to read from fifo*/
    int fiford; /*File descriptor to read write fifo*/

    int i = 0;
    int temp = 0;
    int thread_counter = 0; /*Thread process counter*/

    int status; /*Status fork process*/

    char direc_name[BUFFER];
    const char s[2] = "/";
    char *token;

    pid_t child;

    int pipe_array[2];

    pthread_t thread_array[BUFFER]; /*Thread array*/
    struct GrepData grep_data_array[BUFFER]; /*GrepData structure array to send necessary grep information thread process*/


    /*Finds directory and file number in directory path*/
    numberOfDirectoryAndFile(directory_path, &idirec_counter, &ifile_counter);

    /*Creates fifos*/
    create_fifos(directory_path, fifo_name_arr);


    /*Creates pipe*/
    if (ifile_counter > 0) {
        if (pipe(pipe_array) == -1) {
            perror("Pipe failed...");
            exit(EXIT_FAILURE);
        }
        /*Writes temporary zero value*/
        write(pipe_array[1], &temp, sizeof (int));

    }


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

                if (isdirectory(sub_directory_path) != 0) { /*Directory*/

                    /*Fork process for directory*/
                    child = fork();

                    if (child == 0) {

                        grepfromDir(sub_directory_path, grep_name);

                        exit(EXIT_SUCCESS);

                    }

                } else if (isdirectory(sub_directory_path) == 0) { /*File*/

                    sprintf(grep_data_array[thread_counter].grep_name, "%s", grep_name);

                    grep_data_array[thread_counter].pipe_arr[0] = pipe_array[0];
                    grep_data_array[thread_counter].pipe_arr[1] = pipe_array[1];

                    sprintf(grep_data_array[thread_counter].file_path, "%s", sub_directory_path);


                    /*Thread process for file*/
                    if (pthread_create(&thread_array[thread_counter], NULL, grep_from_file_th, &grep_data_array[thread_counter])) {
                        fprintf(stderr, "Error in pthread_create!\n");
                        exit(EXIT_FAILURE);
                    }

                    ++thread_counter;

                }

            }

        }

        /*Close directories*/
        closedir(directory_pointer);
    }


    /*Reads grep numbers in fifo*/
    for (i = 0; i < idirec_counter; ++i) {

        sprintf(fifo_name, "%s", fifo_name_arr[i]);

        if ((fiford = open(fifo_name, O_RDONLY)) != -1) {
            read(fiford, &temp_grep, sizeof (int));
            close(fiford);

            /*Unlink fifo*/
            unlink(fifo_name);

            total_grep_dir += temp_grep;

        }
    }


    /*Waits all thread process*/
    for (i = 0; i < thread_counter; ++i) {
        if (pthread_join(thread_array[i], NULL) != 0) {
            fprintf(stderr, "Some error in wait thread process!");
            exit(EXIT_FAILURE);
        }
    }


    /*Waits all child*/
    while (wait(&status) != -1) {

        if (status != EXIT_SUCCESS) {
            fprintf(stderr, "Error in fork procces!----%d\n", status);
        }

    }


    /*Reads grep number from pipe*/
    if (ifile_counter > 0) {
        read(pipe_array[0], &temp_grep, sizeof (int));

    }


    /* Total dir grep */
    totalGrep = temp_grep + total_grep_dir;


    /* Wirtes to fifos if process is not root procces */
    if (getpid() != main_pid_flag) {

        token = strtok(directory_path, s);

        while (token != NULL) {
            strcpy(direc_name, token);

            token = strtok(NULL, s);

        }

        sprintf(fifo_name, "%d-%s", getppid(), direc_name);
        fifowr = open(fifo_name, O_WRONLY);
        write(fifowr, &totalGrep, sizeof (int));
        close(fifowr);

    }

    return totalGrep;
}


void numberOfDirectoryAndFile(const char *directory_path, int *idrec_counter, int *ifile_counter) {

    DIR *directory_pointer;
    struct dirent *dirent_pointer;
    char sub_directory_path[BUFFER];

    directory_pointer = opendir(directory_path);


    if (directory_pointer == NULL) {

        fprintf(stderr, "\nDirectory couldn't open..!\n");

    } else {

        while ((dirent_pointer = readdir(directory_pointer)) != NULL) {

            if (strcmp(dirent_pointer->d_name, ".") != 0 && strcmp(dirent_pointer->d_name, "..") != 0) {

                sprintf(sub_directory_path, "%s/%s", directory_path, dirent_pointer->d_name);

                if (isdirectory(sub_directory_path) != 0) {
                    ++(*idrec_counter);
                    numberOfDirectoryAndFile(sub_directory_path, idrec_counter, ifile_counter);
                } else if (dirent_pointer->d_name[strlen(dirent_pointer->d_name) - 1] != '~') {
                    ++(*ifile_counter);
                }

            }
        }

        closedir(directory_pointer);
    }

}


void create_fifos(const char *directory_path, char charArr[BUFFER][BUFFER]) {

    DIR *directory_pointer;
    struct dirent *dirent_pointer;
    char sub_directory_path[BUFFER];

    char fifo_name[BUFFER];
    int i = 0;

    directory_pointer = opendir(directory_path);


    if (directory_pointer == NULL) {

        fprintf(stderr, "\nDirectory couldn't open..!\n");

    } else {

        while ((dirent_pointer = readdir(directory_pointer)) != NULL) {

            if (strcmp(dirent_pointer->d_name, ".") != 0 && strcmp(dirent_pointer->d_name, "..") != 0) {

                sprintf(sub_directory_path, "%s/%s", directory_path, dirent_pointer->d_name);

                if (isdirectory(sub_directory_path) != 0) {

                    sprintf(fifo_name, "%d-%s", getpid(), dirent_pointer->d_name);
                    strcpy(charArr[i++], fifo_name);

                    if (mkfifo(fifo_name, 0666) == -1) {
                        perror("Failed to create FIFO...");
                        exit(EXIT_FAILURE);
                    }

                    sprintf(fifo_name_array[fifo_name_counter++], "%s", fifo_name);

                }
            }
        }

        closedir(directory_pointer);
    }

}


int isdirectory(const char *path) {
    struct stat temp_stat;

    /* Get info */
    if (stat(path, &temp_stat) == -1)
        return 0;
    else
        return S_ISDIR(temp_stat.st_mode); /* Returns directory's info */
}


int numberOfFileSize(const char *file_name) {

    FILE* file_pointer;
    char temp_char;
    int counter_character = -1;

    file_pointer = fopen(file_name, "r");
    if (file_pointer == NULL) {
        printf("\nFile couldn't open..!\n\n");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file_pointer, "%c", &temp_char) != EOF) {
        ++counter_character;
    }

    fclose(file_pointer);



    return counter_character;
}


int grepFromFile(const char *input_file_name, const char *output_file_name, const char *target_string) {

    FILE* input_file_pointer;
    FILE* output_file_pointer;

    char *char_arr; /* Characters in the file */
    char temp_char; /* For read character from file */

    int file_size;
    int i = 0, j = 0;
    int counter_word = 0; /* Number of target words */
    int line = 1, column = 1; /* File's line and column for determine target point */
    int founded = 0; /* If available target word in file founded = 1 otherwise founded = 0 */

    /* Finds file size */
    file_size = numberOfFileSize(input_file_name);

    if (file_size == -1) {
        return 0;
    }

    /* Open input file */
    input_file_pointer = fopen(input_file_name, "r");
    if (input_file_pointer == NULL) {
        printf("\nFile couldn't open..!\n\n");
        exit(1);
    }

    /* Dynamic allocation */
    char_arr = (char*) malloc(file_size * sizeof (char));

    /* Reads file into char_arr character by character */
    i = 0;
    while (file_size > 0) {

        fscanf(input_file_pointer, "%c", &temp_char);
        char_arr[i] = temp_char;

        --file_size;
        ++i;
    }

    /* For string */
    char_arr[i] = '\0';

    fclose(input_file_pointer);

    /* Open gff.log file */
    output_file_pointer = fopen(LOG_FILE, "a+");
    if (output_file_pointer == NULL) {
        printf("\nFile couldn't open..!\n\n");
        exit(1);
    }

    fprintf(output_file_pointer, "###### File name : %s ----> Target word : %s\n", input_file_name, target_string);
    fprintf(output_file_pointer, "<><><><><><><><><>\n");



    /* Finds number of target string in character array and writes to file it's coordinates */
    i = 0;
    while (char_arr[i + strlen(target_string) - 1 ] != '\0') {

        if (char_arr[i] == target_string[0]) {

            founded = 1;
            for (j = 1; j < strlen(target_string) && char_arr[i + j] != '\0'; ++j) {
                if (char_arr[i + j] != target_string[j])
                    founded = 0;
            }

            if (founded == 1) {
                ++counter_word;
                fprintf(output_file_pointer, "(%d) Line = %-3d   Column = %-3d\n", counter_word, line, column);

            }

            founded = 0;

        }

        if (char_arr[i] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }

        ++i;
    }

    fprintf(output_file_pointer, "\n");

    if (counter_word == 0) {
        fprintf(output_file_pointer, "Could not find the target word!\n\n");
    }

    /* Free dynamic memory */
    free(char_arr);

    fclose(output_file_pointer);

    return counter_word;
}


static void signal_handler(int signal_number) {

    FILE *log_file_pointer;
    int i;

    /*Wait childs*/
    while (wait(NULL) != -1);


    if (getpid() == main_pid_flag) {

        log_file_pointer = fopen(LOG_FILE, "a+");
        fprintf(log_file_pointer, "\nSIGINT signal detected!\nTerminate the program...\n");
        fclose(log_file_pointer);

        /*Unlink junk files*/
        for (i = 0; i < fifo_name_counter; ++i) {
            unlink(fifo_name_array[i]);

        }
    }

    exit(EXIT_FAILURE);
}


void * grep_from_file_th(void *grep_data) {

    struct GrepData *data = (struct GrepData*) grep_data;

    int temp_grep;
    int total_grep;

    temp_grep = grepFromFile(data->file_path, LOG_FILE, data->grep_name);

    read(data->pipe_arr[0], &total_grep, sizeof (int));

    total_grep += temp_grep;

    write(data->pipe_arr[1], &total_grep, sizeof (int));

    return 0;

}

