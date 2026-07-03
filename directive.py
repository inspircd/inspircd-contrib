#!/usr/bin/env python3
#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2026 Sadie Powell <sadie@sadiepowell.dev>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import pathlib
import re
import sys

if len(sys.argv) < 3:
    program = pathlib.Path(__file__).name
    print(f"Usage: {program} <file> <directive>", file=sys.stderr)
    sys.exit(1)

try:
    with open(sys.argv[1], encoding="utf-8") as file:
        matches = []
        for line in file:
            if match := re.match(r"^\/\/\/ \$(\S+): (.+)", line):
                if match.group(1) == sys.argv[2]:
                    matches.append(match.group(2))
        print(" ".join(matches))

except OSError as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(1)
