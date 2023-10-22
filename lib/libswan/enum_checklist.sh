#!/bin/sh

# enum name checklist, for libreswan
#
# Copyright (C) 2023  Andrew Cagney
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

names=$1 ; shift

list()
{
    # the special comment /* #ifdef MACRO */, at the end of a declaration is
    # used to flag that the declaration should be wrapped in #ifdef
    # MACRO.
    sed -n \
	-e "s/^extern ${names} \([a-z0-9_]*\);.* #ifdef \([A-Z0-9_]*\).*$/\1 \2/p" \
	-e "s/^extern ${names} \([a-z0-9_]*\);.*$/\1/p" \
	-e "s/^extern const struct ${names} \([a-z0-9_]*\);.* #ifdef \([A-Z0-9_]*\).*$/\1 \2/p" \
	-e "s/^extern const struct ${names} \([a-z0-9_]*\);.*$/\1/p" \
	"$@"
}

echo $(list "$@") 1>&2


cat <<EOF
/* enum name checklist, for libreswan
 *
 * Copyright (C) 2023  Andrew Cagney
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
EOF

echo
echo '#include "enum_names.h"'
echo

list "$@" | while read name ifdef ; do
    test -z "${ifdef}" || echo "#ifdef ${ifdef}"
    echo "extern struct ${names} ${name};"
    test -z "${ifdef}" || echo "#endif"
done

echo

echo "const struct ${names} *${names}_checklist[] = {"
list "$@" | while read name ifdef ; do
    test -z "${ifdef}" || echo "#ifdef ${ifdef}"
    echo "  &${name},"
    test -z "${ifdef}" || echo "#endif"
done
echo "  NULL,"
echo "};"
