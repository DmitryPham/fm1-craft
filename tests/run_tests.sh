#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# FM-1 CRAFT's tests: the app in fm1-base's host simulator (app/tests/basic.txt). Screenshots in app/build/host/.
set -u
cd "$(dirname "$0")/.."
sh build.sh --host || exit 1
app/build/host/craft_host app/tests/basic.txt app/build/host
