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
void print_error(void){
    fprintf(stderr, "An error has occurred\n");
}

// Global state
void init_shell(ShellState *s) {
    s->path.directories = NULL;
    s->path.directory_count = 0;
}

// freeing paths 
void clear_path(SearchPath *path){
    // init case if directories is NULL:
    if (path->directories == NULL){
        return; 
    }

    // for p->directories not NULL
    for (size_t i = 0; i < path-> directory_count; i++){
        free(path->directories[i]) // Free the owned strings
    }
    free(path->directories);  // Free the container array
    path->directories = NULL;
    path->directory_count = 0;
}


// Bultin commands:

// "exit" 0-ary with -1 output by contract
// sentinel node 
int handle_exit(void){ 
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
    for (size_t i = 0; args[i] != NULL; ++i) {
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
    // no path to 
    if (path->directories == NULL) {
        print_error();
        return 1;
    }

    // deep copy each arg into path
    for (size_t i = 0; i < argc; i++) {
        path->directories[i] = strdup(args[i]);
    }
    path->directory_count = argc; 

    return 0;
}

// hanle external also n-ary operator depending on definition:
int handle_external(SearchPath *path, char *name, char **argv) {
    (void) path; (void)argv;
    fprintf(stderr, "external dispatch for '%s' not yet implemented\n", name);
    return 1;
}



// Lexing:

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



// Parsing:
/* ---------- B: the coproduct ----------------------------------------------
 * Command = Exit + Cd(str) + Path(str*) + External(str, str*) + ParseError
 * Each summand's fields are already arity-correct by construction: there is
 * no way to build a CMD_CD with zero or two arguments. That guarantee is the
 * entire content of "parse, don't validate" — it lives in this type, not in
 * a runtime check.
 * ---------------------------------------------------------------------- */

typedef enum { CMD_EXIT, CMD_CD, CMD_PATH, CMD_EXTERNAL, CMD_PARSE_ERROR } CommandTag;


// tag x argument word. Argument Word = cd_args | path_args | external_args.
// cd_args = char *dir . path_args 
typedef struct {
    CommandTag tag; 
    union {
        struct { char *dir; }                 cd;        // exactly one borrowed string
        struct { char **dirs; size_t count; }  path;      // borrowed strings, may be empty
        struct { char *name; char **argv; }    external;  // borrowed strings; argv is NULL-terminated for execv
    } as; // Argument tokens above.
} Command;

/* ---------- A -> 1+B : the parse step --------------------------------------
 * All arity/shape validation happens exactly once, here. tokens[] is BORROWED
 * from the caller's `line` buffer (see lex_line below) — classify_command
 * neither copies nor frees anything; it only reads and tags.
 * ---------------------------------------------------------------------- */


// token stream -> Command = tag \times (argument word)
Command classify_command(char **tokens){
    Command cmd; 

    // disjunction of init/ tag and catch parse error on ~ (constructor / argument word match).
    if (tokens[0] == NULL) {
        cmd.tag = CMD_PARSE_ERROR;
        return cmd;
    }

    if (strcmp(tokens[0], "exit") == 0){
        if (tokens[1] != NULL){
            cmd.tag = CMD_PARSE_ERROR;
            return cmd;
        }
        cmd.tag = CMD_EXIT;
        return cmd;
    }

    if (strcmp(tokens[0], "cd") == 0){
        if (tokens[1] == NULL || tokens[2] != NULL){
            cmd.tag = CMD_PARSE_ERROR;
            return cmd;
        }
    }

    if (strcmp(tokens[0], "path") == 0){
        cmd.tag = CMD_PATH;
        cmd.as.path.dirs = &tokens[1]; // borrow may point straight to NULL, but we have 
        size_t n = 0;
        while (tokens[1 + n] != NULL) n++;
        cmd.as.path.count = n;  // borrow — execv wants exactly this shape
        return cmd;
    }
    
    // else:
    cmd.tag = CMD_EXTERNAL
    cmd.as.external.name = tokens[0];
    cmd.as.external.argv = tokens;
    return cmd;
}


// routing table for execution dispatching: Shell Env x Command -> Shell' Env x int (result).
int execute_command(ShellState *shell, Command cmd) {
    switch (cmd.tag) {
        case CMD_EXIT:
            return handle_exit();
        case CMD_CD:
            return handle_cd(cmd.as.cd.dir);
        case CMD_PATH:
            return handle_path(&shell->path,
                cmd.as.path.dirs,
                cmd.as.path.count
                );
        case CMD_EXTERNAL:
            return handle_path(&shell->path,
                                cmd.as.external.name,
                                cmd.as.external.argv
                              );
        case CMD_PARSE_ERROR:
        default:
            print_error();
            return 1; 
    }
}

// 0 success, 1 failure, 
int handle_line(ShellState *shell, char *line){
    char **tokens = lex_line(line);
    // return the output resolved on single shell and its tokens.
    Command cmd = classify_command(tokens); //take the vector of tokens and classify the command tag.
    int result = execute_command(shell, cmd); //take in the command = commandTag \times (cd_args + path_args + external_args)
    free(tokens);
    //
}


int main(int argc, char *argv[]){
    // init 
    // Define struct to hold shell state, including a dynamic array for the search path.
    // Helper to initialize with "/bin"
    ShellState shell;
    init_shell(&shell);  

    // partition on the cases.
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
            free(line);
            clear_path(&shell.path);
            fclose(fp);
        }
    } // interactive
    else if (argc == 1){
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
    } else {
        clear_path(&shell.path);
        print_error();
        exit(1);
    }
}