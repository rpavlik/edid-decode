# EDID Decode

> Fork maintained at <https://github.com/rpavlik/edid-decode>

A friendly fork of the upstream [edid-decode](https://git.linuxtv.org/edid-decode.git/) tool
(formerly maintained on freedesktop.org with XOrg),
with an emphasis on dealing with/providing useful info on displays (especially HMDs) seen in the wild,
and running nicely on Windows.

Intermittently rebased on the upstream source (with some commits reverted as required),
and incorporating patches from the wide "network" of this tool (Mandriva, other GitHub repos),
as well as my own patches.

Each time I update the upstream base, I create a new branch, to avoid force-pushes.

Note that the upstream repo is much more active now than it was when I first
created this fork (at which point it was mostly unmaintained), so the changes
here are likely to get out of date quickly.

## Changes

For a complete list of changes, please see the commit log:
each commit contains as complete of information as possible on their source.
Commits not otherwise attributed were written by myself (Ryan Pavlik).
The `master` branch is left synced with whatever the upstream `master` branch was
at the last time I updated this repo.

My own changes:

- Open EDID files in binary mode, for those of use using the tool on Windows.
  It does build just fine with MSYS2/MinGW-w64.
- Add some scripts for building and for using on Windows.
  - That includes this readme file and some utilities for formatting it,
    including a bundled copy of `markdeep.min.js` from <https://casual-effects.com/markdeep/>,
    which is subject to its own license, found in the `misc/` folder in the source or bundled with a binary.

Patch sources besides myself:

- Some commits cherry-picked from https://github.com/mike-bourgeous/edid-decode ;
  just a normal git cherry-pick so the original commit info is still intact. :)
