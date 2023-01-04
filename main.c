//// includes
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <ctype.h>

//// settings
#define TAB_SIZE 2

//// Handling terminal crap.
int read_char() {
    struct termios oldattr, newattr;
    int key;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    newattr.c_oflag &= ~(OPOST);
    newattr.c_cflag |= (CS8);
    newattr.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    key = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return key;
}

static int screen_size_x = 0;
static int screen_size_y = 0;
void set_cursor(int x, int y) {
	// Flipped because of course, the terminal has to.
	x %= screen_size_x;
	y %= screen_size_y;
	printf("\x1b[%d;%df", y + 1, x + 1);
}

void clear_screen() {
	set_cursor(0, 0);
	printf("\x1b[2J");
	set_cursor(0, 0);
}

void clear_line(int y) {
	set_cursor(0, y);
	printf("\x1b[2K");
}

void screen_size() {
	struct winsize ws;
	if(
		ioctl(
			STDOUT_FILENO, 
			TIOCGWINSZ, 
			&ws
		) == -1 || 
		ws.ws_col == 0
	) {
		screen_size_x = ws.ws_col;
		screen_size_y = ws.ws_row;
		return;
	}

	screen_size_x = 80;
	screen_size_y = 24;
}

//// text editing
static char *text = NULL;
static int cursor_x = 0;
static int cursor_y = 0;
static int text_size_x = 0;
static int text_size_y = 0;
void init_text() {
	if(text != NULL) {
		return;
	}
	
	text = (char*) malloc(
		sizeof(char*) * 
		screen_size_x *
		screen_size_y
	);

	if(text_size_x == 0 && text_size_y == 0) {
		text_size_x = screen_size_x;
		text_size_y = screen_size_y;
	}
}

int is_text_valid() {
	return !(
		text == NULL ||
		cursor_x < 0 ||
		cursor_y < 0 ||
		cursor_x >= text_size_x ||
		cursor_y >= text_size_y
	);
}

char text_at(int x, int y) {
	return text[(y * text_size_x) + x];
}

int get_max_x() {
	int i;
	for(
		i = text_size_x - 1;
		i >= 0 &&
		(
			text_at(i, cursor_y) == ' ' ||
			text_at(i, cursor_y) == 0
		);
		
		i--
	);

	return i + 1;
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
void wrap_cursor() {
	if(cursor_x < 0) {
		cursor_y--;
		cursor_x = text_size_x - 1;
		int max_x = get_max_x();
		cursor_x = MIN(cursor_x, max_x);
	}

	if(cursor_y < 0) {
		cursor_y = 0;
		cursor_x = 0;
	}
	
	if(cursor_x >= text_size_x) {
		cursor_x = 0;
		cursor_y++;
	}

	if(cursor_y >= text_size_y) {
		cursor_x = text_size_x - 1;
		cursor_y = text_size_y - 1;
	}
}

void print_text_at_cursor() {
	set_cursor(cursor_x, cursor_y);
	printf("%c", text_at(cursor_x, cursor_y));
}

void set_text_char(char value) {
	if(!is_text_valid()) {
		return;
	}

	text[(cursor_y * text_size_x) + cursor_x] = value;
	print_text_at_cursor();
	cursor_x++;
	wrap_cursor();
}

void text_delete() {
	if(!is_text_valid()) {
		return;
	}
	
	cursor_x--;
	if(cursor_x >= 0) {
		text[(cursor_y * text_size_x) + cursor_x] = ' ';
	}
	
	print_text_at_cursor();
	wrap_cursor();
}

void text_newline() {
	if(!is_text_valid()) {
		return;
	}

	cursor_y++;
	cursor_x = 0;
	wrap_cursor();
}

//// file
static char *filename = NULL;
static FILE *file = NULL;
void file_save() {
	if(filename == NULL) {
		return;
	}

	rewind(file);
	int i = 0;
	while(i < text_size_x * text_size_y) {
		fputc(text[i], file);
		if(i % text_size_x == 0 && i > 1) {
			fputc('\n', file);	
		}
		
		i++;
	}
}

void text_to_file_size() {
	if(text_size_x != 0 || text_size_y != 0) {
		return;
	}
	
	int i = 0;
	while(!feof(file)) {
		if(fgetc(file) == '\n' && text_size_x == 0) {
			text_size_x = i;
		}

		i++;
	}

	if(text_size_x != 0) {
		text_size_y = (int)(i / (text_size_x - 1));
	}

	rewind(file);
}

void file_load() {
	if(filename == NULL) {
		return;
	}

	file = fopen(filename, "w+");
	if(file == NULL) {
		fprintf(stderr, "mined can't open file!");
		exit(-1);
	}
	
	text_to_file_size();
	init_text();
	int i = 0;
	while(!feof(file)) {
		text[i] = fgetc(file);
		i++;
	}
}

//// input
#define KEY_ESCAPE 27
#define KEY_BACKSPACE 127
#define KEY_NULL 0
#define KEY_ARROW_UP 1000
#define KEY_ARROW_LEFT 1001
#define KEY_ARROW_DOWN 1002
#define KEY_ARROW_RIGHT 1003
#define KEY_ENTER '\n'
#define KEY_TAB '\t'
#define CTRL_KEY(k) ((k) & 0x1f)
static int is_keep_open = 1;
void handle_input(int key) {
	int seq[2];
	switch(key) {
		case KEY_ESCAPE:
			seq[0] = read_char();
			seq[1] = read_char();
			if(seq[0] == 0 || seq[1] == 0) {
				key = KEY_ESCAPE;
				break;
			}

			if(seq[0] == '[' && isupper(seq[1])) {
				switch(seq[1]) {
					case 'A':
						key = KEY_ARROW_UP;
						break;

					case 'B':
						key = KEY_ARROW_DOWN;
						break;

					case 'C':
						key = KEY_ARROW_RIGHT;
						break;

					case 'D':
						key = KEY_ARROW_LEFT;
						break;

					default:
						key = KEY_ESCAPE;
						break;
				}
			}
			
			break;

		case KEY_BACKSPACE:
			text_delete();
			break;

		case KEY_ENTER:
			text_newline();
			break;

		case KEY_TAB:
			for(int i = 0; i < TAB_SIZE; i++) {
				set_text_char(' ');
			}

			break;

		case CTRL_KEY('q'):
			file_save();
			clear_screen();
			exit(0);
			break;

		case CTRL_KEY('c'):
			clear_screen();
			exit(0);
			break;
					
		default:
			if(!isprint(key)) {
				break;
			}
			
			set_text_char((char)key);
			break;
	}

	if(key >= KEY_ARROW_UP && key <= KEY_ARROW_RIGHT) {
		cursor_x += (
			-(key == KEY_ARROW_LEFT) + 
			 (key == KEY_ARROW_RIGHT)
		);
		
		cursor_y += (
			-(key == KEY_ARROW_UP) + 
			 (key == KEY_ARROW_DOWN)
		);
	}
}

//// main
void exit_handler() {
	if(file == NULL) {
		return;
	}

	fclose(file);
}

int main(int argc, char *argv[]) {
	screen_size();
	clear_screen();
	if(argc == 2) {
		filename = argv[1];
	}

	atexit(exit_handler);
	file_load(filename);
	if(text_size_x == 0 || text_size_y == 0) {
		init_text();
	}
	
	int key = '\0';
	while(is_keep_open) {
		key = read_char();
		handle_input(key);
		set_cursor(cursor_x, cursor_y);
	}
	
	return 0;
}
