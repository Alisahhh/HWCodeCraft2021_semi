#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
cd $BASEDIR

cd CodeCraft-2021
sh ./pack.sh
cd ..
sh build.sh
cd bin
./CodeCraft-2021