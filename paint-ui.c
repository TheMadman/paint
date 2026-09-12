#include <srvsh.h>
#include <gamesh.h>
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <poll.h>
#include <stdlib.h>
#include <libadt.h>
#include <string.h>

typedef struct libadt_vector vec_t;

const char *app_name = NULL;
int mouse_fd = -1;
int toolbar_height = 0;
bool click = false;
double x = 0.f, y = 0.f;
struct pollfd *pollfds = NULL;
vec_t mouse_listeners = {.size = sizeof(int)};

#define pexit(...) SDL_Log("%s:", app_name), SDL_Log(__VA_ARGS__), exit(EXIT_FAILURE)

#define MESSAGE_TYPES(OPERATION) \
	OPERATION(gamesh_event_listen_op) \
	OPERATION(gamesh_sdl_render_surface) \
	OPERATION(gamesh_sdl_event_mouse)

#define INIT_GLOBALS(MESSAGE) int MESSAGE = -1;

MESSAGE_TYPES(INIT_GLOBALS)

static void forward_response(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	sendmsgop(
		*(int*)context,
		opcode,
		data,
		size,
		header.msg_control,
		header.msg_controllen
	);
	close_cmsg_fds(header);
}

static void handle_request(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	bool close_fds = true;
	if (opcode == gamesh_event_listen_op) {
		int *listen_opcodes = data;
		int length = size / sizeof(int);
		for (int i = 0; i < length; i++) {
			int *listen_opcode = &listen_opcodes[i];
			if (*listen_opcode != gamesh_sdl_event_mouse)
				continue;

			int mouse_listener = gamesh_get_fd(header);
			if (mouse_listener < 0)
				break;

			if (libadt_vector_push(&mouse_listeners, &mouse_listener) < 0)
				break;

			close_fds = false;
			int remaining = length - i;
			memmove(
				listen_opcode,
				listen_opcode + 1,
				remaining * sizeof(int)
			);
			size -= sizeof(int);

			// if this was the only opcode, we don't need to send
			// anything to our server
			if (size == 0) {
				writeop(fd, gamesh_event_listen_op, NULL, 0);
				return;
			}
		}
	} else if (opcode == gamesh_sdl_render_surface) {
		if (size < sizeof(int[4])) {
			writeop(fd, -1, NULL, 0);
			return;
		}
		struct {
			int x;
			int y;
			int w;
			int h;
		} *coords = data;
		coords->y += toolbar_height;
	}

	ssize_t result = sendmsgop(
		SRV_FILENO,
		opcode,
		data,
		size,
		header.msg_control,
		header.msg_controllen
	);
	if (result < 0)
		writeop(fd, -1, NULL, 0);

	const struct pollfd pollfd = {.fd = SRV_FILENO};
	pollopfd(pollfd, forward_response, &fd, -1);

	if (close_fds)
		close_cmsg_fds(header);
}

static void handle_mouse_event(
	int fd,
	int opcode,
	void *data,
	int size
)
{
	SDL_Event *event = data;
	if (event->motion.y < toolbar_height) {
		// handle toolbar mouse stuff
	}

	event->motion.y -= toolbar_height;
	for (int i = 0; i < mouse_listeners.length; i++) {
		int *listener = libadt_vector_index(mouse_listeners, i);
		struct pollfd mouse_listener_pollfd = {
			.fd = *listener,
			.events = POLLOUT,
		};
		poll(&mouse_listener_pollfd, 1, 0);
		if (mouse_listener_pollfd.revents & POLLOUT) {
			writeop(*listener, opcode, event, size);
		}
	}

}

static void handle_message(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	if (fd == SRV_FILENO) {
		// something went seriously wrong here
		close_cmsg_fds(header);
		return;
	}

	if (is_cli(fd)) {
		handle_request(fd, opcode, data, size, header, context);
		return;
	}

	if (fd == mouse_fd) {
		handle_mouse_event(fd, opcode, data, size);
	}
}

typedef struct {
	int *dest;
	const char *name;
} message_t;

#define CREATE_STRUCT(MESSAGE) { .dest = &MESSAGE, .name = #MESSAGE },

int main(int arg, char **argv)
{
	app_name = argv[0];
	opcode_db *db = open_opcode_db();
	if (!db)
		pexit("open_opcode_db");

	static const message_t messages[] = {
		MESSAGE_TYPES(CREATE_STRUCT)
		{ 0 },
	};

	for (const message_t *message = messages; message->dest; message++) {
		*message->dest = get_opcode(db, message->name);
		if (*message->dest < 0)
			pexit("get_opcode(%s)", message->name);
	}

	close_opcode_db(db);

	mouse_fd = gamesh_event_listen(gamesh_sdl_event_mouse);
	if (mouse_fd < 0)
		pexit("gamesh_event_listen");

	size_t client_count = cli_count();
	// + 1 for the mouse_fd, + 1 for the SRV_FILENO
	int pollfds_count = client_count + 2;
	pollfds = calloc(pollfds_count, sizeof(struct pollfd));
	if (!pollfds)
		pexit("calloc");

	if (srvcli_polls(pollfds, pollfds_count) < 0)
		pexit("srvcli_polls");

	pollfds[pollfds_count - 1] = (struct pollfd){
		.fd = mouse_fd,
		.events = POLLIN
	};

	SDL_Surface *surface = SDL_LoadPNG("./toolbar.png");
	if (!surface)
		pexit("SDL_LoadPNG: %s", SDL_GetError());

	toolbar_height = surface->h;

	gamesh_graphic_t graphic = gamesh_create_graphic_from(
		0,
		0,
		surface->w,
		surface->h,
		surface
	);
	if (!graphic.buffer)
		pexit("gamesh_create_graphic_from");

	SDL_DestroySurface(surface);

	gamesh_graphic_commit(graphic);

	do {
		struct pollfd result = pollopfds(
			pollfds,
			pollfds_count,
			handle_message,
			NULL,
			-1
		);
		if (result.fd == SRV_FILENO && result.revents & POLLHUP)
			break;
		if (is_cli(result.fd) && result.revents & POLLHUP)
			client_count--;
	} while (0 < client_count);
}
