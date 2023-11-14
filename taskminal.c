#include <curses.h>
#include <errno.h>
#include <ncurses.h>
#include <panel.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <string.h>

#define TSK_TASKS_CAPACITY 1024
#define TSK_COMPONENTS_STACK_CAPACITY 32
#define TSK_DIALOG_INPUT_CAPACITY 512
#define TSK_TEMPLATE_DIR "data/template"

void init_ncurses() {
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	keypad(stdscr, TRUE);
	assert(has_colors() == TRUE);
	start_color();
	use_default_colors();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(5, COLOR_BLUE, COLOR_WHITE);
	init_pair(2, COLOR_WHITE, COLOR_RED);
	init_pair(3, COLOR_BLACK, COLOR_CYAN);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
}

typedef enum {
	CALENDAR_HANDLER = 0,
	TASK_MANAGER_HANDLER,
	DIALOG_HANDLER,
} Action_Handlers; 

typedef struct {
	WINDOW *background;
	WINDOW *shadow;
	WINDOW *overlay;
	PANEL *panel;
} Basewin;

typedef struct {
	int active;
	struct tm* tm;
	int y;
	int x;
	WINDOW* location;
	int tsk_id;
} Calendar;

typedef struct {
	char* data;
	int status;
} Task;

typedef struct {
	Task tasks[TSK_TASKS_CAPACITY];
	WINDOW* location;
	int tasks_size; 
	int active_task;
	int y;
	int x;
	Calendar *cal;
} Task_Manager;

typedef struct {
	Action_Handlers handler;
	void* structure;
} Component;

typedef struct {
	Component components[TSK_COMPONENTS_STACK_CAPACITY];	
	int components_size;
	int active_component;
} App;

void tsk_push_to_app(App *app, Component c) {
	app->components[app->components_size++] = c; 
}

void tsk_pop_from_app(App *app) {
	app->components_size -= 1;	
}

void render_base_win(WINDOW* win, Basewin* bswin) {
	wbkgd(bswin->background, COLOR_PAIR(3));
	wbkgd(bswin->shadow, COLOR_PAIR(4));
	wbkgd(bswin->overlay, COLOR_PAIR(1));
	box(bswin->overlay, 0, 0);
	update_panels();	
	doupdate();
}

Basewin* create_base_win(int y, int x, int h, int w) {
	Basewin *win = malloc(sizeof(Basewin));
	win->background = newwin(h, w, y, x);
	win->shadow = newwin(0.8*h, 0.8*w, y + 0.1*h, x + 0.1*w);
	win->overlay = newwin(0.8*h, 0.8*w, y + 0.05*h, x + 0.05*w); 
	win->panel = new_panel(win->background);
	new_panel(win->shadow);
	new_panel(win->overlay);
	render_base_win(stdscr, win);
	return win;
}

typedef enum {
	CAL_ACTIVE_NEXT,
	CAL_ACTIVE_PREV,
	CAL_ACTIVE_UP,
	CAL_ACTIVE_DOWN,
} Calendar_Actions;
int day_in_month(int month, int year) {
	switch(month) {
		case 1:
			if ((year % 4 == 0 && year % 100 != 0) || (year % 400)) {
				return 29;
			} else {
				return 28;
			}
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
			return 30;
		case 0:
		case 2:
		case 4:
		case 6:
		case 9: 
		case 11:
			return 31;
		default:
			assert(0 && "day_in_month: Illegal month");
	}
}

void render_calendar(Calendar *cal) {
	struct tm first_day_in_month = {0};
	first_day_in_month.tm_mday = 0;
	first_day_in_month.tm_mon = cal->tm->tm_mon;
	first_day_in_month.tm_year = cal->tm->tm_year;
	mktime(&first_day_in_month);

	int start_wday = (first_day_in_month.tm_wday + 7) % 8;
	int max_day = day_in_month(cal->tm->tm_mon, cal->tm->tm_year);

	mvwprintw(cal->location, cal->y, cal->x, "Календарь");
	mvwprintw(cal->location, cal->y + 1, cal->x, "--------------------");
	mvwprintw(cal->location, cal->y + 2, cal->x, "Пн Вт Ср Чт Пт Сб Вс");

	for (int i = 1; i <= max_day; i++) {
		mvwprintw(cal->location, cal->y + 3 + ((i + start_wday) / 7), cal->x + ((i + start_wday) % 7)*3, "%02d", i);
	}

	wattron(cal->location, COLOR_PAIR(5));
	mvwprintw(cal->location, cal->y + 3 + ((cal->tm->tm_mday + start_wday) / 7), cal->x + ((cal->tm->tm_mday + start_wday) % 7)*3, "%02d", cal->tm->tm_mday);
	wattroff(cal->location, COLOR_PAIR(5));

	wattron(cal->location, COLOR_PAIR(2));
	mvwprintw(cal->location, cal->y + 3 + ((cal->active + start_wday) / 7), cal->x + ((cal->active + start_wday) % 7)*3, "%02d", cal->active);
	wattroff(cal->location, COLOR_PAIR(2));

	update_panels();
	doupdate();
}

Calendar* create_calendar(WINDOW *location, int tsk_id, int y, int x) {
	time_t t = time(NULL);
	struct tm* ltm = localtime(&t);
	Calendar *cal = (Calendar*) malloc(sizeof(Calendar));
	cal->active = ltm->tm_mday;
	cal->tm = ltm;
	cal->y = y;
	cal->x = x;
	cal->location = location;
	cal->tsk_id = tsk_id;
	render_calendar(cal);
	return cal;
}

void do_calendar_action(Calendar* cal, Calendar_Actions act) {
	switch(act) {
		case CAL_ACTIVE_NEXT:
			if (cal->active < day_in_month(cal->tm->tm_mon, cal->tm->tm_year)) {
				cal->active += 1;
			}
			break;
		case CAL_ACTIVE_PREV:
			if (cal->active > 1) {
				cal->active -=1;
			}
			break;
		case CAL_ACTIVE_UP:
			if (cal->active - 7 > 0) {
				cal->active -= 7;
			}
			break;
		case CAL_ACTIVE_DOWN:
			if (cal->active + 7 <= day_in_month(cal->tm->tm_mon, cal->tm->tm_year)) {
				cal->active += 7;
			}
			break;
		default:
			assert(0 && "do_calendar_action: calendar action is unreachble");
	}
}

typedef struct {
	char* title;
	int h;
	int w;
	int y;
	int x;
	WINDOW* overlay;
	WINDOW* decoration;
	PANEL* panel;
	char* input;
	int input_size;
	int parent_id;
} Dialog;

typedef enum {
	DLG_CLOSE,
} Dialog_Actions;

int centered_pos_x(WINDOW *win, int wp) {
	return (getmaxx(win) - wp) / 2;
}

int u8strlen(char *s) {
	int count = 0;
	while (*s != '\0') {
		count += ((*s & 0xC0) != 0x80);
		s += 1;
	}
	return count;
}

void render_dialog(Dialog* dialog) {
	wclear(dialog->decoration);
	wbkgd(dialog->decoration, COLOR_PAIR(5));
	box(dialog->decoration, 0, 0);
	mvwaddstr(dialog->decoration, 1, centered_pos_x(dialog->overlay, u8strlen(dialog->title)), dialog->title);
	mvwaddstr(dialog->decoration, 2, 1, dialog->input);
	update_panels();
	doupdate();
}

Dialog* create_dialog(int parent_id, char* title, int h, int w) {
	Dialog* dialog = (Dialog*) malloc(sizeof(Dialog));
	dialog->parent_id = parent_id;
	dialog->title = title;
	dialog->input = (char*) malloc(sizeof(char)*TSK_DIALOG_INPUT_CAPACITY);
	dialog->input[0] = '\0';
	dialog->input_size = 0;
	dialog->h = h;
	dialog->w = w;
	dialog->y = (getmaxy(stdscr) - dialog->h) / 2;
	dialog->x = (getmaxx(stdscr) - dialog->w) / 2;
	dialog->decoration = newwin(h, w, dialog->y, dialog->x);
	dialog->overlay = derwin(dialog->decoration, h - 3, w - 2, 2, 1);
	dialog->panel = new_panel(dialog->decoration);
	return dialog;
}

void do_dialog_action(App *app, Dialog_Actions act) {
	Dialog *dlg = app->components[app->active_component].structure;
	switch(act) {
		case DLG_CLOSE:
			del_panel(dlg->panel);
			delwin(dlg->decoration);
			delwin(dlg->overlay);
			tsk_pop_from_app(app);
			app->active_component = app->components_size - 1;
			break;
		default:
			assert(0 && "do_dialog_action: Unreacheable action");
			break;
	}
}

typedef enum {
	TASKS_ADD,
	TASKS_CREATE,
	TASKS_REMOVE_ACTIVE,
	TASKS_TOGGLE_ACTIVE,
	TASKS_UP,
	TASKS_DOWN,
	TASKS_SAVE_TO_FILE,
	TASKS_SAVE_TO_FILE_TODAY,
	TASKS_LOAD_FROM_FILE,
	TASKS_LOAD_FROM_FILE_TODAY,
	TASKS_CREATE_TEMPLATE,
	TASKS_PASTE_TEMPLATE,
} Tasks_Actions;

char *tsk_make_file_path(Calendar* cal) {
	char* file_path = (char*) malloc(20);
	sprintf(file_path, "data/%d-%02d-%02d", 1900 + cal->tm->tm_year, cal->tm->tm_mon + 1, cal->active); 
	return file_path;
}

void tsk_save_to_file(Task_Manager *tsk, char *file_path) {
	FILE* f = fopen(file_path, "w");
	if (f == NULL) {
		fprintf(stderr, "do_tasks_action: can't open file `%s`: %s", file_path, strerror(errno));
		exit(1);
	}

	for (int i = 0; i < tsk->tasks_size; i++) {
		fprintf(f, "%d %s\n", tsk->tasks[i].status, tsk->tasks[i].data);
		if (ferror(f)) {
			fprintf(stderr, "do_tasks_action: can't open file `%s`: %s", file_path, strerror(errno));
			exit(1);
		}
	}

	fclose(f);
}

void tsk_load_from_file(Task_Manager *tsk, const char *file_path) {
	FILE *f = fopen(file_path, "r");
	if (f == NULL) {
		tsk->tasks_size = 0;
		return;
		//fprintf(stderr, "do_tasks_action: can't open file `%s`: %s", file_path, strerror(errno));
		//exit(1);
	}

	size_t len = 0;
	int i = 0;
	char *line;

	while (getline(&line, &len, f) != -1) {
		char* res = strdup(line);
		int space_char = strchr(res, ' ') - res;
		line[space_char] = '\0';
		tsk->tasks[i].status = strcmp(res, "1") == 0 ? 1 : 0;
		tsk->tasks[i].data = res + space_char + 1;
		tsk->tasks[i].data[strlen(tsk->tasks[i].data) - 1] = '\0';
		i++;
	}

	tsk->tasks_size = i;

	fclose(f);
}

void do_tasks_action(App *app, Task_Manager* tsk, Tasks_Actions act, ...) {
	va_list args;
	va_start(args, act);
	switch(act) {
		case TASKS_CREATE:
			tsk_push_to_app(app, (Component) {.structure=create_dialog(app->active_component, "Введите задачу и нажмите Enter", 4, getmaxx(stdscr)*0.5), .handler=DIALOG_HANDLER});
			app->active_component = app->components_size - 1;
			break;
		case TASKS_ADD: {
			char *task_data = va_arg(args, char*);
			assert(task_data != NULL);
			assert(strlen(task_data) > 0);
			tsk->tasks[tsk->tasks_size++] = (Task) {.data = task_data, .status = 0};
			break;
		}
		case TASKS_REMOVE_ACTIVE:
			for (int i = tsk->active_task; i < tsk->tasks_size - 1; i++) {
				tsk->tasks[i] = tsk->tasks[i + 1];
			}
			tsk->tasks_size -= 1;
			do_tasks_action(app, tsk, TASKS_UP); 
			break;
		case TASKS_UP:
			if (tsk->active_task > 0) {
				tsk->active_task -= 1;
			}
			break;
		case TASKS_DOWN:
			if (tsk->active_task + 1 < tsk->tasks_size) {
				tsk->active_task += 1;
			}
			break;
		case TASKS_TOGGLE_ACTIVE:
			tsk->tasks[tsk->active_task].status = 
				tsk->tasks[tsk->active_task].status ? 0 : 1; 
			break;
		case TASKS_SAVE_TO_FILE: {
			char* file_path = va_arg(args, char*);
			assert(file_path != NULL);
			tsk_save_to_file(tsk, file_path);
			break;
		}
		case TASKS_SAVE_TO_FILE_TODAY: {
			char *file_path = tsk_make_file_path(tsk->cal);
			do_tasks_action(app, tsk, TASKS_SAVE_TO_FILE, file_path); 
			break;
	    }
		case TASKS_LOAD_FROM_FILE: {
			const char *file_path = va_arg(args, char*);
			assert(file_path != NULL);
			tsk_load_from_file(tsk, file_path);
			break;
		}
		case TASKS_LOAD_FROM_FILE_TODAY: {
			char *file_path = tsk_make_file_path(tsk->cal);	
			do_tasks_action(app, tsk, TASKS_LOAD_FROM_FILE, file_path);
			break;
		}
		case TASKS_CREATE_TEMPLATE: {
			do_tasks_action(app, tsk, TASKS_SAVE_TO_FILE, TSK_TEMPLATE_DIR);
			break;
		}
		case TASKS_PASTE_TEMPLATE: {
			do_tasks_action(app, tsk, TASKS_LOAD_FROM_FILE, TSK_TEMPLATE_DIR); 
			do_tasks_action(app, tsk, TASKS_SAVE_TO_FILE_TODAY); 
			do_tasks_action(app, tsk, TASKS_LOAD_FROM_FILE_TODAY);
			break;
		}
	}
	va_end(args);
}

void dialog_key_handler(App* app, int key) {
	Dialog *dlg = app->components[app->active_component].structure;
	switch(key) {
		case 27: // ESC
			do_dialog_action(app, DLG_CLOSE);
			break;
		case KEY_BACKSPACE: 
			dlg->input_size -= 1;
			dlg->input[dlg->input_size] = '\0';
			break;
		case '\n':
			do_tasks_action(app, app->components[dlg->parent_id].structure, TASKS_ADD, dlg->input);
			do_dialog_action(app, DLG_CLOSE);
			break;
		default:
			dlg->input[dlg->input_size++] = key;
			dlg->input[dlg->input_size] = '\0';
			break;
	}
}

void calendar_key_handler(App *app, Calendar* cal, int key) {
	switch(key) {
		case 'k':
		case KEY_UP:
			do_calendar_action(cal, CAL_ACTIVE_UP);
			do_tasks_action(app, app->components[cal->tsk_id].structure, TASKS_LOAD_FROM_FILE_TODAY);
			break;
		case 'j':
		case KEY_DOWN:
			do_calendar_action(cal, CAL_ACTIVE_DOWN);
			do_tasks_action(app, app->components[cal->tsk_id].structure, TASKS_LOAD_FROM_FILE_TODAY);
			break;
		case 'h':
		case KEY_LEFT:
			do_calendar_action(cal, CAL_ACTIVE_PREV);
			do_tasks_action(app, app->components[cal->tsk_id].structure, TASKS_LOAD_FROM_FILE_TODAY);
			break;
		case 'l':
		case KEY_RIGHT:
			do_calendar_action(cal, CAL_ACTIVE_NEXT);
			do_tasks_action(app, app->components[cal->tsk_id].structure, TASKS_LOAD_FROM_FILE_TODAY);
			break;
	}
}

char task_status_as_char(int status) {
	return status ? 'X' : ' '; 
}

void render_task_manager(Task_Manager* tsk) {
	int max_length = getmaxx(tsk->location) - 2; 
	mvwprintw(tsk->location, tsk->y, tsk->x, "Задачи");	
	if (tsk->tasks_size == 0) {
		mvwprintw(tsk->location, tsk->y + 1, tsk->x, "[Пусто]");	
	}
	for (int i = 0; i < tsk->tasks_size; i++) {
		mvwprintw(tsk->location, tsk->y + 1 + i, tsk->x, "[%c] %.*s", 
				task_status_as_char(tsk->tasks[i].status),
				max_length,
				tsk->tasks[i].data);	
		if (i == tsk->active_task) {
			wattron(tsk->location, COLOR_PAIR(5));
			mvwprintw(tsk->location, tsk->y + 1 + i, tsk->x, "[%c] %.*s", 
					task_status_as_char(tsk->tasks[i].status),
					max_length,	
					tsk->tasks[i].data);
			wattroff(tsk->location, COLOR_PAIR(5));
		}
	}
	update_panels();
	doupdate();
}

Task_Manager* create_task_manager(WINDOW* location, Calendar* cal, int y, int x) {
	Task_Manager *tsk = (Task_Manager*) malloc(sizeof(Task_Manager));	
	if (tsk == NULL) {
		fprintf(stderr, "ERROR: can't allocate memory for Task_Manager\n");
		exit(1);
	}
	tsk->active_task = 0;
	tsk->tasks_size = 0;
	tsk->y = y;
	tsk->x = x;
	tsk->location = location;
	tsk->cal = cal;

	render_task_manager(tsk);
	
	return tsk;
}

void task_manager_key_handler(App* app, Task_Manager* tsk, int key) {
	switch(key) {
		case 'k':
		case KEY_UP:
			do_tasks_action(app, tsk, TASKS_UP);
			break;
		case 'j':
		case KEY_DOWN:
			do_tasks_action(app, tsk, TASKS_DOWN);
			break;
		case ' ':
		case '\n':
			do_tasks_action(app, tsk, TASKS_TOGGLE_ACTIVE);
			break;
		case 'r':
			do_tasks_action(app, tsk, TASKS_REMOVE_ACTIVE); 
			break;
		case 'c':
			do_tasks_action(app, tsk, TASKS_CREATE); 
			break;
		case 's':
			do_tasks_action(app, tsk, TASKS_SAVE_TO_FILE_TODAY);	
			break;
		case 't':
			do_tasks_action(app, tsk, TASKS_CREATE_TEMPLATE);
			break;
		case 'p':
			do_tasks_action(app, tsk, TASKS_PASTE_TEMPLATE);
			break;
	}
}

void tsk_render_app(App *app) {
	for (int i = 0; i < app->components_size; i++) {
		switch(app->components[i].handler) {
			case CALENDAR_HANDLER:
				render_calendar(app->components[i].structure);
				break;
			case TASK_MANAGER_HANDLER:
				render_task_manager(app->components[i].structure);
				break;
			case DIALOG_HANDLER:
				render_dialog(app->components[i].structure);
				break;
		}
	}
}


App app = {0};

int main () {
	setlocale(LC_ALL, "");
	init_ncurses();

	Basewin *win = create_base_win(0, 0, getmaxy(stdscr), getmaxx(stdscr));
	Calendar *cal = create_calendar(win->overlay, 1, 1, 5);
	// TODO: Тоже сделать через id а не через ссылку. Такое двухсторонее связывание 
	Task_Manager *tsk = create_task_manager(win->overlay, cal, 11, 5);
	do_tasks_action(&app, tsk, TASKS_LOAD_FROM_FILE_TODAY);
	
	tsk_push_to_app(&app, (Component) {.structure = cal, .handler = CALENDAR_HANDLER});
	tsk_push_to_app(&app, (Component) {.structure = tsk, .handler = TASK_MANAGER_HANDLER});
	//tsk_push_to_app(&app, (Component) {.structure = create_dialog("SALAMALEKIM", 5, 10), .handler = DIALOG_HANDLER});

	//doetasks_action(tsk, TASKS_ADD, "Первое");
	//do_tasks_action(tsk, TASKS_ADD, "Второе");
	//do_tasks_action(tsk, TASKS_ADD, "Третье");
	//do_tasks_action(tsk, TASKS_ADD, "Четвертоe");
	//do_tasks_action(tsk, TASKS_ADD, "Пятое");
	//do_tasks_action(tsk, TASKS_ADD, "Six");

	//tsk_save_to_file(tsk, "./data.bin");
	//tsk_load_from_file(tsk, "data.bin");
	render_base_win(stdscr, win);

	tsk_render_app(&app);
	int key;
	while ((key = getch())) {
		switch(key) {
			case 9:
				app.active_component = (app.active_component + 1) % app.components_size;	
				break;
		}

		switch(app.components[app.active_component].handler) {
			case CALENDAR_HANDLER:
				calendar_key_handler(&app, cal, key);
				break;
			case TASK_MANAGER_HANDLER:
				task_manager_key_handler(&app, app.components[app.active_component].structure, key);
				break;
			case DIALOG_HANDLER:
				dialog_key_handler(&app, key);
				break;
		}
		
		wclear(stdscr);
		wclear(win->overlay);
		tsk_render_app(&app);
		render_base_win(stdscr, win);

	}
	endwin();
	return 0;
}
