#!/bin/sh
# you can either set the environment variables AUTOCONF, AUTOHEADER, AUTOMAKE,
# ACLOCAL, AUTOPOINT and/or LIBTOOLIZE to the right versions, or leave them
# unset and get the defaults

autoreconf --verbose --force --install --make || {
 echo 'autogen.sh failed';
 exit 1;
}

./configure $@ || {
 echo 'configure failed';
 exit 1;
}

# install pre-commit hook for doing clean commits
if test ! \( -x .git/hooks/pre-commit -a -L .git/hooks/pre-commit \);
then
rm -f .git/hooks/pre-commit
ln -s ../../hooks/pre-commit.hook .git/hooks/pre-commit
fi

echo
echo "Now type 'make' to compile this module."
echo
