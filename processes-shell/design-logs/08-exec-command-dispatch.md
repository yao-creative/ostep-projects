Your **intent is algebraic decomposition**: you want to see `execute_command` not primarily as C control flow, but as a function over a **sum type** (`Command`) whose cases are dispatched to different semantic operations.

## 1. Start with the type

Conceptually, your `Command` is a tagged coproduct:

$$
\mathrm{Command}
=
\mathrm{Exit}
+
\mathrm{Cd}
+
\mathrm{Path}
+
\mathrm{External}
+
\mathrm{ParseError}
$$

In C, the `tag` is the runtime representation of which coproduct injection you have:

```c
cmd.tag == CMD_EXIT
cmd.tag == CMD_CD
cmd.tag == CMD_PATH
cmd.tag == CMD_EXTERNAL
cmd.tag == CMD_PARSE_ERROR
```

So mathematically, `Command` is a **sum/coproduct type**.

Each variant carries different data:

$$
\begin{aligned}
\mathrm{Exit} &\cong 1 \\
\mathrm{Cd} &\cong \mathrm{Dir} \\
\mathrm{Path} &\cong \mathrm{List}(\mathrm{Dir}) \\
\mathrm{External} &\cong \mathrm{Name} \times \mathrm{Argv} \\
\mathrm{ParseError} &\cong 1
\end{aligned}
$$

Therefore:

$$
\mathrm{Command}
\cong
1
+
\mathrm{Dir}
+
\mathrm{List}(\mathrm{Dir})
+
(\mathrm{Name}\times\mathrm{Argv})
+
1
$$

The `cmd.as.*` fields are basically **projections/eliminators for the corresponding coproduct branch**.

---

# 2. What is `execute_command` algebraically?

Your function is:

```c
int execute_command(ShellState *shell, Command cmd)
```

Ignoring C pointer representation for a moment:

$$
\mathrm{executeCommand} :
\mathrm{ShellState}
\times
\mathrm{Command}
\to
\mathrm{Int}
$$

Or curried:

$$
\mathrm{executeCommand} :
\mathrm{ShellState}
\to
(\mathrm{Command}\to\mathrm{Int})
$$

But there is an important detail:

**some commands modify the shell state.**

So `int` alone doesn't describe the whole semantic operation.

A more faithful model is:

$$
\mathrm{executeCommand} :
S \times C
\to
S \times R
$$

where:

$$
S = \mathrm{ShellState}
$$

and

$$
R = \mathrm{ExitCode}
$$

Then:

$$
\boxed{
\mathrm{executeCommand} : S\times C\to S\times R
}
$$

That exposes the real algebra.

---

# 3. Each `case` is a function on one summand

Your switch is essentially **coproduct elimination**.

You have five functions:

$$
\begin{aligned}
f_{\mathrm{exit}} &: S\times\mathrm{Exit}
\to S\times R\\
f_{\mathrm{cd}} &: S\times\mathrm{Cd}
\to S\times R\\
f_{\mathrm{path}} &: S\times\mathrm{Path}
\to S\times R\\
f_{\mathrm{external}} &: S\times\mathrm{External}
\to S\times R\\
f_{\mathrm{parseError}} &: S\times\mathrm{ParseError}
\to S\times R
\end{aligned}
$$

Then `execute_command` combines them into one function:

$$
\mathrm{executeCommand}
=
[f_{\mathrm{exit}},
 f_{\mathrm{cd}},
 f_{\mathrm{path}},
 f_{\mathrm{external}},
 f_{\mathrm{parseError}}]
$$

The square brackets here mean **coproduct elimination / case analysis**.

Conceptually:

$$
\begin{aligned}
S\times
(\mathrm{Exit}+\mathrm{Cd}+\mathrm{Path}+\mathrm{External}+\mathrm{ParseError})
\longrightarrow S\times R
\end{aligned}
$$

The switch is the concrete C implementation of this eliminator.

---

# 4. Now look at your actual handlers

There is an interesting asymmetry.

### `exit`

```c
handle_exit()
```

Doesn't receive the shell:

$$
f_{\mathrm{exit}} : S\times 1\to S\times R
$$

Potentially:

$$
(s,*)\mapsto(s,\mathrm{EXIT})
$$

The shell isn't necessarily modified; you're telling the outer runtime to terminate.

---

### `cd`

```c
handle_cd(cmd.as.cd.dir)
```

Conceptually:

$$
\mathrm{handleCd} :
S\times\mathrm{Dir}
\to
S\times R
$$

because `cd` changes shell state:

$$
s \xrightarrow{\mathrm{cd}(d)} s'
$$

For example:

$$
s.\mathrm{cwd}
\mapsto
d
$$

So `cd` is fundamentally a **state transition**.

---

### `path`

```c
handle_path(
    &shell->path,
    cmd.as.path.dirs,
    cmd.as.path.count
)
```

Again:

$$
\mathrm{handlePath} :
S\times\mathrm{List(Dir)}
\to
S\times R
$$

It transforms part of the state:

$$
s.\mathrm{path}
\mapsto
P
$$

---

### `external`

```c
handle_external(
    &shell->path,
    cmd.as.external.name,
    cmd.as.external.argv
)
```

This is particularly interesting.

The command itself contains:

$$
(\mathrm{Name},\mathrm{Argv})
$$

but execution requires information from the shell:

$$
\mathrm{Path}
$$

So:

$$
\mathrm{handleExternal} :
S\times
(\mathrm{Name}\times\mathrm{Argv})
\to
S\times R
$$

Operationally:

$$
(\text{shell},\text{command})
\mapsto
\text{resolve executable using shell.path}
\mapsto
\text{execute}
\mapsto
\text{exit status}
$$

---

# 5. The key algebraic distinction: `Command` is data, `execute_command` is interpretation

This is probably the most important way to think about your architecture.

You have:

$$
\boxed{
\mathrm{Command}
}
$$

as **data**.

And:

$$
\boxed{
\mathrm{executeCommand}
}
$$

as an **interpreter** of that data.

So:

$$
\mathrm{Command}
\xrightarrow{\quad\mathrm{interpret}\quad}
\mathrm{ShellEffect}
$$

Your parser creates the data:

$$
\mathrm{tokens}
\to
\mathrm{Command}
$$

Then execution interprets it:

$$
\mathrm{Command}
\to
\mathrm{Effect}
$$

So your pipeline is roughly:

$$
\mathrm{Input}
\to
\mathrm{Tokens}
\to
\mathrm{Command}
\to
\mathrm{Execution}
$$

The critical boundary is:

$$
\boxed{
\mathrm{Command}
\text{ is a value; }
\mathrm{executeCommand}
\text{ gives that value meaning.}
}
$$

---

# 6. Why the `switch` is actually mathematically clean

It might look like ugly imperative C, but algebraically it's very canonical.

Suppose:

$$
C=A+B
$$

Then to define:

$$
f:C\to X
$$

you must provide:

$$
f_A:A\to X
$$

and

$$
f_B:B\to X
$$

Then:

$$
f=[f_A,f_B]
$$

Your situation is simply a larger coproduct:

$$
C=A+B+C+D+E
$$

Therefore:

$$
\mathrm{executeCommand}
=
[
\mathrm{handleExit},
\mathrm{handleCd},
\mathrm{handlePath},
\mathrm{handleExternal},
\mathrm{handleParseError}
]
$$

The C `switch` is implementing exactly that case distinction.

---

# 7. `ShellState *shell` is the environment/context

There is another useful abstraction.

Rather than viewing:

```c
ShellState *shell
```

as "some pointer I'm passing around", model it as an **environment/state parameter**:

$$
S
$$

Then:

$$
\mathrm{executeCommand}:S\times C\to S\times R
$$

The state contains things like:

$$
S =
\mathrm{Cwd}
\times
\mathrm{Path}
\times
\mathrm{OtherShellState}
$$

So:

```c
shell->path
```

is just accessing the projection:

$$
\pi_{\mathrm{path}}:S\to\mathrm{Path}
$$

and:

```c
&shell->path
```

means you're giving a handler access to the mutable location representing that component.

---

# 8. Your current C type hides this

Your declaration says:

```c
int execute_command(ShellState *shell, Command cmd)
```

but semantically you're closer to:

$$
\boxed{
\mathrm{executeCommand}:
S\times C
\to
S\times\mathrm{ExitCode}
}
$$

C hides the state transition because you're using a pointer.

For example:

```c
handle_path(&shell->path, ...)
```

looks like:

$$
\mathrm{handlePath}:
\mathrm{Path}^*
\times
\mathrm{PathCommand}
\to
\mathrm{ExitCode}
$$

but semantically it is:

$$
S\times\mathrm{PathCommand}
\to
S\times\mathrm{ExitCode}
$$

with mutation performed through the pointer.

---

# 9. There are therefore two different algebras here

This distinction is useful for your shell design.

### Command algebra

This is the **sum type**:

$$
C =
\mathrm{Exit}
+
\mathrm{Cd}
+
\mathrm{Path}
+
\mathrm{External}
+
\mathrm{ParseError}
$$

It answers:

> **What kind of command is this?**

### State-transition algebra

Each command induces a transition:

$$
S\xrightarrow{c}S'
$$

or more completely:

$$
S\times C\to S\times R
$$

It answers:

> **What does this command do to the shell?**

These are separate concerns.

---

# 10. And `CMD_EXTERNAL` has a second algebra inside it

This is particularly relevant to what you've been thinking about with `$PATH`.

An external command:

$$
\mathrm{External}
=
\mathrm{Name}\times\mathrm{Argv}
$$

doesn't directly contain the executable path.

You need a resolution function:

$$
\mathrm{resolve}:
\mathrm{Path}\times\mathrm{Name}
\to
\mathrm{Option}(\mathrm{Executable})
$$

Then execution becomes:

$$
\mathrm{External}
\times
S
\to
\mathrm{Option}(\mathrm{Executable})
\to
\mathrm{Process}
\to
\mathrm{ExitCode}
$$

So the shell's `path` state isn't itself the executable registry. It is **input to a resolution function**.

That gives you a very clean conceptual decomposition:

$$
\boxed{
\mathrm{Command}
\xrightarrow{\mathrm{dispatch}}
\mathrm{Handler}
\xrightarrow{\mathrm{resolve}}
\mathrm{Operation}
\xrightarrow{\mathrm{execute}}
\mathrm{Result}
}
$$

---

## The whole function in one algebraic expression

Your C:

```c
int execute_command(ShellState *shell, Command cmd) {
    switch (cmd.tag) {
        case CMD_EXIT:
            return handle_exit();

        case CMD_CD:
            return handle_cd(cmd.as.cd.dir);

        case CMD_PATH:
            return handle_path(
                &shell->path,
                cmd.as.path.dirs,
                cmd.as.path.count
            );

        case CMD_EXTERNAL:
            return handle_external(
                &shell->path,
                cmd.as.external.name,
                cmd.as.external.argv
            );

        case CMD_PARSE_ERROR:
        default:
            print_error();
            return 1;
    }
}
```

is essentially:

$$
\boxed{
\begin{aligned}
\mathrm{executeCommand}(s,c)
=
\begin{cases}
f_{\mathrm{exit}}(s) & c\in\mathrm{Exit}\\
f_{\mathrm{cd}}(s,d) & c\in\mathrm{Cd}\\
f_{\mathrm{path}}(s,P) & c\in\mathrm{Path}\\
f_{\mathrm{external}}(s,n,a) & c\in\mathrm{External}\\
f_{\mathrm{error}}(s) & c\in\mathrm{ParseError}
\end{cases}
\end{aligned}
}
$$

So **the `switch` isn't really the important abstraction**. The important abstraction is:

$$
\boxed{
\text{`Command` = coproduct of possible command values}
}
$$

and

$$
\boxed{
\text{`execute_command` = coproduct eliminator/interpreter}
}
$$

while

$$
\boxed{
\text{`ShellState` = evolving state/environment parameter}
}
$$

That is the algebraic skeleton underneath your C implementation.
