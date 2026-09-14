# Kryos AI Assistant Guidelines (AGENTS.md)

**CRITICAL RULE:** The current phase is authoritative. Do not implement future-phase functionality, even if it appears simple, useful, or architecturally convenient. Future ideas must be documented and deferred.

## Commit Strategy
- **Commits should represent meaningful engineering progress.**
- **Commit messages must use the imperative mood and be scoped.** Examples:
  - `arch/x86_64: add Multiboot2 bootstrap`
  - `kernel: add minimal entry point`
  - `docs: add initial ADRs`
- **Small, coherent, reviewable commits—not artificially small commits.** Group tightly coupled files together if it tells the clearest story, but never bulk unrelated changes.

## Milestone Releases
- We tag releases at the end of every major milestone (e.g., `v0.0.1` at the end of Phase 0).

## Architecture Rules
- **Ship milestones:** Never allow the project to become an endless unfinished architecture.
- **Avoid premature abstraction:** Design the smallest useful implementation, test it, and only then refactor.
- **Preserve learning value:** Do not outsource core OS concepts to external libraries.
- **Scope Control:** Do not start on Phase N+1 until Phase N is strictly complete and verified.

## Quality Standards
- **Principal staff-level engineering quality.**
- **Document everything:** Issues, fixes, design choices, invariants, hardware mechanisms, and expected behavior.
- **Testing:** Tests must be derived from invariants and failure conditions, not just reproducing the implementation.
- **Debugging:** When things fail: Reproduce -> Explain failure layer -> Form hypothesis -> Test -> Fix -> Add regression test. Do not randomly rewrite working code.
