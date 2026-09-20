# Operational Workflow

## Purpose

This directory stores operational project state for the AI-assisted workflow.

## Source hierarchy

1. Actual Git/repository state
2. `docs/master/CURRENT.md`
3. `docs/master/DECISIONS.md`
4. `docs/master/TASKS.md`
5. `docs/master/WORKFLOW.md`
6. Project specs / research artifacts
7. External Dropbox artifacts
8. Chat history only as fallback

## State files

- `CURRENT.md` = compact current snapshot
- `DECISIONS.md` = durable architecture/technical decisions
- `TASKS.md` = active TODO / IN PROGRESS / BLOCKED / DONE

## Access rule

Do not assume Dropbox or another external artifact is accessible to the current agent without an explicit access check.

MASTER access != Codex access.

## Status model

Keep these distinct:

- IMPLEMENTED
- VERIFIED
- HARDWARE VERIFIED
- REVIEWED
- MERGED

If a check was not performed, do not promote the status.

## Context efficiency

- Do not repeat completed research without new evidence.
- Do not pass full chat history when state/artifacts are sufficient.
- Use minimum necessary context.
- Do not read the whole repo when scope is already known.

## Codex

Codex is the primary executor for repo/code/build/test/git work.

If Codebase Memory is available:

- Use it first for repo narrowing, architecture relationships, and impact analysis.
- Then inspect the relevant source files directly.

## Scope discipline

- No unrelated refactors.
- No new dependencies without necessity.
- Stop if a new architectural decision or scope expansion is required.
