/*
	Project:   mplayer-adstop
	           A little wrapper for mplayer to stop these crappy french ads in di.fm's radio
	           This was also really interesting, especially to learn pipes and linux related stuff

	Usage:     mplayer-adstop -loop 0 -playlist http://listen.di.fm/public3/vocaltrance.pls
	Author:    cLx - http://clx.freeshell.org/
	Copyright: CC-BY-NC
	Date:      January 2013 (beginning, then the project was paused when ads suddently stopped)
	           October 2013 (continuation because these ads are back)
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <string.h>

#define MPLAYER       "/usr/bin/mplayer"
#define LOGFILEOK     "./mplayer-adstop.log"
#define LOGFILEFAIL   "./mplayer-adstop-ads.log"
#define ICY_SIGNATURE "\nICY Info: "

char is_an_ad(const char *trackdata){
	if (strstr(trackdata, "insertionType='preroll'")){   return 'm'; }
	if (strstr(trackdata, "insertionType='midroll'")){   return '>'; }
	if (strstr(trackdata, "metadata='adswizzContext=")){ return '>'; }
	return 0;
}

int exit_requested = 0;
int pid=0;
static void sigkilled(int sig){
	switch(sig){
		case SIGINT:
			printf("[Got SIGINT (^C), resending signal to child...]\n");
			kill(pid, SIGINT);
			if (!exit_requested){
				exit_requested = 1;
				return;
			}
			break;

		case SIGTERM:
			printf("[Got SIGTERM, exiting...]\n");
			kill(pid, SIGINT);
			exit_requested = 1;
			return;
			break;

		case SIGSEGV:
			printf("[Got SIGSEGV, dying...]\n");
			kill(pid, SIGINT);
			exit_requested = 1;
			break;

		case SIGABRT:
			printf("[Got SIGABRT, dying...]\n");
			kill(pid, SIGINT);
			exit_requested = 1;
			break;

		case SIGQUIT:
			printf("[Got SIGQUIT, dying...]\n");
			kill(pid, SIGINT);
			exit_requested = 1;
			break;

		default:
			break;
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

int install_sighandlers(void){
	signal(SIGTERM, sigkilled);
	signal(SIGINT, sigkilled);
	signal(SIGSEGV, sigkilled); // you may need to comment this one in case of debugging
	signal(SIGQUIT, sigkilled);
	signal(SIGABRT, sigkilled);
	return 0;
}

void writelog(char *logfile, char *msg1, char *msg2) {
	FILE *fd;
	struct tm *ptm;
	time_t lt;

	lt = time(NULL);
	ptm = localtime(&lt);

	fd = fopen(logfile, "a");
	if (!fd){ // log not ok
		perror(logfile);
	}
	else {
		fprintf(fd, "[%04u-%02u-%02u %02u:%02u:%02u] ", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		if (msg1)   { fprintf(fd, "%s", msg1); }
		if (msg2)   { fprintf(fd, " %s", msg2); }
		fprintf(fd, "\n");
		fclose(fd);
	}
}

int main(int argc, char **argv){
	int in[2], out[2], err[2];

	// in a pipe, x[0] is for reading, x[1] is for writing
	if (pipe(in)  < 0) { perror("pipe in"); }
	if (pipe(out) < 0) { perror("pipe out"); }
	if (pipe(err) < 0) { perror("pipe err"); }

	pid=fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	}
	else if (pid == 0) { // ok, this is the child process
		// close stdin, stdout, stderr
		close(0);
		close(1);
		close(2);

		// make our pipes, our new stdin, stdout and stderr
		dup2(in[0],  0);
		dup2(out[1], 1);
		dup2(err[1], 2);

		// close the other ends of the pipes that the parent will use, because if
		// we leave these open in the child, the child/parent will not get an EOF
		// when the parent/child closes their end of the pipe.
		close(in[1]);
		close(out[0]);
		close(err[0]);

		// over-write the child process with the mplayer binary
		execv(MPLAYER, argv);
		perror(MPLAYER);
		return 1;
	}

	// OK, so this is the parent process
	printf("[Spawned mplayer as a child process at pid %d]\n", pid);
	install_sighandlers();

	// Close the pipe ends that the child uses to read from / write to so
	// the when we close the others, an EOF will be transmitted properly.
	close(in[0]);
	close(out[1]);
	close(err[1]);

	// we need all read() non blocking.
	fcntl(out[0], F_SETFL, O_NONBLOCK);
	fcntl(err[0], F_SETFL, O_NONBLOCK);
	fcntl(in[0], F_SETFL, O_NONBLOCK);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	{
		char buf[400], *p, *p2;
		int i, n, status;
		struct termios old, t;
		char k[2];
		int muted = 0;
		//int in_ads = 0;

		// allow the use of keyboard without the need to use the "return" key.
		tcgetattr(STDIN_FILENO, &old);
		t = old;
		t.c_lflag &= ~(ICANON);
		tcsetattr(STDIN_FILENO, TCSANOW, &t);

		i=0;
		for(;;){
			n = read(STDIN_FILENO, buf, sizeof(buf)-1);
			if (n>0){
				write(in[1], buf, n);
			}

			n = read(out[0], buf, sizeof(buf)-1);
			if (n>0){
				buf[n] = 0;
				printf("%s", buf);
				p = strstr(buf, ICY_SIGNATURE);
				if (p) {
					p+=strlen(ICY_SIGNATURE);
					k[0]=is_an_ad(p); k[1]=0;
					if (k[0]) {
						//in_ads = 1;
						if ((p2 = strstr(p, "\n"))) { p2[0] = '\0'; }
						if (k[0] == 'm') {
							if (muted) {
								writelog(LOGFILEFAIL, p, "[ALREADY MUTE]");
							}
							else {
								usleep(500000);
								write(in[1], k, 1); // mute
								writelog(LOGFILEFAIL, p, "[MUTE]");
								muted=1;
							}
						}
						else {
							write(in[1], k, 1); // zap or watever
							writelog(LOGFILEFAIL, p, NULL);
						}
					}
					else {
						//in_ads = 0;
						p  = strstr(p, "='")+2;
						if ((p2 = strstr(p, "';StreamUrl='';\n"))) { p2[0] = '\0'; }
						if ((p2 = strstr(p, "';\n"))) { p2[0] = '\0'; }
						if ((p2 = strstr(p, "\n"))) { p2[0] = '\0'; }
						if (muted){
							sleep(16);
							write(in[1], "m", 1); // unmute
							writelog(LOGFILEFAIL, p, "[UNMUTE]");
							muted=0;
						}
						writelog(LOGFILEOK, p, NULL);
					}
				}
				i++;
				fflush(stdout);
			}

			n = read(err[0], buf, sizeof(buf)-1);
			if (n>0){
				buf[n] = 0;
				fprintf(stderr, "%s", buf);
				fflush(stderr);
			}

			usleep(100000);
			if (waitpid(pid, &status, WNOHANG) == -1){
				pid=0;
				break;
			}
		}
		close(in[1]);
		close(out[0]);
		close(err[0]);
		tcsetattr(STDIN_FILENO, TCSANOW, &old);
		return WEXITSTATUS(status);
	}
	return 0;
}
