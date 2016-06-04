/*############################################################################*/
/* HW01_Furkan_Erdol_131044065                                                */
/*                                                                            */
/* grepFromFile.c                                                             */
/* ______________________________                                             */
/* Written by Furkan Erdol on March 10, 2016                                  */
/* Description                                                                */
/*_______________                                                             */
/* Grep From File                                                             */
/*............................................................................*/
/*                               Includes                                     */
/*............................................................................*/
#include <stdio.h>
#include <stdlib.h> /* For malloc and exit */
#include <string.h> /* For strlen */

#define FILE_NAME "gfF.log"

/* Takes a file name as an input parameter. It returns number of character in file. */
int numberOfFileSize(char *file_name);

/* Takes two file names and one word as a string. */
/* Finds number of target string in input file and writes to output file it's coordinates */
int grepFromFile(char *input_file_name, char *output_file_name, char *target_string);

int main(int argc, char** argv) {

    int number_of_word;

    if (argc != 3) {

        printf("\n#################### USAGE ####################\n");
        printf("\n-You need enter the file name as first argument");
        printf("\n-You need enter the word name who you want search in file as second argument");
        printf("\n\n-Sample");
        printf("\n./grepFromFile sample.txt targetWord\n");

    } else {

        number_of_word = grepFromFile(argv[1], FILE_NAME, argv[2]);
        printf("\nNumber of '%s' in '%s' = %d\n\n", argv[2], argv[1], number_of_word);
    }

    return 0;
}

int numberOfFileSize(char *file_name) {

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

int grepFromFile(char *input_file_name, char *output_file_name, char *target_string) {

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
    output_file_pointer = fopen(FILE_NAME, "a+");
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
