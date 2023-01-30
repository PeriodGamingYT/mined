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
	x = abs(x) % screen_size_x;
	y = abs(y) % screen_size_y;
	printf("\x1b[%d;%df", y + 1, x + 1);
}

void clear_screen() {
	set_cursor(0, 0);
	printf("\x1b[2J");
	set_cursor(0, 0);
}

void screen_size() {
	struct winsize ws;
	if(
		ioctl(
			STDOUT_FILENO, 
			TIOCGWINSZ, 
			&ws
		) != -1 || 
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

	if(text_size_x == 0 && text_size_y == 0) {
		text_size_x = screen_size_x;
		text_size_y = screen_size_y;
	}
	
	text = (char*) malloc(
		sizeof(char*) * 
		text_size_x *
		text_size_y
	);

	for(int i = 0; i < text_size_x * text_size_y; i++) {
		text[i] = ' ';
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

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
void print_text() {
	clear_screen();
	int off_x = MAX(0, cursor_x - screen_size_x);
	int off_y = MAX(0, cursor_y - screen_size_y);
	set_cursor(0, 0);
	for(int y = 0; y < screen_size_y && y < text_size_y; y++) {
		for(int x = 0; x < screen_size_x && x < text_size_x; x++) {
			char text_char = text_at(x + off_x, y + off_y);
			set_cursor(x, y);
			if(!isprint(text_char)) {
				printf(" ");
				continue;	
			}
			
			printf("%c", text_char);
		}
	}

	set_cursor(cursor_x, cursor_y);
}

void wrap_cursor() {
	if(cursor_x < 0) {
		cursor_x = text_size_x - 1;
	}

	if(cursor_y < 0) {
		cursor_y = text_size_y - 1;
	}
	
	if(cursor_x >= text_size_x) {
		cursor_x = 0;
	}

	if(cursor_y >= text_size_y) {
		cursor_y = 0;
	}
}

void print_text_at_cursor() {
	wrap_cursor();
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
}

int input_number() {
	struct termios oldattr, newattr;
  tcgetattr( STDIN_FILENO, &oldattr );
  newattr = oldattr;
  newattr.c_oflag &= ~(OPOST);
  newattr.c_lflag &= ~(ECHO);
  tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
  int result;
  scanf("%d", &result);
  tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
  return result;
}

void text_user_goto() {
	int x = input_number();
	set_cursor(cursor_x, cursor_y);
	int y = input_number();
	cursor_x = x;
	cursor_y = y;
	wrap_cursor();
}

void text_user_insert() {
	int user_char = input_number();
	set_text_char((char)user_char);
}

//// file
static char *filename = NULL;
static FILE *file = NULL;
void file_save() {
	if(filename == NULL) {
		return;
	}

	file = fopen(filename, "w+");
	int i = 0;
	while(i < text_size_x * text_size_y) {
		if(!isprint(text[i])) {
			fputc(' ', file);
			i++;
			continue;
		}
		
		fputc(text[i], file);
		if(i % text_size_x == 0 && i > 1) {
			fputc('\n', file);
			i += i % text_size_x;
		}
		
		i++;
	}

	fclose(file);
}

void text_to_file_size() {
	if(text_size_x != 0 || text_size_y != 0) {
		return;
	}
	
	int i = 0;
	while(!feof(file)) {
		char current_char = fgetc(file);
		if(current_char == '\n' && text_size_x == 0) {
			text_size_x = i;
		}

		i++;
	}

	if(text_size_x != 0) {
		text_size_y = (int)(i / (text_size_x - 1));
	}
}

void file_load() {
	if(filename == NULL) {
		return;
	}

	file = fopen(filename, "r");
	if(file == NULL) {
		return;
	}
	
	text_to_file_size();
	rewind(file);
	init_text();
	int i = 0;
	while(!feof(file)) {
		int current_char = fgetc(file);
		if(current_char == '\n') {
			continue;
		}

		if(!isprint(current_char)) {
			text[i] = ' ';
			continue;
		}
		
		text[i] = current_char;
		i++;
	}

	print_text();
	printf("\n");
	fclose(file);
}

//// input
#define KEY_ESCAPE 27
#define KEY_BACKSPACE 127
#define KEY_NULL 0
#define KEY_ARROW_UP 1000
#define KEY_ARROW_LEFT 1001
#define KEY_ARROW_DOWN 1002
#define KEY_ARROW_RIGHT 1003
#define KEY_ENTER 13
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

		case CTRL('h'):
		case KEY_BACKSPACE:
			cursor_x--;
			break;

		case KEY_ENTER:
			cursor_y++;
			cursor_x = 0;
			break;

		case KEY_TAB:
			for(int i = 0; i < TAB_SIZE; i++) {
				set_text_char(' ');
				wrap_cursor();
			}

			break;

		case CTRL_KEY('a'):
			text_user_insert();
			break;

		case CTRL_KEY('g'):
			text_user_goto();
			break;

		case CTRL_KEY('q'):
			file_save();
			clear_screen();
			exit(0);
			break;

		case CTRL_KEY('c'):
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

	wrap_cursor();
	if(
		cursor_x > screen_size_x || 
		cursor_y > screen_size_y
	) {
		print_text();
	}
}

//// main
int main(int argc, char *argv[]) {
	screen_size();
	clear_screen();
	if(argc == 2) {
		filename = argv[1];
	}

	file_load();
	if(filename == NULL || file == NULL) {
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
