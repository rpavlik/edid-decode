# EDID Decode

> Fork maintained at <https://github.com/rpavlik/edid-decode>

A friendly fork of the upstream [XOrg edid-decode](https://cgit.freedesktop.org/xorg/app/edid-decode) tool,
with an emphasis on dealing with/providing useful info on displays (especially HMDs) seen in the wild,
and running nicely on Windows.

Intermittently rebased on the upstream source (with some commits reverted as required),
and incorporating patches from the wide "network" of this tool (Mandriva, other GitHub repos),
as well as my own patches.

Each time I update the upstream base, I create a new branch, to avoid force-pushes.

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

- Mandriva distribution patches: down to just a single one in the latest rebase since the makefile cleanup.
  Extract string patch originally from https://github.com/OpenMandrivaAssociation/edid-decode
  (specifically [this patch file](https://github.com/OpenMandrivaAssociation/edid-decode/blob/d05eb27d3bf2d046290724468f216bb6287ee449/edid-decode-extract-string.patch))
- Some commits cherry-picked from https://github.com/mike-bourgeous/edid-decode -
  while one in particular (bit rate is measured in bps, not Hz) gets harder to resolve conflicts with every time,
  it's a valuable correctness fix.
  I cherry-picked more than that, though;
  however, it's just a normal git cherry-pick so the original commit info is still intact. :)

## Commits reverted from upstream

**[57c73067](https://cgit.freedesktop.org/xorg/app/edid-decode/commit/?id=57c73067385e0bd29d0a67fd73db4ebecc8fb084)**

This was a change to attempt to improve terminator/padding behavior.
However, it did not apppear to follow the spec, or at least follow it in the way that extant EDIDs had.
An existing patch from Mandriva solved the issue better (defined as "parsing more EDIDs as expected" and
"not freaking out when there's a space in a string before the newline terminator"),
so I revert this commit every time I update the upstream base commit,
before rebasing the rest of the commits unique to this fork.