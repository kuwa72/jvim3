{{CHANGES}}

---

JVim 3 {{VERSION}} — descended from JVim 3.0-j2.1b, with UTF-8 inside.

Two Windows builds, each with a GUI and a console executable, the help file and
a sample `_jvimrc`. Unpack one and run it: nothing needs setting, and `:help`
finds `vim.hlp` beside the exe.

| | |
| --- | --- |
| `jvim3-*-win32.zip` | `jvim32w.exe`, `jvim32.exe`. Runs on 64 bit Windows under WoW64. |
| `jvim3-*-win64.zip` | `jvim64w.exe`, `jvim64.exe`. Native 64 bit, for a machine you know is 64 bit. |

What it is and how to drive it: [README](https://github.com/kuwa72/jvim3#readme)
([日本語](https://github.com/kuwa72/jvim3/blob/master/README.ja.md)),
[USAGE](https://github.com/kuwa72/jvim3/blob/master/USAGE.md)
([日本語](https://github.com/kuwa72/jvim3/blob/master/USAGE.ja.md)).

Built by CI from this tag, after the build and {{TESTS}} tests passed on
{{PLATFORMS}}.

Build it yourself with `scripts/build-mingw.sh both` (`ARCH=x86_64` for 64 bit),
or `scripts/build-unix.sh test` on any of those.

Every change since {{PREV_TAG}}: {{COMPARE_URL}}
