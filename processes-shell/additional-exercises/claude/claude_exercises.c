
// Exercise 1 

// Identify the owner of the string this function is trying to produce, and state precisely why 
// scope(return value) ⊈ lifetime(owner)

char *make_greeting(char *name) {
    char buf[GREETING_BUF_SIZE];
    snprintf(buf, sizeof(buf), "hello, %s", name);
    return buf;
}
// owner of the string is buf. 
// scope of the return value can be greater than the life time of buf variable itself, char *outside_ref = char *make_greeting(name) will return the buf value in the heap. 

// additional questions:
// 1. Do we associate owners to variables binded to memory or to the function containing binded variable.

// Summary of Critique:
// scope of outside ref is disjoint lifetime(buf)=[call entry, return) scope(outside_ref)=[return, …)

// fix is to change where the storage lives so its lifetime genuinely extends past the function:
//  either malloc inside make_greeting (caller now owns it, must free) 
// or have the caller pass in the buffer (snprintf(caller_buf, ...), 
// caller already owns it, function only borrows write access to it).

// Exercise 1 fix 1: heap allocated variable. usage life span of buf references is fully within the lifespan of buf
char *make_greeting(char *name) {
    char *buf = malloc(GREETING_BUF_SIZE);
    if (buf == NULL) {
        fprintf(stderr, "make_greeting: allocation error\n");
        exit(1);
    }
    snprintf(buf, GREETING_BUF_SIZE, "hello, %s", name);
    return buf;
}

void make_greeting_caller(void){
    char* name = "hello";
    char* buf = make_greeting(name);
    free(buf);
}

// Stack frame -> [make_greeting_caller, make_greeting, snprintf].
// Exercise 1 fix 2: caller owned variable, buf is in the stack life span of caller.
char *make_greeting(char *name, char *buf){
    snprintf(buf, GREETING_BUF_SIZE, "hello, %s", name);
    return buf;
}
void make_greeting_caller(void){
    char* name = "hello";
    char buf[GREETING_BUF_SIZE];
    make_greeting(name, buf);
}


// Exercise 2 — borrow vs. copy, one bit changed

typedef struct {
    char **directories;
    size_t directory_count;
} SearchPath;

void set_first_dir(SearchPath *p, char *dir) {
    p->directories[0] = dir; 
}

// This compiles and often "works" in a quick test. 
// Under what caller lifetime assumption does it silently become a dangling borrow?
// Rewrite it so SearchPath genuinely owns its entries (not just holds pointers into someone else's storage).

// Caller life time example
// Global state
void init_path(SearchPath *path) {
    path->directories = malloc(10 * sizeof(char *));
    if (path->directories == NULL){
        fprintf(stderr, "malloc error");
        exit(1);
    }
    path.directory_count = 0;
}

void dangling_borrow(void){
    SearchPath p;
    init_path(&p);
    char *dir = "dir";
    set_first_dir(&p, dir);
    free(p.directories); 
    // however p->directories[0] = *(p->directories + 0) is not fried. even though p->directories is freeied
}

// Attempt 2 of caller life time example

void populate_from_stack(SearchPath *p){
    char local_dir[16]; //stack frame allocated data
    strcpy(local_dir, "dir");
    set_first_dir(p, local_dir); // p->directories[0] now borrows local_dir's address
}


void dangling_borrow(void) {
    SearchPath p;
    init_path(&p);
    populate_from_stack(&p);          // after this call returns, p->directories[0] dangles
    // because populate_from_stack stack frame is removed 
    // so local_dir is removed from stack frame 
    // but p.directories[0] is set to data from the stack frame which is now gone
    printf("%s\n", p.directories[0]); // UB: reads through a pointer whose referent's frame is gone
    free(p.directories);              // only this call is legal — releases the array itself but nothing todo with the dangling borrow.
}
//so the biggest change here is not the strcpy
// but the intermediate function which stack frame leaves first and sandwiched in between 

//strdup() function allocates sufficient memory for a copy of the string s1, does the copy, and returns a pointer to it
void set_first_dir(SearchPath *p, char *dir) {
    p->directories[0] = strdup(dir); 
    p->directories[0] = ; }



// Exercise 3 — double free via two owners


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
    while ((token = strsep(&line, " \t\n")) != NULL) {
        // Skip empty tokens from multiple spaces
        if (*token == '\0') continue;
        tokens[position] = token;
        position++;

        // Resize the array if necessary
        if (position >= bufsize) {
            bufsize += 64;
            // for safety realloc can turn null and then original pointer lost
            char **tmp = realloc(tokens, bufsize * sizeof(char*));
            if (tokens == NULL) {
                free(tokens);
                fprintf(stderr, "wish: allocation error\n");
                exit(EXIT_FAILURE);
            }
            tokens = tmp;
        }
    }
    tokens[position] = NULL; // Null-terminate the array
    return tokens;
}

// freeing paths 
void clear_path(SearchPath *path){
    // init case if directories is NULL:
    if (path->directories == NULL){
        return; 
    }

    // for p->directories not NULL
    for (size_t i = 0; i < path-> directory_count; i++){
        free(path->directories[i]); // Free the owned strings
    }
    free(path->directories);  // Free the container array
    path->directories = NULL;
    path->directory_count = 0;
}

void handle_path_broken(SearchPath *p, char **dirs, size_t n) {
    p->directories = dirs;          // no copy
    p->directory_count = n;
}

// caller:
void exercise_3_caller(void){
    char *line = "cd .";
    size_t count = 2;
    char **tokens = lex_line(line);
    handle_path_broken(&shell.path, &tokens[1], count);
    free(tokens);
    clear_path(&shell.path);   // frees shell.path.directories[i] for each i
}

// Two things are wrong here, of different kinds — one is a borrow-outliving-owner problem,
// one is a literal double-free. Name each separately;
// they have different root causes even though they interact.

// I'm thinking the double free is on tokens with lex_line freeing tokens and free(tokens) also freeing tokens.
// the borrowing out living owner problem. is for the variable  &tokens[1] = char **dirs. 
// clear_path checks again p->directories, but since dirs is already freed by tokens.

// Correction:
// Borrow out living 
// p.directories = &tokens[1]
// ordering itself is the bug, not any single event in isolation.
// e3 ⇢ e6 — the double free. Two separate calls, free(tokens) and free(p.directories), due to     
// p->directories = dirs;          // no copy in path broken
//


int handle_path_fixed(SearchPath *p, char **dirs, size_t n) {
    // dirs is reference, n is pass by value, (hence first copied into the stackframe for this current function)
    clear_path(p);  //release whatever p owned before for safety.

    // init case
    if (n == 0) { p->directory_count = 0; return 0; }

    // constructor case 
    p->directories = malloc(n * sizeof(char *)); // NEW allocation, independent of tokens
    if (p->directories == NULL) return 1;
    
    for (size_t i = 0; i < n; i++) {
        p->directories[i] = strdup(dirs[i]);     // COPY each string out of tokens's block
        if (p->directories[i] == NULL) return 1; // (leak-free handling left as the earlier exercise)
    } 

    // copy/ allocate within heap, so that p->directories and &tokens[0] are separate items on the heap.
    p->directory_count = n; 
    return 0;
}

// so if i'm not mistaken how the borrow double free pair is fixed, is first by making each assignment or potential assignment of p disjoint in life times, 
// so there's no corruption of memory. and then making tokens and p->directories also disjoint in memory storage so that their freeing do not interact 
// and then their borrowing is just a read instead of two pointers one item in the heap? algebraically can you also formalize this for me?

// Correction borrowing was eliminated by direct copy strdup(dirs[i]);    
// this eliminates pointer aliasing (Two pointers one object in memory) between tokens and path->directories of tokens[1] object in memory.
// That they're strictly copied and space wise disjoint objects. So life times won't have side effects.


// Exercise 4 — the strdup failure path
int handle_path(SearchPath *path, char **dirs, size_t count) {
    clear_path(path);
    path->directories = malloc(count * sizeof(char *));
    for (size_t i = 0; i < count; i++) {
        path->directories[i] = strdup(dirs[i]);
    }
    path->directory_count = count;
    return 0;
}


//  strdup can return NULL on allocation failure. If it does on iteration i = 3 of a 5-element copy,


//  what is the ownership state of path->directories[0..2], [3], and [4..] at that moment? 
//  path->directories is an allocated array of pointers size of count.
//  path->directories[0..2] have heap allocate values of dir[0..2], these a separate chunks in memory from wherever dir[0..2] is stored.
//  path->directories[4..] are just random values unallocated because we only allocated an array of pointers to strings in the heap.
//  Assuming path->directories[i] = strdup(dirs[i]) is not run yet we just have i=3 and
//  the rest of the necessary stuff such as dirs pointer to array and count and pointer ot path in stack frame

//  Is clear_path(path) still safe to call afterward? Why or why not — tie this to what directory_count claims versus what's actually been allocated.
//  clear path  won't be safe to call if it iterates through all of free(path->directories[i]), because for i = 4.. we would be freeing addresses which weren't owned/ allocated by us in the first place.


// assumptions were wrong. it meant strdup fail = NULL for 3 but then continue, now to check clear_path ok?
// Silent correctness that free(NULL) is NO-OP/ safe.
// hence clear_path is ok. reset of [4..] resolved normally during handle_path loop hence cleart_path later on too.


// Exercise 5 — borrow escaping a stack frame

Command classify_command(char **tokens){
    Command cmd; 
    char joined[256];
    snprintf(joined, sizeof(joined), "%s %s", tokens[0], tokens[1]);
    cmd.tag = CMD_EXTERNAL;
    cmd.as.external.name = joined; // <--
    return cmd;
}
// This is a variant of Exercise 1 but harder to spot because it's buried inside a struct field rather than a direct return value. State the rule in general form: *any pointer stored into an escaping struct must borrow from a lifetime that is 
// ⊇
// ⊇ the lifetime of the struct itself* — and identify which lifetime joined actually has.