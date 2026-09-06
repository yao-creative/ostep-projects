## Part 1 — User-level interface, extracted and formalized

Before touching implementation, separate **what the user can type** (the interface surface) from **how you resolve it** (already discussed). This is a classic *signature-before-implementation* move: define the domain and codomain of every user-facing operation first.

### 1.1 Invocation interface (process-level)

$$
\text{wish} : \Sigma^{*n} \to \{\text{interactive}, \text{batch}, \text{error}\}
$$

where $n = |\text{argv}| - 1$:

- $n = 0 \Rightarrow$ interactive mode (prompt `wish> ` printed before each read).
- $n = 1 \Rightarrow$ batch mode (argv[1] = filename; **no prompt** printed; bad-open-file $\Rightarrow$ generic error, then `exit(1)`).
- $n \geq 2 \Rightarrow$ error $\Rightarrow$ generic error message, `exit(1)`.

Note the exit-code asymmetry that matters causally later: most errors *continue the loop*; only **bad invocation argc** and **bad batch file open** terminate the whole process with `exit(1)`. Every other error is a *recoverable* transition back to "await next line."

### 1.2 Line-resolution pipeline (this is the actual funnel)

A raw input line is not a single command — it's a **composite** that must be decomposed in a fixed order before dispatch:

$$
\text{line} \;\xrightarrow{\text{split on } \&}\; (\text{cmd}_1, \dots, \text{cmd}_k) \;\xrightarrow{\text{split each on } >}\; (\text{argv}_i,\ \text{redirect\_target}_i)
$$

So the parsing hierarchy is: **line → parallel-command-set → (command, optional redirection target) pairs → tokens**. If you tokenize on whitespace *before* stripping out `&` and `>`, you've inverted the funnel — that's a design smell to watch for, since your current `lex_line` only does the innermost step (whitespace tokenization) and has no notion of the two outer layers yet.

### 1.3 Built-in command interface — exact arity contracts

$$
\text{exit} : () \to \bot \quad\text{(zero args only; any arg} \Rightarrow \text{error, no exit)}
$$
$$
\text{cd} : \Sigma^* \to \{\text{unit}, \text{error}\} \quad\text{(exactly one arg; 0 or }{\geq}2\Rightarrow\text{error; chdir failure}\Rightarrow\text{error)}
$$
$$
\text{path} : \Sigma^{*m},\ m \geq 0 \to \text{unit} \quad\text{(replaces } P \text{ entirely; } m=0 \Rightarrow P = \varnothing \text{, a valid, non-error state)}
$$

Note `path` is the odd one out: it's total over *all* $m \geq 0$, never itself an error condition. Your earlier design treating `path` with the wrong arg count as an error is a spec violation — worth flagging explicitly since it's easy to carry the "cd-style arity checking" instinct over by habit.

### 1.4 External command interface

Anything not matching 1.3 is dispatched as: **resolve via $P$ → `fork()` → `execv()` in child → parent `wait()`s** (except when inside a parallel group, in which case *all* forks in the group happen first, then all waits happen together — this ordering is the crux of the parallel feature and is easy to get backwards).

### 1.5 Redirection interface

$$
(\text{cmd},\ \text{args})\ {>}\ \text{file} \;\Rightarrow\; \text{stdout and stderr of child} \to \text{file (truncate-create)}
$$

Constraint: more than one `>` or more than one filename token after it is a syntax error, caught by the shell *before* forking — this is a parse-time check, not a runtime one.

### 1.6 The error interface (single, unified)

$$
\text{error} : \text{Cond} \to \{\texttt{"An error has occurred\textbackslash n"} \to \text{stderr}\}
$$

This is a constant function — every distinct failure condition maps to the *same* output. That's a strong hint about how to structure your code: one `print_error()` call site conceptually, invoked from many branches, never a bespoke message per branch.

---

## Part 2 — Critique of current code (document 2)

Ordered by blocking severity.

### Won't compile
- `main` references `shell.path_count` / `shell.path_dirs`, but `ShellState` only defines a nested `SearchPath path` with fields `directories` / `directory_count`. Field names don't exist — compile error.
- `access(tokens[1], X_OK)` has no terminating semicolon.
- Two `int handle_res = handle_line(&shell, line)` calls, both missing semicolons.
- `while (true)` used without `#include <stdbool.h>`.
- Final `printf("Invalid number of arguments...")` missing closing semicolon.
- `main` has no `return`/`exit` reached in the fallthrough after the `if/else if/else` — falls off the end in the two successful branches too (each branch does call `exit(0)` or `exit(1)` internally though, so check each path individually; the point stands that this is fragile).

### Compiles-but-wrong (logic)
- **`path` builtin is not implemented as state mutation.** It `printf`s and calls `access()` on `tokens[1]` itself — that checks whether the *argument string* is an executable file, which is semantically unrelated to "add this directory to the search list." No write to `shell.path` occurs anywhere. This is the core gap from your actual question above.
- **`path` arity is wrong per 1.3**: your guard requires exactly one argument and rejects zero or ≥2, but the spec's `path` is total over $m \geq 0$.
- **`cd` failure doesn't call `print_error()`** — it `return 1`s silently. `handle_line`/`main` never inspect that `1` to print anything, so a bad `cd` currently produces *no output at all*, violating 1.6.
- **No guard against `tokens[0] == NULL`.** A blank or whitespace-only line produces `position == 0`, so `tokens[0]` is `NULL`, and `strcmp(tokens[0], "exit")` dereferences a null pointer — crash. This needs to be checked at the top of `handle_single_command_tokens` (or `handle_line`), before any `strcmp`.
- **`exit` with arguments accidentally "works"** only because it falls through every `if`/`else if` into the generic `else` branch, which happens to print an error. That's coincidence, not design — if you ever reorder branches or add a fourth builtin, this silently breaks. Make the arity check explicit inside the `exit` branch itself.
- **No external-command dispatch exists.** The `else` branch treats *every* non-builtin (including plain `ls`) as a hard error. Per 1.4, this is the single largest missing piece — nothing currently calls `fork()`/`execv()`, and per 1.1 that's the shell's primary job.
- **No redirection or parallel splitting (1.2/1.5) anywhere in `lex_line`.** It only performs the innermost whitespace-tokenization layer of the funnel; the two outer layers (`&`-split, `>`-split) don't exist yet, so `cmd1 & cmd2` or `ls > out` would currently be tokenized as one flat argument list and misinterpreted.
- **Batch-mode open failure message is wrong per 1.6**: `printf("wcat: cannot open file\n")` should be the single generic error string to `stderr`, not a custom message to `stdout`.

### Memory
- `lex_line`'s `tokens` array is never freed — leaked once per line, indefinitely.
- `shell.path_dirs[0] = strdup("/bin")` — fine on its own, but note nothing ever frees the *old* array/strings when `path` eventually does get implemented as "replace $P$" (1.3) — that replacement needs to free the previous `directories` array and its strings before installing the new ones, or you leak on every `path` invocation.

### Structural note
The `ShellState` / `SearchPath` split is the right shape for holding $P$ persistently (matches the "must outlive a single call" requirement from before) — that part of your design is sound. The remaining work is: (a) make `main` actually address the fields that exist, (b) implement `path` as a real overwrite of `shell->path`, (c) add the two missing outer parsing layers, (d) add the fork/execv branch, (e) centralize error reporting so every failure path — including the ones currently silent — routes through it.