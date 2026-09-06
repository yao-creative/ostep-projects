## The buffer/cache question — decision-oriented framing

**Your goal (G):** given a command name `cmd`, resolve it to an absolute executable path before calling `execv()`.

**Constraint set:** the search space is your `path` list, which is *mutable at runtime* (the user can invoke your `path` builtin at any point, and in principle the filesystem itself can change between shell invocations — someone could `chmod` a binary or install a new one mid-session).

Funnel this down:

1. **Sub-goal:** does resolution need to be fast, or does it need to be *correct against the current filesystem state*?
2. For a shell, correctness dominates — a stale cache that says "`/usr/local/bin/foo` exists" after the user deleted it is a wrong answer, not a performance win.
3. The cost of *not* caching is one or two `access()` syscalls per directory in your path per command typed interactively. That's microseconds. There is no performance problem to solve.
4. Therefore a pre-populated "cache of all bins" is solving a problem you don't have, at the cost of introducing a correctness bug class (cache invalidation) you didn't have before.

**Conclusion:** no cache/buffer of binaries. The only thing you should hold in memory is the **path list itself** — an ordered, mutable collection of directory strings (default `{"/bin"}`), which you already correctly identified as "a variable your shell updates." Resolution should be re-derived from disk on every command dispatch.

Formally, if $P = (d_1, d_2, \dots, d_n)$ is your current path list (an ordered set — order matters, since you search $d_1$ before $d_2$), resolution is the function:

$$
\text{resolve}(cmd) = \text{first}\big(\, d_i \in P \;\big|\; \text{access}(d_i \frown cmd,\ X\_OK) = 0 \,\big)
$$

i.e. the first element of $P$ (in order) for which the concatenation $d_i \frown cmd$ names an executable file, or $\bot$ (no valid path found → your generic error) if no such $d_i$ exists. This is a pure fold-with-early-exit over $P$, recomputed fresh each call. That's the entire "path" subsystem — no cache needed.

---

## Critique of your implementation

Organizing hierarchically from "won't compile" → "compiles but wrong" → "missing entirely":

### 1. Won't compile
- `strcmp(token[0], ...)` — you named the parameter `tokens`, not `token`. Typo, compile error.
- `access(tokens[1], X_OK)` — missing semicolon, and you never use the return value.
- `void main(...)` — should be `int main(...)`, returning `int`. `void main` is non-standard and some compilers will refuse or warn hard.
- `dummy_function()` declared `int`, has no `return` — undefined behavior on any caller (though you never call it, so it's just dead code you can delete).
- In batch mode: `read = getline(&line, &len, fp)` — `read` is never declared in that scope (only declared inside the interactive branch). Compile error.
- `true` used in `while (true)` without `#include <stdbool.h>`.
- Missing includes entirely: `<unistd.h>` (for `chdir`, `access`, `fork`, `execv`), `<sys/wait.h>` (for `wait`/`waitpid`, which you'll need once you fork).

### 2. Compiles but logically wrong
- **`exit(0)` after `cd` and `path` in `handle_single_command_tokens`.** This is your most important bug conceptually. A builtin should *return control to the REPL loop*, not terminate the shell. Right now, typing `cd ..` once would end your entire program. The causal chain you want is: *user types line → dispatch → builtin executes → loop continues*, not *builtin executes → process exits*. Only the actual `exit` builtin should call `exit()`.
- **`path` builtin doesn't update any state.** You `printf` and call (a broken) `access()`, but nothing is written back into a persistent path variable. Given the resolution function above, `path` needs to *replace* $P$ with the new argument list — and note the real spec (this looks like the classic OSTEP "wish" shell) allows **multiple** directories after `path`, not just `tokens[1]` with a hard `tokens[2] == NULL` guard. Your current condition rejects `path /bin /usr/bin` outright.
- **`handle_line` never calls `handle_single_command_tokens`.** You tokenize and then... stop. The function is currently a no-op with respect to dispatch.
- **Return-type mismatch at the call site:** `handle_line` is declared `void` but your comment says it should return `0`/`1`/`-1`, and `main` checks `handle_line(line) == -1`, which won't compile against a `void` return. Pick one contract and make the signature match it.
- **Redundant nested `while(true)` loops in `main`'s interactive branch** — the outer loop has no break condition and does nothing the inner loop doesn't already do. It's dead structure; delete it.
- **Batch mode reads lines but the loop body is empty** — `getline` succeeds, nothing happens with `line`. It should call the same `handle_line` your interactive path uses (the two modes should converge to one code path after "how do I get a line of input").

### 3. Missing entirely (not bugs, just gaps you know about)
- No `fork()`/`execv()` path for external commands at all — `handle_single_command_tokens` only branches on the three builtins and does nothing (silently) otherwise.
- No parallel-command handling (`&`-separated commands run concurrently, waited on together).
- No redirection (`>`) handling, despite your own comment flagging it.
- No `wait()`/`waitpid()` after `fork()`, which you'll need to avoid zombie processes and to correctly serialize non-parallel commands.

### 4. Memory management
- `lex_line` mallocs `tokens`; nothing ever `free()`s it — leaked once per line, forever.
- `line` from `getline` is freed once at the very end of `main`'s interactive branch (after the loop exits), but not per-iteration — technically fine since `getline` reuses the same buffer across calls if you pass the same pointer, but only if you're **not** setting `line = NULL` again inside the loop. Right now you re-declare/reset `char *line = NULL; size_t len = 0;` *inside* the `while(true)` before each `getline` call — that orphans the previous buffer and leaks it every single iteration.

### 5. Error-message spec compliance
This looks like the OSTEP "wish" assignment, which requires **one single generic error message** (`"An error has occurred\n"` to `stderr`) for *every* error condition — bad `cd`, bad `path`, invalid command, batch file not found, allocation failure, everything. Your code currently uses `perror("wish: cd")`, custom `fprintf` allocation messages, a custom `printf("wcat: cannot open file\n")`, and a custom invalid-argument message. If the grading spec is what I think it is, **all of these need to collapse into the single generic error string** — that's a hierarchical constraint sitting above every individual builtin's error handling, worth fixing once centrally rather than per-function.

---

**Priority order if you want to fix these top-down:** (1) make it compile, (2) remove the erroneous `exit(0)` calls from `cd`/`path`, (3) wire `handle_line` → dispatch → builtins, (4) add the fork/execv path for externals, (5) centralize error output, (6) fix the memory leaks, (7) layer in `&` and `>` last, since those are parsing-level concerns on top of a working single-command pipeline.