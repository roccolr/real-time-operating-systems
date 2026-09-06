
# GDB Vademecum

A practical, self-contained reference for the GNU Debugger (GDB). Keep it in your
repo and reach for it whenever you need to inspect a running C program instead of
sprinkling `printf` everywhere.

---

## 1. Compile for debugging

GDB works on *debug symbols* (variable names, line numbers, types). Tell the
compiler to emit them with `-g`, and disable optimizations with `-O0` so the
compiler does not reorder or elide instructions.

```sh
gcc -std=c11 -Wall -Wextra -O0 -g example.c -o example
```

| Flag  | Purpose                                                           |
|-------|------------------------------------------------------------------|
| `-g`  | Emit debug symbols (names, line numbers, types).                  |
| `-O0` | Disable optimizations so stepping matches the source line by line.|

Without `-g` GDB still runs, but you only see addresses and assembly, not names.

---

## 2. Start GDB

```sh
gdb ./example          # start the interactive prompt
gdb -tui ./example     # start with the split-screen TUI (source + prompt)
```

The program does **not** run yet — GDB waits at the `(gdb)` prompt for your
commands. Almost every command has a short alias, shown in parentheses below.

---

## 3. Command reference

### Breakpoints — stop at a location

| Command                        | Alias | Effect                                             |
|--------------------------------|-------|----------------------------------------------------|
| `break main`                   | `b`   | Stop when entering `main`.                          |
| `break file.c:15`              | `b`   | Stop at line 15 of `file.c`.                        |
| `break func`                   | `b`   | Stop every time `func` is entered.                  |
| `break file.c:20 if i == 100`  | `b`   | Conditional: stop only when the condition is true.  |
| `info breakpoints`             |       | List breakpoints with their id numbers.             |
| `disable N` / `enable N`       |       | Turn breakpoint `N` off / on.                       |
| `delete N`                     | `d`   | Remove breakpoint `N` (no `N` = remove all).        |

### Running and stepping

| Command    | Alias | Effect                                                              |
|------------|-------|--------------------------------------------------------------------|
| `run`      | `r`   | Start the program (pass args: `run arg1 arg2`).                     |
| `next`     | `n`   | Execute current line, **stepping over** function calls.            |
| `step`     | `s`   | Execute current line, **stepping into** function calls.            |
| `continue` | `c`   | Resume until the next breakpoint or program end.                    |
| `finish`   |       | Run until the current function returns, then stop at the caller.    |
| `until`    | `u`   | Run until a line past the current one (handy to exit a loop).       |

> **Rule of thumb:** `next` to scan the logic of a function; `step` when you
> suspect the bug is *inside* a called function; `finish` to climb back out.

### Inspecting variables

| Command             | Alias | Effect                                              |
|---------------------|-------|-----------------------------------------------------|
| `print expr`        | `p`   | Evaluate any C expression in the current context.   |
| `print var.member`  | `p`   | Works on struct/union members and bitfields.        |
| `info locals`       |       | All local variables of the current function.        |
| `info args`         |       | The current function's arguments.                   |
| `whatis var`        |       | The type of `var`.                                  |
| `ptype Type`        |       | Full definition of a type (great for struct/union). |
| `display expr`      |       | Auto-print `expr` after every step.                 |

### Print format modifiers (`print/F`)

| Modifier      | Meaning       | Example                 |
|---------------|---------------|-------------------------|
| `print/x`     | Hexadecimal   | `print/x reg.raw`       |
| `print/t`     | Binary (base 2)| `print/t reg.raw`      |
| `print/d`     | Decimal       | `print/d reg.raw`       |
| `print/o`     | Octal         | `print/o mask`          |
| `print/c`     | Character     | `print/c 65`            |
| `print/a`     | Address       | `print/a ptr`           |

### Examine raw memory — `x/NFU address`

`N` = number of units, `F` = format, `U` = unit size.

```
(gdb) x/4xb &reg        # 4 units, heX format, Byte size
```

| Unit sizes            | Formats                                             |
|-----------------------|----------------------------------------------------|
| `b` byte (1)          | `x` hex   `d` decimal   `t` binary                 |
| `h` halfword (2)      | `c` char  `o` octal     `u` unsigned              |
| `w` word (4)          | `a` address `f` float   `s` string                |
| `g` giant (8)         | `i` instruction (disassembly)                     |

Endianness check: on `u.value = 0x01020304`, `x/4xb &u.value` shows
`0x04 0x03 0x02 0x01` → little-endian.

### Watchpoints — stop when data changes

| Command          | Effect                                              |
|------------------|-----------------------------------------------------|
| `watch expr`     | Stop when `expr` is written (shows old → new value).|
| `rwatch expr`    | Stop when `expr` is read.                            |
| `awatch expr`    | Stop on read or write.                               |

Watchpoints on locals stop working once you leave that variable's scope.

### The call stack

| Command       | Alias | Effect                                             |
|---------------|-------|----------------------------------------------------|
| `backtrace`   | `bt`  | Show the chain of calls up to `main`.              |
| `frame N`     | `f`   | Switch to stack frame `N`.                          |
| `up` / `down` |       | Move one frame toward the caller / callee.          |
| `info frame`  |       | Details about the current frame.                    |

### Session control

| Command    | Alias | Effect                          |
|------------|-------|---------------------------------|
| `list`     | `l`   | Show source around current line.|
| `Ctrl-x a` |       | Toggle the TUI split view.      |
| `quit`     | `q`   | Exit GDB.                        |

---

## 4. Debugging a crash (segfault)

No breakpoints needed. Just:

```
(gdb) run
```

When the program segfaults, GDB stops on the offending line automatically. Then:

```
(gdb) backtrace         # how did we get here?
(gdb) print ptr         # often reveals a NULL pointer or bad index
(gdb) info locals
```

---

## 5. Worked session — union + bitfield register

Given a control register defined as a `union` of a raw byte and a bitfield
struct, compiled with `-O0 -g`:

```
(gdb) break main
(gdb) run
(gdb) next                    # reg.raw = 0;
(gdb) next                    # reg.bits.enable = 1;
(gdb) print/t reg.raw         # bit pattern so far
(gdb) next                    # reg.bits.mode = 2;
(gdb) next                    # reg.bits.priority = 5;
(gdb) print/t reg.raw         # -> 101101
(gdb) print/x reg.raw         # -> 0x2D
(gdb) x/1xb &reg              # same byte from raw memory
(gdb) print reg.bits.priority # -> 5
(gdb) continue
(gdb) quit
```

> **Key gotcha:** GDB stops *before* executing the highlighted line. The
> highlighted line is the **next** one to run, not the last one executed. When
> `reg.bits.mode = 2;` is highlighted, the value is not written yet — it will be
> after the next `next`.

---

## 6. Quick tips

- `-O0 -g` is the standard build combo while debugging.
- `run` alone reproduces a crash and lands you on the guilty line for free.
- `print/t` is the fastest way to eyeball a bitfield's bit layout.
- `ptype SomeType` reprints a struct/union definition without leaving GDB.
- `.gdbinit` in your home dir auto-loads settings at startup — skip it until the
  core commands are second nature, then automate.
