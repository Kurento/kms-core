#!/bin/bash
# you can either set the environment variables AUTOCONF, AUTOHEADER, AUTOMAKE,
# ACLOCAL, AUTOPOINT and/or LIBTOOLIZE to the right versions, or leave them
# unset and get the defaults

SRC_DIR=`pwd`

if [ -e .git ]; then
  git submodule update --init
fi

autoreconf --verbose --force --install || {
 echo 'autogen.sh failed';
 exit 1;
}

if [[ -z "$BUILD_DIR" ]]; then
  BUILD_DIR=$SRC_DIR
fi

if [ ! -d $BUILD_DIR ]; then
 mkdir $BUILD_DIR
fi

cd $BUILD_DIR

$SRC_DIR/configure $@ || {
 echo 'configure failed';
 cd $SRC_DIR
 exit 1;
}

cd $SRC_DIR
# install pre-commit hook for doing clean commits
GIT_DIR=`git rev-parse --git-dir`
if test ! \( -x $GIT_DIR/hooks/pre-commit -a -L $GIT_DIR/hooks/pre-commit \);
then
rm -f $GIT_DIR/hooks/pre-commit
ln -s $SRC_DIR/hooks/pre-commit.hook $GIT_DIR/hooks/pre-commit
fi

if [ $SRC_DIR == $BUILD_DIR ]; then
 MAKE_STR="make"
else

 MAKE_STR="cd ${BUILD_DIR#$SRC_DIR}; make"
fi

echo
echo "Now type '$MAKE_STR' to compile this module."
echo
