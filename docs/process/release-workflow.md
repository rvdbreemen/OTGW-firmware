Release workflow

- prepare `dev` branch:
  - checkout 'last' commit of everything that is part of new release. This can be any commit on `dev` that's ahead of `main`
  - ensure there are no pending changes, stash or discard any local changes
  - test build code, ensure there are no compiler errors
  - merge `main` into dev
    NOTE: normally this merge will not produce any result as `main` normally is only updated by merging `dev` into `main`. 
    However, this might happen sometimes (quick security fix, etc). This step is catch those times and ensure there are no conflicts when merging into `main`. Merge conflicts should be solved on `dev` branch, not on `main`.
- merge `dev` into `main`:
  - checkout main
  - merge `dev` 
  - there should be no conflicts on core files. If there are, STOP. The last step of prepping the `dev` branch was probaby skipped. Reset your local `main` branch to the last commit of the GH/`main` branch. Go back and complete prepping `dev` branch.
  - test build code, ensure there are no compiler errors
- create release commit:
  - update the readme or release docs. Since you can't have empty commits I have a small workaround to allow you to have a nice clean release commit:
    - I add a .x to the version number in the release file when working on it in dev. So the release file in dev wil ALWAYS have .x added to it. Only on in the release commit on the main branch i remove the .x. that's the content of the final release commit. but there are many roads that lead to Rome. Just pick one.
  - commit change, commit message should be standerdized like: "Release x.x.x" --> Release 0.8.0
  - add tag `vx.x.x` to this last commit
  - NOTE: the `main` branch should now have 2 new commits: 1) the merge `dev` into `main` commit. 2) the 'actual' release commit.
- build release:
  - clean your entire workspace and build both BIN files with tag `vx.x.x` checked out.
    NOTE: ensure the final build version EXACTLY matches whatever info you have in the version.h file. So if your bat-auto-update file increments the build nr each time, it should increment AFTER a successful build. Not before. Otherwise your final git commit will have build no. 123 and you actual builds will have nr 124.
  - flash bins to nodeMCU, test for a few minutes
- publish release:
  - push `main` AND tag `vx.x.x` to GH. Note that tags are not pushed by default!
  - upload the bin files to GH
- update version info on dev:
  - checkout `dev`
  - update version.h and increment the minor release with +1
  - commit change

Automated release workflow

 - The `.github/workflows/release.yml` workflow runs whenever you publish (not draft) a release on GitHub.
 - It verifies the release tag exists in the repository before doing any work; if the tag is missing, the workflow stops.
 - When the tag exists, it checks out the released tag, runs the shared setup and build scripts, and then pushes the compiled `build/*.elf`, `build/*.bin`, `build/*.littlefs.bin`, and `LICENSE` files into the GitHub Release assets via `softprops/action-gh-release`, inheriting the release metadata (name/body/prerelease) and preventing duplicate uploads with `allow_updates: true`.
