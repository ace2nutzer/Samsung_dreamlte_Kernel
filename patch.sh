#!/bin/bash

if [ -f rejects ] ; then
	rm rejects
fi

patch -p 1 $@ -i 0.patch -r rejects

if [ -f rejects ] ; then
	echo "#####################"
	echo "###### REJECTS ######"
	echo "#####################"
fi
