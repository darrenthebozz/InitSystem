/* 
	Copyright (c) 2021 Darren Wallcroft
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include <linux/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>

static void cleanup(int cmd);

static sigset_t set;

int main(void) {
	int sig;
	size_t i;
	
	if(getpid() != 1) {
		return 1;
	}
	
	reboot(LINUX_REBOOT_CMD_CAD_OFF);
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);

	if (fork() == 0) {
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		setsid();
		char *const rcinit[] = { "/etc/rc.init", NULL };
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
	/* not reachable */
	return 0;
}

static void cleanup(int cmd) {
	char *const rcshutdown[] = { "/etc/rc.shutdown", NULL };
	setsid();
	execvp(rcshutdown[0], rcshutdown);
	umount2("/", 0);
	kill(-1, SIGTERM);
	sleep(3);
	kill(-1, SIGKILL);
	sync();
	mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, (void*) NULL);
	reboot(cmd);
}
