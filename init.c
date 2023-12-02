#include <linux/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>

unsigned int SHUTDOWN_DELAY = 3;

char *const RC_INIT[] = { "/etc/rc.init", NULL };
char *const RC_SHUTDOWN[] = { "/etc/rc.shutdown", NULL };

static void cleanup(int cmd);
static sigset_t set;
int main(void) {
	if (getpid() != 1)
		return 1;

	mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, (void*) NULL); //Remounts root as RO

	if (0 != mount("none", "/dev", "devtmpfs", 0, "")) {
		openlog("Init", LOG_PID, LOG_USER);
	    	syslog(LOG_EMERG, "Couldn't mount /dev!");
    		closelog();
		
		return 1;
	}
	
	if(stdout == NULL)
		freopen("/dev/null", "w", stdout);
	if(stderr == NULL)
		freopen("/dev/null", "w", stderr);
	if(stdin == NULL)
		freopen("/dev/null", "r", stdin);

	int sig;

	size_t i;

	setsid();
	
	if(getpid() != 1) {
		if (fork() == 0) {
			sigprocmask(SIG_UNBLOCK, &set, NULL);
			execvp(RC_INIT[0], RC_INIT);
			return 0;
		}
		return 0;
	}
	
	reboot(LINUX_REBOOT_CMD_CAD_OFF);
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);

	if (fork() == 0) {
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		execvp(RC_INIT[0], RC_INIT);
		return 0;
	}
loop:
	alarm(30);
	sigwait(&set, &sig);
	switch(sig) {
		case SIGCHLD:
		case SIGALRM:
			while (waitpid(-1, NULL, WNOHANG) > 0);
			alarm(30);
			break;
		case SIGUSR1:
			cleanup(LINUX_REBOOT_CMD_POWER_OFF);
			break;
		case SIGINT:
			cleanup(LINUX_REBOOT_CMD_RESTART);
			break;
	}
	goto loop;

	return 0;
}

static void cleanup(int cmd) {
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	execvp(RC_SHUTDOWN[0], RC_SHUTDOWN); //Executes shutdown script and waits for it to finish 'can cause hang'
	umount2("/", 0);
	kill(-1, SIGTERM);
	sleep(SHUTDOWN_DELAY);
	kill(-1, SIGKILL);
	sync(); //Dumps all writes onto disk.
	mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, (void*) NULL); //Remounts root as RO
	reboot(cmd);
}
