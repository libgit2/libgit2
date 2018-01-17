# Releasing the library

We have three kinds of releases: "full" releases, maintenance releases and security releases. Full ones release the state of the `master` branch whereas maintenance releases provide bugfixes building on top of the currently released series. Security releases are also for the current series but only contain security fixes on top of the previous release.

## Full release

We aim to release once every six months. We start the process by opening an issue. This is accompanied with a feature freeze. From now until the release, only bug fixes are to be merged. Use the following as a base for the issue

    Release v0.X
    
    Let's release v0.X, codenamed: <something witty>
    
	- [ ] Bump the versions in the headers
    - [ ] Make a release candidate
    - [ ] Plug any final leaks
    - [ ] Fix any last-minute issues
    - [ ] Make sure CHANGELOG reflects everything worth discussing
    - [ ] Update the version in CHANGELOG and the header
    - [ ] Produce a release candidate
    - [ ] Tag
    - [ ] Create maint/v0.X
    - [ ] Update any bindings the core team works with

We tag at least one release candidate. This RC must carry the new version in the headers. If there are no significant issues found, we can go straight to the release after a single RC. This is up to the discretion of the release manager.

The tagging happens via GitHub's "releases" tab which lets us attach release notes to a particular tag. In the description we include the changes in `CHANGELOG.md` between the last full release. Use the following as a base for the release notes

    This is the first release of the v0.X series, <codename>. The changelog follows.

followed by the three sections in the changelog. For release candidates we can avoid copying the full changelog and only include any new entries.

During the freeze, and certainly after the first release candidate, any bindings the core team work with should be updated in order to discover any issues that might come up with the multitude of approaches to memory management, embedding or linking.

Create a branch `maint/v0.X` at the current state of `master` after you've created the tag. This will be used for maintenance releases and lets our dependents track the latest state of the series.

## Maintenance release

Every once in a while, when we feel we've accumulated backportable fixes in the mainline branch, we produce a maintenance release in order to provide updates for those who track the releases and so our users and integrators don't have to upgrade immediately.

As a rule of thumb, it's a good idea to produce a maintenance release when we're getting ready for a full release. This gives a last round of fixes without having to upgrade (which with us potentially means adjusting to API changes).

Start by opening an issue. Use the following as a base.

    Release v0.X.Y
    
    Enough fixes have accumulated, let's release v0.X.Y
    
    - [ ] Select the changes we want to backport
    - [ ] Update maint/v0.X
    - [ ] Tag

The list of changes to backport does not need to be comprehensive and we might not backport something if the code in mainline has diverged significantly.

Do not merge into the `maint/v0.X` until we are getting ready to produce a new release. There is always the possibility that we will need to produce a security release and those must only include the relevant security fixes and not arbitrary fixes we were planning on releasing at some point.

Here we do not use release candidates as the changes are supposed to be small and proven.

## Security releases

This is the same as a maintenance release, except that the fix itself will most likely be developed in a private repository and will only be visible to a select group of people until the release.

Everything else remains the same. Occasionally we might opt to backport a security fix to the previous series, based on how recently we started the new series and how serious the issue is.
