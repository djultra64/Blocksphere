# Options beta working instructions

This is the active checkout for the user's in-game Options beta work. Preserve
existing changes and do not switch, merge or reset worktrees automatically.
The repository root and `.worktrees/m1/AGENTS.md` provide the project, privacy,
controller, publication-identity and English-documentation requirements.

## Mandatory UI design supervision

Every game UI change requires a dedicated graphic-design supervisor subagent.
The user explicitly requests this delegation and authorizes the supervisor to
make visual decisions and redesign affected screens when necessary. Follow the
original game's 1990s visual language and exact low-resolution menu font.
Review hierarchy, spacing, alignment, focus, feedback and device-correct prompts.
Use the supervisor's decisions in `docs/qa/ui-design-supervision.md`.

Before delivering the build, have that supervisor review actual native captures
of relevant focus, navigation and edit states in 4:3 and widescreen. Resolve
material findings and review corrected captures again. Tests do not substitute
for visual acceptance. Record the outcome in English and keep captured game
assets, ROMs, saves and other personal evidence private.
