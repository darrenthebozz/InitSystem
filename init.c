#include <linux/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>

unsigned int SHUTDOWN_DELAY 3
char *const RC_INIT[] = { "/etc/rc.init", NULL };
char *const RC_SHUTDOWN[] = { "/etc/rc.shutdown", NULL };

static void cleanup(int cmd);

static sigset_t set;

int main(void) {
	int sig;
	size_t i;
	
	setsid(); //Creates a new session and we are the leader?
	
	if(getpid() != 1) {
		if (fork() == 0) { //If it has a pid that isn't 1 then just execute the start up script.
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
		//setsid();
		execvp(rcinit[0], rcinit);
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
	//setsid(); //Shouldn't be needed.
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	execvp(rcshutdown[0], RC_SHUTDOWN); //Executes shutdown script and waits for it to finish 'can cause hang'
	umount2("/", 0);
	kill(-1, SIGTERM);
	sleep(SHUTDOWN_DELAY);
	kill(-1, SIGKILL);
	sync(); //Dumps all writes onto disk.
	mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, (void*) NULL); //Remounts root as RO
	reboot(cmd);
}
