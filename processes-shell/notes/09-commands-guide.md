It is completely normal to be confused by this when building your first shell! The biggest mental hurdle in this project is realizing that your shell handles two completely different categories of commands.

Here is the breakdown of what a path is and exactly what you are responsible for programming.

### 1. Built-in Commands (You write the logic)

These are commands that your shell executes internally. You don't launch a new process; you just write the C code to make them happen right inside your main `while` loop. You **only** need to implement these three built-in commands:

* **`exit`**: You simply call `exit(0)` in your C code to close the shell.
* **`cd`**: You use the `chdir()` system call in your C code to change the current directory.
* **`path`**: You update a variable (like an array) in your C code that stores the list of directories your shell is allowed to search.

### 2. External Commands (The OS does the work)

For *any other command* the user types (like `ls`, `cat`, `grep`, or `echo`), your shell does **not** know what these mean, and you do **not** have to program them. These are actual, pre-compiled programs sitting on your computer's hard drive. Your shell's only job is to find them and run them.

* You will use `fork()` to create a new child process.
* You will use `execv()` in that child process to run the requested program.

### 3. What is a "Path"?

When the user types an external command like `ls`, your shell needs to know exactly where the `ls` program is located on the hard drive so `execv()` can run it.

* The **path** is just a list of specific folders where your shell is allowed to look for these programs.
* By default, the instructions say your shell should start with just one folder in its path: `/bin`.
* When the user types `ls`, your shell uses the `access()` system call to check: "Does a file named `/bin/ls` exist, and is it executable?"
* If it does, your shell runs `/bin/ls`. If it doesn't, your shell prints the exact error message specified in the instructions.
* If the user runs your built-in `path` command (e.g., `wish> path /bin /usr/bin`), they are telling your shell to update its search list. The next time they type a command, your shell must check inside `/bin` first, and if it's not there, check `/usr/bin`.

Would you like me to break down how to use the `access()` system call to search through your path directories?