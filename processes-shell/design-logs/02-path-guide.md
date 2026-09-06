## Goal decomposition for the `path` builtin

**Top-level goal (G):** the shell must maintain a piece of *mutable session state* — the search path — that persists across command invocations and can be queried by the resolution function and rewritten by the `path` builtin.

This is a **state-management problem**, not a **parsing problem**, even though it's triggered by parsing. Keep those two concerns separate in your head — that separation is the actual insight you're missing right now, not any specific syscall.

### Sub-goal 1: Represent the state correctly

Formally, treat the path as an ordered collection $P \subseteq \Sigma^*$ (strings over your directory-name alphabet), where **order matters** — this is a *sequence*, not a set, even though people casually call it "the path set." The reason order matters: your resolution function does first-match-wins over $P$, so $P = (\texttt{/bin}, \texttt{/usr/bin})$ and $P = (\texttt{/usr/bin}, \texttt{/bin})$ are different functions even though they contain the same elements.

Decision you need to make: where does $P$ *live*?
- Global/static array of `char*` with a separate count variable, or
- A `char**` you `realloc` — same shape as what you already built for `tokens` in `lex_line`.

Either is fine. The requirement is: it must be declared **outside** `handle_line`/`handle_single_command_tokens` (file scope or passed by pointer), because it needs to survive from one call to the next. Right now your `path` branch operates on purely local data that vanishes when the function returns — that's the actual bug to fix, not the `access()` call.

### Sub-goal 2: Define the *mutation* the builtin performs

When the user types `path d1 d2 ... dn`, ask yourself: is this an **append** to $P$, or a **replacement** of $P$? Check the assignment spec's wording carefully — most versions of this assignment (and the default-`/bin`-then-user-overrides framing you described earlier) want **replacement**: $P_{\text{new}} = (d_1, \dots, d_n)$, discarding whatever $P_{\text{old}}$ was. This also means `path` with **zero** arguments is a valid, meaningful case — $P_{\text{new}} = \varnothing$, the empty sequence, meaning *no* command should resolve afterward. Your current code's `tokens[1] != NULL` guard forbids this case; check whether the spec requires you to support it.

### Sub-goal 3: Fix the control-flow contract

You already know from my earlier critique: builtins must **not** call `exit()` except the literal `exit` builtin. So the shape of the `path` branch should be: *validate → mutate $P$ → return control to caller*, with no side effect other than the mutation itself (not even a `printf` — the spec gives no output for a successful `path` call, only for errors).

### Sub-goal 4: Reconnect resolution to the new state

Once $P$ is a persistent, mutable variable, your resolution function (the `access()`-based fold I gave you earlier) needs to read from it. Make sure whatever function does resolution takes $P$ (or has access to it) at *call time*, not at shell-startup time — otherwise a `path` call won't actually change future command dispatch, which defeats the entire purpose of the builtin.

---

**Concretely, the order of operations to implement, without me writing the code:**
1. Decide the storage shape for $P$ and where it's declared (must outlive a single `handle_line` call).
2. Initialize $P = (\texttt{/bin})$ before your main loop starts.
3. In the `path` builtin branch: free/discard old $P$, allocate new storage sized to `tokens[1..]`, copy the strings in, update the count.
4. In your (currently missing) external-command dispatch: loop over $P$ in order, building each candidate full path (directory + `/` + command name), and `access(candidate, X_OK)` each one until success or exhaustion.
5. Only fall through to the generic error message if step 4 exhausts $P$ with no match.

Want me to go through the same treatment for how `fork()`/`execv()` should be structured around that resolved path?