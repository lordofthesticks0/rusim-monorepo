# LLM Agent Rules and Guidelines

## Description

This project is a monorepo for an in-vitro rumen simulation. Remote is available on `https://github.com/lordofthesticks0/rusim-monorepo`

Context is scarce. Do not read files or documentation unless they are relevant to the subsystem you are working on. Avoid exploring unrelated parts of the repository.

Your users are undergraduate students who are learning while working on this project. Explain what you did, why you did it, and how it works. Be educational and sufficiently detailed without unnecessary wording.

## Prerequisites

The following tools must be available in your terminal:

- `bun`: https://bun.com/docs/installation
- `pio`: https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html
- `uv`: https://docs.astral.sh/uv/getting-started/installation

Check their availability before starting work. Install missing prerequisites using their official instructions.

## Rules

The following rules are mandatory for all agents.

### General
- NEVER modify this `AGENTS.md` file unless the user explicitly authorizes it. Report discrepancies to the user instead.
- ALWAYS document subsystems.
- When a decision needs to be made, ask the user first. Present the available options and their pros and cons before asking them to choose.
- Before starting work, check the Git working tree and ensure you are working with the latest remote code. Do not overwrite local changes or resolve divergent branches without consulting the user.
- NEVER commit anything unless the user explicitly authorizes it.
- Never add AI attribution to commit messages.
- Never read or modify any `.env` file. `.env.example` is allowed.
- The `reusable-prompts/` directory is strictly off-limits. Only access its contents when the user explicitly instructs you to.

### Firmware
- Scope firmware-related work to `firmware/`. 
- Read `docs/hardware.md` before making firmware changes. This documentation relies on human testing and input. Data may be stale or incorrect.
- Put one-off or temporary tests in `firmware/tests/`. 
- Adjust the relevant PlatformIO environment configuration when necessary to accommodate firmware changes.

### Dashboard
- Scope dashboard-related work to `dashboard/`.
- Use Bun for package management, scripts, development, and testing. Do not substitute npm, yarn, or pnpm.
- Consult the relevant database documentation in `docs/` before making database-related changes.
- Never read or modify `.env` files. Use `.env.example` when configuration details are needed.

### Legacy Work
- Scope legacy-related work to `legacy/`.
- The legacy system runs on a Raspberry Pi 3B.
- The device is reachable at `invitro@10.92.128.23`. Password authentication is disabled; SSH key authentication is configured.
- The system consists of a self-contained HTML dashboard displayed through Chromium in kiosk mode and a Python Flask server that processes the data.
- Before making changes, retrieve the existing files and data from the Raspberry Pi into this repository. Make and verify changes locally before deploying them.
- Ask the user for explicit approval before pushing changes to the Raspberry Pi.

### Documentation

#### General Requirements

- Every tool, subsystem, calibration methods, frontend, backend, and other components must be documented.
- For tests, do NOT document them. 
- Write all documentation in `docs/`.
- ALWAYS update `docs/index.md` when adding new documentation files.
- Do not write documentation elsewhere.
- Read documentation only when relevant to the subsystem being worked on.
- Do not read unrelated documentation merely to learn its formatting. Keep documentation factual, descriptive, and educational.
- Use minimal wording without unnecessary metaphors, embellishments, or vague expressions. Keep it simple for the user.

#### Documentation Format

Every generated document must end with the following footer:

```
---
Updated at YYYY-MM-DD-HH:MM.
```

When updating an existing generated document, update its contents and footer accordingly. Do not ask the user for clarification merely to revise documentation.

#### Graphify

Graphify is a local CLI tool that builds and queries a knowledge graph of the codebase. It is not an MCP dependency. Use its CLI directly to understand architecture and dependencies without repeatedly reading unrelated source files.

**Installation**

The PyPI package is named `graphifyy` (two `y`s), while its executable is named `graphify`.

If Graphify is not installed, install it with:

```bash
uv tool install graphifyy
```

Verify its availability:

```bash
graphify --help
```

**Usage**

1. Build or refresh the codebase graph:

   ```bash
   graphify update .
   ```

2. Ask architectural questions and explore relevant relationships:

   ```bash
   graphify query "How does the data flow through the backend?"
   ```

3. Inspect a specific component and its connections:

   ```bash
   graphify explain "ComponentName"
   ```

4. Trace the shortest dependency path between two components:

   ```bash
   graphify path "ComponentA" "ComponentB"
   ```

5. The default graph is stored in `graphify-out/graph.json`. CLI queries read from this graph.

**Rules for Using Graphify**

- Query Graphify before performing broad source-code searches.
- Use `query` to understand architecture and locate relevant components.
- Use `explain` to understand a specific component and its immediate relationships.
- Use `path` to trace dependencies between components.
- Use the graph's file and line references to locate the relevant source code.
- Verify important findings against the actual source code. Graph relationships may be inferred or ambiguous.
- Refresh the graph after significant code changes.
- Before committing, run `graphify update .` to ensure the graph reflects the latest code.

### Context Management

- Do not explore unrelated files, it will bloat your context.
- Prefer Graphify queries over broad file searches when exploring code relationships.
- Read source files only after establishing which files are relevant.
- Do not open entire documentation directories. Read only the documents necessary for the current task.
- When running raw commands, a lot of the text will be mostly noise. Greatly utilize `tail` or `grep` to filter out unnecessary output.
