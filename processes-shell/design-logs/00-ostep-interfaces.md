**Intent: API/domain decomposition** — you want to identify the *minimal conceptual interfaces* implied by the assignment before implementing functions. The key is not to make an interface for every syscall; instead, expose the shell's semantic operations and keep `fork`, `execv`, `waitpid`, `open`, etc. at the execution boundary.

The assignment decomposes nicely into:

$$
\text{Shell}
=
\text{Input}
\to
\text{Parse}
\to
\text{Classify}
\to
\text{Resolve}
\to
\text{Execute}
\to
\text{Synchronize}
$$

## 1. The core state

There is really only one important persistent piece of shell state:

```c
typedef struct {
    char **directories;
    size_t directory_count;
} SearchPath;
```

Conceptually:

$$
P = (D_1,D_2,\ldots,D_n)
$$

where each $D_i$ is a directory searched in order.

Operations:

```c
SearchPath path_default(void);          // {"/bin"}
void search_path_replace(SearchPath *, char **dirs, size_t count);
char *search_path_resolve(const SearchPath *, const char *command);
```

The important semantic distinction is:

```text
path /bin /usr/bin
```

means

$$
P \mapsto \{"/bin","/usr/bin"\}
$$

not

$$
P \mapsto P \cup \{"/bin","/usr/bin"\}
$$

because `path` **replaces** the existing path.

---

# 2. Input API

You need an abstraction representing where commands come from.

```c
typedef struct {
    FILE *stream;
    bool interactive;
} Input;
```

Potential API:

```c
int input_next_line(Input *, char **line);
```

Semantics:

$$
\text{Input} \to \text{Option(String)}
$$

where:

* `Some(line)` → process another command
* `None` → EOF → `exit(0)`

You could avoid making this an elaborate abstraction for this assignment. `getline()` directly in your main loop is perfectly reasonable.

---

# 3. Lexer / tokenizer API

The next semantic object is the token sequence.

For this assignment, your lexer primarily needs to distinguish:

$$
\text{Token}
=
\text{Word}
\mid
>
\mid
\&
$$

So conceptually:

```c
typedef enum {
    TOKEN_WORD,
    TOKEN_REDIRECT,
    TOKEN_PARALLEL
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;
} Token;
```

Then:

```c
int lex_line(char *line, Token **tokens, size_t *count);
```

For:

```text
ls -la /tmp > output
```

you want:

```text
WORD("ls")
WORD("-la")
WORD("/tmp")
REDIRECT
WORD("output")
```

For:

```text
ls & pwd & echo hello
```

you get:

```text
WORD("ls")
PARALLEL
WORD("pwd")
PARALLEL
WORD("echo")
WORD("hello")
```

### Important design point

Don't make parsing directly execute things.

You want:

$$
\text{bytes}
\overset{\text{lex}}{\longrightarrow}
\text{tokens}
\overset{\text{parse}}{\longrightarrow}
\text{command structure}
$$

That gives you a clean place to validate syntax.

---

# 4. Command representation

This is probably your most important domain structure.

A command needs:

```c
typedef struct {
    char **argv;
    size_t argc;

    char *redirect_file;
} Command;
```

So:

```text
ls -la /tmp > output
```

becomes approximately:

$$
C =
(
["ls","-la","/tmp"],
"output"
)
$$

And a line is:

```c
typedef struct {
    Command *commands;
    size_t count;
} CommandLine;
```

Therefore:

$$
L = (C_1,C_2,\ldots,C_n)
$$

where `count > 1` means parallel execution.

This is a much better abstraction than having `execute_line()` manipulate strings everywhere.

---

# 5. Parser API

Now you can expose:

```c
int parse_line(Token *tokens,
               size_t token_count,
               CommandLine *out);
```

Its responsibility is **only structural validity**.

For example:

```text
ls -la > output
```

→ valid.

```text
ls > output > foo
```

→ error.

```text
ls > 
```

→ error.

```text
ls & pwd
```

→ two commands.

```text
ls & & pwd
```

→ syntax error.

So:

$$
\text{Parser} :
\text{Token}^*
\to
\text{CommandLine}
+
\text{Error}
$$

This is an extremely useful boundary.

---

# 6. Built-in classification

You then need to distinguish:

$$
\text{Command}
=
\text{Builtin}
\mid
\text{External}
$$

For example:

```c
typedef enum {
    COMMAND_EXIT,
    COMMAND_CD,
    COMMAND_PATH,
    COMMAND_EXTERNAL
} CommandKind;

CommandKind classify_command(const Command *);
```

This is conceptually important because built-ins **do not cross the process boundary**.

For example:

```text
cd /tmp
```

must modify the shell's own process.

If you did:

$$
\text{shell}
\xrightarrow{\text{fork}}
\text{child}
\xrightarrow{\text{chdir}}
/tmp
$$

only the child changes directory. The shell remains where it was.

Therefore:

```text
builtin
    ↓
shell process

external command
    ↓
fork
    ↓
child
    ↓
execv
```

---

# 7. Built-in API

I'd make one semantic dispatcher:

```c
typedef enum {
    BUILTIN_OK,
    BUILTIN_EXIT,
    BUILTIN_ERROR,
    BUILTIN_NOT_BUILTIN
} BuiltinResult;

BuiltinResult execute_builtin(
    const Command *command,
    SearchPath *path
);
```

Or even simpler for a student project:

```c
bool is_builtin(const Command *);
void execute_builtin(const Command *, SearchPath *);
```

Then internally:

```c
execute_exit(...)
execute_cd(...)
execute_path(...)
```

The three operations have very clear semantics:

### `exit`

$$
\text{ShellState} \to \text{Terminated}
$$

### `cd`

$$
\text{cwd} \mapsto \text{cwd}'
$$

through:

```c
chdir()
```

### `path`

$$
P \mapsto P'
$$

through your `SearchPath`.

---

# 8. Executable resolution

This deserves its own API.

```c
int resolve_executable(
    const SearchPath *,
    const char *command,
    char **resolved_path
);
```

Semantically:

$$
R : (P,c) \to
\begin{cases}
p & \exists p = D_i/c,\ access(p,X\_OK)\\
\bot & \text{otherwise}
\end{cases}
$$

For:

```text
path = ["/bin", "/usr/bin"]
command = "ls"
```

you calculate:

$$
/bin/ls,\quad /usr/bin/ls,\ldots
$$

until:

```c
access(candidate, X_OK)
```

succeeds.

Then the child executes:

```c
execv(resolved_path, command->argv);
```

---

# 9. Process execution API

This is your **OS boundary**.

I'd conceptually define:

```c
typedef struct {
    pid_t pid;
} Process;

int spawn_process(
    const Command *,
    const SearchPath *,
    Process *
);

int wait_process(Process *);
```

But for this particular assignment, you don't actually need to build a general process abstraction.

A very reasonable implementation API is:

```c
pid_t spawn_command(const Command *, const SearchPath *);
int wait_for_process(pid_t);
```

The implementation is where:

```c
fork()
access()
execv()
```

live.

The child does:

$$
\text{resolve}
\to
\text{redirect}
\to
\text{execv}
$$

The parent gets:

$$
pid
$$

and later:

```c
waitpid(pid, ...)
```

---

# 10. Redirection API

Redirection is really a property of a command:

```c
command.redirect_file
```

Then execution needs:

```c
int setup_redirection(const Command *);
```

Internally:

```c
open(...)
dup2(...)
close(...)
```

The required semantics are:

$$
stdout \mapsto F
$$

and

$$
stderr \mapsto F
$$

where:

$$
F = open(filename, O\_WRONLY | O\_CREAT | O\_TRUNC,\ldots)
$$

This should happen **in the child after `fork()` and before `execv()`**.

That's critical because otherwise you'd redirect the shell itself.

---

# 11. Parallel execution

Your `CommandLine` abstraction makes this surprisingly clean.

For:

```text
cmd1 & cmd2 & cmd3
```

you have:

$$
L = [C_1,C_2,C_3]
$$

Execution becomes:

```c
int execute_command_line(
    const CommandLine *,
    SearchPath *
);
```

The external case is:

```text
for each command:
    fork()
    execv()

after ALL forks:
    waitpid() for every child
```

The ordering invariant is:

$$
\forall i,\quad fork(C_i)
$$

must occur before:

$$
wait(C_j)
$$

for any $j$.

That's what makes them parallel rather than sequential.

---

# 12. The actual minimal architecture

If I were doing this assignment, I would **not** create 15 interfaces.

I'd make roughly this:

```c
// input
int read_line(FILE *, char **);

// parsing
int lex_line(char *, Token **, size_t *);
int parse_tokens(Token *, size_t, CommandLine *);

// command classification
bool is_builtin(const Command *);

// shell state
typedef struct {
    SearchPath path;
} Shell;

// builtins
int execute_builtin(Shell *, const Command *);

// external execution
int execute_external(Shell *, const Command *);

// process execution
pid_t spawn_command(Shell *, const Command *);

// redirection
int setup_redirection(const Command *);

// top-level
int execute_command_line(Shell *, const CommandLine *);
```

And the dependency structure is:

$$
\boxed{
Input
\rightarrow
Lexer
\rightarrow
Parser
\rightarrow
CommandLine
\rightarrow
Dispatcher
}
$$

with the dispatcher branching:

$$
\text{Command}
\rightarrow
\begin{cases}
\text{Builtin} \rightarrow \text{Shell State}\\
\text{External} \rightarrow \text{Process}\\
\end{cases}
$$

---

# 13. What I would *not* abstract

For this project, don't create interfaces like:

```c
IFileSystem
IProcessManager
IPathResolver
IStdin
IStdout
ISyscall
```

unless you're specifically practicing dependency inversion/testing.

That would obscure what the assignment is trying to teach.

The interesting boundaries are **semantic boundaries**, not every syscall:

| Concern                 | Core abstraction       |
| ----------------------- | ---------------------- |
| Input                   | `read_line`            |
| Lexing                  | `Token[]`              |
| Parsing                 | `CommandLine`          |
| Shell state             | `Shell` / `SearchPath` |
| Built-ins               | `execute_builtin`      |
| Path lookup             | `resolve_executable`   |
| External execution      | `spawn_command`        |
| Redirection             | `setup_redirection`    |
| Synchronization         | `waitpid`              |
| Top-level orchestration | `execute_command_line` |

The central domain model is therefore tiny:

$$
\boxed{
Shell
=
(Path)
}
$$

and

$$
\boxed{
CommandLine
=
(Command^*)
}
$$

with

$$
Command
=
(argv,\ redirect?)
$$

and then the runtime interpretation:

$$
\text{Command}
\overset{\text{classify}}{\longrightarrow}
Builtin
\;|\;
External
$$

That is the abstraction I'd build first. The assignment is fundamentally teaching you that **parsing produces a command value, while execution interprets that value against shell state and the Unix process model**.
