#include <gamesh.h>
#include <srvsh.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define pexit(msg) perror(msg), exit(EXIT_FAILURE)

int gamesh_sdl_event_mouse = -1;
int mouse_fd = -1;
bool click = false;
float x = 0.f, y = 0.f;
gamesh_graphic_t canvas = { 0 };
SDL_Surface *brush = NULL;

void handle_mouse_nonblock(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode != gamesh_sdl_event_mouse || size < sizeof(SDL_Event))
		return;
	SDL_Event *event = buffer;
	switch (event->type) {
		case SDL_EVENT_MOUSE_MOTION:
			x = event->motion.x - (brush->w / 2.f);
			y = event->motion.y - (brush->h / 2.f);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event->button.button == 1)
				click = true;
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event->button.button == 1)
				click = false;
			break;
	}

	if (click)
		gamesh_graphic_blit(canvas, brush, (int)x, (int)y);
}

void handle_mouse(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	handle_mouse_nonblock(fd, opcode, buffer, size, header, context);
	struct pollfd mouse_pollfd = { .fd = fd };
	do {
		static const int instant_timeout = 0;
		mouse_pollfd = pollopfd(mouse_pollfd, handle_mouse_nonblock, NULL, instant_timeout);
	} while (mouse_pollfd.revents & POLLIN);
	gamesh_graphic_commit(canvas);
}

int main()
{
	canvas = gamesh_create_graphic(0, 0, 640, 480, SDL_PIXELFORMAT_RGBA4444);
	if (!canvas.buffer)
		pexit("gamesh_create_graphic");
	gamesh_graphic_commit(canvas);

	brush = SDL_CreateSurface(9, 9, SDL_PIXELFORMAT_RGBA4444);
	if (!brush)
		pexit("brush");

	SDL_ClearSurface(brush, 1.f, 1.f, 1.f, 1.f);

	opcode_db *db = open_opcode_db();
	if (!db)
		pexit("db");

	gamesh_sdl_event_mouse = get_opcode(db, "gamesh_sdl_event_mouse");
	if (gamesh_sdl_event_mouse < 0)
		pexit("get_opcode");

	close_opcode_db(db);

	mouse_fd = gamesh_event_listen(gamesh_sdl_event_mouse);
	if (mouse_fd < 0)
		pexit("gamesh_event_listen");

	static const int never_timeout = -1;
	struct pollfd mouse_pollfd = { .fd = mouse_fd };
	do {
		mouse_pollfd = pollopfd(mouse_pollfd, handle_mouse, NULL, never_timeout);
	} while (mouse_pollfd.revents & POLLIN);
}
