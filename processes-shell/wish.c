#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// takes in a string which is the line
// handle for parallel commands and redirection.

// Search path as a list of directories.
typedef struct {
    char **directories;
    size_t directory_count;
} SearchPath;

// Global shell state containing search paths
typedef struct {
    SearchPath path;
} ShellState;

// utilities 
void init_shell(ShellState *s) {
    s->path.directories = NULL;
    s->path.directory_count = 0;
}

void clear_path(SearchPath *path){
    for (size_t i = 0; i < path-> directory_count; i++){
        free(p->directories[i]) // Free the owned strings
    }
    free(p->directories);  // Free the container array
    p->directories = NULL;
    p->directory_count = 0;
}

char **lex_line(char *line) {

    // init case
    // States
    int bufsize = 64;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "wish: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // constructor case
    while ((token = strsep(&line, " ")) != NULL) {
        // Skip empty tokens from multiple spaces
        if (*token == '\0') continue;
        tokens[position] = token;
        position++;

        // Resize the array if necessary
        if (position >= bufsize) {
            bufsize += 64;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "wish: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    tokens[position] = NULL; // Null-terminate the array
    return tokens;
}

void print_error(){
    fprintf(stderr, "An error has occurred\n");
}

// "exit" 0-ary with -1 output by contract
int handle_exit(){
    return -1;
}

// "cd" 1-ary with string directory argument
int handle_chdir(char* directory){
    if (chdir(directory) != 0) {
        print_error();
        return 1;
    }
    return 0;
}



// "path" n-ary operator 
int handle_path(SearchPath *path, char **args) {
    // free old directories
    clear_path(path);



    // count number of arguments:
    size_t argc = 0;
    for (size_t i = 1; args[i] != NULL; ++i) {
        argc++;
    }

    // Induction on number of args:
    // Base case:
    // 2. If argc == 0, we are done (path is empty)
    if (argc == 0) {
        return 0;
    }


    // Inductive case:
    // allocate memory:
    path->directories = malloc(argc * sizeof(char *));
    if (path->directories == NULL) {
        print_error();
        return 1;
    }


    path->directories = args;
    path->directory_count = argc;
    return 0;
}


// array of tokens which make up a single command
int parse_handle_single_command(ShellState *shell, char **tokens){
    // exit | cd <path> | path <path> | ERROR
    // parse in if statement then excute within.
    //single length tokens:
    if (strcmp(tokens[0], "exit") == 0 && tokens[1] == NULL){
        return handle_exit();
    } else if (strcmp(tokens[0], "cd") == 0 && tokens[1] != NULL && tokens[2] == NULL){
        // Fix 'cd' command: check for chdir failure, print error if needed
        return handle_chdir(tokens[1]);
    } else if (strcmp(tokens[0], "path") == 0 && tokens[1] != NULL){
        // 'path' is not implemented yet
        // Pass tokens[1] (the start of the path arguments) and their count to handle_path
        
        return handle_path(shell, &tokens[1], path_argc);

    } else{
        print_error();
        return 1;
    }
}


// 0 success, 1 failure, 
int handle_line(ShellState *shell, char *line){
    char **tokens = lex_line(line);
    // return the output resolved on single shell and its tokens.
    return parse_handle_single_command(shell, tokens);

    //
}


int main(int argc, char *argv[]){
    // init 
    // Define struct to hold shell state, including a dynamic array for the search path.
    // Helper to initialize with "/bin"
    ShellState shell;
    init_shell(&shell);  

    // batch
    if (argc == 2){
        char *file = argv[1]; //file is path is string first argument
        FILE *fp = fopen(file, "r"); //Get file pointer from file name

        // handle error
        if (fp == NULL){
            printf("wcat: cannot open file\n");
            exit(1);
        }  else {
            // No error 
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, fp)) != -1){
                // Per line resolution and execution
                int handle_res = handle_line(&shell, line);
                if ( handle_res == -1) {  // explicit "exit" command
                    break;
                }
            }
            //cleanup
            free(&line);
            clear_path(&shell.path);
            fclose(fp);
        }
    } // interactive
    else if (argc == 1){
        while (true) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        
        while (true) {
            printf("wish> ");
            fflush(stdout);
            read = getline(&line, &len, stdin);
            if (read == -1) {          // EOF, e.g. Ctrl-D
                break;
            }
            // Per line resolution and execution
            int handle_res = handle_line(&shell, line);
            if ( handle_res == -1) {  // explicit "exit" command
                break;
            }
        }
        //cleanup.
        free(line);
        clear_path(&shell.path);
        exit(0);
        }
    } else {
        clear_path(&shell.path);
        print_error();
        exit(1);
    }

}