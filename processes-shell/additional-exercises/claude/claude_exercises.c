
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
    char local_dir[16];
    strcpy(local_dir, "dir");
    set_first_dir(p, local_dir); // p->directories[0] now borrows local_dir's address
}


void dangling_borrow(void) {
    SearchPath p;
    init_path(&p);
    populate_from_stack(&p);          // after this call returns, p->directories[0] dangles
    // because local_dir is removed from stack frame 
    printf("%s\n", p.directories[0]); // UB: reads through a pointer whose referent's frame is gone
    free(p.directories);              // only this call is legal — releases the array itself
}
//so the biggest change here is not the strcpy
// but the intermediate function which stack frame leaves first and sandwiched in between 
// a modification of p's internal values and later on a reference to p.directories[0]?



//e2​
// p.directories[0]←&local_dir​​<e3​
// death(local_dir)​​<e4​
// read p.directories[0]​​


// Exercise 3 — double free via two owners
void handle_path_broken(SearchPath *p, char **dirs, size_t n) {
    p->directories = dirs;          // no copy
    p->directory_count = n;
}

// caller:
void exercise_3_caller(void){
    char **tokens = lex_line(line);
    handle_path_broken(&shell.path, &tokens[1], count);
    free(tokens);
    clear_path(&shell.path);   // frees shell.path.directories[i] for each i
}

// Two things are wrong here, of different kinds — one is a borrow-outliving-owner problem,
// one is a literal double-free. Name each separately;
// they have different root causes even though they interact.