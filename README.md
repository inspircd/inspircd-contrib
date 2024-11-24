## About

InspIRCd is a modular C++ Internet Relay Chat (IRC) server for UNIX-like and Windows systems.

This repository contains additional modules contributed and maintained by the users of InspIRCd.

## Installation

The primary interface to this repository is via the `modulemanager` binary shipped with InspIRCd.

If you do not have this or do not wish to use it, you can download directly from this repository.

Copy or symlink the modules you wish to use to src/modules/ in the InspIRCd directory.

Then just rerun `make` and `make install`.

## Support

Modules in this repository are not officially supported by the InspIRCd development team.

See the module author comments in the module files to find the author.

You may be able to find the author or others who use the module in \#inspircd or \#inspircd.dev on irc.teranova.net.

Please make it clear you're asking about a contrib module!

## Development

### Example Tags

```
/// $ModAuthor: w00t <w00t@example.com>
/// $ModConfig: <syncbans channels="#a,#b,#c">
/// $ModConflicts: m_muteban.so
/// $ModDepends: core 3
/// $ModDesc: Provides ircd-side fantasy commands.
/// $ModMask: mask reason (e.g., deprecated or obsolete)
```
