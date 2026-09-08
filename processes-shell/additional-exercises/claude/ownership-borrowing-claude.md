## Ownership & borrowing in C — incremental exercise set

**Framing, stated once so I don't repeat it per exercise:** every heap object has exactly one *owner* at any time — the entity responsible for calling `free()` on it exactly once. Everything else touching that memory is a *borrower* — holds a pointer, may read (and, if unambiguously permitted, write through it), but must never `free()` it and must never use it once the owner's lifetime ends. Formally, if $O$ is the owner and $\text{lifetime}(O)$ is an interval, every borrow $b$ must satisfy $\text{scope}(b) \subseteq \text{lifetime}(O)$ — borrow containment is literally an interval-inclusion constraint. Violations are exactly two dual failure modes: **use-after-free** (a borrow escapes past the owner's lifetime) and **double-free** (two "owners" both call `free`, i.e. the ownership relation wasn't actually a function — it was one-to-many when it needed to be one-to-one).

I'll give you a snippet per exercise. Your job: identify (a) who owns what, (b) where the borrow-containment inclusion is violated, if it is, and (c) the minimal fix. I'll hold the answers until you respond — work through them and tell me your analysis; I'll critique rather than just hand you the answer.

---

**Exercise 1 — baseline, single owner**
```c
char *make_greeting(char *name) {
    char buf[64];
    snprintf(buf, sizeof(buf), "hello, %s", name);
    return buf;
}
```
Identify the owner of the string this function is trying to produce, and state precisely why $\text{scope}(\text{return value}) \not\subseteq \text{lifetime}(\text{owner})$.

---

**Exercise 2 — borrow vs. copy, one bit changed**
```c
void set_first_dir(SearchPath *p, char *dir) {
    p->directories[0] = dir;
}
```
This compiles and often "works" in a quick test. Under what caller lifetime assumption does it silently become a dangling borrow? Rewrite it so `SearchPath` genuinely owns its entries (not just holds pointers into someone else's storage).

---

**Exercise 3 — double free via two owners**
```c
void handle_path_broken(SearchPath *p, char **dirs, size_t n) {
    p->directories = dirs;          // no copy
    p->directory_count = n;
}
// caller:
char **tokens = lex_line(line);
handle_path_broken(&shell.path, &tokens[1], count);
free(tokens);
clear_path(&shell.path);   // frees shell.path.directories[i] for each i
```
Two things are wrong here, of different kinds — one is a borrow-outliving-owner problem, one is a literal double-free. Name each separately; they have different root causes even though they interact.

---

**Exercise 4 — the `strdup` failure path**
```c
int handle_path(SearchPath *path, char **dirs, size_t count) {
    clear_path(path);
    path->directories = malloc(count * sizeof(char *));
    for (size_t i = 0; i < count; i++) {
        path->directories[i] = strdup(dirs[i]);
    }
    path->directory_count = count;
    return 0;
}
```
`strdup` can return `NULL` on allocation failure. If it does on iteration $i=3$ of a 5-element copy, what is the ownership state of `path->directories[0..2]`, `[3]`, and `[4..]` at that moment? Is `clear_path(path)` still safe to call afterward? Why or why not — tie this to what `directory_count` claims versus what's actually been allocated.

---

**Exercise 5 — borrow escaping a stack frame**
```c
Command classify_command(char **tokens) {
    Command cmd;
    char joined[256];
    snprintf(joined, sizeof(joined), "%s %s", tokens[0], tokens[1]);
    cmd.tag = CMD_EXTERNAL;
    cmd.as.external.name = joined;   // <-- 
    return cmd;
}
```
This is a variant of Exercise 1 but harder to spot because it's buried inside a struct field rather than a direct return value. State the rule in general form: *any pointer stored into an escaping struct must borrow from a lifetime that is $\supseteq$ the lifetime of the struct itself* — and identify which lifetime `joined` actually has.

---

**Exercise 6 — realloc invalidation (a borrow you didn't know you had)**
```c
char **tokens = malloc(64 * sizeof(char *));
char *first = NULL;
tokens[0] = "ls";
first = tokens[0];              // "borrow" — but of what, exactly?
tokens = realloc(tokens, 128 * sizeof(char *));
printf("%s\n", first);
```
Trick question in disguise: is `first` actually dangling here? Distinguish between *borrowing the array* (`tokens` itself, which `realloc` may move) versus *borrowing a string `tokens[i]` points to* (a separate allocation `realloc` never touches). Which one did this code do, and does the printf crash?

---

**Exercise 7 — write the ownership contract, not just the code**
Without writing implementation, write **only the function signatures plus a one-line comment per pointer parameter** stating whether it's borrowed (B) or must-be-owned/copied-in (O), for a function `add_redirection(Command *cmd, char *filename)` that needs to store `filename` inside `cmd` for use *after* the token array that produced it has been freed. This is the exercise most directly relevant to extending your shell with `>` support — get the contract right before writing the body.

---

Send me your answers to as many as you want in one go or one at a time — I'd rather see your reasoning on the lifetime-inclusion argument than just "yes it's a bug," since that's the part that transfers to the next bug you'll hit that isn't in this list.