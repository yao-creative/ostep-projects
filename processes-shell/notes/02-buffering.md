**Intent: systems-I/O semantics / buffering discipline.** The key is to stop thinking of `fflush()` as “write to disk.” It is more precisely a **boundary operation between a user-space stdio buffer and the underlying file descriptor/device**.

## 1. The fundamental model

For ordinary C stdio on Linux, think of the pipeline as:

$$
\text{your program}
\rightarrow
\text{stdio } FILE\text{ buffer}
\rightarrow
\text{write(2)}
\rightarrow
\text{kernel page cache / device buffers}
\rightarrow
\text{storage}
$$

So:

```c
fprintf(file, "hello");
```

may initially produce:

$$
\text{program state}
\rightarrow
\boxed{\text{stdio buffer: "hello"}}
\rightarrow
\text{kernel}
\rightarrow
\text{disk}
$$

whereas:

```c
fprintf(file, "hello");
fflush(file);
```

requests:

$$
\boxed{\text{stdio buffer: "hello"}}
\xrightarrow{\text{fflush}}
\text{write()}
\rightarrow
\boxed{\text{kernel buffer}}
\rightarrow
\text{disk eventually}
$$

Linux explicitly documents that `fflush()` only flushes the **C-library/user-space buffer**; it does **not** guarantee physical persistence. `fsync()` is the operation relevant to pushing filesystem data toward persistent storage. ([man7.org][1])

That distinction is probably the most important thing to internalize.

---

# 2. What `fflush()` actually means

For an output stream:

```c
fflush(fp);
```

means roughly:

> "Take any output that the stdio layer has buffered for `fp` and pass it to the underlying output mechanism now."

Linux's implementation ultimately uses the stream's underlying write operation. ([man7.org][1])

It **doesn't mean**:

> "Make sure this data survives a power failure."

That's a different durability contract.

A useful hierarchy is:

$$
\text{application visibility}
<
\text{stdio flush}
<
\text{kernel write}
<
\text{filesystem persistence}
<
\text{hardware persistence}
$$

with different mechanisms responsible for each boundary.

| Goal                                     | Typical mechanism            |
| ---------------------------------------- | ---------------------------- |
| Put bytes into stdio buffer              | `fprintf`, `fwrite`          |
| Push stdio buffer out                    | `fflush`                     |
| Push kernel buffered data toward storage | `fsync`                      |
| Close stream + flush                     | `fclose`                     |
| Force all stdio output streams           | `fflush(NULL)` on POSIX      |
| Avoid stdio buffering                    | `setbuf`, `setvbuf`, `write` |

---

# 3. Why buffering exists in the first place

Suppose you write one byte at a time:

```c
for (...) {
    fputc(c, fp);
}
```

Without buffering you might conceptually perform:

$$
N\text{ application calls}
\rightarrow
N\text{ system calls}
$$

With stdio buffering:

$$
N\text{ application calls}
\rightarrow
\text{one/few large writes}
$$

So buffering changes:

$$
\text{many small expensive operations}
\rightarrow
\text{fewer larger operations}
$$

This is why **you generally should not sprinkle `fflush()` everywhere**.

Every flush potentially turns accumulated work into an OS write.

---

# 4. The classic terminal case

Consider:

```c
printf("Enter your name: ");
scanf("%s", name);
```

You might expect:

```text
Enter your name:
```

to appear before the program waits.

Usually it does if `stdout` is connected to a terminal because `stdout` is normally **line-buffered** in that situation. But if you've printed only a partial line, an explicit flush is the robust way to establish the output boundary:

```c
printf("Enter your name: ");
fflush(stdout);

scanf("%s", name);
```

Linux documents this terminal behavior: terminal `stdout` is normally line-buffered, and an explicit `fflush()` is useful when computation or interaction occurs before a newline. ([man7.org][2])

### Best practice

For interactive prompts:

```c
printf("Password: ");
fflush(stdout);
```

That's an excellent use of `fflush()`.

---

# 5. `stdout` vs `stderr`

A useful mental model:

```c
stdout  → normal program output
stderr  → diagnostic/error output
```

On Linux, `stderr` is not fully buffered, while `stdout` is line-buffered when attached to a terminal and may be fully buffered when redirected. ([man7.org][2])

Therefore:

```bash
./program
```

and

```bash
./program > output.txt
```

can have **different buffering behavior**.

This surprises people.

The program has not changed.

The **sink changed**, so the buffering policy can change.

Formally, buffering behavior can depend on the mapping:

$$
\text{stream}
\rightarrow
\text{underlying device}
$$

rather than merely on the stream itself.

---

# 6. The big distinction: `fflush()` vs `fsync()`

This is probably the most important systems distinction.

Suppose:

```c
fprintf(fp, "transaction completed\n");
fflush(fp);
```

After the `fflush()` succeeds, you can reasonably say:

> The stdio layer has handed its buffered bytes to the underlying output mechanism.

You **cannot automatically say**:

> The transaction is durable on disk.

On Linux:

```c
fflush(fp);
fsync(fileno(fp));
```

has a substantially stronger persistence intent.

Conceptually:

$$
\text{stdio buffer}
\xrightarrow{\texttt{fflush}}
\text{kernel}
\xrightarrow{\texttt{fsync}}
\text{filesystem/storage durability}
$$

The Linux man page explicitly warns about this distinction. ([man7.org][1])

---

# 7. `fclose()` already flushes

Normally:

```c
fprintf(fp, "hello");
fclose(fp);
```

is sufficient to flush the stdio output as part of closing the stream.

So this:

```c
fprintf(fp, "hello");
fflush(fp);
fclose(fp);
```

is usually redundant if you don't need the data to become visible before closing.

Use:

```c
fclose(fp);
```

when your semantic requirement is simply:

> "I'm finished with this stream."

Use:

```c
fflush(fp);
```

when your requirement is:

> "I'm keeping this stream open, but I need its buffered output emitted now."

---

# 8. A good decision rule

Think in terms of **required observation boundary**.

### A. "I want the user to see this now"

```c
printf("Loading...");
fflush(stdout);
```

### B. "I want another process reading this file to have a chance to see the data"

```c
fprintf(fp, "event\n");
fflush(fp);
```

This establishes the stdio → kernel boundary, although the exact visibility semantics also depend on the reader and filesystem.

### C. "I want the data to survive a crash/power failure"

```c
fprintf(fp, "important data\n");
fflush(fp);
fsync(fileno(fp));
```

Now you're expressing a durability requirement.

### D. "I'm done"

```c
fclose(fp);
```

Don't manually flush unless there's a reason.

---

# 9. `fflush(NULL)`

POSIX allows:

```c
fflush(NULL);
```

which flushes all open output streams for which flushing is defined. ([man7.org][1])

This is occasionally useful, but I wouldn't make it normal application practice.

Prefer:

```c
fflush(stdout);
```

over:

```c
fflush(NULL);
```

because the former expresses the actual dependency.

This fits a general systems principle:

> **Flush the smallest state boundary necessary to establish the invariant.**

---

# 10. What about input?

This is where cross-platform portability gets interesting.

Historically, people learn:

```c
fflush(stdin);
```

as a way to "clear keyboard input."

**Don't do that as portable C.**

POSIX specifies behavior for input streams in particular circumstances—for seekable input files, buffered unread data can be discarded and the underlying file position adjusted. ([man7.org][3])

But this is not the same thing as:

> "clear the terminal's input buffer."

Those are different layers:

$$
\text{terminal}
\rightarrow
\text{kernel terminal buffer}
\rightarrow
\text{stdio stdin buffer}
$$

`fflush(stdin)` is not a general-purpose "clear keyboard buffer" primitive.

For terminal input, you need terminal-specific mechanisms such as `termios` when you actually need to manipulate terminal behavior.

---

# 11. Linux vs Windows

The **conceptual model is the same**:

$$
\text{application}
\rightarrow
\text{language/runtime buffer}
\rightarrow
\text{OS I/O}
\rightarrow
\text{storage/device}
$$

but the APIs and exact semantics differ.

On Microsoft C runtime, `fflush()` similarly flushes a writable stream's buffered data to its underlying file/device. Microsoft also explicitly distinguishes this from committing data to disk. ([Microsoft Learn][4])

So the portable conceptual abstraction is:

$$
\boxed{
\text{flush runtime buffer}
\neq
\text{persist storage}
}
$$

This is much more useful than memorizing Linux-specific behavior.

---

# 12. The deeper systems theory

I would model `fflush()` as a **buffer state transition**.

Let a stream have state:

$$
S = (B, D)
$$

where:

* $B$ = bytes currently buffered in user space
* $D$ = bytes already handed to the underlying OS mechanism

A write operation can be:

$$
(B,D)
\xrightarrow{\text{write }x}
(B \mathbin{\|} x,D)
$$

where $\mathbin{|}$ denotes concatenation.

Then:

$$
(B,D)
\xrightarrow{\texttt{fflush}}
(\epsilon,D\mathbin{\|}B)
$$

So `fflush()` isn't primarily a **data-producing operation**.

It's a **state-transition operation that changes where ownership/responsibility for the bytes resides**.

That's why I think it fits nicely with the state-machine thinking you've been using recently.

---

# 13. Buffering policies

C stdio generally gives you three useful buffering modes:

```c
setvbuf(fp, buffer, _IOFBF, size); // full buffering
setvbuf(fp, buffer, _IOLBF, size); // line buffering
setvbuf(fp, buffer, _IONBF, 0);    // unbuffered
```

Conceptually:

### Full buffering

$$
\text{write calls}
\rightarrow
\boxed{\text{large buffer}}
\rightarrow
\text{flush when appropriate}
$$

Good for files and high-throughput I/O.

### Line buffering

$$
\text{write calls}
\rightarrow
\boxed{\text{buffer}}
\xrightarrow{\text{newline}}
\text{flush}
$$

Good for interactive terminal output.

### Unbuffered

$$
\text{write}
\rightarrow
\text{underlying mechanism}
$$

Useful for special cases, but don't use it reflexively; buffering usually exists for performance.

---

# 14. Best-practice rules I'd memorize

### Rule 1 — Don't flush after every write

Bad:

```c
for (...) {
    fprintf(fp, "%s\n", event);
    fflush(fp);
}
```

unless you specifically require every event to cross the stdio boundary immediately.

Better:

```c
for (...) {
    fprintf(fp, "%s\n", event);
}

fflush(fp);
```

or batch according to your application's latency requirement.

This is directly analogous to the `BufferedSink` design you've been exploring:

$$
\text{append}
\rightarrow
\text{accumulate}
\rightarrow
\text{flush on boundary}
$$

---

### Rule 2 — Explicitly flush interactive prompts

```c
printf("Choice: ");
fflush(stdout);
```

This is one of the canonical uses.

---

### Rule 3 — Don't confuse flush with durability

```c
fflush(fp);
```

means approximately:

> "empty the stdio buffer."

Not:

> "survive a power loss."

For the latter, investigate:

```c
fsync(fileno(fp));
```

on POSIX systems. ([man7.org][1])

---

### Rule 4 — Check the return value when it matters

```c
if (fflush(fp) == EOF) {
    // handle I/O failure
}
```

Linux specifies `0` for success and `EOF` on failure, with `errno` indicating the error. ([man7.org][1])

This matters especially for logging, databases, transactional output, etc.

---

### Rule 5 — Prefer `fclose()` when you're finished

```c
fclose(fp);
```

rather than:

```c
fflush(fp);
fclose(fp);
```

unless you need the intermediate flush.

---

### Rule 6 — Avoid `fflush(stdin)` as a portability technique

It is not a general portable "clear input" operation.

---

## The hierarchy I'd keep in your head

Ultimately, there are **three different questions**:

$$
\boxed{
\begin{array}{c}
\text{Has my runtime emitted the bytes?}\\
\downarrow\\
\texttt{fflush}\\[4pt]
\text{Has the OS received/processed the write?}\\
\downarrow\\
\texttt{write}\\[4pt]
\text{Do I have a durability guarantee?}\\
\downarrow\\
\texttt{fsync}
\end{array}}
$$

And the architectural principle is:

> **Buffer freely inside a layer; flush when crossing a semantic boundary; synchronize when the contract requires durability.**

That's the same abstraction you're reaching toward with event sinks and batching: **buffering is an optimization over a stream, while flushing is a boundary in the stream's state machine.** Linux's `fflush()` documentation is unusually explicit about exactly where that boundary sits. ([man7.org][1])

[1]: https://man7.org/linux/man-pages/man3/fflush.3.html?utm_source=chatgpt.com "fflush(3) - Linux manual page"
[2]: https://man7.org/linux/man-pages/man3/stdin.3.html?utm_source=chatgpt.com "stdin(3) - Linux manual page"
[3]: https://man7.org/linux/man-pages/man3/fflush.3p.html?utm_source=chatgpt.com "fflush(3p) - Linux manual page"
[4]: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fflush?view=msvc-170&utm_source=chatgpt.com "fflush | Microsoft Learn"
