#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <gtk/gtk.h>
#include <math.h>
#include "DashGL/dashgl.h"

static void on_realize(GtkGLArea *area);
static void on_render(GtkGLArea *area, GdkGLContext *context);
static gboolean on_idle(gpointer data);
static gboolean on_keydown(GtkWidget *widget, GdkEventKey *event);
static gboolean on_keyup(GtkWidget *widget, GdkEventKey *event);

#define NUM_BULLETS 10
#define WIDTH 640.0f
#define HEIGHT 480.0f

GLuint program;
GLuint vao;
GLint attribute_texcoord, attribute_coord2d;
GLint uniform_mytexture, uniform_mvp;

struct bullet {
	vec3 pos;
	GLfloat rot;
	gboolean active;
	int ticks;
};

struct {
	GLuint vbo[2];
	GLuint tex[2];
	vec3 pos;
	mat4 mvp;
	gboolean left;
	gboolean right;
	gboolean up;
	gboolean space;
	GLfloat rot;
	GLfloat dx, dy, max;
	float bullet_speed;
	int num_ticks;
	struct bullet bullets[NUM_BULLETS];
} player;

int main(int argc, char *argv[]) {

	GtkWidget *window;
	GtkWidget *glArea;

	gtk_init(&argc, &argv);

	// Initialize Window

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Astroids Tutorial");
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_UTILITY);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(on_keydown), NULL);
	g_signal_connect(window, "key-release-event", G_CALLBACK(on_keyup), NULL);

	// Initialize GTK GL Area

	glArea = gtk_gl_area_new();
	gtk_widget_set_vexpand(glArea, TRUE);
	gtk_widget_set_hexpand(glArea, TRUE);
	g_signal_connect(glArea, "realize", G_CALLBACK(on_realize), NULL);
	g_signal_connect(glArea, "render", G_CALLBACK(on_render), NULL);
	gtk_container_add(GTK_CONTAINER(window), glArea);

	// Show widgets

	gtk_widget_show_all(window);
	gtk_main();

	return 0;

}

static void on_realize(GtkGLArea *area) {

	// Debug Message
	
	int i;
	g_print("on realize\n");

	gtk_gl_area_make_current(area);
	if(gtk_gl_area_get_error(area) != NULL) {
		fprintf(stderr, "Unknown error\n");
		return;
	}

	const GLubyte *renderer = glGetString(GL_RENDER);
	const GLubyte *version = glGetString(GL_VERSION);

	printf("Renderer: %s\n", renderer);
	printf("OpenGL version supported %s\n", version);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLfloat player_vertices[] = {
		-24.0, -24.0, 0.0, 0.0, 
		-24.0,  24.0, 0.0, 1.0,
		 24.0,  24.0, 1.0, 1.0,
		
		-24.0, -24.0, 0.0, 0.0,
		 24.0,  24.0, 1.0, 1.0,
		 24.0, -24.0, 1.0, 0.0
	};
	
	glGenBuffers(1, &player.vbo[0]);
	glBindBuffer(GL_ARRAY_BUFFER, player.vbo[0]);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(player_vertices),
		player_vertices,
		GL_STATIC_DRAW
	);

	GLfloat bullet_vertices[] = {
		-10.0, -10.0, 0.0, 0.0, 
		-10.0,  10.0, 0.0, 1.0,
		 10.0,  10.0, 1.0, 1.0,
		
		-10.0, -10.0, 0.0, 0.0,
		 10.0,  10.0, 1.0, 1.0,
		 10.0, -10.0, 1.0, 0.0
	};
	
	glGenBuffers(1, &player.vbo[1]);
	glBindBuffer(GL_ARRAY_BUFFER, player.vbo[1]);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(bullet_vertices),
		bullet_vertices,
		GL_STATIC_DRAW
	);
	
	player.pos[0] = WIDTH / 2.0f;
	player.pos[1] = HEIGHT / 2.0f;
	player.pos[2] = 0.0;
	player.left = FALSE;
	player.right = FALSE;
	player.up = FALSE;
	player.space = FALSE;
	mat4_translate(player.pos, player.mvp);
	player.tex[0] = shader_load_texture("sprites/player.png");
	player.tex[1] = shader_load_texture("sprites/bullet.png");

	player.dx = 0.0f;
	player.dy = 0.0f;
	player.max = 8.0f;
	player.bullet_speed = 6.0f;
	player.num_ticks = 70;

	for(i = 0; i < 3; i++) {
		player.bullets[i].active = FALSE;
	}

	const char *vs = "shader/vertex.glsl";
	const char *fs = "shader/fragment.glsl";

	program = shader_load_program(vs, fs);

	const char *attribute_name = "coord2d";
	attribute_coord2d = glGetAttribLocation(program, attribute_name);
	if(attribute_coord2d == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
		return;
	}

	const char *uniform_name = "orthograph";
	GLint uniform_ortho = glGetUniformLocation(program, uniform_name);
	if(uniform_ortho == -1) {
		fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
		return;
	}

	uniform_name = "mvp";
	uniform_mvp = glGetUniformLocation(program, uniform_name);
	if(uniform_mvp == -1) {
		fprintf(stderr, "Could not bind uniform %s\n", uniform_name);
		return;
	}

	glUseProgram(program);
	mat4 orthograph;
	mat4_orthagonal(WIDTH, HEIGHT, orthograph);
	glUniformMatrix4fv(uniform_ortho, 1, GL_FALSE, orthograph);

	g_timeout_add(20, on_idle, (void*)area);

}

static void on_render(GtkGLArea *area, GdkGLContext *context) {
	
	int i;
	mat4 pos, rot;
	gboolean draw_mirror = FALSE;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(program);

	glBindVertexArray(vao);
	glEnableVertexAttribArray(attribute_coord2d);
	glEnableVertexAttribArray(attribute_texcoord);

	// Draw bullet sprites

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, player.tex[1]);
	glUniform1i(uniform_mytexture, 0);

	for(i = 0; i < NUM_BULLETS; i++) {
		
		if(!player.bullets[i].active) {
			continue;
		}

		mat4_translate(player.bullets[i].pos, pos);
		mat4_rotate_z(player.bullets[i].rot, rot);
		mat4_multiply(pos, rot, player.mvp);
	
		glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, player.mvp);

		glBindBuffer(GL_ARRAY_BUFFER, player.vbo[1]);
		glVertexAttribPointer(
			attribute_coord2d,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			0
		);

		glVertexAttribPointer(
			attribute_texcoord,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			(void*)(sizeof(float) * 2)
		);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		if(player.bullets[i].pos[0] > WIDTH - 24.0f) {
			player.bullets[i].pos[0] -= WIDTH;
			draw_mirror = TRUE;
		} else if(player.bullets[i].pos[0] < 24.0f) {
			player.bullets[i].pos[0] += WIDTH;
			draw_mirror = TRUE;
		}

		if(player.bullets[i].pos[1] > HEIGHT - 24.0f) {
			player.bullets[i].pos[1] -= HEIGHT;
			draw_mirror = TRUE;
		} else if(player.bullets[i].pos[1] < 24.0f) {
			player.bullets[i].pos[1] += HEIGHT;
			draw_mirror = TRUE;
		}

		if(!draw_mirror) {
			continue;
		}

		mat4_translate(player.bullets[i].pos, pos);
		mat4_rotate_z(player.bullets[i].rot, rot);
		mat4_multiply(pos, rot, player.mvp);
	
		glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, player.mvp);

		glBindBuffer(GL_ARRAY_BUFFER, player.vbo[1]);
		glVertexAttribPointer(
			attribute_coord2d,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			0
		);

		glVertexAttribPointer(
			attribute_texcoord,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			(void*)(sizeof(float) * 2)
		);
		glDrawArrays(GL_TRIANGLES, 0, 6);

	}

	// Draw Player Sprite

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, player.tex[0]);
	glUniform1i(uniform_mytexture, 0);

	mat4_translate(player.pos, pos);
	mat4_rotate_z(player.rot, rot);
	mat4_multiply(pos, rot, player.mvp);
	
	glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, player.mvp);

	glBindBuffer(GL_ARRAY_BUFFER, player.vbo[0]);
	glVertexAttribPointer(
		attribute_coord2d,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(float) * 4,
		0
	);

	glVertexAttribPointer(
		attribute_texcoord,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(float) * 4,
		(void*)(sizeof(float) * 2)
	);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	if(player.pos[0] > WIDTH - 24.0f) {
		player.pos[0] -= WIDTH;
		draw_mirror = TRUE;
	} else if(player.pos[0] < 24.0f) {
		player.pos[0] += WIDTH;
		draw_mirror = TRUE;
	}

	if(player.pos[1] > HEIGHT - 24.0f) {
		player.pos[1] -= HEIGHT;
		draw_mirror = TRUE;
	} else if(player.pos[1] < 24.0f) {
		player.pos[1] += HEIGHT;
		draw_mirror = TRUE;
	}

	if(draw_mirror) {

		mat4_translate(player.pos, pos);
		mat4_rotate_z(player.rot, rot);
		mat4_multiply(pos, rot, player.mvp);
	
		glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, player.mvp);

		glBindBuffer(GL_ARRAY_BUFFER, player.vbo[0]);
		glVertexAttribPointer(
			attribute_coord2d,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			0
		);

		glVertexAttribPointer(
			attribute_texcoord,
			2,
			GL_FLOAT,
			GL_FALSE,
			sizeof(float) * 4,
			(void*)(sizeof(float) * 2)
		);
		glDrawArrays(GL_TRIANGLES, 0, 6);

	}
	// Draw Bullet Sprites

	glDisableVertexAttribArray(attribute_coord2d);
	glDisableVertexAttribArray(attribute_texcoord);

}


static gboolean on_idle(gpointer data) {

	if(player.right) {
		player.rot -= 0.05f;
	}

	if(player.left) {
		player.rot += 0.05f;
	}
	
	int i;
	float dx, dy;

	if(player.up) {
		player.dx -= sin(player.rot) * 0.05;
		player.dy += cos(player.rot) * 0.05;
	}
	
	if(player.dx < -player.max) {
		player.dx = -player.max;
	} else if(player.dx > player.max) {
		player.dx = player.max;
	}

	if(player.dy < -player.max) {
		player.dy = -player.max;
	} else if(player.dy > player.max) {
		player.dy = player.max;
	}

	player.pos[0] += player.dx;
	player.pos[1] += player.dy;
	
	for(i = 0; i < NUM_BULLETS; i++) {
		if(!player.bullets[i].active) {
			continue;
		}

		if(player.bullets[i].ticks < 0) {
			player.bullets[i].active = FALSE;
		}

		dx = sin(player.bullets[i].rot) * player.bullet_speed;
		dy = cos(player.bullets[i].rot) * player.bullet_speed;
		player.bullets[i].pos[0] -= dx;
		player.bullets[i].pos[1] += dy;
		player.bullets[i].ticks--;
	}

	gtk_widget_queue_draw(GTK_WIDGET(data));
	return TRUE;

}

static gboolean on_keydown(GtkWidget *widget, GdkEventKey *event) {

	switch(event->keyval) {
		case GDK_KEY_Left:
			player.left = TRUE;
		break;
		case GDK_KEY_Right:
			player.right = TRUE;
		break;
		case GDK_KEY_Up:
			player.up = TRUE;
		break;
		case GDK_KEY_space:
			
			if(!player.space) {
				int i;
				for(i = 0; i < NUM_BULLETS; i++) {
					if(player.bullets[i].active) {
						continue;
					}
					player.bullets[i].active = TRUE;
					player.bullets[i].pos[0] = player.pos[0];
					player.bullets[i].pos[1] = player.pos[1];
					player.bullets[i].pos[2] = player.pos[2];
					player.bullets[i].rot = player.rot;
					player.bullets[i].ticks = player.num_ticks;
					break;
				}
			}

			player.space = TRUE;
		break;
	}

}

static gboolean on_keyup(GtkWidget *widget, GdkEventKey *event) {

	switch(event->keyval) {
		case GDK_KEY_Left:
			player.left = FALSE;
		break;
		case GDK_KEY_Right:
			player.right = FALSE;
		break;
		case GDK_KEY_Up:
			player.up = FALSE;
		break;
		case GDK_KEY_space:
			player.space = FALSE;
		break;
	}

}
