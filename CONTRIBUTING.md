# Code Formatting

We use [this script](/scripts/format.sh#L20) to install a consistent version of [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html) to format the code base. The format is automatically checked via a Travis CI build as well. Run the following script locally to ensure formatting is ready to merge:

```
make format
```

We also use [`clang-tidy`](https://clang.llvm.org/extra/clang-tidy/) as a C++ linter. Run the following command to lint and ensure your code is ready to merge:

```
make tidy
```

These commands are set from within [the Makefile](./Makefile).

## Releasing a new version

Releases are published to npm via GitHub Actions.

### Steps

1. **Bump the version** in `package.json` (follow [semver](https://semver.org))
2. **Update `CHANGELOG.md`** with a summary of what changed
3. **Open a PR**, get it reviewed and merged to `master`
4. **Trigger the release** from the [Actions tab](../../actions/workflows/npm-release.yml):
   - Select **NPM release** → **Run workflow** → run from `master`

The workflow will publish to npm and create a GitHub release with auto-generated notes.

> **Note:** Only Mapbox maintainers with write access to this repository can trigger the release workflow. External contributors can open and contribute to PRs, but releases are always cut by the owning team.
