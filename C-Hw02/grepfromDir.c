/*############################################################################*/
/* HW02_Furkan_Erdol_131044065                                                */
/*                                                                            */
/* grepfromDir.c                                                              */
/* ______________________________                                             */
/* Written by Furkan Erdol on March 19, 2016                                  */
/* Description                                                                */
/*_______________                                                             */
/* Grep From Directory                                                        */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h> /* exit, malloc */
#include <string.h> /* strlen */
#include <unistd.h> /* fork */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#define BUFFER 1024
#define LOG_FILE "gfD.log"
#define TEMP_FILE "temp.txt~"

/* Takes a file name as an input parameter. It returns number of character in file. */
int numberOfFileSize(const char *file_name);

/* Takes two file names and one word as a string. */
/* Finds number of target string in input file and writes to output file it's coordinates */
int grepFromFile(const char *input_file_name, const char *output_file_name, const char *target_string);

/* If path is not a directory returns 0 otherwise returns directory's st_mod number */
int isdirectory(const char *path);

/* To go in all subdirectories and returns total grep number as recursively */
int grepfromDir(const char *directory_path, const char *string);

int main(int argc, char** argv) {

    int number_of_word;

    FILE *file_pointer;

    if (argc != 3) {

        printf("\n#################### USAGE #########################\n");
        printf("\n-You need enter the directory name as first argument");
        printf("\n-You need enter the word name who you want search in directory as second argument");
        printf("\n\n-Sample");
        printf("\n./grepfromDir sample targetWord\n");

    } else {

        file_pointer = fopen(LOG_FILE, "r");

        if (file_pointer != NULL){
            remove(LOG_FILE);
            
        fclose(file_pointer);    
        }

        number_of_word = grepfromDir(argv[1], argv[2]);
        printf("\nNumber of '%s' in '%s' = %d\n\n", argv[2], argv[1], number_of_word);
    }

    return 0;
}

int grepfromDir(const char *directory_path, const char *string) {


    int temp_grep_child = 0; /* Temp grep counter for files */
    int temp_grep_parent_child = 0; /* temp grep counter for directories */
    int total_grep_child = 0; /* Grep counter for files */
    int total_grep_parent_child = 0; /* Grep counter for directories */

    pid_t child; /* For proccess to files */
    pid_t parent_child; /* For process to folders */

    DIR *directory_pointer;
    struct dirent *dirent_pointer;

    FILE *temp_file_pointer;

    char sub_directory_path[BUFFER]; /* Sub directory path */

    /* Open directory */
    directory_pointer = opendir(directory_path);

    if (directory_pointer == NULL) {

        printf("\nDirectory couldn't open..!\n");

    } else {

        while ((dirent_pointer = readdir(directory_pointer)) != NULL) {

            /* Skip for current directory and parent directory */
            if (strcmp(dirent_pointer->d_name, ".") != 0 && strcmp(dirent_pointer->d_name, "..") != 0) {

                /* Sub directory name ( sub_directory_path ) */
                sprintf(sub_directory_path, "%s/%s", directory_path, dirent_pointer->d_name);

                if (isdirectory(sub_directory_path) == 0 && dirent_pointer->d_name[strlen(dirent_pointer->d_name) - 1] != '~') {

                    child = fork();
                    if (child >= 0) {
                        if (child == 0) { /***** Child *****/

                            /* Grep from file */
                            temp_grep_child = grepFromFile(sub_directory_path, LOG_FILE, string);

                            temp_file_pointer = fopen(TEMP_FILE, "w");

                            fprintf(temp_file_pointer, "%d", temp_grep_child);
                            fclose(temp_file_pointer);

                            /* End child process */
                            exit(0);

                        } else { /***** Parent *****/

                            child = waitpid(child, NULL, 0);

                            temp_file_pointer = fopen(TEMP_FILE, "r");

                            if (temp_file_pointer != NULL) {

                                fscanf(temp_file_pointer, "%d", &temp_grep_parent_child);
                                fclose(temp_file_pointer);

                                /* Counts grep */
                                total_grep_child += temp_grep_parent_child;
                                remove(TEMP_FILE);

                            }
                        }
                    }
                } else if (isdirectory(sub_directory_path) != 0) {

                    parent_child = fork();
                    if (parent_child >= 0) {
                        if (parent_child == 0) { /***** Child ( parent for sub directory ) *****/

                            /* Recursive call */
                            total_grep_parent_child += grepfromDir(sub_directory_path, string);

                            temp_file_pointer = fopen(TEMP_FILE, "w");

                            fprintf(temp_file_pointer, "%d", total_grep_parent_child);
                            fclose(temp_file_pointer);

                            /* End child process */
                            exit(0);

                        } else { /***** Parent *****/

                            parent_child = waitpid(parent_child, NULL, 0);

                            temp_file_pointer = fopen(TEMP_FILE, "r");

                            if (temp_file_pointer != NULL) {
                                fscanf(temp_file_pointer, "%d", &temp_grep_parent_child);
                                fclose(temp_file_pointer);

                                /* Counts grep */
                                total_grep_child += temp_grep_parent_child;

                                /* Remove temp file */
                                remove(TEMP_FILE);
                            }
                        }
                    }
                }
            }
        }
        
        /* Close directories */
        closedir(directory_pointer);
    }

    return total_grep_child;
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
        exit(1);
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

    if (counter_word == 0) {
        fprintf(output_file_pointer, "Could not find the target word!\n");
    }

    /* Free dynamic memory */
    free(char_arr);

    fprintf(output_file_pointer, "\n");

    fclose(output_file_pointer);

    return counter_word;
}
