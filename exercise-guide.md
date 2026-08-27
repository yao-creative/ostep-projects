**Intent — curriculum sequencing / prerequisite graph:** you want the projects in `yao-creative/ostep-projects` ordered by *conceptual dependency*, and mapped to the exact point in *Operating Systems: Three Easy Pieces (OSTEP)* where each project becomes appropriate.

I read the repository you linked. It is a fork of the original UW OSTEP projects, and the repo explicitly separates **C/Unix projects** from **xv6 kernel-hacking projects**. ([GitHub][1]) The important point is that the GitHub ordering is **not itself the optimal learning order**; the projects are organized by course area. The OSTEP authors also distinguish the book's conceptual progression from the practical projects. ([Research at Wisconsin][2])

## My recommended sequence

| Order | Project                   | Do after OSTEP    | Why                                                         |
| ----: | ------------------------- | ----------------- | ----------------------------------------------------------- |
| **0** | `initial-utilities`       | **Ch. 2**         | C + Unix prerequisite                                       |
| **1** | `initial-reverse`         | Ch. 2             | Tiny C warm-up; optional                                    |
| **2** | `processes-shell`         | **Ch. 5** + Ch. 6 | First real process project                                  |
| **3** | `initial-xv6`             | **Ch. 6**         | Learn how processes/syscalls actually exist inside a kernel |
| **4** | `scheduling-xv6-lottery`  | **Ch. 9**         | Modify kernel scheduling                                    |
| **5** | `vm-xv6-intro`            | **Ch. 23**        | Requires the whole VM story                                 |
| **6** | `concurrency-xv6-threads` | **Ch. 31–32**     | Kernel threads + synchronization                            |
| **7** | `filesystems-checker`     | **Ch. 40–42**     | File-system invariants and crash consistency                |

That is the **core path I would recommend for you**.

The repository contains additional projects—`concurrency-webserver`, `concurrency-pzip`, `concurrency-sort`, `concurrency-mapreduce`, distributed filesystem projects, `initial-kv`, `initial-memcached`, etc.—but I would treat those as **extensions**, not prerequisites for understanding OS internals. ([GitHub][1])

---

# 1. `initial-utilities`

**OSTEP prerequisite: Chapters 1–2**

Do:

* `wcat`
* `wgrep`
* `wzip`
* `wunzip`

This is deliberately **not an OS project yet**. It is a C/Unix fluency test.

The README explicitly says its purpose is to reacquaint you with C, the Unix terminal, command-line programming, and basic Unix utilities. ([GitHub][3])

In particular, you'll encounter:

$$
\text{program}
\rightarrow
\text{compile}
\rightarrow
\text{executable}
\rightarrow
\text{process}
$$

but you're not yet manipulating the process abstraction yourself.

### Why before Ch. 4?

Because later projects assume that you can comfortably write:

```c
int main(int argc, char *argv[])
```

deal with:

* pointers
* buffers
* `FILE *`
* stdin/stdout
* `fopen`
* `fread`
* `fwrite`
* command-line arguments
* exit status
* compilation/debugging

The project itself explicitly assumes basic C and Unix knowledge. ([GitHub][3])

**Do this once, quickly. Don't turn it into a giant C course.**

---

# 2. `processes-shell`

**Do after Chapters 4–6, with Chapter 5 being the key prerequisite.**

I'd actually read:

> Ch. 4 Processes → Ch. 5 Process API → Ch. 6 Limited Direct Execution → **then shell**

The shell project is where the abstraction becomes concrete.

The assignment asks you to implement a command interpreter where the shell creates a child process to execute commands. ([GitHub][4])

So conceptually you're now implementing:

$$
\text{Shell}
\xrightarrow{\text{fork}}
\text{Child Process}
\xrightarrow{\text{exec}}
\text{Program}
$$

and

$$
\text{Parent}
\xrightarrow{\text{wait}}
\text{Child termination}
$$

This makes Chapters 4–6 stop being abstract descriptions.

### This is probably the first project I'd emphasize heavily for you.

Given your recent interest in **dispatchers, traps, processes, schedulers, and runtime architecture**, this project establishes the user-space side of the boundary:

$$
\text{user program}
\leftrightarrow
\text{OS process abstraction}
$$

---

# 3. `initial-xv6`

**Do after Chapter 6.**

This is the transition from:

> "I know what processes and system calls are"

to:

> "I can find where the kernel implements them."

The repository describes xv6 projects as giving you direct experience **inside a real, working operating system**, albeit a deliberately small Unix-like kernel. ([GitHub][1])

This is why I would **not** start xv6 before you've done the process chapters.

The conceptual dependency is roughly:

$$
\text{Process API}
\rightarrow
\text{System call}
\rightarrow
\text{trap}
\rightarrow
\text{kernel handler}
\rightarrow
\text{process structure}
$$

That maps extremely well to the questions you've been asking recently about trap tables and dispatch.

---

# 4. `scheduling-xv6-lottery`

**Do after Chapter 9.**

Read:

* Ch. 7 — Scheduling Introduction
* Ch. 8 — MLFQ
* Ch. 9 — Lottery Scheduling
* optionally Ch. 10 — Multiprocessor Scheduling

Then implement the xv6 lottery scheduler.

The repository explicitly places this under **Processes and Scheduling → Scheduling (Lottery)**. ([GitHub][1])

This is an especially good project because you move from:

$$
\text{scheduler as mathematical policy}
$$

to:

$$
\text{scheduler as kernel mechanism}
$$

You'll see the relationship between:

$$
\text{ready processes}
\rightarrow
\text{scheduler}
\rightarrow
\text{context switch}
\rightarrow
\text{CPU}
$$

---

# 5. `vm-xv6-intro`

**Do much later: after Chapter 23.**

This is where I would **not** follow the temptation to do the project immediately after Ch. 13.

The OSTEP VM section is deliberately cumulative:

$$
13\rightarrow14\rightarrow15\rightarrow16\rightarrow17\rightarrow18
\rightarrow19\rightarrow20\rightarrow21\rightarrow22\rightarrow23
$$

You progressively build:

* address spaces
* memory API
* address translation
* segmentation
* free-space management
* paging
* TLBs
* multi-level page tables
* swapping
* complete virtual memory

Then the xv6 VM project becomes much more intelligible.

The project is specifically the xv6 **Virtual Memory (Null Pointer and Read-Only Regions)** project. ([GitHub][1])

The important conceptual jump is:

$$
\text{virtual address}
\xrightarrow{\text{page table}}
\text{physical address}
$$

and then understanding that the kernel itself is responsible for establishing the mapping and enforcing protection.

---

# 6. `concurrency-xv6-threads`

**Do after Chapters 26–32**, especially:

* Ch. 26 — Concurrency Introduction
* Ch. 27 — Thread API
* Ch. 28 — Locks
* Ch. 29 — Locked Data Structures
* Ch. 30 — Condition Variables
* Ch. 31 — Semaphores
* Ch. 32 — Concurrency Bugs

Then do the xv6 threads project.

The repository categorizes this as **Concurrency → Kernel Threads (Basic Implementation)**. ([GitHub][1])

This is important because otherwise you can easily confuse:

$$
\text{process}
$$

with

$$
\text{thread}
$$

and

$$
\text{CPU scheduling}
$$

with

$$
\text{concurrent execution}.
$$

After this project you should have a much more concrete model:

$$
\text{process}
=
\text{address space}
+
\text{one or more threads}
$$

rather than treating "process" as synonymous with "running program."

---

# 7. `filesystems-checker`

**Do after Chapters 39–42.**

I'd read:

* Ch. 39 — Files and Directories
* Ch. 40 — File System Implementation
* Ch. 41 — Fast File System
* Ch. 42 — FSCK and Journaling

Then do `filesystems-checker`.

The project is explicitly listed under **File Systems → File System Checker**. ([GitHub][1])

This one is particularly nice because the central concept is **invariants**.

Instead of primarily asking:

> "How do I implement this filesystem?"

you're asking:

> "Given this filesystem state, does it satisfy the filesystem's invariants?"

That gives you a very useful systems-programming pattern:

$$
\text{state}
\xrightarrow{\text{invariant checker}}
\begin{cases}
\text{valid}\\
\text{invalid}
\end{cases}
$$

This connects strongly to the way you've been thinking about **compile-time/business-level correctness and state invariants**.

---

# The complete OSTEP → project map

I'd use this as your actual curriculum:

$$
\boxed{
\text{Ch 1–2}
\rightarrow
\text{initial-utilities}
}
$$

$$
\boxed{
\text{Ch 4–6}
\rightarrow
\text{processes-shell}
\rightarrow
\text{initial-xv6}
}
$$

$$
\boxed{
\text{Ch 7–10}
\rightarrow
\text{scheduling-xv6-lottery}
}
$$

$$
\boxed{
\text{Ch 13–23}
\rightarrow
\text{vm-xv6-intro}
}
$$

$$
\boxed{
\text{Ch 26–32}
\rightarrow
\text{concurrency-xv6-threads}
}
$$

$$
\boxed{
\text{Ch 39–42}
\rightarrow
\text{filesystems-checker}
}
$$

with the skipped chapters still being **read**, not skipped conceptually.

---

## What I'd do with the other projects

The repo actually has a useful second layer of projects:

### Concurrency

* `concurrency-webserver`
* `concurrency-pzip`
* `concurrency-sort`
* `concurrency-mapreduce`

These are excellent **application-level concurrency** projects, whereas `concurrency-xv6-threads` is **kernel-level concurrency**. ([GitHub][1])

I'd therefore do:

$$
\text{Ch 26–32}
\rightarrow
\text{pzip/sort/webserver}
\rightarrow
\text{xv6 threads}
$$

if your goal is deep concurrency understanding.

### Distributed systems

The repository also has:

* `filesystems-distributed`
* `filesystems-distributed-ufs`

I'd leave these until **after Ch. 48–50**, because OSTEP's distributed-systems material comes after the filesystem section. ([UW Computer Sciences User Pages][5])

### `initial-kv` / `initial-memcached`

I'd treat these as optional systems-programming projects rather than part of the core OS progression.

---

# The important distinction: homework vs projects

Don't confuse the OSTEP **homework simulators** with these projects.

For example, OSTEP's normal learning sequence has you interactively simulate:

$$
\text{scheduler}
\rightarrow
\text{MLFQ}
\rightarrow
\text{paging}
\rightarrow
\text{TLB}
\rightarrow
\text{disk}
\rightarrow
\text{RAID}
$$

while the projects have you build things in C/Linux or modify xv6. The OSTEP site explicitly presents these as complementary components of the course. ([Research at Wisconsin][2])

So my preferred learning loop for you is:

$$
\boxed{
\text{Read}
\rightarrow
\text{simulate}
\rightarrow
\text{implement}
\rightarrow
\text{inspect kernel}
\rightarrow
\text{explain invariants}
}
$$

rather than:

$$
\text{Read entire OSTEP}
\rightarrow
\text{do all projects}
$$

---

## If your goal is specifically **systems programming / kernel understanding**

I'd make the curriculum even narrower:

| Phase | OSTEP | Project                   | Core abstraction              |
| ----- | ----- | ------------------------- | ----------------------------- |
| 1     | 1–2   | `initial-utilities`       | C / Unix                      |
| 2     | 4–6   | `processes-shell`         | Process                       |
| 3     | 6     | `initial-xv6`             | Syscall / trap / process      |
| 4     | 7–9   | `scheduling-xv6-lottery`  | Scheduler                     |
| 5     | 13–23 | `vm-xv6-intro`            | Address space / page tables   |
| 6     | 26–32 | `concurrency-xv6-threads` | Threads / synchronization     |
| 7     | 39–42 | `filesystems-checker`     | Persistent state / invariants |

**That is the path I'd recommend for you.**

It has a very clean increasing abstraction boundary:

$$
\boxed{
\text{Unix program}
\rightarrow
\text{process}
\rightarrow
\text{kernel}
\rightarrow
\text{scheduler}
\rightarrow
\text{virtual memory}
\rightarrow
\text{concurrency}
\rightarrow
\text{filesystem}
}
$$

And importantly, it gets you into **xv6 relatively early** rather than spending weeks doing only simulated OS exercises. The official project repository itself describes xv6 as the part that gives direct experience *inside* an operating system. ([GitHub][1])

[Your yao-creative/ostep-projects repository](https://github.com/yao-creative/ostep-projects?utm_source=chatgpt.com)
[Official OSTEP textbook site](https://pages.cs.wisc.edu/~remzi/OSTEP/?utm_source=chatgpt.com)

[1]: https://github.com/yao-creative/ostep-projects "GitHub - yao-creative/ostep-projects: Projects for an undergraduate OS course · GitHub"
[2]: https://research.cs.wisc.edu/wind/OSTEP/?utm_source=chatgpt.com "Operating Systems: Three Easy Pieces"
[3]: https://github.com/yao-creative/ostep-projects/tree/master/initial-utilities "ostep-projects/initial-utilities at master · yao-creative/ostep-projects · GitHub"
[4]: https://github.com/yao-creative/ostep-projects/tree/master/processes-shell "ostep-projects/processes-shell at master · yao-creative/ostep-projects · GitHub"
[5]: https://pages.cs.wisc.edu/~remzi/OSTEP/?utm_source=chatgpt.com "Operating Systems: Three Easy Pieces"
