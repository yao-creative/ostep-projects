Here’s an ASCII view of the same model: creating an allocation adds a new `alloc` identity `a0`, then adds the first ownership/borrow tuple for it.

```text
LEGEND
a0 = (birth_time, [p, p+n), kind)      kind = heap/stack/static/page
owner tuple  = (time, a0, loc, owner, agent)
borrow tuple = (time, a0, loc, mode, agent)
time = [start, end)
loc  = [p, p+n)
```

## Generic before/after

```text
BEFORE ALLOCATION a0 EXISTS
===========================
Allocs:   { ... }              // a0 is not here
Borrows:  { ... }              // no tuple refers to a0

Memory:
        p                         p+n
        |                          |
        v                          v
      +----------------------------+
      | free / invalid / metadata  |
      +----------------------------+
```

```text
AFTER ALLOCATION a0 EXISTS
==========================
Allocs:   { ..., a0 }
Borrows:  { ..., (T0, a0, L0, owner, x) }

Memory:
        p                         p+n
        |                          |
        v                          v
      +----------------------------+
      | allocated to a0            |
      | uninit / zeroed / value    |
      +----------------------------+

Relation:
      x ---- owns/exclusive ----> a0 ---- points to ----> [p, p+n)
```

## Concrete heap allocation

```rust
let x = Box::new(42);
```

```text
BEFORE
------
Stack:                 Heap:
+--------+             +----------------------+
| (none) |             | free block           |
+--------+             | [0x1000,0x1008)      |
                       +----------------------+

Allocs:  {}
Borrows: {}
```

```text
AFTER
-----
Stack:                 Heap:
+-------------+        +----------------------+
| x: 0x1000   |------->| a0: 42               |
+-------------+        | [0x1000,0x1008)      |
                       +----------------------+

Allocs:  { a0 = (t0, [0x1000,0x1008), heap) }

Borrows/Owners set:
+----------------+------+------------------+---------+-------+
| time           | alloc| loc              | mode    | agent |
+----------------+------+------------------+---------+-------+
| [t0, t_free)   | a0   | [0x1000,0x1008)  | owner   | x     |
+----------------+------+------------------+---------+-------+
```

Now add a shared borrow:

```rust
let r = &x;
```

```text
Stack:                 Heap:
+-------------+        +----------------------+
| x: 0x1000   |------->| a0: 42               |
| r: 0x1000   |---+    | [0x1000,0x1008)      |
+-------------+   |    +----------------------+
                  |            ^
                  |            |
                  +------------+
                  shared borrow of a0

Borrows/Owners set:
+----------------+------+------------------+---------+-------+
| time           | alloc| loc              | mode    | agent |
+----------------+------+------------------+---------+-------+
| [t0, t_free)   | a0   | [0x1000,0x1008)  | owner   | x     |
| [t1, t2)       | a0   | [0x1000,0x1008)  | shared  | r     |
+----------------+------+------------------+---------+-------+
```

The `owner` tuple is special: it can hand out reborrows. Ordinary `shared` / `mutable` borrows must obey aliasing and lifetime rules.

## Same shape for OS page allocation

```text
OS creates a page mapping for [p, p+page)

BEFORE:
  Page table: [p] invalid
  Physical:   no frame
  VMA:        no mapping
  Memory:     inaccessible

AFTER:
  Page table: [p] -> frame F
  Physical:   frame F zeroed
  VMA:        mapping [p, p+page)
  Owner:      process address space
  Borrows:    {}
```

## Same shape for compiler stack slot

```text
Compiler creates stack slot for x

BEFORE:
  SP = S
  Frame: no slot for x

AFTER:
  SP = S - n
  Frame: [S-n, S) belongs to x
  Owner: current activation record
  Borrows: {}
```

The invariant is always:

```text
before:  a0 does not exist, no owner/borrow tuple refers to it
after:   a0 exists, owner tuple exists, memory is allocated,
         and the borrow set for a0 is empty except for the owner tuple
```