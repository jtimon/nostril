* Agent instructions

** Working style

- Keep changes focused and follow existing patterns. Fix causes rather than
  hiding errors, and read actual build or test failures before changing course.
- Preserve unrelated work. Commit only when asked; do not create branches,
  worktrees, or rewrite history without being asked.
- Do not invent relay, event, key-storage, or NIP APIs without documenting the
  ownership and compatibility decisions they introduce.

** Build and verification

- Build the library and both clients with =./make.til=. Run the library smoke
  test with =./make.til test= and checks relevant to the change.
- The project uses the installed =til= compiler and a Til build driver; GNU
  Make and a compiler bootstrap are not required.
- Prefer tests written in Til. Keep scratch files in =tmp/=; =bin/= and =gen/=
  are generated outputs and must not be edited by hand.
- =./make.til clean= removes =bin/=, =gen/=, and =tmp/=. It preserves reference
  repositories under the Git-ignored =.inspirations/= directory.
- Til development is at =~/code/jt/til/=. Distinguish that checkout's =bin/til=
  from the installed compiler used by this project.
