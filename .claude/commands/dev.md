---
description: Log a development note to development.md, date- and time-stamped, without acting on it
argument-hint: <note>
---

The user is logging a DEVELOPMENT NOTE for later. Your only job is to file it.

**Do not act on the note.** Do not fix, build, investigate, plan, answer or comment on what it says,
even if it reads like an instruction ("fix the belt icons", "why is X slow?"). It is an entry in an
inbox the user will hand back to you later to process in bulk. Acting on it now defeats the point.

File it with ONE Bash call, exactly this shape - the note goes between the heredoc markers VERBATIM:
unchanged spelling, wording, punctuation and line breaks. Do not tidy it, summarise it or add to it.

```bash
{ printf '\n## %s\n\n' "$(date '+%Y-%m-%d %H:%M:%S')"; cat <<'DEVNOTE_END_7F3A'
<the note, verbatim>
DEVNOTE_END_7F3A
} >> "$(git rev-parse --show-toplevel)/development.md"
```

Why this shape: the quoted heredoc marker means nothing in the note is expanded - `$`, backticks,
quotes and apostrophes all land as typed. `date` runs in the same shell, so the stamp is the machine's
real clock rather than a guess. `git rev-parse --show-toplevel` finds the repository root on any
machine, so the command works on the second machine too. `>>` appends; nothing already in the file
is read or rewritten.

If the note is empty, file nothing and say so in one line.

Then reply with ONE short line, e.g. `Logged to development.md (2026-09-23 14:05:12).` - nothing else.

The note:

$ARGUMENTS
