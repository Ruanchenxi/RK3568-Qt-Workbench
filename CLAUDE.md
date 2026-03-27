# CLAUDE.md

## Purpose

This file defines the working standard for Claude agents in the RK3568 project.
Its job is not to repeat all documentation, but to compress the most important
project truths, engineering constraints, and refactoring rules into one place.

Use this file together with:

- `RK3568/docs/PROJECT_CONTEXT.md`
- `RK3568/docs/operations/CONFIG_REFERENCE.md`
- `RK3568/docs/operations/STATUS_SNAPSHOT.md`
- `RK3568/docs/operations/BOARD_PRODUCT_UI_DEPLOYMENT_PLAN_2026-03-24.md`

If documentation and code disagree, prefer the latest verified product truth and
update the docs in the same task.

## Priority Hierarchy

When deciding how new code should be written, use this order:

1. Verified product truth from real behavior, logs, captures, and successful board validation
2. Current code behavior when it is already proven correct
3. Project docs under `RK3568/docs/`
4. This `CLAUDE.md`
5. Generic tool or plugin preferences

This means:

- New code should normally follow docs and `CLAUDE.md`
- But if real product truth proves a documented rule is stale, fix the rule instead of forcing code to match stale guidance
- “Looks cleaner” is never a reason to break verified behavior

## Project Identity

- Tech stack: Qt Widgets, C++, qmake, `.ui` files
- Main app source tree: `RK3568/`
- UI files stay in: `RK3568/ui/`
- Target platforms:
  - Windows local development
  - RK3588 Ubuntu board deployment
- Current product direction: reproduce original product behavior first, then
  improve maintainability without breaking user-facing behavior

## Primary Product Truths

### Original Product Baseline

- Original product frontend on board: `/home/linaro/AdapterService/bin/AdapterService`
- Original product backend recovery script: `/home/linaro/outage/start.sh`
- Original product backend work directory: `/home/linaro/outage`
- Original product service logs:
  - `/home/linaro/outage/target/kids/log/info-*.log`
  - `/home/linaro/outage/target/kids/log/error-*.log`

When reproducing Linux board behavior, prefer these fixed paths over generic or
clever auto-discovery. Auto-search is only a fallback for Windows local development.

### Board UI Reality

- Board desktop environment: XFCE
- Effective safe content height is approximately `1024x743`, not full `1024x768`
- Do not assume kiosk-exclusive fullscreen client area
- Critical controls must remain visible within the reduced safe area

### Card Login Truth

- Card reader device path on board: `/dev/ttyS3`
- Card reader serial parameters currently align with observed product behavior:
  - `9600 8-N-1`
- Card reader frames are continuous byte-stream frames, not one-read-per-message
- Observed frame shape:
  - 8 bytes
  - frame head `AA`
  - example: `AA 31 08 00 49 A7 E0 06`
- Original product actually posts:
  - `cardNo = 080049A7E006`
  - `tenantId = 000000`
- Original product card login endpoint:
  - `POST /api/kids-outage/third-api/login-by-card`

Treat card login as original-product compatibility work, not a fresh redesign.

## Non-Negotiable Engineering Rules

1. Preserve behavior unless explicitly asked to change product behavior.
2. Do not casually change protocol constants, serial semantics, framing rules,
   or backend request contracts.
3. Keep `.ui` files in `RK3568/ui/`; do not migrate UI structure to a different system.
4. Prefer small, local changes over broad rewrites.
5. Do not apply React, TypeScript, or frontend-web style conventions to this Qt/C++ project.
6. Platform branches must remain explicit:
   - `Q_OS_WIN`
   - `Q_OS_LINUX`
7. If a board sync/build step fails remotely, assume the board is still running
   the old binary until proven otherwise.

## Coding Standards

### General Style

- Choose clarity over terseness.
- Prefer straightforward control flow over clever compact code.
- Avoid nested ternary expressions.
- Prefer `if/else` or `switch` when multiple conditions are involved.
- Extract small helper functions when a block becomes hard to scan.
- Remove redundant code, duplicate branching, and obvious comments.
- Do not remove useful abstractions that help debugging or maintenance.

### Qt / C++ Style

- Use `QStringLiteral` for fixed string literals where appropriate.
- Keep signal/slot wiring easy to scan.
- Keep page/controller responsibilities clean:
  - UI logic in pages/controllers
  - low-level IO and process logic outside UI pages
- Keep Windows and Linux behavior readable and separate when necessary.
- Do not rewrite code merely to reduce line count.
- Add comments only when they are genuinely necessary to explain intent,
  protocol assumptions, platform quirks, or non-obvious constraints.
- When necessary comments are added, prefer concise Chinese comments.
- Do not add obvious comments such as “assign value to variable” or comments
  that merely restate the code line-by-line.

### Refactoring Scope

- By default, only refine code touched in the current task.
- Do not reformat or restructure unrelated files just because they could be cleaner.
- When simplifying, preserve exact behavior first, elegance second.

## UI Rules

1. Industrial touch usability matters more than decorative complexity.
2. Buttons and inputs must be usable on the RK3588 touch screen.
3. If a control is important, it must be visible both:
   - statically in Designer
   - reliably at runtime on board
4. Prefer simpler, clearer layouts over heavy card nesting.
5. Match original product interaction patterns when reproducing existing pages.

## Service and Log Rules

1. On Linux board, prefer original-product-compatible service behavior:
   - fixed script path
   - fixed work dir
   - real service-side logs first
2. Do not treat frontend self-logs as backend service logs unless explicitly intended.
3. For board service log pages, prioritize:
   - `service_startup.log` when it is the intentional startup trace
   - `/home/linaro/outage/target/kids/log/*.log` for real backend logs
4. If reproducing original product startup behavior, align with `start.sh` first
   before adding extra governance logic.

## Auth and Identity Rules

1. Password login behavior is the baseline.
2. Card login should behave the same as password login after authentication succeeds:
   - same login state transition
   - same top-bar user update
   - same footer user update
   - same page accessibility rules
3. Account lifecycle and credential binding are different concerns:
   - account add/delete belongs to backend/admin management
   - card/fingerprint binding on device should follow verified original-product permissions, not a guessed hardcoded admin-only rule
4. Do not expose account add/delete to ordinary roles.

## Simplifier-Specific Instructions

When a code simplification agent is active:

1. Preserve exact functionality.
2. Follow this `CLAUDE.md`, not generic frontend framework conventions.
3. Focus on recently modified code unless explicitly asked to widen scope.
4. Prefer readable, explicit code over dense or clever rewrites.
5. Document only meaningful structural changes.

## Subagent Guidance

If subagents are used for this project:

1. Keep the number small. Prefer one main implementer plus at most:
   - one explorer for truth/codepath verification
   - one worker for bounded implementation or cleanup
2. Do not split overlapping write scopes across multiple workers.
3. Use subagents to accelerate understanding or bounded patches, not to create parallel churn in `.ui` files or tightly coupled auth/process logic.

## Recommended Workflow

1. Read the task and identify the narrowest safe write scope.
2. Check project truth from docs if behavior is product-sensitive.
3. Modify code in small steps.
4. Verify build/runtime impact where possible.
5. If board deployment is involved, remember:
   - local success does not imply remote success
   - remote build failure means old board binary is still in use
6. If a change affects project truth, update docs in the same task.

## What Not To Do

- Do not port the project toward web-stack patterns just because a generic tool suggests it.
- Do not rewrite working protocol code without a verified product reason.
- Do not broaden a task into unrelated cleanup.
- Do not assume board behavior matches local Windows behavior.
- Do not hide runtime-only fixes from Designer when static visibility matters for maintenance.
