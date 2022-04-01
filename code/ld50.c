/********************************************************************************
* (C) Copyright 2022 Omid Ghavami Zeitooni. All Rights Reserved                 *
********************************************************************************/



/*

  Seed + Water => Food


  Worm[Seed, Seed] => Gem1
  Word[Seed, Food] => Gem2
  Word[Food, Food] => Gem3

 */


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include "imp_sdl.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

typedef uintptr_t umm;

typedef u8 b8;
typedef u16 b16;
typedef u32 b32;

#include "colors.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define AUDIO_FREQ 48000
#define FONT_SIZE 24
#define SMALL_FONT_SIZE 16
#define MAX_ENTITY_COUNT 128
#define MAX_ENTITY_PART_COUNT 32


#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))



struct memory_arena
{
	void *base;
	u32 size;
	u32 used;
};

static void *
push_size(struct memory_arena *arena, u32 size)
{
	u32 remaining = arena->size - arena->used;
	assert(remaining >= size);

	umm p = (umm)arena->base + arena->used;

	arena->used += size;

	void *result = (void *)p;

	return result;
}

#define PUSH_STRUCT(arena, type) (type *)push_size(arena, sizeof(type))

static void
zero_memory(void *base, umm size)
{
	memset(base, 0, size);
}

#define ZERO_STRUCT(source) zero_memory(&source, sizeof(source))

static const f64 F64_ZERO = 0;
static const f32 F32_ZERO = 0;

#define IS_F64_ZERO(x) (memcmp(&x, &F64_ZERO, sizeof(f64)) == 0)
#define IS_F32_ZERO(x) (memcmp(&x, &F32_ZERO, sizeof(f32)) == 0)

struct v2
{
	f32 x;
	f32 y;
};

struct v2i
{
	s32 x;
	s32 y;
};

inline static struct v2
v2(f32 x, f32 y)
{
	struct v2 result;
	result.x = x;
	result.y = y;
	return result;
}

inline static struct v2
add_v2(struct v2 a, struct v2 b)
{
	struct v2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline static struct v2
sub_v2(struct v2 a, struct v2 b)
{
	struct v2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

inline static f32
dot_v2(struct v2 a, struct v2 b)
{
	f32 result = a.x * b.x + a.y * b.y;
	return result;
}

inline static f32
len_v2(struct v2 v)
{
	f32 sqrd_len = dot_v2(v, v);
	return sqrtf(sqrd_len);
}

inline static struct v2
normalize_v2(struct v2 v)
{
	f32 len = len_v2(v);
	return IS_F32_ZERO(len) ? v2(0, 0) : v2(v.x / len, v.y / len);
}

inline static struct v2
scale_v2(struct v2 a, f32 s)
{
	struct v2 result;
	result.x = a.x * s;
	result.y = a.y * s;
	return result;
}


inline static struct v2i
v2i(s32 x, s32 y)
{
	struct v2i result;
	result.x = x;
	result.y = y;
	return result;
}

inline static struct v2i
add_v2i(struct v2i a, struct v2i b)
{
	struct v2i result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline static struct v2i
sub_v2i(struct v2i a, struct v2i b)
{
	struct v2i result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

inline static s32
dot_v2i(struct v2i a, struct v2i b)
{
	s32 result = a.x * b.x + a.y * b.y;
	return result;
}

inline static f32
len_v2i(struct v2i v)
{
	s32 sqrd_len = dot_v2i(v, v);
	return sqrtf(sqrd_len);
}

inline static struct v2
normalize_v2i(struct v2i v)
{
	s32 sqrd_len = dot_v2i(v, v);

	if (sqrd_len == 0)
		return v2(0, 0);

	f32 len = sqrtf(sqrd_len);
	return v2(v.x / len, v.y / len);
}

inline static struct v2i
scale_v2i(struct v2i a, s32 s)
{
	struct v2i result;
	result.x = a.x * s;
	result.y = a.y * s;
	return result;
}

struct entity_part {
	u16 index;
	u16 length;
	u16 size;
	u16 color;
	u16 parent_index;
	u16 render_size;
	u16 depth;
	b16 disposed;
	f32 stiffness;
	f32 mass;
	u8 max_alpha;
	b8 suspended_for_frame;
	b8 internal_collisions;
	u8 content;
	u8 hydration;
	u8 accept;
	b16 passthrough;
	u32 content_value;
	f32 audio_gen;
	struct v2 p;
	struct v2 v;
	struct v2 a;
	struct v2 force;
};

enum entity_type {
	ENTITY_NONE,
	ENTITY_PLAYER      = 0x001,
	ENTITY_FOOD        = 0x002,
	ENTITY_SEED        = 0x004,
	ENTITY_WATER       = 0x008,
	ENTITY_SLIME       = 0x010,
	ENTITY_WORM        = 0x020,
	ENTITY_WATER_EATER = 0x040,
	ENTITY_SOCKET      = 0x080,
	ENTITY_GEM        = 0x100
};

struct entity {
	u32 id;
	u32 index;
	u32 seed;
	u32 type;
	u8 part_count;
	b8 internal_collisions;
	b8 disposed;
	b8 suspended_for_frame;
	struct entity_part parts[MAX_ENTITY_PART_COUNT];

	b16 fixed;
	b16 passthrough;

	struct v2 target;
	f32 pull_of_target;
	u32 target_entity_index;
	u32 target_entity_id;
	b32 has_target;
	f32 next_target_check_t;
	f32 expiration_t;

	f32 z;
	f32 accum_z;
};

enum waveform_type {
	SINE,
	SAW
};

struct waveform {
	u16 tag;
	u16 freq;
	f32 amp;
};

enum game_event_type {
	GAME_EVENT_NONE,
	GAME_EVENT_SEED_TOUCH_WATER,
	GAME_EVENT_WORM_EAT_FOOD,
	GAME_EVENT_WATER_EATER_EAT_WATER,
	GAME_EVENT_GEM_TOUCH_SOCKET
};

struct generic_collision_event {
	u32 e1;
	u32 e2;
	u32 p1;
	u32 p2;
};

struct game_event {
	enum game_event_type type;
	union {
		struct generic_collision_event seed_touch_water;
		struct generic_collision_event worm_eat_food;
		struct generic_collision_event water_eater_eat_water;
		struct generic_collision_event gem_touch_socket;
	};
};

struct spawn_item {
	enum entity_type type;
	u32 param;
	f32 time;
};

struct track_event {
	uint32_t t;
	uint8_t a;
	uint8_t b;
	uint8_t c;
};

struct game_state {
	struct entity entities[MAX_ENTITY_COUNT];
	u32 entity_count;

	u32 entity_index_by_z[MAX_ENTITY_COUNT];

	u32 player_index;
	u32 player_id;

	struct spawn_item spawn_bag[32];
	u32 spawn_bag_count;
	u32 spawn_bag_offset;

	u32 frame_index;
	f32 time;
	f32 real_time;

	f32 last_frame_real_time;

	f32 last_level_end_t;
	f32 tunnel_begin_t;

	f32 level_begin_t;
	f32 level_end_t;

	f32 tunnel_size;

	u32 current_level;

	f32 next_note_t;
	u32 note;

	u32 counter_1;

	f32 next_spawn_t;

	u32 time_speed_up;
	b16 skip_to_begin;
	b16 skip_to_end;

	struct waveform sine_waves[MAX_ENTITY_COUNT * MAX_ENTITY_PART_COUNT];
	u32 sine_wave_count;

	struct waveform saw_waves[MAX_ENTITY_COUNT * MAX_ENTITY_PART_COUNT];
	u32 saw_wave_count;

	struct waveform noise_waves[MAX_ENTITY_COUNT * MAX_ENTITY_PART_COUNT];
	u32 noise_wave_count;

	u64 played_audio_sample_count;

	struct game_event events[64];
	u32 event_count;

	u32 entity_id_seq;

	u32 score;

	b32 game_over;

	b32 pad_;
	const char *level_instr;

	struct track_event *track_events;
	u32 track_event_count;
	u32 track_event_current;
	f32 last_track_event_time;
	u8 last_track_event_status;

	

	f32 last_audio_mix[1024];
	f32 audio_mix_sample[1024];

	/* u32 note_wave_number[128]; */
};

struct input_state
{
	u8 left;
	u8 right;
	u8 up;
	u8 down;

	u8 start;

	u8 speed_up;
	u8 speed_down;

	s8 dleft;
	s8 dright;
	s8 dup;
	s8 ddown;
	s8 dstart;

	s8 dspeed_up;
	s8 dspeed_down;
};

enum text_align
{
	TEXT_ALIGN_LEFT,
	TEXT_ALIGN_CENTER,
	TEXT_ALIGN_RIGHT
};



static s32
random_int(s32 min, s32 max)
{
	s32 range = max - min;
	return min + rand() % range;
}

static f32
random_f32()
{
	u32 r = (u32)(rand());
	return (f32)r / (f32)RAND_MAX;
}

static s32
min(s32 x, s32 y)
{
	return x < y ? x : y;
}

static s32
max(s32 x, s32 y)
{
	return x > y ? x : y;
}

static const struct v2 screen_center = { .x = WINDOW_WIDTH / 2, .y = WINDOW_HEIGHT / 2 };
static struct game_state *global_game;


static inline bool
find_intersection_between_lines_(f32 P0_X,
				 f32 P0_Y,
				 f32 P1_X,
				 f32 P1_Y,
				 f32 P2_X,
				 f32 P2_Y,
				 f32 P3_X,
				 f32 P3_Y,
				 f32 *T1,
				 f32 *T2)
{
    f32 D1_X = P1_X - P0_X;
    f32 D1_Y = P1_Y - P0_Y;
    f32 D2_X = P3_X - P2_X;
    f32 D2_Y = P3_Y - P2_Y;

    f32 Denominator = (-D2_X * D1_Y + D1_X * D2_Y);

    *T1 = (D2_X * (P0_Y - P2_Y) - D2_Y * (P0_X - P2_X)) / Denominator;
    *T2 = (-D1_Y * (P0_X - P2_X) + D1_X * (P0_Y - P2_Y)) / Denominator;

    if (*T1 >= 0 && *T1 <= 1 && *T2 >= 0 && *T2 <= 1)
        return(true);

    return(false);
}

__attribute__((always_inline))
static inline bool
find_intersection_between_lines(struct v2 P1,
				struct v2 P2,
				struct v2 P3,
				struct v2 P4,
				f32 *T1, f32 *T2)
{
	return find_intersection_between_lines_(P1.x, P1.y,
						P2.x, P2.y,
						P3.x, P3.y,
						P4.x, P4.y,
						T1, T2);
}

__attribute__((always_inline))
static inline bool
test_collision_against_line(struct v2 P,
			    struct v2 W1,
			    struct v2 W2,
			    struct v2 Normal,
			    struct v2 *NewP,
			    struct v2 *NewV,
			    f32 *T)
{
	struct v2 dP = sub_v2(*NewP, P);
	struct v2 Tangent = v2(Normal.y, -Normal.x);

	f32 MagnitudeAlongNormal = dot_v2(dP, Normal);
	struct v2 MovementAlongNormal = scale_v2(Normal, MagnitudeAlongNormal);

	if (MagnitudeAlongNormal < 1)
		MovementAlongNormal = normalize_v2(MovementAlongNormal);

	f32 T1, T2;
	bool Collision = find_intersection_between_lines(P,
							 *NewP,
							 W1,
							 W2,
							 &T1,
							 &T2);
	if (Collision)
	{
		struct v2 VAlongTangent = scale_v2(Tangent, dot_v2(*NewV, Tangent));
		struct v2 VAlongNormal = scale_v2(Normal, -0.1f * dot_v2(*NewV, Normal));

		*NewP = add_v2(add_v2(P, scale_v2(dP, T1)), Normal); // + NewV;
		*NewV = add_v2(VAlongTangent, VAlongNormal);
		*T = T1;
	}
	else
	{
		Collision = find_intersection_between_lines(*NewP,
							    sub_v2(*NewP, Normal),
							    W1,
							    W2,
							    &T1,
							    &T2);
		if (Collision)
		{
			struct v2 VAlongTangent = scale_v2(Tangent, dot_v2(*NewV, Tangent));
			struct v2 VAlongNormal = scale_v2(Normal, -0.1f * dot_v2(*NewV, Normal));

			*NewP = add_v2(*NewP, scale_v2(Normal, (1.1f - T1)));
			*NewV = add_v2(VAlongTangent, VAlongNormal);
			*T = T1;
		}
	}
	return(Collision);
}

__attribute__((always_inline))
static inline bool
test_collision_against_box(const struct entity_part *Obstacle,
			   struct entity_part *Entity,
			   struct v2 P,
			   struct v2 *NewP,
			   struct v2 *NewV)
{
    struct v2 MinkowskiSize = v2(Obstacle->size + Entity->size, Obstacle->size + Entity->size);

    struct v2 Diff = sub_v2(P, Obstacle->p);
    if (fabsf(Diff.x) > MinkowskiSize.x || fabsf(Diff.y) > MinkowskiSize.y)
        return false;

#if 0
    struct v2 TopLeft = add_v2(Obstacle->p, scale_v2(v2(-MinkowskiSize.x, MinkowskiSize.y), 0.5f));
    struct v2 TopRight = add_v2(Obstacle->p, scale_v2(MinkowskiSize, 0.5f));
    struct v2 BottomRight = add_v2(Obstacle->p, scale_v2(v2(MinkowskiSize.x, -MinkowskiSize.y), 0.5f));
    struct v2 BottomLeft = add_v2(Obstacle->p, scale_v2(MinkowskiSize, 0.5f));
#else
    struct v2 TopLeft = add_v2(Obstacle->p, scale_v2(v2(-MinkowskiSize.x, -MinkowskiSize.y), 0.5f));
    struct v2 TopRight = add_v2(Obstacle->p, scale_v2(v2(MinkowskiSize.x, -MinkowskiSize.y), 0.5f));
    struct v2 BottomRight = add_v2(Obstacle->p, scale_v2(v2(MinkowskiSize.x, MinkowskiSize.y), 0.5f));
    struct v2 BottomLeft = add_v2(Obstacle->p, scale_v2(v2(-MinkowskiSize.x, MinkowskiSize.y), 0.5f));
#endif
    // NOTE(Omid): Check if inside.
    if (P.x > TopLeft.x &&
        P.x < TopRight.x &&
        P.y > TopLeft.y &&
        P.y < BottomLeft.y)
    {
        f32 D1 = fabsf(P.y - TopLeft.y) + 0.1f;
        f32 D2 = fabsf(P.y - BottomLeft.y) + 0.1f;
        NewP->y -= D1 < D2 ? D1 : -D2;
	return true;
    }

    f32 T;
    if (test_collision_against_line(P, TopLeft, TopRight, v2(0, -1), NewP, NewV, &T))
	    return true;

    if (test_collision_against_line(P, BottomLeft, BottomRight, v2(0, 1), NewP, NewV, &T))
	    return true;

    if (test_collision_against_line(P, TopLeft, BottomLeft, v2(-1, 0), NewP, NewV, &T))
	    return true;

    if (test_collision_against_line(P, TopRight, BottomRight, v2(1, 0), NewP, NewV, &T))
	    return true;

    return false;
}



static void
fill_rect(SDL_Renderer *renderer, s32 x, s32 y, s32 width, s32 height, struct color color)
{
	SDL_Rect rect = {0};
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

static void
draw_rect(SDL_Renderer *renderer, s32 x, s32 y, s32 width, s32 height, struct color color)
{
	SDL_Rect rect = {0};
	rect.x = x;
	rect.y = y;
	rect.w = width;
	rect.h = height;
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawRect(renderer, &rect);
}

static void
draw_string(SDL_Renderer *renderer,
            TTF_Font *font,
            const char *text,
            s32 x, s32 y,
            enum text_align alignment,
            struct color color)
{
	SDL_Color sdl_color =  { color.r, color.g, color.b, color.a };
	SDL_Surface *surface = TTF_RenderText_Solid(font, text, sdl_color);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_Rect rect;
	rect.y = y;
	rect.w = surface->w;
	rect.h = surface->h;
	switch (alignment) {
	case TEXT_ALIGN_LEFT:
		rect.x = x;
		break;
	case TEXT_ALIGN_CENTER:
		rect.x = x - surface->w / 2;
		break;
	case TEXT_ALIGN_RIGHT:
		rect.x = x - surface->w;
		break;
	}

	SDL_RenderCopy(renderer, texture, 0, &rect);
	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

static void
draw_string_f(SDL_Renderer *renderer, TTF_Font *font, s32 x, s32 y, enum text_align alignment, struct color color, const char *format, ...)
{
	char buffer[4096];

	va_list args;
	va_start(args, format);
	#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
	vsnprintf(buffer, ARRAY_COUNT(buffer), format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

	draw_string(renderer, font, buffer, x, y, alignment, color);

	va_end(args);
}


static void
render_cell_(SDL_Renderer *renderer,
	     u8 value, u8 alpha, s32 offset_x, s32 offset_y, s32 w, s32 h,
	     b32 outline)
{
	struct color base_color = BASE_COLORS[value];
	struct color light_color = LIGHT_COLORS[value];
	struct color dark_color = DARK_COLORS[value];

	base_color.a = alpha;
	light_color.a = alpha;
	dark_color.a = alpha;

	s32 edge = 4; // GRID_SIZE / 8;

	s32 x = (s32)(round(offset_x - (w / 2.0)));
	s32 y = (s32)(round(offset_y - (h / 2.0)));

	if (outline) {
		draw_rect(renderer, x, y, w, h, base_color);
		return;
	}

	fill_rect(renderer, x, y, w, h, base_color);

	edge = (s32)((f32)w * fabsf((f32)offset_x - WINDOW_WIDTH / 2) / WINDOW_WIDTH);

	if (offset_x > (WINDOW_WIDTH / 2)) {
		fill_rect(renderer, x, y, edge, h, light_color);
		fill_rect(renderer, x + w - edge, y, edge, h, dark_color);
	} else {
		fill_rect(renderer, x, y, edge, h, dark_color);
		fill_rect(renderer, x + w - edge, y, edge, h, light_color);
	}

	edge = (s32)((f32)h * fabsf((f32)offset_y - WINDOW_HEIGHT / 2) / WINDOW_HEIGHT);

	if (offset_y > (WINDOW_HEIGHT / 2)) {
		fill_rect(renderer, x, y, w, edge, light_color);
		fill_rect(renderer, x, y + h - edge, w, edge, dark_color);
	} else {
		fill_rect(renderer, x, y, w, edge, dark_color);
		fill_rect(renderer, x, y + h - edge, w, edge, light_color);
	}


#if 0
	fill_rect(renderer, x, y, w, h, dark_color);
	fill_rect(renderer, x + edge, y,
		  w - edge, h - edge, light_color);
	fill_rect(renderer, x + edge, y + edge,
		  w - edge * 2, h - edge * 2, base_color);
#endif
}

static void
render_cell(SDL_Renderer *renderer,
	    u8 value, s32 offset_x, s32 offset_y, s32 w, s32 h,
	    b32 outline)
{
	render_cell_(renderer, value, 0xFF, offset_x, offset_y, w, h, outline);
}

static void
render_rect(SDL_Renderer *renderer, struct color color, s32 offset_x, s32 offset_y, s32 w, s32 h, b32 outline)
{
	s32 x = (s32)(round(offset_x - (w / 2.0)));
	s32 y = (s32)(round(offset_y - (h / 2.0)));

	if (outline) {
		draw_rect(renderer, x, y, w, h, color);
		return;
	}

	fill_rect(renderer, x, y, w, h, color);
}

static void
fill_cell_(SDL_Renderer *renderer,
	   u8 value, u8 alpha, s32 x, s32 y, s32 w, s32 h)
{
	render_cell_(renderer, value, alpha, x, y, w, h, 0);
}

static void
fill_cell(SDL_Renderer *renderer,
          u8 value, s32 x, s32 y, s32 w, s32 h)
{
	fill_cell_(renderer, value, 0xFF, x, y, w, h);
}

static void
draw_cell(SDL_Renderer *renderer,
          u8 value, s32 x, s32 y, s32 w, s32 h)
{
	render_cell(renderer, value, x, y, w, h, 1);
}

static void
special_fill_cell_(SDL_Renderer *renderer,
		  u8 value, u8 alpha, s32 x, s32 y, s32 w, s32 h)
{
	if (w < 80) {
		fill_cell_(renderer, value, alpha, x, y, w, h);
		return;
	} else if (w < 120) {
		struct color c = BASE_COLORS[value];
		c.a = alpha;
		render_rect(renderer, c, x, y, w, h, false);
	} else {
		struct color c = DARK_COLORS[value];
		c.a = alpha;
		render_rect(renderer, c, x, y, w, h, false);
	}
}








static void
end_level(struct game_state *game)
{
	game->level_end_t = game->time;

	for (u32 i = 0; i < game->entity_count; ++i) {
		struct entity *entity = game->entities + i;
		if (entity->expiration_t > 0)
			entity->expiration_t = game->level_end_t;
	}


}

static struct game_event *
push_game_event(struct game_state *game)
{
	assert(game->event_count < ARRAY_COUNT(game->events));
	struct game_event *result = game->events + (game->event_count++);
	ZERO_STRUCT(*result);
	return result;
}

static bool
check_suspension(struct entity *e1, struct entity_part *p1, struct entity *e2, struct entity_part *p2)
{
	bool result = !e1->suspended_for_frame && !p1->suspended_for_frame && !e2->suspended_for_frame && !p2->suspended_for_frame;
	return result;
}

static struct game_event *
seed_touch_water(struct game_state *game, struct entity *seed, struct entity *water, struct entity_part *droplet)
{
	struct game_event *e = push_game_event(game);
	e->type = GAME_EVENT_SEED_TOUCH_WATER;

	seed->suspended_for_frame = true;
	droplet->suspended_for_frame = true;
	e->seed_touch_water.e1 = seed->index;
	e->seed_touch_water.e2 = water->index;
	e->seed_touch_water.p2 = droplet->index;

	return e;
}

static bool
try_seed_touch_water(struct game_state *game, struct entity *e1, struct entity_part *p1, struct entity *e2, struct entity_part *p2)
{
	if (!check_suspension(e1, p1, e2, p2))
		return false;

	if ((e1->type & ENTITY_SEED) && (e2->type & ENTITY_WATER))
		return seed_touch_water(game, e1, e2, p2);
	else if ((e1->type & ENTITY_WATER) && (e2->type & ENTITY_SEED))
		return seed_touch_water(game, e2, e1, p1);
	else
		return false;
}


static struct game_event *
worm_eat_food(struct game_state *game, struct entity *worm, struct entity *food, struct entity_part *part)
{
	struct game_event *e = push_game_event(game);
	e->type = GAME_EVENT_WORM_EAT_FOOD;

	food->suspended_for_frame = true;
	part->suspended_for_frame = true;

	e->worm_eat_food.e1 = worm->index;
	e->worm_eat_food.e2 = food->index;
	e->worm_eat_food.p2 = part->index;
	return e;
}

static bool
try_worm_eat_food(struct game_state *game, struct entity *e1, struct entity_part *p1, struct entity *e2, struct entity_part *p2)
{
	if (!check_suspension(e1, p1, e2, p2))
		return false;

	if (e1->type == ENTITY_WORM && (e2->type & ENTITY_FOOD) > 0 && p1->index == 0)
		return worm_eat_food(game, e1, e2, p2);
	else if ((e1->type & ENTITY_FOOD) > 0 && e2->type == ENTITY_WORM && p2->index == 0)
		return worm_eat_food(game, e2, e1, p1);
	else
		return false;
}

static struct game_event *
water_eater_eat_water(struct game_state *game, struct entity *water_eater, struct entity *water, struct entity_part *droplet)
{
	struct game_event *e = push_game_event(game);
	e->type = GAME_EVENT_WATER_EATER_EAT_WATER;

	water_eater->suspended_for_frame = true;
	droplet->suspended_for_frame = true;

	e->water_eater_eat_water.e1 = water_eater->index;
	e->water_eater_eat_water.e2 = water->index;
	e->water_eater_eat_water.p2 = droplet->index;

	return e;
}

static bool
try_water_eater_eat_water(struct game_state *game, struct entity *e1, struct entity_part *p1, struct entity *e2, struct entity_part *p2)
{
	if (!check_suspension(e1, p1, e2, p2))
		return false;

	if (e1->type == ENTITY_WATER_EATER && (e2->type & ENTITY_FOOD) && p1->index == 0)
		return water_eater_eat_water(game, e1, e2, p2);
	else if ((e1->type & ENTITY_FOOD) && e2->type == ENTITY_WATER_EATER && p2->index == 0)
		return water_eater_eat_water(game, e2, e1, p1);
	else
		return false;
}


static struct game_event *
gem_touch_socket(struct game_state *game, struct entity *gem, struct entity *socket, struct entity_part *socket_part) {
	struct game_event *e = push_game_event(game);
	e->type = GAME_EVENT_GEM_TOUCH_SOCKET;

	gem->suspended_for_frame = true;
	socket->suspended_for_frame = true;

	e->gem_touch_socket.e1 = gem->index;
	e->gem_touch_socket.e2 = socket->index;
	e->gem_touch_socket.p2 = socket_part->index;

	return e;
}


static bool
try_gem_touch_socket(struct game_state *game, struct entity *e1, struct entity_part *p1, struct entity *e2, struct entity_part *p2)
{
	if (!check_suspension(e1, p1, e2, p2))
		return false;

	if ((e1->type & ENTITY_GEM) && (e2->type & ENTITY_SOCKET) && p1->index == 0)
		return gem_touch_socket(game, e1, e2, p2);
	else if ((e1->type & ENTITY_SOCKET) && (e2->type & ENTITY_GEM) && p2->index == 0)
		return gem_touch_socket(game, e2, e1, p1);
	else
		return false;
}



static struct entity *
push_entity(struct game_state *game)
{
	u32 index = game->entity_count++;
	struct entity *result = game->entities + index;
	ZERO_STRUCT(*result);
	result->id = ++game->entity_id_seq;
	result->index = index;
	result->seed = (u32)(rand());
	return result;
}

static struct entity_part *
push_entity_part_(struct entity *entity, u16 parent_index)
{
	assert(entity->part_count < ARRAY_COUNT(entity->parts));
	u16 index = entity->part_count++;
	struct entity_part *result = entity->parts + index;
	ZERO_STRUCT(*result);
	result->index = index;
	result->p = v2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	result->parent_index = parent_index;

	if (parent_index != index)
		result->depth = entity->parts[parent_index].depth + 1;

	return result;
}

static struct entity_part *
push_entity_part(struct entity *entity, u16 length, u16 size, u16 color, u16 parent_index)
{
	struct entity_part *p = push_entity_part_(entity, parent_index);
	p->length = length;
	p->size = size;
	p->render_size = p->size;
	p->mass = p->size * p->size;
	p->color = color;
	return p;
}


static void
add_squid_leg(struct entity *entity, u16 parent_index, u16 color, u16 length, u16 spacing, u16 size, f32 stiffness)
{
	struct entity_part *p;

	for (u16 i = 0; i < length; ++i) {
		p = push_entity_part(entity, spacing, size - i, color, parent_index);
		p->stiffness = stiffness;
		parent_index = p->index;
	}

	/* p = add_part(entity); */
	/* p->length = 25; */
	/* p->size = 25; */
	/* p->color = 2; */
	/* p->p = v2(-40, 20); */
	/* p->parent_index = parent_index; */

	/* parent_index = p->index; */

	/* p = add_part(entity); */
	/* p->length = 25; */
	/* p->size = 20; */
	/* p->color = 4; */
	/* p->parent_index = parent_index; */

	/* parent_index = p->index; */

	/* p = add_part(entity); */
	/* p->length = 25; */
	/* p->size = 20; */
	/* p->color = 5; */
	/* p->parent_index = parent_index; */

	/* parent_index = p->index; */

	/* p = add_part(entity); */
	/* p->length = 25; */
	/* p->size = 20; */
	/* p->color = 6; */
	/* p->parent_index = parent_index; */
}

static struct entity *
init_socket(struct entity *entity)
{
	entity->type = ENTITY_SOCKET;
	struct entity_part *p;
	p = push_entity_part(entity, 0, 40, 0, 0);

	/* p = push_entity_part(entity, 0, 10, 80, 0); */
	/* p->passthrough = true; */
	/* p->render_size = 0; */

	/* entity->fixed = true; */
	/* entity->passthrough = true; */
	return entity;
}

static struct entity *
init_seed(struct entity *entity)
{
	entity->type = (ENTITY_FOOD | ENTITY_SEED);
	struct entity_part *p;
	p = push_entity_part(entity, 0, 25, 2, 0);
	p->content_value = 8;
	return entity;
}

static struct entity *
init_liquid(struct entity *entity, u32 type, u8 color, u16 count)
{
	entity->type = type;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 10, color, 0);
	p->render_size = 20;
	p->max_alpha = 0xa0;

	for (u32 i = 0; i < count; ++i) {
		p = push_entity_part(entity, 20, 10, color, 0);
		p->render_size = 20;
		p->max_alpha = 0xa0;
		p->p = add_v2(entity->parts[0].p, v2(20 * cosf((f32)i * 2 * 3.14f / 8), 20 * sinf((f32)i * 2 * 3.14f / 8)));
		p->content_value = 0;
	}

	return entity;
}


static struct entity *
init_water(struct entity *entity, u16 count)
{
	return init_liquid(entity, (ENTITY_FOOD | ENTITY_WATER), 8, count);
}

static struct entity *
init_slime(struct entity *entity, u16 count)
{
	return init_liquid(entity, (ENTITY_FOOD | ENTITY_WATER), 4, count);
}

static struct entity *
init_food(struct entity *entity)
{
	entity->type = ENTITY_FOOD;
	entity->part_count = 0;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 25, 4, 0);
	p->content_value = 16;

	return entity;
}

static struct entity *
init_gem(struct entity *entity, u8 color)
{
	entity->type = ENTITY_GEM;
	struct entity_part *p;
	p = push_entity_part(entity, 0, 25, color, 0);
	return entity;
}


static void
push_worm_tail(struct entity *entity)
{
	if (entity->part_count >= ARRAY_COUNT(entity->parts))
		return;

	u16 parent_index = entity->part_count ? (entity->part_count - 1) : 0;

	struct entity_part *p;
	p = push_entity_part(entity, 25, (u16)(40 - entity->part_count), 2, parent_index);
	p->p = entity->parts[p->index - 1].p;
}

static struct entity *
init_worm(struct entity *entity)
{
	entity->type = ENTITY_WORM;
	entity->part_count = 0;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 40, 2, 0);

	for (u32 i = 1; i < 2; ++i)
		push_worm_tail(entity);

	return entity;
}

static struct entity *
init_water_eater(struct entity *entity)
{
	entity->type = ENTITY_WATER_EATER;
	entity->part_count = 0;
	entity->internal_collisions = true;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 25, 5, 0);
	p->mass = 50 * 50;

	struct entity_part *l1;
	struct entity_part *l2;
	l1 = push_entity_part(entity, 25, 20, 2, 0);
	l2 = push_entity_part(entity, 25, 20, 2, 0);
	l1->internal_collisions = l2->internal_collisions = true;
	l1->stiffness = 2;
	l2->stiffness = 2;
	l1->render_size = l2->render_size = 15;

	add_squid_leg(entity, l1->index, 7, 4, 30, 20, 2);
	add_squid_leg(entity, l2->index, 7, 4, 30, 20, 2);

	return entity;
}

static struct entity *
init_squid(struct entity *entity, u16 leg_count)
{
	struct entity_part *p;

	entity->type = ENTITY_PLAYER;

	p = push_entity_part(entity, 0, 50, 1, 0);
	p->mass = 10000;

	add_squid_leg(entity, 0, 6, leg_count, 20, 25, 0);

	return entity;
}

static bool
find_entity_by_id(const struct game_state *game, u32 entity_id, u32 *index)
{
	u32 i = *index;
	if (i < game->entity_count && game->entities[i].id == entity_id)
		return true;

	for (i = 0; i < game->entity_count; ++i) {
		if (game->entities[i].id == entity_id) {
			*index = i;
			return true;
		}
	}

	return false;
}

static u32
count_entity_of_type(const struct game_state *game, enum entity_type type)
{
	u32 result = 0;
	for (u32 i = 0; i < game->entity_count; ++i)
		result += (game->entities[i].type & type) > 0;
	return result;
}

static u16
compute_entity_part_depth(const struct entity *entity, const struct entity_part *part)
{
	u16 depth = 0;
	const struct entity_part *p = part;
	while (p->parent_index != p->index) {
		depth++;

		p = entity->parts + p->parent_index;
	}

	return depth;
}

static f32
compute_entity_mass(const struct entity *entity)
{
	f32 mass = 0;
	for (u32 i = 0; i < entity->part_count; ++i)
		mass += entity->parts[i].mass;

	return mass;
}

static f32
compute_relative_mass_of_entity_head(const struct entity *entity)
{
	f32 total_mass = compute_entity_mass(entity);
	return entity->parts->mass / total_mass;
}

static void
push_spawn_item(struct game_state *game, enum entity_type type, u32 param, f32 t)
{
	struct spawn_item *item = game->spawn_bag + (game->spawn_bag_count++);
	item->type = type;
	item->param = param;
	item->time = game->spawn_bag_count == 1 ? t : (game->spawn_bag[game->spawn_bag_count - 2].time + t);
}

static void
goto_level(struct game_state *game, u32 level)
{
	game->spawn_bag_count = 0;
	game->spawn_bag_offset = 0;

	f32 level_length = 45;
	if (level == 0) {
		push_spawn_item(game, ENTITY_PLAYER, 0, 0);

		push_spawn_item(game, ENTITY_SOCKET, 3, 0);
		push_spawn_item(game, ENTITY_WORM, 0, 0);

		push_spawn_item(game, ENTITY_WATER, 2, 5);

		push_spawn_item(game, ENTITY_SEED, 0, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 2);

		game->level_instr = "FEED THE WORM";
	} else if (level == 1) {
		push_spawn_item(game, ENTITY_PLAYER, 0, 0);

		push_spawn_item(game, ENTITY_SOCKET, 5, 0);

		push_spawn_item(game, ENTITY_SEED, 0, 1);
		push_spawn_item(game, ENTITY_SEED, 0, 2);

		push_spawn_item(game, ENTITY_WATER, 4, 5);

		push_spawn_item(game, ENTITY_WORM, 0, 10);


		game->level_instr = "WATER THE SEEDS";
	} else if (level == 2) {
		level_length = 75;

		push_spawn_item(game, ENTITY_PLAYER, 4, 0);

		push_spawn_item(game, ENTITY_SOCKET, 3, 0);
		push_spawn_item(game, ENTITY_WORM, 0, 0);

		push_spawn_item(game, ENTITY_SEED, 0, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 2);

		push_spawn_item(game, ENTITY_WATER, 4, 5);

		push_spawn_item(game, ENTITY_SOCKET, 5, 10);

		push_spawn_item(game, ENTITY_WATER, 4, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 0);
		push_spawn_item(game, ENTITY_SEED, 0, 0);

		push_spawn_item(game, ENTITY_WATER, 2, 10);
		push_spawn_item(game, ENTITY_SEED, 0, 0);
		push_spawn_item(game, ENTITY_SEED, 0, 1);

		game->level_instr = "HERD THE WORM";
	} else if (level == 3) {
		level_length = 90;

		push_spawn_item(game, ENTITY_PLAYER, 8, 0);

		push_spawn_item(game, ENTITY_SOCKET, 3, 0);
		push_spawn_item(game, ENTITY_SOCKET, 3, 0);
		push_spawn_item(game, ENTITY_WORM, 0, 0);

		push_spawn_item(game, ENTITY_SEED, 0, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 2);

		push_spawn_item(game, ENTITY_WATER, 8, 5);

		push_spawn_item(game, ENTITY_SOCKET, 5, 10);

		push_spawn_item(game, ENTITY_WATER, 8, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 0);
		push_spawn_item(game, ENTITY_SEED, 0, 0);

		push_spawn_item(game, ENTITY_WATER, 8, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 0);
		push_spawn_item(game, ENTITY_SEED, 0, 1);

		game->level_instr = "HERD THE WORM, AGAIN";
	} else if (level == 4) {
		level_length = 30;

		push_spawn_item(game, ENTITY_PLAYER, 0, 0);

		push_spawn_item(game, ENTITY_SOCKET, 3, 0);
		push_spawn_item(game, ENTITY_WORM, 0, 0);

		push_spawn_item(game, ENTITY_SEED, 0, 5);
		push_spawn_item(game, ENTITY_SEED, 0, 2);

		push_spawn_item(game, ENTITY_WATER_EATER, 0, 5);

		push_spawn_item(game, ENTITY_WATER, 8, 5);

		game->level_instr = "YOU ARE THE SHEPHERD";
	} else {
		game->game_over = true;
		game->level_instr = 0;
	}

	game->current_level = level;
	game->last_level_end_t = game->level_end_t;

	game->tunnel_begin_t = game->level_end_t + 5;
	game->level_begin_t = game->tunnel_begin_t + 10;
	game->level_end_t = game->level_begin_t + level_length;

	game->next_spawn_t = game->tunnel_begin_t;

	game->tunnel_size = 0;

	game->counter_1 = 0;

	game->skip_to_begin = false;
	game->skip_to_end = false;
	game->time_speed_up = 0;
}


static void
begin_game_frame(struct game_state *game)
{
	game->event_count = 0;

	/* NOTE(omid): Clean-up dead entities and initialize the live ones. */

	u32 entity_index = 0;
	while (entity_index < game->entity_count) {
		struct entity *entity = game->entities + entity_index;
		entity->suspended_for_frame = false;

		if (entity->part_count == 0)
			entity->disposed = true;

		if (entity->disposed) {
			game->entities[entity_index] = game->entities[--game->entity_count];
			game->entities[entity_index].index = entity_index;
			continue;
		}

		/* NOTE(omid): Recursively dispose child parts of disposed parents. */
		for (;;) {
			bool found_child_to_dispose = false;
			for (u32 i = 0; i < entity->part_count; ++i) {
				struct entity_part *part = entity->parts + i;
				if (part->disposed)
					continue;

				if (part->parent_index != part->index)
					continue;

				if (entity->parts[part->parent_index].disposed)
					found_child_to_dispose = part->disposed = true;
			}

			if (!found_child_to_dispose)
				break;
		}

		u16 part_index = 0;
		while (part_index < entity->part_count) {
			struct entity_part *part = entity->parts + part_index;
			if (part->disposed) {
				for (u32 i = 0; i < entity->part_count; ++i)
					if (entity->parts[i].parent_index == (entity->part_count - 1))
						entity->parts[i].parent_index = part_index;

				entity->parts[part_index] = entity->parts[--entity->part_count];
				entity->parts[part_index].index = part_index;
			} else {
				/* NOTE(omid): Init part for the new frame. */
				part->suspended_for_frame = false;
				part->a = part->force;
				part->force = v2(0, 0);

				++part_index;
			}
		}

		++entity_index;
	}

	for (u32 i = 0; i < game->entity_count; i++)
		game->entity_index_by_z[i] = i;
}


static bool
spawn_next(struct game_state *game)
{
	u32 c = game->spawn_bag_count - game->spawn_bag_offset;
	if (!c)
		return false;

	struct spawn_item item = game->spawn_bag[game->spawn_bag_offset];
	if (game->time > (item.time + game->tunnel_begin_t)) {
		struct entity *entity = 0;
		switch (item.type) {
		case ENTITY_PLAYER:
			entity = init_squid(push_entity(game), (u16)item.param);
			entity->expiration_t = game->level_end_t;

			game->player_id = entity->id;
			game->player_index = entity->index;
			break;

		case ENTITY_WORM:
			entity = init_worm(push_entity(game));
			entity->expiration_t = game->level_end_t;
			break;

		case ENTITY_SOCKET:
			entity = init_socket(push_entity(game));
			entity->expiration_t = game->level_end_t;
			entity->parts->accept = (u8)item.param;
			break;

		case ENTITY_SEED:
			entity = init_seed(push_entity(game));
			entity->expiration_t = game->level_end_t;
			break;

		case ENTITY_WATER:
			entity = init_water(push_entity(game), (u16)item.param);
			entity->expiration_t = game->level_end_t;
			break;

		case ENTITY_WATER_EATER:
			entity = init_water_eater(push_entity(game));
			entity->expiration_t = game->level_end_t;
			break;
		}

		game->spawn_bag_offset++;
	}

	return true;
}

static bool
check_win_condition(struct game_state *game)
{
	u32 socket_count = 0;
	for (u32 i = 0; i < game->spawn_bag_count; ++i)
		if (game->spawn_bag[i].type == ENTITY_SOCKET)
			socket_count++;

	for (u32 i = 0; i < game->entity_count; ++i)
		if ((game->entities[i].type & ENTITY_SOCKET) && game->entities[i].parts->content && game->entities[i].z >= 1)
			--socket_count;

	return socket_count == 0;
}

static void
run_level_scenario_control(struct game_state *game)
{
	if (game->time < game->level_begin_t)
		game->tunnel_size = WINDOW_WIDTH * (game->time - game->tunnel_begin_t) / (game->level_begin_t - game->tunnel_begin_t);
	else
		game->tunnel_size = WINDOW_WIDTH;

	if (game->time > game->level_end_t) {
		if (check_win_condition(game))
			goto_level(game, game->current_level + 1);
		else
			goto_level(game, game->current_level);
	}

	spawn_next(game);
}

static struct entity *
find_player(struct game_state *game)
{
	if (!find_entity_by_id(game, game->player_id, &game->player_index))
		return 0;

	return game->entities + game->player_index;
}

static void
apply_user_input(struct game_state *game, const struct input_state *input)
{
	if (input->dstart > 0) {
		if (game->time < game->level_begin_t) {
			game->skip_to_begin = true;
			game->time_speed_up = 31;
		} else if (check_win_condition(game) && game->time < game->level_end_t) {
			game->skip_to_end = true;
			game->time_speed_up = 31;
		}
	}

	struct entity *player = find_player(game);
	if (player) {
		struct entity_part *root = player->parts;

		if (input->left)
			root->a.x -= 2;

		if (input->right)
			root->a.x += 2;

		if (input->up)
			root->a.y -= 2;

		if (input->down)
			root->a.y += 2;

		s32 mouse_x, mouse_y;
		u32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
		if (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			struct v2 m = v2((f32)mouse_x, (f32)mouse_y);
			struct v2 d = sub_v2(m, root->p);
			root->a = add_v2(root->a, scale_v2(normalize_v2(d), 2));
		}
	}
}


static bool
target_nearest_entity_of_type(struct game_state *game, struct entity *entity, enum entity_type type, f32 max_dist) {
	bool result = false;

	struct entity_part *head = entity->parts;

	f32 min_dist = 100000000;
	for (u32 i = 0; i < game->entity_count; ++i) {
		struct entity *other = game->entities + i;
		if (other->z < 1)
			continue;

		if (other->expiration_t > 0 && game->time > other->expiration_t)
			continue;

		if (other->type & type) {
			f32 dist = len_v2(sub_v2(head->p, other->parts->p));
			if (dist > max_dist)
				continue;

			if (dist < min_dist) {
				min_dist = dist;
				entity->target_entity_id = other->id;
				entity->target_entity_index = i;
				result = true;
			}
		}
	}

	return result;
}

static bool
entity_should_check_target(struct game_state *game, struct entity *entity)
{
	struct entity_part *head = entity->parts;

	bool check_target = game->time > entity->next_target_check_t;
	if (!check_target && entity->has_target) {
		f32 dist = len_v2(sub_v2(head->p, entity->target));
		if (isnan(dist) || dist < 10)
			check_target = true;
	}

	return check_target;
}

static void
update_roaming_ai(struct entity *entity)
{
	f32 r1 = (f32)(random_int(0, WINDOW_WIDTH / 2));
	f32 r2 = (f32)(random_int(0, WINDOW_HEIGHT / 2));

	f32 a = (f32)random_f32() * 2 * 3.14f;
	entity->target = add_v2(screen_center, v2(r1 * cosf(a), r2 * sinf(a)));
	entity->has_target = true;
}

static void
update_worm_ai(struct game_state *game, struct entity *entity)
{
	if (entity->z < 1)
		return;

	if (!entity_should_check_target(game, entity))
		return;

	if (!target_nearest_entity_of_type(game, entity, ENTITY_FOOD, 200))
		update_roaming_ai(entity);

	entity->pull_of_target = 1.0f;
	entity->next_target_check_t = game->time + 2;
}

static void
update_water_eater_ai(struct game_state *game, struct entity *entity)
{
	if (entity->z < 1)
		return;

	if (!entity_should_check_target(game, entity))
		return;

	if (!target_nearest_entity_of_type(game, entity, ENTITY_FOOD, 600))
		update_roaming_ai(entity);

	entity->pull_of_target = 1.0f;
	entity->next_target_check_t = game->time + 2;
}

static void
update_entity_ai(struct game_state *game)
{
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		struct entity *entity = game->entities + entity_index;
		struct entity_part *head = entity->parts;
		assert(!entity->disposed);
		assert(!entity->suspended_for_frame);
		assert(entity->index == (u16)entity_index);

		/* TODO(omid): Remove this? */
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			struct entity_part *part = entity->parts + part_index;
			assert(!part->disposed);
			assert(!part->suspended_for_frame);
			assert(part->index == (u16)part_index);

			if (entity->expiration_t > 0 && game->time > entity->expiration_t)
				part->color = 0;

		}

		bool reverse_z = entity->expiration_t > 0 && game->time > entity->expiration_t;
		const f32 z_speed = 0.001f;

		while (reverse_z && entity->z > 1) {
			entity->accum_z -= entity->z;
			entity->z -= z_speed;
		}

		if (entity->z < 1 && entity->accum_z > 0) {
			f32 zsqrd = entity->z * entity->z;
			f32 r1 = zsqrd * WINDOW_WIDTH / 2.0f + 50;
			f32 r2 = zsqrd * WINDOW_HEIGHT / 2.0f;

			f32 phi = fmodf((f32)entity->seed, 2 * 3.14f);

			entity->target = add_v2(screen_center, v2(r1 * cosf(2048 * 3.14f * entity->z / entity->accum_z + phi), r2 * sinf(2048 * 3.14f * entity->z / entity->accum_z + phi)));
			entity->pull_of_target = 1 / compute_relative_mass_of_entity_head(entity);
			entity->has_target = true;
			entity->z += reverse_z ? -z_speed : z_speed;
			if (entity->z > 1)
				entity->has_target = false;
		} else if (entity->z < 2) {
			entity->z += reverse_z ? -z_speed : z_speed;
		}

		if (entity->z < 2)
			entity->accum_z += reverse_z ? -entity->z : entity->z;

		if (entity->z < 0) {
			entity->z = 0;
			entity->disposed = true;
		}
		if (entity->accum_z < 0)
			entity->accum_z = 0;


		switch (entity->type) {
		case ENTITY_WORM:
			update_worm_ai(game, entity);
			break;

		case ENTITY_WATER_EATER:
			update_water_eater_ai(game, entity);
			break;
		}

		if (entity->target_entity_id && find_entity_by_id(game, entity->target_entity_id, &entity->target_entity_index)) {
			struct entity *target = game->entities + entity->target_entity_index;
			if (target->disposed || (target->expiration_t > 0 && game->time > target->expiration_t)) {
				entity->has_target = false;
				entity->target_entity_id = 0;
			} else {
				entity->target = target->parts[0].p;
				entity->has_target = true;
			}
		} else if (entity->target_entity_id) {
			entity->has_target = false;
			entity->target_entity_id = 0;
		}

		if (entity->has_target) {
			struct v2 d = normalize_v2(sub_v2(entity->target, head->p));
			head->a = add_v2(head->a, scale_v2(d, entity->pull_of_target));
		}
	}
}

static void
update_spring_physics(struct game_state *game)
{
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		struct entity *entity = game->entities + entity_index;
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			struct entity_part *part = entity->parts + part_index;
			if (part_index != part->parent_index) {
				struct entity_part *parent = entity->parts + part->parent_index;

				struct v2 offset = sub_v2(part->p, parent->p);
				struct v2 ideal = add_v2(parent->p, scale_v2(normalize_v2(offset), part->length));
				struct v2 d = sub_v2(ideal, part->p);

				const f32 K = 10.0f + part->stiffness;
				f32 force = len_v2(d) * K;

				part->a = add_v2(part->a, scale_v2(d, force / part->mass));
				parent->a = add_v2(parent->a, scale_v2(d, -force / parent->mass));
			}
			part->a = sub_v2(part->a, scale_v2(part->v, 0.2f));
		}
	}
}

static void
check_for_collisions_against_entities(struct game_state *game, struct entity *entity, struct entity_part *part, struct v2 *new_p, struct v2 v)
{
	for (u32 other_index = 0; other_index < game->entity_count; ++other_index) {
		if (other_index == entity->index && (!entity->internal_collisions || !part->internal_collisions))
			continue;

		struct entity *other = game->entities + other_index;

		if (other->z < 1)
			continue;

		for (u32 other_part_index = 0; other_part_index < other->part_count; ++other_part_index) {
			if (other_index == entity->index && part->index == other_part_index)
				continue;

			struct entity_part *other_part = other->parts + other_part_index;

			if (other_index == entity->index && !other_part->internal_collisions)
				continue;

			bool do_collision_response = false;

			struct v2 tmp = v;
			struct v2 tmp_p = *new_p;
			if (test_collision_against_box(other_part, part, part->p, &tmp_p, &tmp)) {
				do_collision_response = !entity->passthrough && !other->passthrough && !part->passthrough && !other_part->passthrough;

				if (!entity->suspended_for_frame && !part->suspended_for_frame && !other->suspended_for_frame && !other_part->suspended_for_frame) {
					try_seed_touch_water(game, entity, part, other, other_part);
					try_worm_eat_food(game, entity, part, other, other_part);
					try_water_eater_eat_water(game, entity, part, other, other_part);
					try_gem_touch_socket(game, entity, part, other, other_part);
				}
			}

			if (do_collision_response) {
				*new_p = tmp_p;
				/* struct v2 d = normalize_v2(sub_v2(op->p, part->p)); */
				struct v2 d = normalize_v2(v);
				f32 v1 = dot_v2(v, d);
				f32 v2 = dot_v2(other_part->v, d);
				f32 total_mass = part->mass + other_part->mass;
				f32 dv = v1 - v2;

				f32 f1 = -dv * other_part->mass / total_mass * 2;
				f32 f2 = dv * part->mass / total_mass * 2;

#if 0
				printf("Collision (%u,%u) <=> (%u,%u)\n", entity_index, part_index, other_index, other_part_index);
				printf("\tV1: %f, V2: %f, M1: %f, M2: %f, DV: %f\n", (f64)v1, (f64)v2, (f64)part->mass, (f64)op->mass, (f64)dv);
				printf("\tF1: %f, F2: %f\n", (f64)(f1), (f64)(f2));
#endif
				part->force = add_v2(part->force, scale_v2(d, f1));
				other_part->force = add_v2(other_part->force, scale_v2(d, f2));
			}
		}
	}
}

static void
force_entity_part_within_bounds(struct entity_part *part)
{
	if (isnan(part->p.x))
		part->p.x = 0;
	if (isnan(part->p.y))
		part->p.y = 0;

	if (part->p.x < 0) {
		part->p.x = 0;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
	} else if (part->p.x > WINDOW_WIDTH) {
		part->p.x = WINDOW_WIDTH;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
	}

	if (part->p.y < 0) {
		part->p.y = 0;
		part->v.y = -part->v.y * 0.4f;
		part->a.y = 0;
	} else if (part->p.y > WINDOW_HEIGHT) {
		part->p.y = WINDOW_HEIGHT;
		part->v.y = -part->v.y * 0.4f;
		part->a.y = 0;
	}
}

static void
update_newtonian_physics(struct game_state *game)
{
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		struct entity *entity = game->entities + entity_index;

		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			struct entity_part *part = entity->parts + part_index;
			struct v2 orig_new_v = add_v2(part->v, part->a);
			struct v2 new_v = orig_new_v;
			if (len_v2(new_v) > 10)
				new_v = scale_v2(normalize_v2(new_v), 10);

			struct v2 new_p = add_v2(part->p, new_v);

			if (entity->z > 1)
				check_for_collisions_against_entities(game, entity, part, &new_p, new_v);

			if (len_v2(new_v) > 10)
				new_v = scale_v2(normalize_v2(new_v), 10);

			if (!entity->fixed) {
				part->p = new_p;
				part->v = new_v;
			}

			force_entity_part_within_bounds(part);
		}
	}
}

static void
process_triggered_events(struct game_state *game)
{
	for (u32 event_index = 0; event_index < game->event_count; ++event_index) {
		struct game_event *e = game->events + event_index;
		switch (e->type) {
		case GAME_EVENT_NONE:
			break;

		case GAME_EVENT_SEED_TOUCH_WATER: {
			struct entity *seed = game->entities + e->seed_touch_water.e1;
			struct entity *water = game->entities + e->seed_touch_water.e2;
			struct entity_part *droplet = water->parts + e->seed_touch_water.p2;

			droplet->disposed = true;
			/* seed->disposed = true; */
			/* struct entity *food = push_entity(game); */

			struct v2 p = seed->parts->p;
			init_food(seed);
			seed->parts->p = p;

			/* food->parts->p = seed->parts->p; */
			/* food->parts->v = seed->parts->v; */
		} break;

		case GAME_EVENT_WORM_EAT_FOOD: {
			struct entity *worm = game->entities + e->worm_eat_food.e1;
			struct entity *food = game->entities + e->worm_eat_food.e2;
			struct entity_part *food_part = food->parts + e->worm_eat_food.p2;

			food_part->disposed = true;

			bool worm_full = true;
			bool worm_hydrated = true;

			for (u32 i = 0; i < worm->part_count; ++i) {
				if (!worm->parts[i].content)
					worm_full = false;
				if (!worm->parts[i].hydration)
					worm_hydrated = false;
			}

			if (food_part->content_value) {
				for (u32 i = 0; i < worm->part_count; ++i) {
					if (!worm->parts[i].content) {
						worm->parts[i].content = (u8)food_part->color;
						worm->parts[i].content_value = food_part->content_value;
						worm_full = i == (worm->part_count - 1);
						break;
					}
				}
			} else if (food->type & ENTITY_WATER) {
				for (u32 i = 0; i < worm->part_count; ++i) {
					if (!worm->parts[i].hydration) {
						worm->parts[i].hydration = 1;
						worm->parts[i].color = 4;
						worm_hydrated = i == (worm->part_count - 1);
						break;
					}
				}
			}

			if (worm_full && worm_hydrated) {
#if 0
				u32 score = 0;
				u32 multiplier = 1;
				for (u32 i = 0; i < worm->part_count; ++i) {
					score += multiplier * worm->parts[i].content_value;
					multiplier += worm->parts[i].content_value;
					worm->parts[i].content = 0;
					worm->parts[i].content_value = 0;
					worm->parts[i].hydration = 0;
					worm->parts[i].color = 2;
				}
				game->score += score;
				/* push_worm_tail(worm); */
#else
				if (worm->part_count == 2) {
					u8 poop = 0;
					if (worm->parts[0].content == 2 &&
					    worm->parts[1].content == 2) {
						poop = 3;
					} else if (worm->parts[0].content == 4 &&
						   worm->parts[1].content == 4) {
						poop = 5;
					}

					if (poop) {
						struct entity *gem = init_gem(push_entity(game), poop);
						gem->expiration_t = game->level_end_t;
						gem->z = 1;
						gem->accum_z = 1;

						struct v2 d = sub_v2(worm->parts[worm->part_count - 1].p, worm->parts[worm->part_count - 2].p);
						gem->parts->p = add_v2(worm->parts[worm->part_count - 1].p, d);
						gem->parts->v = scale_v2(d, 100);
					}
				}

				for (u32 i = 0; i < worm->part_count; ++i) {
					worm->parts[i].content = 0;
					worm->parts[i].content_value = 0;
					worm->parts[i].hydration = 0;
					worm->parts[i].color = 2;
				}


#endif



				/* end_level(game); */
			}
			/* push_worm_tail(worm); */

		} break;

		case GAME_EVENT_WATER_EATER_EAT_WATER: {
			struct entity *water = game->entities + e->water_eater_eat_water.e2;
			struct entity_part *droplet = water->parts + e->water_eater_eat_water.p2;

			droplet->disposed = true;
		} break;

		case GAME_EVENT_GEM_TOUCH_SOCKET: {
			struct entity *gem = game->entities + e->gem_touch_socket.e1;
			struct entity *socket = game->entities + e->gem_touch_socket.e2;
			struct entity_part *part = socket->parts + e->gem_touch_socket.p2;

			if (!part->content && part->accept == gem->parts->color) {
				gem->disposed = true;
				part->content = (u8)gem->parts->color;
			}

		} break;

		}
	}
}

static void
update_audio(struct game_state *game)
{
	if (game->time > game->next_note_t) {
		game->next_note_t += 4;
		if (game->note == 40)
			game->note = 80;
		else if (game->note == 80)
			game->note = 60;
		else
			game->note = 40;
	}

	SDL_LockAudioDevice(1);
	game->noise_wave_count = 0;
	u32 wave_index = 0;
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		struct entity *entity = game->entities + entity_index;
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			struct entity_part *part = entity->parts + part_index;

			struct waveform *sine = game->sine_waves + wave_index;
			struct waveform *saw = game->saw_waves + wave_index;

			f32 v = len_v2(part->v);

			f32 sqrt_v = sqrtf(v);

			sine->amp = sqrt_v / 400.0f;
			if (entity->z < 1)
				sine->amp *= entity->z;

			if (sine->amp > 0.25f)
				sine->amp = 0.25f;
			sine->freq = (u16)((roundf(v * 10 / part->size)) * (f32)game->note);

			saw->amp = sqrt_v / 400.0f; /* part->size / 1000.0f; */
			if (entity->z < 1)
				saw->amp *= entity->z;

			if (saw->amp > 0.25f)
				saw->amp = 0.25f;

			saw->freq = (u16)((roundf(v * 100 / part->mass)) * 4 * (f32)game->note); /* (u16)(roundf(len_v2(part->v)) * 40); */

			if (part->audio_gen > 0) {
				struct waveform *noise = game->noise_waves + (game->noise_wave_count++);
				noise->amp = part->audio_gen * entity->z;
				if (entity->z < 1)
					noise->amp *= entity->z;

				part->audio_gen = 0;
			}


			++wave_index;
		}
	}
	game->sine_wave_count = wave_index;
	game->saw_wave_count = wave_index;
	SDL_UnlockAudioDevice(1);
}

static s32
sort_entity_indices_by_z(const void *x, const void *y)
{
	struct entity *entities = global_game->entities; /* thunk; */
	u32 i1 = *(const u32 *)x;
	u32 i2 = *(const u32 *)y;

	if (entities[i1].z < entities[i2].z)
		return -1;
	return 1;
}

static void
make_lightning_to_point(struct game_state *game, SDL_Renderer *renderer, struct entity *e1, struct v2 to)
{
	struct v2 d = sub_v2(to, e1->parts->p);

	u32 count = 0;
	for (u32 i = 0; i < 8; ++i) {
		f32 t = game->time * (f32)(i + 1) / 10.0f;
		f32 r = fmodf(t, 1);
		u8 alpha = e1->z < 1 ? (u8)(e1->z * 0x80) : 0x80;
		struct color c = color((u8)(0xFF * (1 - r)), (u8)((1-r) * 0xFF), 0xFF, alpha);

		while (r < 1) {
			struct v2 p = add_v2(e1->parts->p, scale_v2(d, r));
			struct v2 tangent = normalize_v2(v2(d.y, -d.x));
			p = add_v2(p, scale_v2(tangent, sinf(r * 5 * 3.14f + t) * fmodf(t, 25)));

			fill_rect(renderer, (s32)p.x, (s32)p.y, 8, 8, c);
			r += 0.01f + fmodf(t, 1) * 0.5f;

			++count;
		}
	}

	f32 power = (f32)count / 800;
	if (game->game_over)
		power = 1;

	e1->parts->audio_gen += power * 0.12f;
}

static void
run_audio_test(struct game_state *game)
{
	SDL_LockAudioDevice(1);
	/* game->saw_wave_count = 1; */

	struct track_event e = game->track_events[game->track_event_current];
	f32 t = game->time;
	f32 next_t = game->last_track_event_time + (f32)e.t / 800.0f;
	if (t >= next_t) {
		game->noise_wave_count = 0;
		if (e.a < 0x80) {
			/* u8 channel = game->last_track_event_status - 0x90 + 1; */
			/* struct waveform *saw = game->saw_waves + channel; */
			/* if (channel > game->saw_wave_count) */
			/* 	game->saw_wave_count = channel; */

			if (game->last_track_event_status <= 0x8f) {
				for (u32 wave_index = 0; wave_index < game->saw_wave_count; ++wave_index) {
					struct waveform *wave = game->saw_waves + wave_index;

					if (wave->tag == e.a) {
						game->saw_waves[wave_index] = game->saw_waves[--game->saw_wave_count];
						break;
					}
				}
			} else if (game->last_track_event_status <= 0x9f) {
				struct waveform *wave = game->saw_waves + (game->saw_wave_count++);
				wave->tag = e.a;
				wave->freq = (u16)(440 * pow(2, ((f64)e.a - 69.0) / 12.0));
				wave->amp = e.b / 127.0f;
			}
			/* saw->amp = e.b; */
		} else {
			if (e.a < 0x8f) {
				/* u8 channel = e.a - 0x90 + 1; */
				/* struct waveform *saw = game->saw_waves + channel; */
				/* if (channel > game->saw_wave_count) */
				/* 	game->saw_wave_count = channel; */

				for (u32 wave_index = 0; wave_index < game->saw_wave_count; ++wave_index) {
					struct waveform *wave = game->saw_waves + wave_index;

					if (wave->tag == e.b) {
						game->saw_waves[wave_index] = game->saw_waves[--game->saw_wave_count];
						break;
					}
				}

				/* saw->freq = (u16)(440 * pow(2, ((f64)e.b - 69.0) / 12.0)); */
				/* saw->amp = e.c; */
			} else if (e.a < 0x9f) {
				/* u8 channel = e.a - 0x90 + 1; */
				/* struct waveform *saw = game->saw_waves + channel; */
				/* if (channel > game->saw_wave_count) */
				/* 	game->saw_wave_count = channel; */

				struct waveform *wave = game->saw_waves + (game->saw_wave_count++);
				wave->tag = e.b;
				wave->freq = (u16)(440 * pow(2, ((f64)e.b - 69.0) / 12.0));
				wave->amp = e.c / 127.0f;

				struct waveform *noise = game->noise_waves + (game->noise_wave_count++);
				noise->amp = 0.5f;

			}

			game->last_track_event_status = e.a;
		}

		game->track_event_current = (game->track_event_current + 1) % game->track_event_count;
		game->last_track_event_time = next_t;
	}

	SDL_UnlockAudioDevice(1);
}


static void
update_game(struct game_state *game,
            const struct input_state *input)
{
	run_audio_test(game);

	/* game->sine_wave_count = 1; */
	/* game->sine_waves->freq = 440; */
	/* game->sine_waves->amp = 1; */

#if 0
	begin_game_frame(game);

	/* NOTE(omid): Run level scenario and timings. */
	run_level_scenario_control(game);

	/* NOTE(omid): Apply user input. */
	apply_user_input(game, input);

	/* NOTE(omid): Entity AI. */
	update_entity_ai(game);

	/* NOTE(omid): Spring physics. */
	update_spring_physics(game);

	/* NOTE(omid): Newtonian physics. */
	update_newtonian_physics(game);

	/* NOTE(omid): Triggered events. */
	process_triggered_events(game);

	/* NOTE(omid): Audio generation. */
	update_audio(game);

	qsort(game->entity_index_by_z, game->entity_count, sizeof(u32), sort_entity_indices_by_z);
#endif
}

static void
render_game(struct game_state *game,
            SDL_Renderer *renderer,
            TTF_Font *font,
	    TTF_Font *small_font)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	struct color white = color(0xFF, 0xFF, 0xFF, 0xFF);

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	/* SDL_LockAudioDevice(1); */
	for (u32 i = 0; i < ARRAY_COUNT(game->last_audio_mix); ++i) {
		f32 v = game->last_audio_mix[i];
		f32 c = game->audio_mix_sample[i];
		game->audio_mix_sample[i] = c + (v - c) * 0.5f;
	}

	s32 mix_length = ARRAY_COUNT(game->last_audio_mix);
	f32 width = (f32)WINDOW_WIDTH / (f32)mix_length;
	for (s32 i = 0; i < mix_length; ++i) {
		f32 v = game->audio_mix_sample[i];
		s32 x = (s32)(width * (f32)i);
		s32 h = (s32)(v * WINDOW_HEIGHT / 20.0f) + 1;
		s32 w = (s32)(roundf(width)) + 1;

		for (s32 y = 0; y < 10; ++y) {
			f32 s = y / 9.0f;
			struct color c = color((u8)(s * s * s * 0xFF), (u8)(s * s * 0xFF), (u8)(s * 0xFF), 0xFF);

			fill_rect(renderer, x, WINDOW_HEIGHT / 2 - y * h, w, h, c);
		}
	}
	/* SDL_UnlockAudioDevice(1); */

	SDL_RenderPresent(renderer);
}

static void
render_game_(struct game_state *game,
            SDL_Renderer *renderer,
            TTF_Font *font,
	    TTF_Font *small_font)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	struct color white = color(0xFF, 0xFF, 0xFF, 0xFF);

	f32 elapsed_t = game->time - game->level_begin_t;
	f32 level_progress = elapsed_t / (game->level_end_t - game->level_begin_t);
	if (level_progress < 0)
		level_progress = 0;

	struct v2 o = screen_center; /* v2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2); */
	f32 len_o = len_v2(o) + 100;

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	/* NOTE(omid): Render tunnel. */
	{
		f32 fade_progress = 1.0f;
		if (game->time < game->level_begin_t) {
			f32 tunnel_d = game->level_begin_t - game->tunnel_begin_t;
			fade_progress = (game->time - game->tunnel_begin_t) / tunnel_d;
		}

		SDL_RenderSetScale(renderer, fade_progress, fade_progress);

		f32 initial_r = len_o * level_progress * level_progress;
		f32 r = initial_r;
		f32 a = 0.25f * game->time;
		while (r < game->tunnel_size) {
			struct v2 p = add_v2(o, v2(r * cosf(a), r * sinf(a)));

			u8 max_alpha = (u8)(0xE0 * sqrtf(r / len_o));
			u8 alpha = max_alpha;
			if (game->time < game->level_begin_t)
				alpha = (u8)(max_alpha * fade_progress);

			#if 0
			special_fill_cell_(renderer, (u8)(game->current_level + 1), alpha, (s32)p.x, (s32)p.y, (s32)(r / 5.0f), (s32)(r / 5.0f));
			#else
			struct v2 sp = add_v2(p, scale_v2(screen_center, (1 - fade_progress) / fade_progress));
			u8 c = (u8)(game->current_level + 9) % ARRAY_COUNT(BASE_COLORS);
			special_fill_cell_(renderer, c, alpha, (s32)sp.x, (s32)sp.y, (s32)(r / 5.0f), (s32)(r / 5.0f));
			#endif


			/* r += 0.5f; */
			r += 0.25f + sqrtf(r - initial_r) * 0.01f;
			a += sqrtf(r - initial_r) * 0.1f;
		}

		SDL_RenderSetScale(renderer, 1, 1);
	}

	/* NOTE(omid): Render shadows. */
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		const struct entity *entity = game->entities + entity_index;
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			const struct entity_part *part = entity->parts + part_index;
			const struct entity_part *parent = entity->parts + part->parent_index;

			struct v2 ep = parent->p;
			struct v2 pp = part->p;

			struct v2 from_c = sub_v2(ep, o);
			f32 dist_to_center = len_v2(from_c);
			f32 scale = 2 * dist_to_center / len_o;

			struct v2 offset = scale_v2(normalize_v2(from_c), 30);
			struct v2 shadow = add_v2(pp, offset);

			u8 max_alpha = (u8)(0xE0 * sqrtf(dist_to_center / len_o));
			u8 alpha = max_alpha;
			if (game->time < game->level_begin_t) {
				f32 tunnel_d = game->level_begin_t - game->tunnel_begin_t;
				f32 fade_progress = (game->time - game->tunnel_begin_t) / tunnel_d;
				alpha = (u8)(max_alpha * fade_progress);
			}

			struct color c = color(0x00, 0x00, 0x00, alpha);
			render_rect(renderer, c, (s32)shadow.x, (s32)shadow.y, (s32)(part->render_size * scale), (s32)(part->render_size * scale), false);
		}
	}

	/* NOTE(omid): Render entities. */
	/* for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) { */
	for (u32 sort_list_index = 0; sort_list_index < game->entity_count; ++sort_list_index) {
		u32 entity_index = game->entity_index_by_z[sort_list_index];
		const struct entity *entity = game->entities + entity_index;

		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			const struct entity_part *part = entity->parts + (entity->part_count - part_index - 1);



			struct v2 part_p = part->p;
#if 0
			const struct entity_part *parent = entity->parts + part->parent_index;
			struct v2 parent_p = parent->p;
			struct v2 d = sub_v2(part_p, parent_p);
			u32 chain_count = (u32)(part->length / 20);
#endif

			u16 depth = compute_entity_part_depth(entity, part);
			f32 z = entity->z - depth * 0.02f;
			if (z < 0)
				z = 0;

			if (z > 1) {
#if 0
				for (u32 chain_index = 1; chain_index < chain_count; ++chain_index) {
					f32 r = (f32)chain_index / (f32)chain_count;
					struct v2 p = add_v2(parent_p, scale_v2(d, r));
					fill_cell(renderer, 3, (s32)p.x, (s32)p.y, 12, 12);
				}
#endif

				u8 alpha = part->max_alpha ? part->max_alpha : 0xFF;

				fill_cell_(renderer, (u8)part->color, alpha, (s32)part_p.x, (s32)part_p.y, (s32)part->render_size, (s32)part->render_size);

				if (part->content) {
					fill_cell_(renderer, part->content, alpha, (s32)part_p.x, (s32)part_p.y, 10, 10);
					draw_cell(renderer, 0, (s32)part_p.x, (s32)part_p.y, 10, 10);
				}

				if (part->accept) {
					fill_cell_(renderer, part->accept, 200, (s32)part_p.x, (s32)(part_p.y - part->render_size / 2.0f), (s32)part->render_size, 10);

				}

			} else {
				u8 max_alpha = (u8)(0xE0);
				u8 alpha = (u8)(max_alpha * z);

				SDL_RenderSetScale(renderer, z, z);
				s32 size = (s32)(part->render_size);
				special_fill_cell_(renderer, (u8)(part->color), alpha, (s32)(part_p.x / z), (s32)(part_p.y / z), size, size);
				SDL_RenderSetScale(renderer, 1, 1);
			}
		}
	}


	u32 sockets[MAX_ENTITY_COUNT] = { 0 };
	u32 socket_count = 0;
	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		const struct entity *entity = game->entities + entity_index;
		if (entity->type & ENTITY_SOCKET)
			sockets[socket_count++] = entity_index;
	}

	for (u32 socket_index = 0; socket_index < socket_count; ++socket_index) {
		 struct entity *e1 = game->entities + sockets[socket_index];
		 if (!e1->parts->content)
			continue;

		make_lightning_to_point(game, renderer, e1, screen_center);

		for (u32 other_socket_index = 0; other_socket_index < socket_count; ++other_socket_index) {
			if (other_socket_index == socket_index)
				continue;

			 struct entity *e2 = game->entities + sockets[other_socket_index];

			if (!e2->parts->content)
				continue;

			make_lightning_to_point(game, renderer, e1, e2->parts->p);
		}
	}


	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

	/* NOTE(omid): Render on-screen text. */

	/* draw_string(renderer, font, "LD48 - InvertedMinds", 5, 5, TEXT_ALIGN_LEFT, white); */
#if 0
	draw_string_f(renderer, small_font, 5, 5, TEXT_ALIGN_LEFT, white, "T: %f (%uX)", (f64)game->time, game->time_speed_up + 1);
#endif
	draw_string_f(renderer, small_font, WINDOW_WIDTH, WINDOW_HEIGHT - SMALL_FONT_SIZE, TEXT_ALIGN_RIGHT, white, "A game by Omid Ghavami Zeitooni");

	/* draw_string_f(renderer, font, WINDOW_WIDTH / 2, 0, TEXT_ALIGN_CENTER, white, "SCORE: %u", game->score); */

	if (game->time < game->tunnel_begin_t) {
		f32 fade_in_d = 1;
		f32 fade_in_start_t = game->last_level_end_t;
		f32 fade_in_end_t = fade_in_start_t + fade_in_d;

		f32 fade_out_d = 1;
		f32 fade_out_start_t = game->tunnel_begin_t - fade_out_d;
		f32 fade_out_end_t = game->tunnel_begin_t;

		f32 alpha = 0xFF;
		if (game->time >= fade_in_start_t && game->time <= fade_in_end_t)
			alpha = (0xFF * (game->time - game->last_level_end_t) / fade_in_d);
		if (game->time >= fade_out_start_t && game->time <= fade_out_end_t)
			alpha = (0xFF * (1 - (game->time - fade_out_start_t) / fade_out_d));

		if (alpha > 0xFF)
			alpha = 0xFF;
		else if (alpha < 0)
			alpha = 0;

		if (alpha > 1) {
			struct color c = color(0xFF, 0xFF, 0xFF, (u8)alpha);

			if (game->game_over) {
				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, c, "CONGRATULATIONS, YOU WON!", game->current_level + 1);
			} else {
				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, c, "LEVEL %u", game->current_level + 1);

				if (game->level_instr) {
					draw_string_f(renderer, small_font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 -9 + FONT_SIZE, TEXT_ALIGN_CENTER, c, "%s", game->level_instr);
				}

				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT - FONT_SIZE, TEXT_ALIGN_CENTER, c, "PRESS 'SPACE' TO SKIP");
			}
		}
	}

	if (game->game_over) {
		draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, white, "CONGRATULATIONS, YOU WON!", game->current_level + 1);
	} else if (check_win_condition(game)) {
		draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, white, "COMPLETED", game->current_level + 1);

		draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT - FONT_SIZE, TEXT_ALIGN_CENTER, white, "PRESS 'SPACE' TO SKIP");
	}


#if 0
	s32 y = 5 + SMALL_FONT_SIZE;

	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		const struct entity *entity = game->entities + entity_index;
		draw_string_f(renderer, small_font, 5, y, TEXT_ALIGN_LEFT, white, "E (%u): HAS TARGET? %s [(%f, %f) * %f]", entity_index, entity->has_target ? "YES" : "NO", (f64)entity->target.x, (f64)entity->target.y, (f64)entity->pull_of_target);
		y += SMALL_FONT_SIZE;

		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			const struct entity_part *part = entity->parts + (entity->part_count - part_index - 1);
			draw_string_f(renderer, small_font, 25, y, TEXT_ALIGN_LEFT, white, "E (%u, %u): (%f, %f)", entity_index, part_index, (f64)part->p.x, (f64)part->p.y);
			y += SMALL_FONT_SIZE;
		}
	}
#endif


	SDL_RenderPresent(renderer);
}

static void
mix_audio(void *state, Uint8 *stream, int len)
{
	struct game_state *game = state;

	u32 length = (u32)(len / 4);
	f32 *s = (f32 *)(void *)stream;

	for (u32 i = 0; i < length; ++i) {
		f32 t = (f32)(game->played_audio_sample_count + i) / AUDIO_FREQ;

		f32 mix = 0;
		for (u32 wave_index = 0; wave_index < game->sine_wave_count; ++wave_index) {
			struct waveform *wave = game->sine_waves + wave_index;
			f32 w = sinf(2.0f * 3.14f * wave->freq * t) * wave->amp;
			mix += w;
			/* wave->t += wave->freq; */
		}

		for (u32 wave_index = 0; wave_index < game->saw_wave_count; ++wave_index) {
			struct waveform *wave = game->saw_waves + wave_index;
			f32 w = 0;
			if (!IS_F32_ZERO(wave->amp))
				w = fmodf(wave->amp * wave->freq * t, wave->amp) - wave->amp / 2;

			mix += w;
		}

		for (u32 wave_index = 0; wave_index < game->noise_wave_count; ++wave_index) {
			struct waveform *wave = game->noise_waves + wave_index;
			f32 w = wave->amp * random_f32();

			mix += w;
		}


		if (mix < -1.0f)
			mix = -1.0f;
		else if (mix > 1.0f)
			mix = 1.0f;

		s[i] = mix;

		if (i < ARRAY_COUNT(game->last_audio_mix))
			game->last_audio_mix[i] = mix;
	}

	game->played_audio_sample_count += length;
}


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_AudioDeviceID audio;
static const char *font_name;
static TTF_Font *font;
static TTF_Font *small_font;


static struct input_state input;
static bool quit;

static s32 window_w, window_h, renderer_w, renderer_h;
static f32 default_scale = 1;


static void
update_and_render()
{
	struct game_state *game = global_game;

	for (u32 i = 0; i < (game->time_speed_up + 1); ++i) {
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
			if (e.type == SDL_QUIT)
				quit = true;

		s32 key_count;
		const u8 *key_states = SDL_GetKeyboardState(&key_count);

		if (key_states[SDL_SCANCODE_ESCAPE])
			quit = true;

		struct input_state prev_input = input;

		input.left = key_states[SDL_SCANCODE_LEFT];
		input.right = key_states[SDL_SCANCODE_RIGHT];
		input.up = key_states[SDL_SCANCODE_UP];
		input.down = key_states[SDL_SCANCODE_DOWN];
		input.start = key_states[SDL_SCANCODE_SPACE];

		input.speed_up = key_states[SDL_SCANCODE_PAGEUP];
		input.speed_down = key_states[SDL_SCANCODE_PAGEDOWN];

		input.dleft = (s8)(input.left - prev_input.left);
		input.dright = (s8)(input.right - prev_input.right);
		input.dup = (s8)(input.up - prev_input.up);
		input.ddown = (s8)(input.down - prev_input.down);
		input.dstart = (s8)(input.start - prev_input.start);

		input.dspeed_up = (s8)(input.speed_up - prev_input.speed_up);
		input.dspeed_down = (s8)(input.speed_down - prev_input.speed_down);

#if 0
		if (input.dspeed_up > 0)
			++game->time_speed_up;
		else if (input.dspeed_down > 0 && game->time_speed_up)
			--game->time_speed_up;
#endif
#if 0
		if (input.dspeed_up > 0)
			goto_level(game, game->current_level + 1);
		else if (input.dspeed_down > 0)
			goto_level(game, game->current_level - 1);

#endif


		if (!game->game_over)
			game->time = (f32)game->frame_index * (1.0f / 60);

		if (game->time > game->level_begin_t && game->skip_to_begin) {
			game->skip_to_begin = false;
			game->time_speed_up = 0;
		}

		if (game->time > game->level_end_t && game->skip_to_end) {
			game->skip_to_end = false;
			game->time_speed_up = 0;
		}

#if !defined(__EMSCRIPTEN__)
		if (i == 0) {
			f32 real_time = (f32)(SDL_GetTicks()) / 1000.0f;
			f32 elapsed_since_last_frame = real_time - game->last_frame_real_time;
			if (elapsed_since_last_frame < 16)
				SDL_Delay((u32)(16 - elapsed_since_last_frame));
			game->last_frame_real_time = real_time;
		}
#endif
		game->real_time = (f32)(SDL_GetTicks()) / 1000.0f;
		update_game(game, &input);
		if (i == 0)
			render_game(game, renderer, font, small_font);

		++game->frame_index;
	}
}

static u8 *
read_entire_file(const char* filename, size_t *size_out)
{
    SDL_RWops *stream = SDL_RWFromFile(filename, "rb");
    if (stream == 0)
	    return 0;

    Sint64 size_ = SDL_RWsize(stream);
    if (size_ < 0)
	    return 0;

    size_t size = (size_t)size_;

    u8 *result = (u8*)malloc(size);

    u8 *next = result;
    size_t remaining = size;
    size_t read_count;
    while ((read_count = SDL_RWread(stream, next, 1, remaining))) {
	    next += read_count;
	    remaining -= read_count;
    }
    SDL_RWclose(stream);

    *size_out = size;

    return result;
}

int
main()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;

	if (TTF_Init() < 0)
		return 2;

	srand(120);

	window_w = WINDOW_WIDTH;
	window_h = WINDOW_HEIGHT;

	window = SDL_CreateWindow(
		"LD50 -- InvertedMinds",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		window_w,
		window_h,
		SDL_WINDOW_SHOWN);

	renderer = SDL_CreateRenderer(
		window,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	SDL_GetRendererOutputSize(renderer, &renderer_w, &renderer_h);

	printf("Render Size: %u, %u\n", renderer_w, renderer_h);

	default_scale = (f32)renderer_w / (f32)window_w;
	/* SDL_RenderSetScale(renderer, default_scale, default_scale); */

	font_name = "novem___.ttf";
	font = TTF_OpenFont(font_name, FONT_SIZE);
	small_font = TTF_OpenFont(font_name, SMALL_FONT_SIZE);

	global_game = (struct game_state *)malloc(sizeof(struct game_state));
	ZERO_STRUCT(*global_game);
	/* game->level_end_t = -5; */
	goto_level(global_game, 0);

	ZERO_STRUCT(input);

	SDL_AudioSpec fmt = { 0 };
	fmt.freq = AUDIO_FREQ;
	fmt.format = AUDIO_F32;
	fmt.channels = 1;
	fmt.samples = 1024;
	fmt.callback = mix_audio;
	fmt.userdata = global_game;

	SDL_AudioSpec obt;

	if (SDL_OpenAudio(&fmt, &obt) < 0)
		return 3;

	audio = 1;

	SDL_PauseAudio(0);

	size_t track_data_size;
	u8 *data = read_entire_file("track.imm", &track_data_size);
	u8 *p = data;
	/* FILE *f = fmemopen(data, track_data_size, "rb"); */
	u32 track_event_count = (u32)(track_data_size / 7);
	global_game->track_event_count = track_event_count;
	global_game->track_events = malloc(sizeof(struct track_event) * track_event_count);
	for (u32 i = 0; i < track_event_count; ++i) {
		struct track_event e;
		memcpy(&e.t, p, 4);
		p += 4;
		e.a = *(p++);
		e.b = *(p++);
		e.c = *(p++);
		global_game->track_events[i] = e;
	}

#if defined(__EMSCRIPTEN__)
	emscripten_set_main_loop(update_and_render, 0, 1);
#else
	while (!quit)
		update_and_render();
#endif

	SDL_CloseAudio();


	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();

	return 0;
}
