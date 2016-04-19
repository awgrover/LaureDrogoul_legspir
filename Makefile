ino=$(shell /bin/ls -1 *.ino | head -n 1)

include *.mk

.DUMY : loadsd
loadsd : mp3-trigger
	cp $</* microsd

README.html : README.md
	@# if you have a markdown command
	markdown README.md > README.html
