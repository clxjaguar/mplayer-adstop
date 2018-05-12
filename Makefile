COMPILER = gcc
CCFLAGS = -Wall

.PHONY: all clean rebuild mrproper test

all: mplayer-adstop
rebuild: clean

test:
	@echo "*** Testing it !"
	./mplayer-adstop -loop 0 -playlist http://listen.di.fm/public3/vocaltrance.pls -playlist http://listen.di.fm/public3/eurodance.pls	
	tail -n 3 *.log

rebuild: mrproper all

mrproper: clean
	@echo "*** Cleaning all binary executables files..."
	@rm mplayer-adstop

clean:
	@echo "*** Removing objects..."
	@rm *.o

mplayer-adstop: mplayer-adstop.o 
	@echo "*** Linking all main objects files..."
	@gcc mplayer-adstop.o -o mplayer-adstop
                        
mplayer-adstop.o: mplayer-adstop.c 
	@echo "*** Compiling mplayer-adstop.o"
	@${COMPILER} ${CCFLAGS} -c mplayer-adstop.c -o mplayer-adstop.o
                                        
