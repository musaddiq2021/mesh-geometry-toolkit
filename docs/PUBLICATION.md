# Publication checklist

No publication has been performed. The working directory originally had no Git
repository or history. A disposable review database under the ignored build tree
was used only to inspect candidates; it is not the project's repository.

## Decisions before publishing

1. Review [LICENSE](../LICENSE): the maintainer selected MIT with
   Copyright (c) 2026 Musaddiq Ali.
2. Review README, SECURITY.md and AUDIT.md. Remote CI and Linux/MSVC builds have not
   executed yet. Enable private vulnerability reporting or establish a private contact
   after the repository exists.
3. Inspect the public file set and rerun the checks below. Agent files, local handover,
   build/tool distributions, manual probes and generated models must remain excluded.

## Verify and inspect locally

Use the tested README build/CTest commands for Debug and Release, or the optional
Windows verification script with installed tool paths. No global dependency install
is part of the build. Gitleaks and actionlint are separate audit tools; obtain them
from their official releases if they are not already installed.

The following commands are for the maintainer's local publication review:

```powershell
if (-not (Test-Path -LiteralPath LICENSE)) { throw 'The project LICENSE must be present.' }
git init -b main
git status --short --ignored
git ls-files --others --exclude-standard
git add --dry-run .
```

Review that preview. It should contain source, public headers, tests/fixtures,
examples, scripts, public docs, workflow, build configuration, ignore rules and license.
It must not include private agent notes, environment files, local tools or binaries.
Legitimate OBJ models in examples/ and tests/fixtures/ must remain visible.

```powershell
git add .github .gitignore CMakeLists.txt README.md SECURITY.md LICENSE build.ps1 include src tests examples scripts docs
git diff --cached --check
git diff --cached --stat
git diff --cached --name-only
git diff --cached
```

Inspect every staged diff before committing. To scan exactly the staged snapshot,
export it into a fresh ignored build directory and use an installed Gitleaks:

```powershell
git checkout-index --all --prefix=build/publication-export/
gitleaks dir build/publication-export --no-banner --redact=100
actionlint .github/workflows/ci.yml
```

Use a new export directory if an earlier one contains stale files. Do not force-add
ignored files to silence a discrepancy. A detected credential requires investigation
and usually rotation; removing a working-tree value does not remove history. Do not
rewrite history or rotate credentials without the owner's decision.

## Create the public repository only when approved

The review did not configure a remote or upload code. These commands are provided
for the maintainer's explicit publication step, after the local review is accepted:

```powershell
git commit -m "Prepare mesh geometry toolkit for public release"
gitleaks git . --no-banner --redact=100
gh auth login
gh repo create OWNER/mesh-geometry-toolkit --public --source=. --remote=origin --push
```

Replace OWNER with the intended account/organization and use the maintainer's real
Git identity. GitHub CLI must be installed/authenticated separately. Consult its
[repo create documentation](https://cli.github.com/manual/gh_repo_create) if the
repository already exists or another visibility is wanted. Review the first Actions
run before claiming Windows/MSVC or Linux CI success.

The workflow uses only checkout pinned to its verified
[v7.0.1 release](https://github.com/actions/checkout/releases/tag/v7.0.1), read-only
contents permission and no persisted credentials, secrets, release uploads or installer
scripts. It runs on pushes and pull requests, not privileged pull_request_target.
