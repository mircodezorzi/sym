/**
 * Sym is a virtualization software to simulate process scheduling, memory management and in future mass-storage management.
 * It is entirely written in c, with no third party libraries. Everything is in one file.
 * The processes are stored in a linked list (which in future might become a skiplist(?)).
 * This file also contains a dialog object to automatically create custom menus and matplotc, a library for plotting values.
 * TODO:
 *   - Instead of printing to the screen, print to a buffer, process it and then flush entire buffer to screen.
 *     This could allow me to automatically draw appropriate unicode line characers without having to do it manually.
 *     All *print* functions would become *printb* and bflush(buffer) would print buffer to screen.
 *     Lastly this would make screen repainting on resize somewhat easier.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

/* define keys */
#define __DVORAK__
#if defined(__DVORAK__)
	/* TODO: add binds for left and right movements */
	#define KEY_DOWN      CTRLMASK('h')
	#define KEY_UP        CTRLMASK('t')
	#define KEY_JUMP_DOWN CTRLMASK('g')
	#define KEY_JUMP_UP   CTRLMASK('c')
	#define KEY_RIGHT     CTRLMASK('d')
	#define KEY_LEFT      CTRLMASK('n')
	#define KEY_QUIT      CTRLMASK('f')
#else
	#define KEY_DOWN      CTRLMASK('j')
	#define KEY_UP        CTRLMASK('k')
	#define KEY_JUMP_DOWN CTRLMASK('u')
	#define KEY_JUMP_UP   CTRLMASK('i')
	#define KEY_RIGHT     CTRLMASK('h')
	#define KEY_LEFT      CTRLMASK('l')
	#define KEY_QUIT      CTRLMASK('c')
#endif /* __DVORAK__ */

/* configs */
#define STRING_MAX_SIZE 128

/* macros */
#define CTRLMASK(k) ((k) & 0x1f)
#define CURSORTO(x, y) printf("\033[%d;%dH", (y) + 1, (x) + 1)
#define SIZE(vec) (sizeof(vec)/sizeof((vec)[0]))
#define VOID_PTR(x) ((void*)(x))

/* global variables */
unsigned int term_h;
unsigned int term_w;

/* structs */
struct Process {

	char name[STRING_MAX_SIZE];
	int pid;
	int priority;

	struct Process* parent;
	struct Process* next;

	struct Stage {
		char name[STRING_MAX_SIZE];
		int namelen;
		enum { Io, Computing } type;
		int t_length;
	} *stages;
	int nstages;
	int cstage; /* current stage */

	struct Segment {
		char name[STRING_MAX_SIZE];
		int namelen;
		int t_load;
		int t_unload;
		int address;
		int size;
	} *segments;
	int nsegments;
	int memory;

	enum { Launched, Acquiring,
	       Ready,    Executing,
	       Blocked,  Zombie,
	       Terminated } status;

	int t_arrival;
	int t_length;
	int t_turnaround;
	int t_ellapsed;

};

struct Entry {
	char* l;
	int length; /* in case value is a string */
	void* v;
	int i;
	enum { String, Integer, Boolean,
	       ProcessStage, ProcessSegment } t;
	int* c;
	int s; /* subentry selected for ProcessStage and ProcessSegment entries */
};

struct Dialog {
	int x, y;
	int w, h;
	int ratio; /* ratio between label column and value column */
	struct Entry* entries;
	int nentries;
	int nelements;
	int selected; /* selected entry */
	int cselected; /* selected entry in entries with c > 1 */
	int scroll;
	char* title;
};

/* prototypes */
int dialog_input(struct Dialog* d);
struct Dialog* dialog_new(struct Entry* entries, int nentries, int x, int y, int w, int h, int ratio);
void _mvprintw(int x, int y, char* str, int len, int w);
void dialog_draw(struct Dialog* d);
void dialog_free(struct Dialog* d);
void dialog_init(struct Dialog* d);
void draw_border(int x, int y, int w, int h);
void draw_heline(int x, int y, int len);
void draw_hline(int x, int y, int len);
void draw_hline(int x, int y, int len);
void draw_veline(int x, int y, int len);
void draw_vline(int x, int y, int len);
void draw_vline(int x, int y, int len);
void endwin();
void initwin();
void mvprintf(int x, int y, char* format, ...);
void mvprintw(int x, int y, char* str, int len, int w);
void resize_handler(int sig);
void unmask_ctrl(char* str, int key);

/* functions */
void draw_border(int x, int y, int w, int h){
	CURSORTO(x, y);
	printf("\u250C");
	for(int i = 0; i < w - 2; i++)
		printf("\u2500");
	printf("\u2510");

	for(int i = 1; i < h - 1; i++) {
		CURSORTO(x, y + i);
		printf("\u2502%*s\u2502", w - 2, "");
	}

	CURSORTO(x, y + h - 1);
	printf("\u2514");
	for(int i = 0; i < w - 2; i++)
		printf("\u2500");
	printf("\u2518");
}

void draw_hline(int x, int y, int len){
	CURSORTO(x, y);
	for(int i = 0; i < len; i++)
		printf("\u2500");
}

void draw_vline(int x, int y, int len){
	for(int i = 0; i < len; i++) {
		CURSORTO(x, y + i + 1);
		printf("\u2502");
	}
}

/**
 * Draw horizontal line with ending characters
 */
void draw_heline(int x, int y, int len){
	CURSORTO(x, y);
	printf("\u251C");
	for(int i = 0; i < len - 2; i++) {
		printf("\u2500");
	}
	printf("\u2524");
}

/**
 * Draw vertical line with ending characters
 */
void draw_veline(int x, int y, int len){
	CURSORTO(x, y + 0);
	printf("\u252C");
	for(int i = 0; i < len; i++) {
		CURSORTO(x, y + i + 1);
		printf("\u2502");
	}
	CURSORTO(x, y + len + 1);
	printf("\u2534");
}

/**
 * Move cursor to (x, y) and print with format.
 */
void mvprintf(int x, int y, char* format, ...){
	CURSORTO(x, y);
	va_list vargs;
	va_start(vargs, format);
	vfprintf(stdout, format, vargs);
	va_end (vargs);
}

/**
 * Move cursor to (x, y) and print string.
 * When len > w troncate string and print dots instead. String is aligned to the right.
 * @param char* str string to print
 * @param int len length of string str
 * @param int w maximum width to print
 */
void mvprintw(int x, int y, char* str, int len, int w){
	CURSORTO(x, y);
	if(len > w) {
		printf("...");
		for(int i = len - w + 2; i < len; i++)
		putchar(str[i]);
	} else {
		for(int i = -1; i < w - len; i++)
			putchar(' ');
		for(int i = 0; i < len; i++)
			putchar(str[i]);
	}
}

/**
 * Move cursor to (x, y) and print string.
 * When len > w troncate string. String is aligned to the left.
 * @param char* str string to print
 * @param int len length of string str
 * @param int w maximum width to print
 */
void _mvprintw(int x, int y, char* str, int len, int w){
	for(int i = 0; i < len && i < w; i++)
		putchar(str[i]);
}

/**
 * Utility function to convert a key to a printable string
 * If key is maksed with CTRLMASK() then the function will return ^key otherwise key.
 * @param char* str string to store result in
 * @param int key key to convert
 */
void unmask_ctrl(char* str, int key){
	if(key < 27) {
		str[0] = '^';
		str[1] = key + 0x61;
		str[2] = '\0';
	} else {
		str[0] = key;
		str[1] = '\0';
	}
}

void resize_handler(int sig){
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	term_h = w.ws_row;
	term_w = w.ws_col;
	signal(SIGWINCH, resize_handler);
}

/**
 * Initialize termal.
 *   - set terminal in raw mode
 *   - set terminal in no echo mode
 *   - hide cursor
 *   - install signal handler for terminal resize
 */
void initwin(){
	system("clear");
	system("stty raw");
	system("stty -echo");
	printf("\033[?25l");
	resize_handler(0);
}

void endwin(){
	system("clear");
	system("stty cooked");
	system("stty echo");
	printf("\033[?25h");
}

struct Dialog* dialog_new(struct Entry* entries, int nentries, int x, int y, int w, int h, int ratio){
	struct Dialog* d = malloc(sizeof(struct Dialog));
	d->entries = malloc(sizeof(struct Entry) * nentries);
	d->entries = entries;
	d->nentries = nentries;
	for(int i = 0; i < nentries; i++) {
		switch(entries[i].t) {
		case String:
			d->entries[i].length = strlen((char*)(entries[i].v));
			break;
		case ProcessStage:
			/* TODO: implement variable size arrays, see dialog_compute() */
			d->entries[i].v = malloc(sizeof(struct Stage) * (*(d->entries[i].c)));
			for(int j = 0; j < (*(d->entries[i].c)); j++) {
				((struct Stage*)(d->entries[i].v))[j].type = Computing;
				((struct Stage*)(d->entries[i].v))[j].t_length = 0;
				sprintf(((struct Stage*)(d->entries[i].v))[j].name, "stage %d", j + 1);
				((struct Stage*)(d->entries[i].v))[j].namelen = strlen(
					((struct Stage*)(d->entries[i].v))[j].name
				);
				d->entries[i].s = 0;
			}
			break;
		case ProcessSegment:
			break;
		}
	}

	d->x = x;
	d->y = y;
	d->w = w;
	d->h = h;
	d->ratio = x / ratio;
	d->cselected = 0;
	d->nelements= 0;
	d->scroll = 0;
	d->selected = 0;
	return d;
}

void dialog_free(struct Dialog* d){
	for(int i = 0; i < d->nentries; i++)
		if(d->entries[i].c != 1)
			free(d->entries[i].v);
	free(d->entries);
	free(d);
}

/**
 * Function to validate ProcessStage and ProcessSegment entries, not needed for simpler dialog objects.
 * This could also be implemented as a lambda in future versions.
 * TODO:
 *   - Implement variable size arrays
 *   - Add total memory and length calculations (quite clumsy)
 */
void dialog_compute(struct Dialog* d){
}

void dialog_draw(struct Dialog* d){

	char format[128];

	draw_border(d->x, d->y, d->w, d->h);

	d->nelements = 0;
	for(int i = 0; i < d->nentries; i++) {
		if(d->entries[i].c != 1)
			d->nelements += *(d->entries[i].c);
	}

	int scrolled = 0; /* keep track of the current cursor position */
	for(int i = d->scroll; scrolled < d->h - 2 && scrolled < d->nelements - 2; i++) {
		CURSORTO(d->x + 1, d->y + 1 + scrolled);
		switch(d->entries[i].t) {
		case String:
			if(d->selected == i)
				printf("\033[0;30;41m");
			_mvprintw(d->x + 1,
				  d->y + 1 + scrolled,
				  d->entries[i].l,
				  strlen(d->entries[i].l),
				  d->ratio - 1);
			mvprintw(d->x + d->ratio + 1,
				 d->y + 1 + scrolled,
				 ((char*)(d->entries[i].v)),
				 (d->entries[i].length),
				 d->w - d->ratio - 3);
			scrolled++;
			break;
		case Integer:
			if(d->selected == i)
				printf("\033[0;30;41m");
			_mvprintw(d->x + 1,
				  d->y + 1 + scrolled,
				  d->entries[i].l,
				  strlen(d->entries[i].l),
				  d->ratio - 1);
			/* TODO: align to the right */
			mvprintf(d->x + d->ratio + 1,
				 d->y + 1 + scrolled,
				 "%d",
				 *((int*)(d->entries[i].v)));
			scrolled++;
			break;
		case ProcessStage:
			/* TODO: better printing */
			for(int j = 0; scrolled < i + *(d->entries[i].c) && scrolled < d->h - 2; j++) {
				int current = d->selected == i && d->cselected == j;
				CURSORTO(d->x + 1, d->y + 1 + scrolled);
				scrolled++;

				sprintf(format, "%%%d.d ", d->ratio - 1);
				printf(format, j + 1);

				/* highlight currently selected subentry */
				if(current && d->entries[i].s == 0)
					printf("\033[0;30;41m");
				printf("[%c]", ((struct Stage*)(d->entries[i].v))[j].type == Io ? '*' : ' ');
				printf("\033[0;30;0m");
				printf(" ");

				if(current && d->entries[i].s == 1)
					printf("\033[0;30;41m");
				printf("%d", ((struct Stage*)(d->entries[i].v))[j].t_length);
				printf("\033[0;30;0m");
				printf(" ");

				if(current && d->entries[i].s == 2)
					printf("\033[0;30;41m");
				printf("%s", ((struct Stage*)(d->entries[i].v))[j].name);
				printf("\033[0;30;0m");
			}
			break;
		default:
			printf("type not yet supported");
			scrolled++;
			break;
		}
		printf("\033[0;30;0m");
	}

	draw_veline(d->x + d->ratio, d->y, d->h - 2);
}

int dialog_input(struct Dialog* d){

	char key;
	int scrolled = 0; /* keep track of the current cursor position */
	switch(key = getchar()) {
		case KEY_UP:
			next:
			if(d->entries[d->selected].c != 1
			&& d->cselected > 0) {
				d->cselected--;
			} else if(d->entries[d->selected - 1].c != 1) {
				d->cselected = *(d->entries[d->selected - 1].c) - 1;
				d->selected--;
			} else {
				d->selected--;
			}
			break;
		case KEY_DOWN:
			prev:
			if(d->entries[d->selected].c != 1
			&& d->cselected < *(d->entries[d->selected].c) - 1) {
				d->cselected++;
			} else {
				d->selected++;
				d->cselected = 0;
			}
			break;
		case KEY_JUMP_UP:
			d->selected--;
			d->cselected = 0;
			break;
		case KEY_JUMP_DOWN:
			d->selected++;
			d->cselected = 0;
			break;
		case KEY_LEFT:
			if(d->entries[d->selected].s < 2)
				d->entries[d->selected].s++;
			break;
		case KEY_RIGHT:
			if(d->entries[d->selected].s > 0)
				d->entries[d->selected].s--;
			break;
		case KEY_QUIT:
			return 0;
		case '\033':
			getchar();
			switch(key = getchar()) {
			case 'A': goto prev;
			case 'B': goto next;
			}
		case ' ':
			if(!d->entries[d->selected].i) break; /* entry is not iteractive */
			switch(d->entries[d->selected].t) {
			case String:
				goto appstr;
			case ProcessStage:
				switch(d->entries[d->selected].s) {
				case 0:
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].type =
					!((struct Stage*)(d->entries[d->selected].v))[d->cselected].type;
					break;
				case 2:
					processapp:
					(((struct Stage*)(d->entries[d->selected].v))[d->cselected].name)[
						((struct Stage*)(d->entries[d->selected].v))[d->cselected].namelen
					] = key;
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].namelen++;
					(((struct Stage*)(d->entries[d->selected].v))[d->cselected].name)[
						((struct Stage*)(d->entries[d->selected].v))[d->cselected].namelen
					] = '\0';
					break;
				default:
					break;
				}
				break;
			}
			break;
		case '!' ... '/': /* ASCII letters */
		case ':' ... '~': /* somewhat niggerlicious, might consider rewriting the function in the future */
			if(!d->entries[d->selected].i) break; /* entry is not iteractive */
			switch(d->entries[d->selected].t) {
			case String:
				appstr:
				if(d->entries[d->selected].length >= STRING_MAX_SIZE) break;
				((char*)(d->entries[d->selected].v))[d->entries[d->selected].length] = key;
				d->entries[d->selected].length++;
				((char*)(d->entries[d->selected].v))[d->entries[d->selected].length] = '\0';
				break;
			case ProcessStage:
				if(d->entries[d->selected].s == 2) goto processapp;
				break;
			}
			break;
		case '0' ... '9':
			if(!d->entries[d->selected].i) break; /* entry is not iteractive */
			switch(d->entries[d->selected].t) {
			case String:
				goto appstr;
			case Integer:
				*((int*)(d->entries[d->selected].v)) =
				*((int*)(d->entries[d->selected].v)) * 10 + key - 0x30;
				break;
			case ProcessStage:
				switch(d->entries[d->selected].s) {
				case 1:
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].t_length =
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].t_length * 10 + key - 0x30;
					break;
				case 2:
					goto processapp;
					break;
				default:
					break;
				}
			}
			break;
		case 127:
			if(!d->entries[d->selected].i) break; /* entry is not iteractive */
			switch(d->entries[d->selected].t) {
			case String:
				if(d->entries[d->selected].length <= 0) break;
				((char*)(d->entries[d->selected].v))[d->entries[d->selected].length] = '\0';
				d->entries[d->selected].length--;
				break;
			case Integer:
				*((int*)(d->entries[d->selected].v)) /= 10;
			case ProcessStage:
				switch(d->entries[d->selected].s) {
				case 1:
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].t_length /= 10;
					break;
				case 2:
					((struct Stage*)(d->entries[d->selected].v))[d->cselected].namelen--;
					(((struct Stage*)(d->entries[d->selected].v))[d->cselected].name)[
						((struct Stage*)(d->entries[d->selected].v))[d->cselected].namelen
					] = '\0';
					break;
				default:
					break;
				}
			}
			break;
		default:
			break;
	}
	return 1;
}

void dialog_init(struct Dialog* d){
	do {
		dialog_draw(d);
	} while(dialog_input(d));
}

int main(int argc, char** argv){

	struct Process p;

	strcpy(p.name, "a really long name");
	p.nstages = 10;
	p.nsegments = 10;
	p.memory = 0;
	p.t_length = 0;
	p.priority = 0;
	p.t_arrival = 0;

	struct Entry entries[] = {
		{ .l = "Name",     .t = String,         .v = VOID_PTR( p.name),      .i = 1, .c = 1 },
		{ .l = "Priority", .t = Integer,        .v = VOID_PTR(&p.priority),  .i = 1, .c = 1 },
		{ .l = "Arrival",  .t = Integer,        .v = VOID_PTR(&p.t_arrival), .i = 1, .c = 1 },
		{ .l = "Stages",   .t = Integer,        .v = VOID_PTR(&p.nstages),   .i = 1, .c = 1 },
		{ .l = "",         .t = ProcessStage,   .v = VOID_PTR( p.stages),    .i = 1, .c = &p.nstages },
		{ .l = "Length",   .t = Integer,        .v = VOID_PTR(&p.t_length),  .i = 0, .c = 1 },
		{ .l = "Segments", .t = Integer,        .v = VOID_PTR(&p.nsegments), .i = 1, .c = 1 },
		{ .l = "",         .t = ProcessSegment, .v = VOID_PTR( p.segments),  .i = 1, .c = &p.nsegments },
		{ .l = "Memory",   .t = Integer,        .v = VOID_PTR(&p.memory),    .i = 0, .c = 1 },
	};

	struct Dialog* d = dialog_new(entries, SIZE(entries), 10, 10, 50, 50, 2);
	d->selected = 0;
	d->cselected = 0;

	initwin();
	dialog_init(d);
	endwin();

}
