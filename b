#!/bin/bash

cd ./default
make || exit 1

echo Press ENTER to burn
read EMPTYLINE

avrdude -p m1284p -c avrispmkII -P usb -U flash:w:./os.hex:i || exit 1

