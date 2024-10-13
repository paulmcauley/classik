#!bin/sh
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >> rc.cpp
$XGETTEXT `find . -name \*.cc -o -name \*.cpp -o -name \*.h` -o $podir/klassy_style_config.pot
rm -f rc.cpp
