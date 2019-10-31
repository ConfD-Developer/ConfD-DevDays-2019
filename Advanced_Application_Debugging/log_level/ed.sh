#!/bin/sh

echo 'Updating developerLogLevel to '$1' and reload logs\n'

cur=`grep -o '<developerLogLevel>[^<]*</developerLogLevel>' confd.conf | cut -d ">" -f 2 | cut -d "<" -f 1
`
sed -i.orig 's/<developerLogLevel>'$cur'/<developerLogLevel>'$1'/g' confd.conf
confd --reload


