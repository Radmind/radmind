# Homebrew Tap Releases

Radmind is published to a third-party Homebrew tap when a GitHub release is
published. The workflow creates a release source archive that includes the
`libsnet` submodule, uploads it to the release, then writes and tests
`Formula/radmind.rb` in the tap.

## One-time setup

1. Create a fine-grained GitHub personal access token that has **Contents:
   Read and write** permission for the tap repository. Save it in this
   repository as the Actions secret `HOMEBREW_TAP_TOKEN`.

The token is necessary because the workflow writes to a different repository;
the release repository's default `GITHUB_TOKEN` cannot do that.

## Releasing

Set the `VERSION` file to the intended formula version and publish a GitHub
release from the corresponding tag. The **Publish Homebrew formula** workflow
will upload `radmind-<version>.tar.gz`, update the tap, run `brew audit` and a
source build/test, and commit the formula. Users can then install it with:

```sh
brew tap radmind/tap
brew install radmind
```

The formula installs the server's runtime data under Homebrew's
`var/radmind`, not `/var/radmind`.
