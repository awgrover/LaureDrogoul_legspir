ino=$(shell /bin/ls -1 *.ino | head -n 1)

include *.mk

.DUMY : loadsd
loadsd : mp3-trigger
	cp $</* /media/awgrover/LEGSOUNDS
