Yes. The cleanest way to understand this is to separate **three different ideas** that C is mixing together:

1. **Algebraic structure:** `Command` is a sum type.
2. **Representation:** C implements that sum using `tag + union`.
3. **Memory access:** `.` and `->` are operators for selecting fields depending on whether you have a value or a pointer.

---

# 1. Formalize `Command` as a set/algebra

Your definition is essentially:

```c
typedef enum {
    CMD_EXIT,
    CMD_CD,
    CMD_PATH,
    CMD_EXTERNAL,
    CMD_PARSE_ERROR
} CommandTag;
```

plus payloads:

```c
Exit        = 1
Cd          = Dir
Path        = List(Dir)
External    = Name × Argv
ParseError  = 1
```

So mathematically:

$$
\boxed{
Command =
Exit + Cd + Path + External + ParseError
}
$$

where \(+\) is a **disjoint union**.

More explicitly:

$$
Command
=
\{Exit\}
+
Dir
+
List(Dir)
+
(Name\times Argv)
+
\{ParseError\}
$$

The `+` is important: it means these are alternatives.

For example:

$$
Cd("/tmp")
$$

is one element of `Command`.

So is:

$$
External("ls", ["ls","-l"])
$$

but a command isn't simultaneously both.

---

# 2. What does the C representation actually look like?

C doesn't have native algebraic data types, so you manually encode:

$$
\boxed{
Command = Tag \times PayloadStorage
}
$$

with the invariant:

$$
tag = CMD\_CD
\implies
payload \text{ is interpreted as } Cd
$$

etc.

Conceptually:

```text
Command
├── tag
└── as
    ├── cd
    ├── path
    └── external
```

The crucial thing is that `as` isn't another conceptual algebraic layer.

It's just the **storage location for the payload**.

You could call it:

```c
union {
    ...
} args;
```

and then write:

```c
cmd.args.cd.dir
```

You could call it:

```c
union {
    ...
} payload;
```

and write:

```c
cmd.payload.cd.dir
```

You could call it:

```c
union {
    ...
} data;
```

and write:

```c
cmd.data.cd.dir
```

`as` has **no special meaning in C**.

It's simply a naming convention.

---

# 3. Why not simply `tag + args`?

You absolutely can.

In fact, if you're thinking of:

> "The tag determines which argument structure I'm looking at."

that's exactly right.

For example:

```c
Command cmd = {
    .tag = CMD_CD,
    .as.cd.dir = "/tmp"
};
```

can be thought of as:

$$
cmd = (CMD\_CD,\; "/tmp")
$$

while:

```c
Command cmd = {
    .tag = CMD_EXTERNAL,
    .as.external.name = "ls",
    .as.external.argv = argv
};
```

is:

$$
cmd = (CMD\_EXTERNAL,\; ("ls", argv))
$$

So your intuition is correct:

$$
\boxed{
as \approx \text{arguments/payload of the selected variant}
}
$$

But `payload` is actually a little more precise than `args`.

Why?

Because the union doesn't necessarily represent *function arguments*. It represents the **data carried by the variant**.

For example:

```c
CMD_EXIT
```

has no arguments.

But algebraically:

$$
Exit \cong 1
$$

It is still a variant of the sum.

So I would personally name it:

```c
union { ... } payload;
```

if designing this from scratch.

Then:

```c
cmd.payload.cd.dir
```

reads naturally as:

> the payload of this command, interpreted as a CD command.

---

# 4. Why do we need the tag?

Because the union itself doesn't tell you which interpretation is valid.

Consider:

```c
union {
    struct { char *dir; } cd;
    struct { char **dirs; size_t count; } path;
    struct { char *name; char **argv; } external;
} payload;
```

A C `union` means these occupy the **same storage**.

So conceptually:

$$
PayloadStorage
=
CdStorage
\;\cup\;
PathStorage
\;\cup\;
ExternalStorage
$$

but not in the safe algebraic sense.

C lets you do:

```c
payload.cd.dir
```

and then:

```c
payload.external.name
```

even though only one interpretation is supposed to be valid.

Therefore you add:

```c
CommandTag tag;
```

which gives you the invariant:

$$
\boxed{
tag \text{ determines which interpretation of payload is valid}
}
$$

That's why this is called a **tagged union**.

---

# 5. `switch` is choosing the one branch

Now your executor:

```c
switch (cmd.tag) {
    case CMD_EXIT:
        ...
    case CMD_CD:
        ...
    case CMD_PATH:
        ...
    case CMD_EXTERNAL:
        ...
}
```

is essentially doing:

$$
\operatorname{case} :
Command \rightarrow Result
$$

with branches:

$$
\begin{aligned}
f_{exit} &: Exit \rightarrow Result\\
f_{cd} &: Cd \rightarrow Result\\
f_{path} &: Path \rightarrow Result\\
f_{external} &: External \rightarrow Result\\
f_{error} &: ParseError \rightarrow Result
\end{aligned}
$$

Then the whole function is:

$$
[f_{exit},f_{cd},f_{path},f_{external},f_{error}]
:
Command\rightarrow Result
$$

This is the algebraic reason the design is so useful.

The command is **data representing an intention**.

The executor is the **interpreter of that data**.

---

# 6. "Choosing a single path" formalized

This is particularly relevant to your shell.

Suppose:

```text
PATH = ["/usr/local/bin", "/usr/bin", "/bin"]
```

and the user enters:

```text
ls
```

Your resolution operation is something like:

$$
resolve : Path \times Name \rightarrow Option(Executable)
$$

where:

$$
Path = List(Directory)
$$

Given:

$$
P = [d_1,d_2,\ldots,d_n]
$$

and command:

$$
x = "ls"
$$

you generate candidates:

$$
d_1/x,\ d_2/x,\ldots,d_n/x
$$

and choose the **first valid executable**.

Formally:

$$
resolve(P,x)
=
d_i/x
$$

where:

$$
i =
\min\{j \mid executable(d_j/x)\}
$$

if such a \(j\) exists.

Otherwise:

$$
resolve(P,x)=None
$$

So:

```text
PATH = [/usr/local/bin, /usr/bin, /bin]

ls
 ↓
/usr/local/bin/ls       ✗
 ↓
/usr/bin/ls             ✓
 ↓
stop
```

The important point is:

$$
\boxed{
PATH \text{ is a search structure, not a registry of binaries}
}
$$

You don't generally register `ls` when executing `path`.

Instead:

```c
path /usr/local/bin /usr/bin /bin
```

changes:

$$
ShellState.path
$$

and later:

```c
ls
```

performs:

$$
resolve(ShellState.path,"ls")
$$

So the state determines the search domain **at runtime**.

---

# 7. Now `.`, the important part

Consider:

```c
cmd.as.cd.dir
```

Every `.` means:

> select a member from an object/value.

So:

```c
cmd.as
```

means:

$$
cmd : Command
$$

therefore:

$$
cmd.as : Union
$$

then:

```c
cmd.as.cd
```

means:

$$
cmd.as.cd : CdPayload
$$

then:

```c
cmd.as.cd.dir
```

means:

$$
cmd.as.cd.dir : char*
$$

So `. ` is essentially:

$$
\boxed{x.m = \text{member }m\text{ of value }x}
$$

It is **not about passing by value**.

---

# 8. Why `->` then?

Suppose:

```c
ShellState *shell;
```

Here:

$$
shell : ShellState*
$$

It is not the `ShellState` itself.

It's a pointer to one.

Therefore:

```c
shell->path
```

means:

```c
(*shell).path
```

Formally:

$$
\boxed{
p->m \equiv (*p).m
}
$$

So:

```c
shell->path
```

means:

1. follow `shell`
2. obtain the `ShellState`
3. select its `path` member

Whereas:

```c
cmd.as
```

means:

1. `cmd` already **is** a `Command`
2. select its `as` member

Hence:

| Expression      | Type of left side | Operator |
| --------------- | ----------------- | -------- |
| `cmd.as`        | `Command`         | `.`      |
| `cmd.as.cd`     | `union`           | `.`      |
| `cmd.as.cd.dir` | `struct`          | `.`      |
| `shell->path`   | `ShellState *`    | `->`     |
| `(*shell).path` | `ShellState`      | `.`      |

---

# 9. Is this determined by the type?

**Yes. Exactly.**

The distinction is determined by the type of the expression on the left.

If:

```c
x : T
```

use:

```c
x.member
```

If:

```c
p : T*
```

use:

```c
p->member
```

which means:

```c
(*p).member
```

So this:

```c
cmd.as
```

is valid because:

```text
cmd : Command
```

while:

```c
shell->path
```

is valid because:

```text
shell : ShellState *
```

This has nothing fundamentally to do with how the object was passed into a function.

For example:

```c
void f(ShellState shell) {
    shell.path;       // .
}
```

versus:

```c
void f(ShellState *shell) {
    shell->path;      // ->
}
```

The **parameter type** determines the operator.

---

# 10. C always passes arguments by value

This is extremely important for your current understanding of C.

C doesn't technically have "pass by reference."

It has:

$$
\boxed{\text{pass by value}}
$$

always.

Suppose:

```c
void f(ShellState *shell)
```

and caller does:

```c
f(&shell);
```

The pointer itself is copied.

Conceptually:

$$
caller:
p = \&shell
$$

then:

$$
callee:
shell = p
$$

Both pointer values refer to the same object.

Therefore:

```c
shell->path.count = 5;
```

mutates the original object.

That's why C can **simulate reference semantics** using pointers.

---

# 11. This is also why your `Command cmd` is interesting

You have:

```c
int execute_command(ShellState *shell, Command cmd)
```

Here:

```text
shell : ShellState *
cmd   : Command
```

Therefore:

```c
shell->path
cmd.as
```

Notice the distinction.

`cmd` itself is copied into `execute_command`.

But its pointers are also copied only as pointer values.

Suppose:

```c
cmd.as.cd.dir = some_string;
```

Then conceptually:

```text
Command copy
    |
    +---- dir ----> "hello"
```

The `Command` object was copied, but `"hello"` wasn't copied.

This is one of the most important C rules:

$$
\boxed{
\text{copying a pointer does not copy its pointee}
}
$$

---

# 12. Borrowing in C

This is where C becomes much less safe than Rust.

Suppose:

```c
char *dir;
```

You can think of it as:

$$
dir : Address(Char)
$$

But C does **not** encode:

> "I don't own this memory."

Nor does it encode:

> "This memory must remain alive until I finish using it."

Those are programmer-level contracts.

So if you write:

```c
Command parse(...) {
    char buffer[1024];

    ...

    return (Command){
        .tag = CMD_CD,
        .as.cd.dir = buffer
    };
}
```

this is broken.

Why?

Because:

$$
lifetime(buffer) < lifetime(returned\ Command)
$$

Once `parse()` returns, `buffer` no longer exists.

You have created a dangling pointer.

---

# 13. The borrowing invariant

If `Command` borrows some object \(x\), the fundamental requirement is:

$$
\boxed{
Lifetime(x) \supseteq Lifetime(Command\text{'s use of }x)
}
$$

For example:

```c
char input[1024];

Command cmd = parse(input);

execute_command(&shell, cmd);
```

This can be fine if `cmd` only points into `input` and `input` remains alive until execution completes.

Conceptually:

$$
input
\supset
cmd
\supset
use
$$

in lifetime ordering.

---

# 14. Best practice: make borrowing explicit with `const`

Your current definition says:

```c
struct { char *dir; }
```

But if the command only reads the directory:

```c
struct { const char *dir; }
```

is generally better.

Likewise:

```c
const char *name;
const char **argv;
```

depending on the exact ownership/mutability model.

For example:

```c
typedef struct {
    CommandTag tag;

    union {
        struct {
            const char *dir;
        } cd;

        struct {
            const char **dirs;
            size_t count;
        } path;

        struct {
            const char *name;
            const char **argv;
        } external;
    } payload;

} Command;
```

Now you're communicating:

> The command refers to these strings, but the command does not intend to modify them.

`const` is not a full ownership system, but it makes the contract much clearer.

---

# 15. The three useful ownership models in C

For every pointer in a C API, try to answer:

### A. Borrowed

```c
void print_command(const Command *cmd);
```

The function doesn't own `cmd`.

Contract:

$$
Lifetime(cmd) \ge Lifetime(call)
$$

---

### B. Owned

For example:

```c
Command *command_create(...);
void command_destroy(Command *cmd);
```

Now the API explicitly establishes:

$$
create \rightarrow ownership
$$

and eventually:

$$
destroy \rightarrow release
$$

The caller is responsible for the lifetime.

---

### C. Transferred ownership

For example:

```c
void set_path(Path *path);
```

could establish a contract such as:

> after calling this, the `ShellState` owns `path`.

C won't enforce that. Documentation and naming conventions have to.

---

# 16. Best practice for your shell specifically

I'd make the conceptual ownership boundary extremely explicit.

Your parser can produce a **borrowed command**:

$$
parse :
Input \rightarrow Command_{borrowed}
$$

Then:

$$
execute :
ShellState \times Command_{borrowed}
\rightarrow
ShellState\times Result
$$

The input owns the actual strings.

The command merely provides views into them.

For example:

```text
input buffer
   │
   ├── "cd"
   ├── "/tmp"
   └── ...
         ↑
         │ borrowed pointers
         │
      Command
         │
         └── Cd("/tmp")
```

Then:

```text
parse
  ↓
Command
  ↓
execute
  ↓
discard Command
```

This is a very reasonable C architecture for a small shell.

---

# 17. A particularly useful mental model

You can view your whole shell as three algebraic layers:

$$
\boxed{
Input
\xrightarrow{parse}
Command
\xrightarrow{execute_{ShellState}}
ShellState
}
$$

More precisely:

$$
parse : Input \rightarrow Command
$$

and:

$$
execute : ShellState\times Command
\rightarrow
ShellState\times ExitCode
$$

Inside `execute`, external commands use:

$$
resolve : Path\times Name
\rightarrow
Option(Executable)
$$

So:

$$
\begin{aligned}
Input
&\xrightarrow{parse} Command\\
&\xrightarrow{execute} ShellState'\\
Command_{External}
&\xrightarrow{resolve(ShellState.path,\cdot)}
Executable\\
&\xrightarrow{exec}
Process
\end{aligned}
$$

And the C syntax is merely the representation machinery:

```c
cmd.as.cd.dir
```

= select fields from a **value**

while:

```c
shell->path
```

= dereference a **pointer to a value**, then select a field.

### The core rule to keep in your head

$$
\boxed{
. \text{ means "select from this value"}
}
$$

$$
\boxed{
-> \text{ means "follow this pointer, then select"}
}
$$

and:

$$
\boxed{
\text{C always passes by value; pointers are values containing addresses}
}
$$

while:

$$
\boxed{
\text{borrowing in C is a lifetime convention, not a type-system guarantee}
}
$$

That last distinction is probably the most important difference between the way you're currently thinking about C and Rust.
