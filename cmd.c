// Originally written by ezhikov
// See http://hackndev.com/node/188

#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define OK 0
#define ERROR (-1)
#define CMD_SIZE 256

#define UP 65
#define DOWN 66
#define RIGHT 67
#define LEFT 68
#define ENTER 10
#define EXIT ("ex")

#define DUP_IN 10
#define DUP_OUT 11

static struct termios term_state;
static char* syms = "abcdefghigklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXWZ'$|><&*/_!";

char* get_sel(int num, int pos) {
	static const char *esc_l = "\033[0;37;40m";
	static const char *esc_sl = "\033[0;30;47m";
	static const char *esc_r = "\033[0;0;0m";
	static char ret[50];
	char l, m, r;
	int sl = strlen(syms);
	l = syms[num * 3 % sl];
	m = syms[(num * 3 % sl) + 1];
	r = syms[(num * 3 % sl) + 2];
	if(pos == 0)
		sprintf(ret, "%s%c%s%c%c%s",esc_sl,l ,esc_l ,m, r,esc_r);
	else if (pos == 1)
		sprintf(ret, "%s%c%s%c%s%c%s",esc_l, l , esc_sl,m , esc_l, r, esc_r);
	else if (pos == 2)
		sprintf(ret, "%s%c%c%s%c%s",esc_l ,l ,m, esc_sl,r , esc_r);
	return ret;
}

int save_term() {
	if(1 == isatty(0) && 0 <= tcgetattr(0, &term_state)) {
		return OK;
	} else {
		return ERROR;
	}
}

void restore_term() {
	tcsetattr(DUP_IN, TCSAFLUSH, &term_state);
}

int set_term() {
	struct termios ts;
	memcpy(&ts, &term_state, sizeof(ts));
	ts.c_lflag &= ~ICANON;
	ts.c_lflag &= ~ECHO;
	ts.c_cc[VMIN] = 1;
	ts.c_cc[VTIME] = 0;
	return (0 == tcsetattr(0, TCSAFLUSH, &ts)) ? OK : ERROR;
}

void drop(int line, int pos) {
	char* str = get_sel(line, pos);
	write(DUP_OUT, str, strlen(str));

}

void clean_drop(int line, int pos) {
	write(DUP_OUT, "\b\b\b", 3);
	drop(line, pos);
}

void clean_show(char c) {
	static char ln[4] = "\b\b\b ";
	ln[3] = c;
	write(DUP_OUT, ln, 4);
}

int execute(char* cmd) {
	restore_term();
	int fds[2];
	int c_s;
	pipe(fds);
	pid_t c_pid = fork();
	if(0 == c_pid) {
		close(fds[1]);
		dup2(fds[0], 0);
		execl(getenv("SHELL"), "sh", 0);
	}
	close(fds[0]);
	write(fds[1], cmd, strlen(cmd));
	close(fds[1]);
	wait(&c_s);

	dup2(DUP_IN, 0);
	set_term();
	close(0);
	return OK;
}

int main_loop() {
	char dest[CMD_SIZE];
	char btn;
	unsigned int cmd_pos = 0;
	int line, pos;
	line = 0;
	pos = 0;
	int table_l = strlen(syms);
	int last_enter = 0;

	drop(line, pos);
	do{
		read(DUP_IN, &btn, 1);
		switch(btn) {
			case LEFT:
				pos = (pos > 0) ? (pos - 1) : 2;
				last_enter = 0;
				clean_drop(line, pos);
				break;
			case RIGHT:
				pos = (pos + 1) % 3;
				last_enter = 0;
				clean_drop(line, pos);
				break;
			case UP:
				line = (line > 0) ? (line - 1) : (table_l / 3);
				last_enter = 0;
				clean_drop(line, pos);
				break;
			case DOWN:
				line = (line + 1) % (table_l / 3);
				last_enter = 0;
				clean_drop(line, pos);
				break;
			case ENTER:
				if (last_enter == 1) {
					if(0 == strcmp(dest, EXIT)) {
						write(DUP_OUT, "\n", 1);
						return OK;
					} else {
						int i;
						dest[ cmd_pos++ ] = '\n';
						dest[cmd_pos]=0;
						write(DUP_OUT, "\n", 1);
						for(i = 0; dest[i] != 0; i++)
							if(dest[i]=='_')
								dest[i] = ' ';
						execute(dest);
					}
					cmd_pos = 0;
					drop(line, pos);
				} else {
					last_enter = 1;
					clean_show(syms[line * 3 + pos]);
					dest[cmd_pos++] = syms[line * 3 + pos];
					dest[cmd_pos] = 0;
					drop(line, pos);
				}
				break;
			default:
				{}
		}
	}while(1);
	return OK;
}

int main(int argc, char** args) {
	if(OK != save_term()) {
		printf("%s\n","descriptor 0 is not terminal");
		return ERROR;
	}
	atexit(restore_term);
	if(OK != set_term()) {
		printf("%s\n","unable to set terminal");
		return ERROR;
	}
	dup2(0, DUP_IN);
	close(0);
	dup2(1, DUP_OUT);
	return main_loop();
}
