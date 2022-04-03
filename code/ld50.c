/********************************************************************************
* (C) Copyright 2022 Omid Ghavami Zeitooni. All Rights Reserved                 *
********************************************************************************/



#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>

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
#define MAX_ENTITY_COUNT 512
#define MAX_ENTITY_PART_COUNT 255
#define MAX_PARTICLE_COUNT (1 << 15)
#define MAX_SOUND_COUNT (1 << 15)
#define AUDIO_SAMPLE_COUNT 1024
#define TUNNEL_SEGMENT_COUNT 1024
#define TUNNEL_SEGMENT_THICKNESS 10

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

struct entity_part_owner {
	u32 entity_id;
	u32 entity_index;
	u16 particle_index;
	u16 entity_part_index;
	b16 direct;
	b16 pad;
};

struct entity_part {
	u16 index;
	u16 length;
	u16 width;
	u16 height;
	u16 color;
	u16 parent_index;
	u16 depth;
	b8 disposed;
	b8 die_at_screen_edge: 2;
	b8 immune_to_wall: 2;
	b8 internal_collisions: 2;
	b8 passthrough: 2;
	f32 stiffness;
	f32 mass;
	u8 fire_rate;
	b8 suspended_for_frame;
	u16 max_hp;
	f32 audio_gen;
	f32 hurt;
	u16 hp;
	u16 dmg;
	f32 next_fire_t;
	f32 angle;
	struct v2 p;
	struct v2 v;
	struct v2 a;
	struct v2 force;
	const char *name;
};

enum particle_type {
	PARTICLE_NONE,
	PARTICLE_BULLET = 1,
	PARTICLE_LIGHTNING_GUIDE = (1 << 1),
	PARTICLE_EXPLOSION = (1 << 2),
	PARTICLE_DEBRIS = (1 << 3),
	PARTICLE_FAT_BULLET = (1 << 4),
	PARTICLE_FIREBALL = (1 << 5)
};

struct particle {
	struct entity_part part;
	enum particle_type type: 16;
	u16 owner_index;
	u32 owner_id;
	u16 owner_part_index;
	u16 pad_;
	f32 spawn_t;
	f32 expiration_t;
	u32 pad2_;
};

enum entity_type {
	ENTITY_NONE        = 0,
	ENTITY_PLAYER      = 1,
	ENTITY_ENEMY       = (1 << 1),
	ENTITY_LIGHTNING   = (1 << 2)
};

struct entity {
	u32 id;
	u32 index;
	u32 seed;
	u32 type;
	u8 part_count;
	b8 internal_collisions: 1;
	b8 disposed: 1;
	b8 suspended_for_frame: 1;
	b8 expire: 1;
	u32 pad: 20;
	u32 pad_;
	struct entity_part parts[MAX_ENTITY_PART_COUNT];

	struct v2 target;
	f32 pull_of_target;
	u32 target_entity_index;
	u32 target_entity_id;
	b32 has_target;
	f32 next_target_check_t;
	f32 spawn_t;
	f32 expiration_t;
	f32 next_bullet_t;

	f32 z;

	u32 from_id;
	u32 to_id;
	u16 from_part_index;
	u16 to_part_index;
};

enum waveform_type {
	SINE,
	SAW,
	WHITENOISE
};

struct waveform {
	u16 tag;
	u16 freq;
	f32 amp;
};

struct sound {
	struct waveform wave;
	enum waveform_type type: 2;
	b8 disposed: 1;
	b8 fadeout: 1;
	b8 echo: 1;
	b8 once: 1;
	u16 pad: 10;
	u16 tag;
	f32 play_begin;
	f32 fadeout_begin;
	f32 fadeout_end;
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
	u32 group;
};

struct track_event {
	uint32_t t;
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
};

struct track {
	struct track_event *events;
	u32 event_count;
	u32 current_event;
	f32 last_event_time;
	u8 last_event_status;
	u32 tempo_inverse_scale: 24;
};

struct tunnel_segment {
	u16 left;
	u16 right;
};

enum card_type {
	REPAIR_CARD,
	UPGRADE_HP_CARD,
	UPGRADE_WEAPON_CARD,
	LIGHTNING_CARD,
	FIREBALL_CARD,
	TURRET_CARD
};

struct card {
	enum card_type type;
	u32 param;
	const char *name;
};

struct game_state {
	struct entity entities[MAX_ENTITY_COUNT];
	u32 entity_count;

	u32 entity_index_by_z[MAX_ENTITY_COUNT];

	u32 last_played_track;
	struct particle particles[MAX_PARTICLE_COUNT];
	u32 particle_count;

	u32 player_index;
	u32 player_id;

	struct spawn_item spawn_bag[512];
	u32 spawn_bag_count;
	u32 spawn_bag_offset;

	u32 frame_index;
	f32 time;
	f32 real_time;

	f32 last_frame_real_time;

	f32 last_level_end_t;
	f32 tunnel_begin_t;

	f32 level_begin_t;

	u32 current_level;

	f32 next_note_t;
	u32 note;

	u32 counter_1;

	f32 next_spawn_t;

	u32 spawn_group;

	u32 time_speed_up;
	b16 skip_to_begin;
	b16 skip_to_end;

	struct sound sounds[MAX_SOUND_COUNT];
	u32 sound_count;

	struct game_event events[64];
	u32 event_count;

	u32 entity_id_seq;
	u32 score;
	u32 visible_score;
	u32 pad_;

	b32 game_over;
	b32 card_select_mode;
	b32 did_select_card;
	f32 card_select_t;
	f32 card_select_start_t;

	u32 selected_card;
	u32 card_count;
	f32 tunnel_difficulty;
	struct card cards[3];

	f32 last_audio_mix[AUDIO_SAMPLE_COUNT];
	f32 audio_mix_sample[AUDIO_SAMPLE_COUNT];
	f32 audio_mix_power;

	b32 waiting_for_spawn;

	b32 shield_active;
	f32 shield_energy;

	struct track tracks[8];
	u32 track_count;

	u32 current_tunnel_segment;
	struct tunnel_segment tunnel_segments[TUNNEL_SEGMENT_COUNT];

	f32 current_tunnel_depth;
	f32 next_tunnel_depth;

	u64 played_audio_sample_count;
	const char *level_instr;

	SDL_Texture *temp_texture;
	/* u32 note_wave_number[128]; */
};

struct input_state
{
	u8 left;
	u8 right;
	u8 up;
	u8 down;

	u8 action;
	u8 start;

	u8 speed_up;
	u8 speed_down;

	s8 dleft;
	s8 dright;
	s8 dup;
	s8 ddown;
	s8 dstart;
	s8 daction;

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

static u32
min_u(u32 x, u32 y)
{
	return x < y ? x : y;
}

static u32
max_u(u32 x, u32 y)
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
    struct v2 MinkowskiSize = v2(Obstacle->width + Entity->width, Obstacle->height + Entity->height);

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
fill_rotated_rect(struct game_state *game, SDL_Renderer *renderer, s32 x, s32 y, s32 width, s32 height, f64 angle, struct color color)
{
	SDL_Rect source = { .w = width, .h = height };
	SDL_Rect dest = { .x = x, .y = y, .w = width, .h = height };
	SDL_Point center = { .x = source.w / 2, .y = source.h / 2 };
	SDL_SetTextureColorMod(game->temp_texture, color.r, color.g, color.b);
	SDL_SetTextureAlphaMod(game->temp_texture, color.a);
	SDL_SetTextureBlendMode(game->temp_texture, SDL_BLENDMODE_BLEND);
	SDL_RenderCopyEx(renderer, game->temp_texture, &source, &dest, angle * 180 / 3.14, &center, SDL_FLIP_NONE);
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
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_FreeSurface(surface);

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

	if (false)
		printf("[%d, %d] -> [%d, %d, %d, %d]\n", surface->w, surface->h, rect.x, rect.y, rect.w, rect.h);

	SDL_RenderCopy(renderer, texture, 0, &rect);
#if defined(__EMSCRIPTEN__)
	SDL_RenderDrawPoint(renderer, 0, 0);
#endif

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
fill_cell_c(SDL_Renderer *renderer, s32 offset_x, s32 offset_y, s32 w, s32 h, struct color c)
{
	s32 x = (s32)(round(offset_x - (w / 2.0)));
	s32 y = (s32)(round(offset_y - (h / 2.0)));

	fill_rect(renderer, x, y, w, h, c);
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
	if (w < 40) {
		fill_cell_(renderer, value, alpha, x, y, w, h);
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


static struct sound *
push_sound(struct game_state *game, enum waveform_type type, u16 freq, f32 amp)
{
	if (game->sound_count >= MAX_SOUND_COUNT)
		return 0;

	struct sound *s = game->sounds + (game->sound_count++);
	ZERO_STRUCT(*s);

	s->type = type;
	s->wave.freq = freq;
	s->wave.amp = amp;

	return s;
}

static struct sound *
push_tagged_sound(struct game_state *game, enum waveform_type type, u16 freq, f32 amp, u16 top_tag, u16 child_tag)
{
	if (game->sound_count >= MAX_SOUND_COUNT)
		return 0;

	struct sound *s = game->sounds + (game->sound_count++);
	ZERO_STRUCT(*s);

	s->type = type;
	s->tag = top_tag;
	s->wave.freq = freq;
	s->wave.amp = amp;
	s->wave.tag = child_tag;

	return s;
}


static struct sound *
find_sound_by_tag(struct game_state *game, u16 top_tag, u16 child_tag)
{
	for (u32 i = 0; i < game->sound_count; ++i) {
		struct sound *s = game->sounds + i;
		if (s->tag == top_tag && s->wave.tag == child_tag)
			return s;
	}
	return 0;
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

/* NOTE(omid): For reference. */
#if 0
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
#endif


static struct particle *
push_particle(struct game_state *game, enum particle_type type)
{
	if (game->particle_count >= MAX_PARTICLE_COUNT)
		return 0;

	struct particle *p = game->particles + (game->particle_count++);
	ZERO_STRUCT(*p);
	p->type = type;
	p->spawn_t = game->time;
	p->expiration_t = p->spawn_t + 5;
	return p;
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
	result->spawn_t = game->time;

	game->entity_index_by_z[index] = index;
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
	result->p = v2(WINDOW_WIDTH / 2, 0);
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
	p->width = size;
	p->height = size;
	p->mass = p->width * p->height;
	p->color = color;
	p->immune_to_wall = true;
	p->max_hp = p->hp = 5;
	return p;
}


static void
add_squid_leg(struct entity *entity, u16 parent_index, u16 color, u16 length, u16 spacing, u16 size, f32 stiffness, u16 hp)
{
	struct entity_part *p;

	for (u16 i = 0; i < length; ++i) {
		p = push_entity_part(entity, spacing, size - i, color, parent_index);
		p->stiffness = stiffness;
		p->max_hp = p->hp = hp;
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
init_lightning(struct entity *entity, u32 from_id, u16 from_part_index, u32 to_id, u16 to_part_index, f32 expiration_t)
{
	entity->type = ENTITY_LIGHTNING;
	entity->expiration_t = expiration_t;
	entity->from_id = from_id;
	entity->to_id = to_id;
	entity->from_part_index = from_part_index;
	entity->to_part_index = to_part_index;
	entity->z = 1;
	entity->expire = true;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 0, UINT8_MAX, 0);

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
	p->immune_to_wall = true;
	p->max_hp = p->hp = 5;
}

static struct entity *
init_worm(struct entity *entity)
{
	entity->type = 0;
	entity->part_count = 0;

	struct entity_part *p;
	p = push_entity_part(entity, 0, 40, 2, 0);

	for (u32 i = 1; i < 4; ++i)
		push_worm_tail(entity);

	return entity;
}


static struct entity *
init_enemy(struct entity *entity, u32 difficulty)
{
	entity->type = ENTITY_ENEMY;
	entity->part_count = 0;

	u8 c1, c2, c3;
	c1 = 0; c2 = 0; c3 = 0;
	u16 color_type = (u16)random_int(0, 4);
	switch (color_type) {
	case 0:
		c1 = 7;
		c2 = 2;
		c3 = 5;
		break;
	case 1:
		c1 = 1;
		c2 = 4;
		c3 = 6;
		break;
	case 2:
		c1 = 3;
		c2 = 8;
		c3 = 5;
		break;
	case 3:
		c1 = 6;
		c2 = 9;
		c3 = 1;
		break;
	}

	u16 head_size = (u16)(random_int(30, 100));
	u16 secondary_max_size = head_size / 2;
	u16 secondary_count = (u16)random_int(0, 8);
	if (secondary_max_size < 20)
		secondary_count = 0;

	u16 max_hp = difficulty > UINT16_MAX ? UINT16_MAX : (u16)difficulty;
	u16 hp = (u16)(min_u(head_size, max_hp));
	u32 fire_rate = difficulty / 10;
	if (secondary_count)
		fire_rate /= secondary_count;
	if (fire_rate < 1)
		fire_rate = 1;
	if (fire_rate > UINT8_MAX)
		fire_rate = UINT8_MAX;

	struct entity_part *head = push_entity_part(entity, 0, head_size, c1, 0);
	head->mass *= 100;
	head->max_hp = head->hp = hp;
	head->internal_collisions = true;
	head->dmg = PARTICLE_BULLET;
	head->fire_rate = (u8)fire_rate;
	/* head->dmg = PARTICLE_FIREBALL; */

	/* u16 secondary_count = (u16)random_int(0, 8); */

	/* secondary_count = 0; */

	if (secondary_count) {
		if (random_f32() > 0.8f) {
			/* NOTE(omid): Snake. */
			u16 secondary_size = (u16)(random_int(19, secondary_max_size));

			f32 scale = (f32)secondary_size / (f32)head_size;
			add_squid_leg(entity, head->index, c2, secondary_count, 30, secondary_size, 2, (u16)(scale * hp));
			if (random_f32() > 0.5f) {
				head->dmg |= PARTICLE_FIREBALL;
				head->fire_rate = 3;
			} else {
				head->dmg |= PARTICLE_LIGHTNING_GUIDE;
				head->fire_rate = 50;
			}
		} else {
			for (u16 i = 0; i < secondary_count; ++i) {
				u16 secondary_size = (u16)(random_int(19, secondary_max_size));
				f32 scale = (f32)secondary_size / (f32)head_size;

				struct entity_part *s = push_entity_part(entity, head_size, secondary_size, c2, head->index);
				s->internal_collisions = true;
				s->stiffness = 10;
				s->max_hp = s->hp = (u16)(scale * hp);
				s->dmg = PARTICLE_BULLET;
				s->fire_rate = (u8)fire_rate;
			}
		}
	}

	/* struct entity_part *p; */
	/* p = push_entity_part(entity, 0, 25 + param, c1, 0); */
	/* p->mass = 50 * 50; */
	/* p->hp = 10; */

	/* struct entity_part *l1; */
	/* struct entity_part *l2; */
	/* l1 = push_entity_part(entity, 25, 20 + param, c2, 0); */
	/* l2 = push_entity_part(entity, 25, 20 + param, c2, 0); */
	/* l1->internal_collisions = l2->internal_collisions = true; */
	/* l1->stiffness = 2; */
	/* l2->stiffness = 2; */

	/* add_squid_leg(entity, l1->index, c2, 4, 30, 20, 2); */
	/* add_squid_leg(entity, l2->index, c2, 4, 30, 20, 2); */

	return entity;
}

static struct entity *
init_player(struct entity *entity)
{
	struct entity_part *p;

	entity->type = ENTITY_PLAYER;

	p = push_entity_part(entity, 0, 25, 1, 0);
	p->p.y = WINDOW_HEIGHT;
	p->mass = 100000;
	p->immune_to_wall = false;
	p->hp = 20;
	p->max_hp = 20;
	p->dmg = PARTICLE_BULLET;
	p->fire_rate = 100;
	p->name = "HULL";

	if (true) {
		struct entity_part *l1 = push_entity_part(entity, 40, 20, 0, 0);
		l1->internal_collisions = true;
		l1->stiffness = 2;
		/* l1->mass = 10; */
		l1->dmg = PARTICLE_FAT_BULLET;
		l1->fire_rate = 100;
		l1->name = "TURRET";
		l1->max_hp = l1->hp = 4;
	}

	/* if (true) { */
	/* 	struct entity_part *l1 = push_entity_part(entity, 40, 20, 0, 0); */
	/* 	l1->internal_collisions = true; */
	/* 	l1->stiffness = 2; */
	/* 	l1->mass = 1; */
	/* 	l1->dmg = PARTICLE_FAT_BULLET; */
	/* 	l1->fire_rate = 255; */
	/* 	l1->name = "TURRET"; */
	/* } */


	/* if (true) { */
	/* 	struct entity_part *l1 = push_entity_part(entity, 20, 20, 0, 0); */
	/* 	l1->internal_collisions = true; */
	/* 	l1->stiffness = 2; */
	/* 	l1->mass = 1; */
	/* 	l1->dmg = PARTICLE_BULLET; */
	/* } */

	/* if (true) { */
	/* 	struct entity_part *l1 = push_entity_part(entity, 50, 20, 5, 0); */
	/* 	l1->internal_collisions = true; */
	/* 	l1->stiffness = 2; */
	/* 	l1->mass = 1; */
	/* 	l1->dmg = PARTICLE_BULLET | PARTICLE_FIREBALL; */
	/* } */

	/* { */
	/* 	struct entity_part *l2 = push_entity_part(entity, 40, 20, 9, 0); */
	/* 	l2->internal_collisions = true; */
	/* 	l2->stiffness = 2; */
	/* 	l2->mass = 1; */
	/* 	l2->dmg = PARTICLE_BULLET | PARTICLE_LIGHTNING_GUIDE; */
	/* } */

	/* { */
	/* 	struct entity_part *l2 = push_entity_part(entity, 40, 20, 9, 0); */
	/* 	l2->internal_collisions = true; */
	/* 	l2->stiffness = 2; */
	/* 	l2->mass = 1; */
	/* 	l2->dmg = PARTICLE_BULLET | PARTICLE_LIGHTNING_GUIDE; */
	/* } */

	/* { */
	/* 	struct entity_part *l2 = push_entity_part(entity, 40, 20, 9, 0); */
	/* 	l2->internal_collisions = true; */
	/* 	l2->stiffness = 2; */
	/* 	l2->mass = 1; */
	/* 	l2->dmg = PARTICLE_BULLET | PARTICLE_LIGHTNING_GUIDE; */
	/* } */

	/* add_squid_leg(entity, 0, 6, leg_count, 20, 25, 0); */

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
push_spawn_item(struct game_state *game, enum entity_type type, u32 param, u32 group)
{
	struct spawn_item *item = game->spawn_bag + (game->spawn_bag_count++);
	item->type = type;
	item->param = param;
	item->group = group;
}

static void
spawn_explosion(struct game_state *game, struct entity_part_owner owner, u8 color, struct v2 location, u32 size)
{
	u32 more_p = size;
	while ((more_p--)) {
		struct particle *particle = push_particle(game, PARTICLE_DEBRIS);
		if (!particle)
			break;

		particle->expiration_t = game->time + 1;
		struct entity_part *p = &particle->part;
		particle->type = more_p == (size - 1) ? PARTICLE_EXPLOSION : PARTICLE_BULLET;
		particle->owner_id = owner.entity_id;
		particle->owner_index = (u16)owner.entity_index;
		particle->owner_part_index = owner.entity_part_index;
		p->dmg = 1;
		p->length = 1;
		p->width = (u16)random_int(4, 8);
		p->height = p->width;
		p->mass = p->width * p->height;
		p->color = color;
		p->p = location;
		p->a = v2((random_f32() - 0.5f) * 100 / p->width, (random_f32() - 0.5f) * 100 / p->height);
		p->passthrough = true;
		p->die_at_screen_edge = particle->type == PARTICLE_BULLET;
		p->immune_to_wall = particle->type == PARTICLE_EXPLOSION;
	}
}

static void
spawn_debris(struct game_state *game, struct entity_part_owner owner, u16 color, struct v2 location, u32 count, bool bang)
{
	u32 more_p = count;
	while ((more_p--)) {
		struct particle *particle = push_particle(game, PARTICLE_DEBRIS);
		if (!particle)
			break;

		particle->expiration_t = game->time + 1;
		struct entity_part *p = &particle->part;
		particle->type = bang && more_p == (count - 1) ? PARTICLE_EXPLOSION : PARTICLE_DEBRIS;
		particle->owner_id = owner.entity_id;
		particle->owner_index = (u16)owner.entity_index;
		particle->owner_part_index = owner.entity_part_index;
		p->length = 1;
		p->width = (u16)random_int(4, 8);
		p->height = p->width;
		p->mass = p->width * p->height;
		p->color = color;
		p->p = location;
		p->a = v2((random_f32() - 0.5f) * 100 / p->width, (random_f32() - 0.5f) * 100 / p->height);
		p->passthrough = true;
		p->die_at_screen_edge = particle->type == PARTICLE_DEBRIS;
		p->immune_to_wall = particle->type == PARTICLE_EXPLOSION;
	}
}

static void
populate_cards(struct game_state *game)
{
	game->card_count = 3;

	/* /\* NOTE(omid): Always add repair card. *\/ */
	/* game->cards[0].name = "REPAIR"; */
	/* game->cards[0].type = REPAIR_CARD; */

	for (u32 i = 0; i < game->card_count; ++i) {
		enum card_type card_type;
		for (;;) {
			card_type = (enum card_type)(random_int(0, 6));
			bool duplicate = false;
			for (u32 j = 0; j < i; ++j)
				if (game->cards[j].type == card_type)
					duplicate = true;

			if (!duplicate)
				break;
		}

		game->cards[i].type = card_type;

		const char *name = 0;
		switch (card_type) {
		case REPAIR_CARD:
			name = "REPAIR";
			break;
		case UPGRADE_HP_CARD:
			name = "UPGRADE HP";
			break;
		case UPGRADE_WEAPON_CARD:
			name = "UPGRADE WEAPON";
			break;
		case LIGHTNING_CARD:
			name = "LIGHTNING";
			break;
		case FIREBALL_CARD:
			name = "FIREBALL";
			break;
		case TURRET_CARD:
			name = "TURRET";
			break;
		}

		game->cards[i].name = name;
	}

}

static void
activate_card_select_mode(struct game_state *game)
{
	game->card_select_mode = true;
	game->did_select_card = false;
	populate_cards(game);
	game->selected_card = 0;
	game->card_select_start_t = game->time;
}

static void
complete_card_select_mode(struct game_state *game)
{
	struct card card = game->cards[game->selected_card];

	if (find_entity_by_id(game, game->player_id, &game->player_index) &&
	    game->entities[game->player_index].part_count &&
	    game->entities[game->player_index].part_count < MAX_ENTITY_PART_COUNT) {
		struct entity *player = game->entities + game->player_index;
		switch (card.type) {
		case REPAIR_CARD:
			for (u32 i = 0; i < player->part_count; ++i)
				player->parts[i].hp = player->parts[i].max_hp;
			break;

		case UPGRADE_HP_CARD: {
			u32 part_index = (u32)(random_int(0, player->part_count));
			struct entity_part *p = player->parts + part_index;

			u16 max_hp = p->max_hp;
			u32 next_max = (u32)(max_hp * 1.2f);
			if (next_max == max_hp)
				p->hp = ++p->max_hp;
			else if (next_max < 255)
				p->hp = p->max_hp = (u16)next_max;
			else
				p->hp = p->max_hp = 255;
		} break;

		case UPGRADE_WEAPON_CARD: {
			u32 part_index = (u32)(random_int(0, player->part_count));
			struct entity_part *p = player->parts + part_index;

			u32 next_rate = (u32)(p->fire_rate + 2);
			if (next_rate <= UINT8_MAX)
				p->fire_rate = (u8)next_rate;
			else
				p->fire_rate = UINT8_MAX;
		} break;

		case LIGHTNING_CARD: {
			struct entity_part *p = push_entity_part(player, 40, 20, 9, 0);
			p->internal_collisions = true;
			p->stiffness = 2;
			/* p->mass = 1; */
			p->dmg = PARTICLE_LIGHTNING_GUIDE;
			p->fire_rate = 50;
			p->hp = p->max_hp = 2;
			p->name = "LIGHTNING";
		} break;

		case FIREBALL_CARD: {
			struct entity_part *p = push_entity_part(player, 50, 20, 5, 0);
			p->internal_collisions = true;
			p->stiffness = 2;
			/* p->mass = 1; */
			p->dmg = PARTICLE_FIREBALL;
			p->fire_rate = 5;
			p->hp = p->max_hp = 2;
			p->name = "FIREBALL";
		} break;

		case TURRET_CARD: {
			struct entity_part *p = push_entity_part(player, 40, 20, 0, 0);
			p->internal_collisions = true;
			p->stiffness = 2;
			/* p->mass = 1; */
			p->dmg = PARTICLE_FAT_BULLET;
			p->fire_rate = 100;
			p->hp = p->max_hp = 2;
			p->name = "TURRET";
		} break;
		}
	}
	game->card_select_mode = false;
	game->tunnel_begin_t = game->time + 2.2f;
	game->level_begin_t = game->tunnel_begin_t;
	game->next_spawn_t = game->tunnel_begin_t;
}

static void
goto_level(struct game_state *game, u32 level)
{
	game->spawn_bag_count = 0;
	game->spawn_bag_offset = 0;
	game->spawn_group = 0;

	if (!find_entity_by_id(game, game->player_id, &game->player_index)) {
		game->entity_count = 0;
		game->score = 0;
		push_spawn_item(game, ENTITY_PLAYER, 0, 0);

		if (false)
			activate_card_select_mode(game);
	} else {
		activate_card_select_mode(game);
	}
	game->level_instr = "KICK ASS";

	u32 difficulty = (level + 1) * 10;

	game->tunnel_difficulty = (f32)difficulty * 2;
	if (game->tunnel_difficulty > 150)
		game->tunnel_difficulty = 150;

	/* push_spawn_item(game, ENTITY_SIMPLE, 0, 0); */
	for (u32 i = 0; i < 4; ++i) {
		difficulty += i;
		u32 enemy_count = (u32)(random_int(1, 5));
		u32 difficult_per_enemy = (u32)(difficulty / enemy_count);
		if (difficult_per_enemy < 1)
			difficult_per_enemy = 1;
		for (u32 j = 0; j < enemy_count; ++j)
			push_spawn_item(game, ENTITY_ENEMY, difficult_per_enemy, 1 + i);
	}

#if 0
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
#endif
	game->current_level = level;
	game->last_level_end_t = game->time;

	if (!game->card_select_mode) {
		game->tunnel_begin_t = game->time + 2.2f;
		game->level_begin_t = game->tunnel_begin_t;
		game->next_spawn_t = game->tunnel_begin_t;
	}

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
	if (game->game_over) {
		for (u32 i = 0; i < game->entity_count; ++i) {
			struct entity *entity = game->entities + i;
			for (u32 j = 0; j < entity->part_count; ++j)
				entity->parts[j].disposed = true;
		}
	}

	u32 entity_index = 0;
	while (entity_index < game->entity_count) {
		struct entity *entity = game->entities + entity_index;
		entity->suspended_for_frame = false;

		if (entity->expire && entity->expiration_t < game->time)
			entity->disposed = true;

		if (entity->part_count == 0)
			entity->disposed = true;

		if (entity->disposed) {
			if (entity->id == game->player_id)
				game->game_over = true;

			game->entities[entity_index] = game->entities[--game->entity_count];
			game->entities[entity_index].index = entity_index;
			continue;
		}

		/* NOTE(omid): Recursively dispose child parts of disposed parents. */
		if (false) {
			for (;;) {
				bool found_child_to_dispose = false;
				for (u32 i = 0; i < entity->part_count; ++i) {
					struct entity_part *part = entity->parts + i;
					if (part->disposed)
						continue;

					if (part->parent_index == part->index)
						continue;

					if (entity->parts[part->parent_index].disposed)
						found_child_to_dispose = part->disposed = true;
				}

				if (!found_child_to_dispose)
					break;
			}
		} else {
			for (;;) {
				bool moved_child = false;
				for (u32 i = 0; i < entity->part_count; ++i) {
					struct entity_part *part = entity->parts + i;
					if (part->disposed)
						continue;

					if (part->parent_index == part->index)
						continue;

					if (entity->parts[part->parent_index].disposed) {
						if (part->parent_index == 0)
							part->disposed = true;
						else
							part->parent_index = entity->parts[part->parent_index].parent_index;
						moved_child = true;
					}
				}

				if (!moved_child)
					break;
			}
		}

		u16 part_index = 0;
		while (part_index < entity->part_count) {
			struct entity_part *part = entity->parts + part_index;
			if (part->disposed) {
				struct entity_part_owner owner = { .direct = true, .entity_id = entity->id };
				spawn_debris(game, owner, part->color, part->p, part->width + part->height, true);

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

	u32 particle_index = 0;
	while (particle_index < game->particle_count) {
		struct particle *particle = game->particles + particle_index;
		if (particle->part.disposed) {
			game->particles[particle_index] = game->particles[--game->particle_count];
		} else {
			++particle_index;
		}
	}

	for (u32 i = 0; i < game->entity_count; i++)
		game->entity_index_by_z[i] = i;
}


static bool
spawn_next_group(struct game_state *game)
{
	u32 c = game->spawn_bag_count - game->spawn_bag_offset;
	if (!c)
		return false;

	while (game->spawn_bag_offset < game->spawn_bag_count) {
		struct spawn_item item = game->spawn_bag[game->spawn_bag_offset];
		if (item.group != game->spawn_group)
			break;
		struct entity *entity = 0;
		switch (item.type) {
		case ENTITY_PLAYER:
			entity = init_player(push_entity(game));
			game->player_id = entity->id;
			game->player_index = entity->index;
			break;

		case ENTITY_ENEMY:
			entity = init_enemy(push_entity(game), item.param);
			break;
		}

		game->spawn_bag_offset++;
	}

	game->waiting_for_spawn = false;

	return true;
}

static bool
begin_spawn_next_group(struct game_state *game)
{
	u32 c = game->spawn_bag_count - game->spawn_bag_offset;
	if (!c)
		return false;

	struct spawn_item item = game->spawn_bag[game->spawn_bag_offset];
	game->next_spawn_t = game->time + 2;
	game->waiting_for_spawn = true;
	game->spawn_group = item.group;

	return true;
}

static bool
get_time_of_next_spawn(struct game_state *game, f32 *out)
{
	*out = game->next_spawn_t;
	return game->waiting_for_spawn;
}



static void
run_level_scenario_control(struct game_state *game)
{
	if (game->game_over) {
	} else if (game->card_select_mode) {
		if (game->did_select_card && (game->time - game->card_select_t) >= 1.0f)
			complete_card_select_mode(game);
	} else if (!game->waiting_for_spawn) {
		u32 enemy_count = 0;
		for (u32 i = 0; i < game->entity_count; ++i)
			enemy_count += game->entities[i].type & ENTITY_ENEMY;

		if (!enemy_count)
			if (!begin_spawn_next_group(game))
				goto_level(game, game->current_level + 1);
	} else if (game->next_spawn_t < game->time) {
		spawn_next_group(game);
	}

	for (u32 i = 0; i < 1; ++i) {
		game->current_tunnel_segment = (game->current_tunnel_segment + 1) & (TUNNEL_SEGMENT_COUNT - 1);

		f32 t = (f32)game->current_tunnel_segment / 10;

		f32 depth = game->current_tunnel_depth;
		game->tunnel_segments[game->current_tunnel_segment].left = (u16)((sinf(t) + 2) * depth);
		game->tunnel_segments[game->current_tunnel_segment].right = (u16)((sinf(t + 3.14f) + 2) * depth);

		f32 next_spawn;
		if (game->card_select_mode || (get_time_of_next_spawn(game, &next_spawn) && (next_spawn - game->time) < 1)) { /* < game->level_begin_t) { */
			game->next_tunnel_depth = 50;
		} else if (fabsf(game->current_tunnel_depth - game->next_tunnel_depth) < 0.001f) {
			game->next_tunnel_depth = 100 + random_f32() * game->tunnel_difficulty;
		}
		game->current_tunnel_depth += (game->next_tunnel_depth - game->current_tunnel_depth) * 0.1f;
	}
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
	if (game->game_over) {
		if (input->dstart > 0) {
			game->game_over = false;
			goto_level(game, 0);
		}
	} else if (game->card_select_mode && !game->did_select_card) {
		if (input->dleft > 0) {
			if (game->selected_card == 0)
				game->selected_card = game->card_count - 1;
			else
				game->selected_card = (game->selected_card - 1) % game->card_count;

			struct sound *s = push_sound(game, SAW, 440, 0.5f);
			if (s)
				s->once = true;
		}
		if (input->dright > 0) {
			game->selected_card = (game->selected_card + 1) % game->card_count;
			struct sound *s = push_sound(game, SAW, 440, 0.5f);
			if (s)
				s->once = true;
		}

		if (input->dstart > 0) {
			game->did_select_card = true;
			game->card_select_t = game->time;
		}
	} else if (!game->card_select_mode) {
		if (input->dstart > 0) {
			if (game->time < game->level_begin_t) {
				game->skip_to_begin = true;
				game->time_speed_up = 31;
			}
		}

		struct entity *player = find_player(game);
		if (player) {
			struct entity_part *root = player->parts;

			if (input->left)
				root->a.x -= 8;

			if (input->right)
				root->a.x += 8;

			if (input->up)
				root->a.y -= 8;

			if (input->down)
				root->a.y += 8;

			game->shield_active = input->action;

			s32 mouse_x, mouse_y;
			u32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
			if (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
				struct v2 m = v2((f32)mouse_x, (f32)mouse_y);
				struct v2 d = sub_v2(m, root->p);
				root->a = add_v2(root->a, scale_v2(normalize_v2(d), 2));
			}
		}
	}
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
update_simple_ai(struct game_state *game, struct entity *entity)
{
	/* if (entity->z < 1) */
	/* 	return; */

	if (!entity_should_check_target(game, entity))
		return;

	update_roaming_ai(entity);

	entity->pull_of_target = 5.0f;
	entity->next_target_check_t = game->time + 1;
}


#if 0
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
#endif

static struct particle *
fire_projectile(struct game_state *game, struct entity *entity, struct entity_part *part, u16 type, u16 width, u16 height)
{
	struct particle *particle = push_particle(game, type);
	if (!particle)
		return 0;

	struct entity_part *p = &particle->part;
	particle->type = type;
	particle->owner_id = entity->id;
	particle->owner_index =  (u16)entity->index;
	particle->owner_part_index = part->index;
	p->length = 1;
	p->width = width;
	p->height = height;
	p->mass = p->width * p->height * 100;
	/* p->color = part->dmg & PARTICLE_LIGHTNING_GUIDE ? UINT8_MAX : 2; */
	p->color = 2;
	p->p = part->p;

	if (entity->type & ENTITY_PLAYER) {
		p->v = v2(0, -100);
	} else {
		struct v2 player_p = game->entities[game->player_index].parts->p;
		p->v = scale_v2(normalize_v2(sub_v2(player_p, p->p)), 10);
	}

	if (part->dmg & PARTICLE_FAT_BULLET) {
		p->height = 16;
		p->mass = p->width * p->height * 100;
		p->color = 4;
		p->angle = atan2f(p->v.y, p->v.x) + 3.14f / 2;
	}

	p->passthrough = false;
	p->dmg = 1;
	p->die_at_screen_edge = true;
	p->immune_to_wall = false;

	return particle;
}

static struct particle *
fire_guided_gun(struct game_state *game, struct entity *entity, struct entity_part *part, u16 type)
{
	struct v2 o = part->p;
	f32 min_dist = 100000000;
	struct v2 v = v2(0, 0);

	for (u32 i = 0; i < game->entity_count; ++i) {
		struct entity *e = game->entities + i;
		if (e == entity)
			continue;
		if ((entity->type & ENTITY_ENEMY) && (e->type & ENTITY_PLAYER) == 0)
			continue;

		for (u32 j = 0; j < e->part_count; ++j) {
			struct entity_part *p = e->parts + j;

			struct v2 d = sub_v2(p->p, o);
			f32 dist = len_v2(d);
			if (dist < min_dist) {
				min_dist = dist;
				v = d;
			}
		}
	}

	if (min_dist < 100000000) {
		struct particle *particle = fire_projectile(game, entity, part, PARTICLE_BULLET | type, 8, 8);
		if (!particle)
			return 0;

		particle->part.v = scale_v2(normalize_v2(v), 50);
		particle->part.color = 0;  /* UINT8_MAX; */
		return particle;
	}

	return 0;
}


static void
fire_lightning_gun(struct game_state *game, struct entity *entity, struct entity_part *part)
{
	fire_guided_gun(game, entity, part, PARTICLE_LIGHTNING_GUIDE);
}

static void
fire_fireball(struct game_state *game, struct entity *entity, struct entity_part *part)
{
	struct particle *particle = fire_guided_gun(game, entity, part, PARTICLE_FIREBALL);
	if (!particle)
		return;
	particle->part.width = particle->part.height = 32;
	particle->part.mass = particle->part.width * particle->part.height * 100;
	particle->part.color = 7;
	particle->part.dmg = 10;
}

static void
fire_regular(struct game_state *game, struct entity *entity, struct entity_part *part)
{
	struct particle *particle = fire_projectile(game, entity, part, PARTICLE_BULLET, 8, 8);
	if (!particle)
		return;
}

static void
fire_fat(struct game_state *game, struct entity *entity, struct entity_part *part)
{
	struct particle *particle = fire_projectile(game, entity, part, PARTICLE_BULLET, 8, 8);
	if (!particle)
		return;
	particle->part.height = 16;
	particle->part.mass = particle->part.width * particle->part.height * 100;
	particle->part.color = 4;
	particle->part.dmg = 2;
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

			/* if (entity->expiration_t > 0 && game->time > entity->expiration_t) */
			/* 	part->color = 0; */
			if (part->hurt > 0) {
				part->hurt -= 0.25f;
				if (part->hurt < 0)
					part->hurt = 0;

				part->audio_gen = part->hurt * part->hurt * part->hurt * part->hurt * 0.5f;
			}
		}

		bool reverse_z = (entity->expire && game->time > entity->expiration_t) || game->card_select_mode;
		const f32 z_speed = 0.08f;

		while (reverse_z && entity->z > 1)
			entity->z -= z_speed;

		if (entity->z < 1) {
			entity->z += reverse_z ? -z_speed : z_speed;
		} else if (entity->z < 2) {
			entity->z += reverse_z ? -z_speed : z_speed;
		}

		if (entity->z < 0) {
			entity->z = 0;
			if (entity->expire)
				entity->disposed = true;
		}

		if (!game->card_select_mode) {
			switch (entity->type) {
			case ENTITY_PLAYER:
				if (entity->z < 1) {
					head->p = v2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 200);
					struct v2 d = normalize_v2(sub_v2(v2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + 200), head->p));
					head->a = add_v2(head->a, scale_v2(d, 2000));
				}
				break;

			case ENTITY_ENEMY:
				update_simple_ai(game, entity);
				break;
			}

			for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
				struct entity_part *part = entity->parts + part_index;
				/* if (part->parent_index == part->index) */
				/* 	continue; */

				if (!part->dmg || !part->fire_rate)
					continue;
				if (game->time < part->next_fire_t)
					continue;

				if (part->dmg & PARTICLE_LIGHTNING_GUIDE)
					fire_lightning_gun(game, entity, part);
				else if (part->dmg & PARTICLE_FIREBALL)
					fire_fireball(game, entity, part);
				else if (part->dmg & PARTICLE_FAT_BULLET)
					fire_fat(game, entity, part);
				else if (part->dmg & PARTICLE_BULLET)
					fire_regular(game, entity, part);

				f32 r = (f32)4.25f / part->fire_rate;
				part->next_fire_t = game->time + r;
				/* f32 r = 1.0f / UINT8_MAX; */
				/* part->next_fire_t = game->time + r * (UINT8_MAX - part->fire_rate); */
			}
		} else if (entity->type == ENTITY_PLAYER) {
			struct v2 d = normalize_v2(sub_v2(v2(WINDOW_WIDTH / 2, 0), head->p));
			head->a = add_v2(head->a, scale_v2(d, 1000));
		}

		/* switch (entity->type) { */
		/* case ENTITY_WORM: */
		/* 	update_worm_ai(game, entity); */
		/* 	break; */

		/* case ENTITY_WATER_EATER: */
		/* 	update_water_eater_ai(game, entity); */
		/* 	break; */
		/* } */

		if (entity->target_entity_id && find_entity_by_id(game, entity->target_entity_id, &entity->target_entity_index)) {
			struct entity *target = game->entities + entity->target_entity_index;
			if (target->disposed || (target->expire && game->time > target->expiration_t)) {
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
				if (parent->index)
					parent->a = add_v2(parent->a, scale_v2(d, -force / parent->mass));
			}
			part->a = sub_v2(part->a, scale_v2(part->v, 0.6f));
		}
	}
}


static void
check_for_collisions_against_entities(struct game_state *game, struct entity_part *part, struct entity_part_owner owner, struct v2 *new_p, struct v2 v)
{
	struct entity *entity = 0;
	if (find_entity_by_id(game, owner.entity_id, &owner.entity_index))
		entity = game->entities + owner.entity_index;

	for (u32 other_index = 0; other_index < game->entity_count; ++other_index) {
		struct entity *other = game->entities + other_index;

		if (owner.entity_id == other->id && (!part->internal_collisions))
			continue;

		if (other->z < 1)
			continue;

		if (other->disposed)
			continue;

		if (owner.entity_id != other->id && entity && (entity->type & ENTITY_ENEMY) && (other->type & ENTITY_ENEMY))
			continue;

		for (u32 other_part_index = 0; other_part_index < other->part_count; ++other_part_index) {
			struct entity_part *other_part = other->parts + other_part_index;

			if (other->id == owner.entity_id && (part->index == other_part_index || !other_part->internal_collisions))
				continue;

			if (other_part->disposed)
				continue;

			bool do_collision_response = false;

			struct v2 tmp = v;
			struct v2 tmp_p = *new_p;
			if (test_collision_against_box(other_part, part, part->p, &tmp_p, &tmp)) {
				do_collision_response = !part->passthrough && !other_part->passthrough;

				if (!part->suspended_for_frame && !other->suspended_for_frame && !other_part->suspended_for_frame) {
					if (!owner.direct) {
						if (game->particles[owner.particle_index].type & PARTICLE_BULLET) {
							if (other->id == game->player_id && game->shield_active && game->shield_energy > 0) {
								spawn_debris(game, owner, 9, other_part->p, max_u(part->dmg, 10), false);
							} else {
								if (other_part->hp > part->dmg)
									other_part->hp -= part->dmg;
								else
									other_part->disposed = true;
								other_part->hurt = 1;
								part->disposed = true;

								if (other->id != game->player_id)
									game->score += part->dmg * 10;

								if (!other_part->disposed)
									spawn_debris(game, owner, other_part->color, other_part->p, max_u(part->dmg, 10), false);
								else
									game->score += other_part->max_hp * 100;
							}

							if ((game->particles[owner.particle_index].type & PARTICLE_LIGHTNING_GUIDE)) {
								if (game->entity_count < MAX_ENTITY_COUNT)
									init_lightning(push_entity(game), owner.entity_id, owner.entity_part_index, other->id, (u16)other_part_index, game->time + 0.5f);
							}
						}
					}
				}
			}

			/* do_collision_response = false; */

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

			if (part->disposed)
				return;
		}
	}
}

static bool
check_for_collisions_against_tunnel_(struct game_state *game, struct entity_part *part, struct entity_part_owner owner)
{
	if (owner.direct && game->entities[owner.entity_index].z < 1)
		return false;

	struct v2 p = part->p;

	u32 segment_1 = (u32)(p.y / TUNNEL_SEGMENT_THICKNESS);
	u32 segment_2 = segment_1 + 1;

	struct tunnel_segment s1 = game->tunnel_segments[(game->current_tunnel_segment - segment_1) & (TUNNEL_SEGMENT_COUNT - 1)];
	struct tunnel_segment s2 = game->tunnel_segments[(game->current_tunnel_segment - segment_2) & (TUNNEL_SEGMENT_COUNT - 1)];

	f32 left = p.x - part->width / 2;
	f32 right = p.x + part->width / 2;
	if (left < s1.left)
		return true;
	else if (right > (WINDOW_WIDTH - s1.right))
		return true;
	else if (left < s2.left)
		return true;
	else if (right > (WINDOW_WIDTH - s2.right))
		return true;

	return false;
}

static void
check_for_collisions_against_tunnel(struct game_state *game, struct entity_part *part, struct entity_part_owner owner)
{
	if (check_for_collisions_against_tunnel_(game, part, owner)) {
		if (!part->immune_to_wall)
			part->disposed = true;
	}
}

static void
kill_entity_part_at_bounds(struct entity_part *part)
{
	if (isnan(part->p.x) ||
	    isnan(part->p.y) ||
	    part->p.x < 0 ||
	    part->p.x > WINDOW_WIDTH ||
	    part->p.y < 0 ||
	    part->p.y > WINDOW_HEIGHT)
		part->disposed = true;
}

static void
force_entity_part_within_tunnel(struct game_state *game, struct entity_part *part)
{
	struct v2 p = part->p;

	u32 segment_1 = (u32)(p.y / TUNNEL_SEGMENT_THICKNESS);
	u32 segment_2 = segment_1 + 1;

	struct tunnel_segment s1 = game->tunnel_segments[(game->current_tunnel_segment - segment_1) & (TUNNEL_SEGMENT_COUNT - 1)];
	struct tunnel_segment s2 = game->tunnel_segments[(game->current_tunnel_segment - segment_2) & (TUNNEL_SEGMENT_COUNT - 1)];

	f32 left = p.x - part->width / 2;
	f32 right = p.x + part->width / 2;
	if (left < s1.left) {
		part->p.x = s1.left + part->width / 2;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
	} else if (right > (WINDOW_WIDTH - s1.right)) {
		part->p.x = WINDOW_WIDTH - s1.right - part->width / 2;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
	} else if (left < s2.left) {
		part->p.x = s2.left + part->width / 2;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
	} else if (right > (WINDOW_WIDTH - s2.right)) {
		part->p.x = WINDOW_WIDTH - s2.right - part->width / 2;
		part->v.x = -part->v.x * 0.4f;
		part->a.x = 0;
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
update_newtonian_physics_for_part(struct game_state *game, struct entity_part *part, struct entity_part_owner owner)
{
	struct v2 orig_new_v = add_v2(part->v, part->a);
	struct v2 new_v = orig_new_v;
	if (len_v2(new_v) > 20)
		new_v = scale_v2(normalize_v2(new_v), 20);

	struct v2 new_p = add_v2(part->p, new_v);

	check_for_collisions_against_entities(game, part, owner, &new_p, new_v);
	check_for_collisions_against_tunnel(game, part, owner);

	if (len_v2(new_v) > 20)
		new_v = scale_v2(normalize_v2(new_v), 20);

	part->p = new_p;
	part->v = new_v;

	if (part->die_at_screen_edge)
		kill_entity_part_at_bounds(part);
	else
		force_entity_part_within_bounds(part);

	force_entity_part_within_tunnel(game, part);
}

static void
update_newtonian_physics(struct game_state *game)
{
	for (u16 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		struct entity *entity = game->entities + entity_index;
		struct entity_part_owner owner = { .entity_id = entity->id, .entity_index = entity_index, .direct = true };
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			owner.entity_part_index = (u16)part_index;
			struct entity_part *part = entity->parts + part_index;
			update_newtonian_physics_for_part(game, part, owner);
		}
	}

	for (u16 particle_index = 0; particle_index < game->particle_count; ++particle_index) {
		struct particle *particle = game->particles + particle_index;

		if (game->time > particle->expiration_t)
			particle->part.disposed = true;

		if (particle->part.disposed)
			continue;

		if (particle->type == PARTICLE_EXPLOSION) {
			particle->part.audio_gen = particle->expiration_t - game->time;
		}

		if ((particle->type & PARTICLE_FIREBALL)) {
			u32 more_p = 3;
			while ((more_p--)) {
				struct particle *pp = push_particle(game, PARTICLE_DEBRIS);
				if (!pp)
					return;

				pp->expiration_t = game->time + 1;
				struct entity_part *p = &pp->part;
				pp->type = PARTICLE_DEBRIS;
				pp->owner_id = particle->owner_id;
				pp->owner_index = (u16)particle->owner_index;
				pp->owner_part_index = particle->owner_part_index;
				p->length = 1;
				p->width = (u16)random_int(8, 16);
				p->height = p->width;
				p->mass = p->width * p->height;

				f32 r = random_f32();
				if (r < 0.2f)
					p->color = 0;
				else if (r < 0.4f)
					p->color = 7;
				else
					p->color = 2;

				p->angle = random_f32();
				p->p = particle->part.p;
				p->v = v2((random_f32() - 0.5f) , (random_f32() - 0.5f));
				p->passthrough = true;
				p->die_at_screen_edge = true;
				p->immune_to_wall = false;
			}
		}

		struct entity_part_owner owner = { .entity_id = particle->owner_id, .entity_part_index = particle->owner_part_index, .particle_index = particle_index };
		u32 index;
		if (find_entity_by_id(game, particle->owner_id, &index))
			owner.entity_index = (u16)index;
		update_newtonian_physics_for_part(game, &particle->part, owner);

		if ((particle->type & PARTICLE_FIREBALL) && particle->part.disposed) {
			spawn_explosion(game, owner, 2, particle->part.p, 200);
		}
	}
}

static void
process_triggered_events(struct game_state *game)
{
	if (game->shield_active) {
		game->shield_energy -= 0.05f;
		if (game->shield_energy < 0)
			game->shield_energy = 0;
	} else {
		game->shield_energy += 0.01f;
		if (game->shield_energy > 1)
			game->shield_energy = 1;
	}

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
			seed->disposed = true;
		} break;
		}
	}
}

static void
stop_track(struct game_state *game, u16 index)
{
	if (index > game->track_count)
		return;

	SDL_LockAudioDevice(1);
	for (u32 i = 0; i < game->sound_count; ++i) {
		if (game->sounds[i].tag == index && game->sounds[i].wave.tag)
			game->sounds[i].disposed = true;
	}
	SDL_UnlockAudioDevice(1);
}

static void
play_track(struct game_state *game, u16 index)
{
	if (index > game->track_count)
		return;

	struct track *track = game->tracks + index;

	SDL_LockAudioDevice(1);

	f32 t = (f32)game->played_audio_sample_count / AUDIO_FREQ;

	if (game->last_played_track != index) {
		stop_track(game, (u16)game->last_played_track);
		track->current_event = 0;
		track->last_event_status = 0;
		track->last_event_time = t;
	}
	game->last_played_track = index;

	f32 volume = 0.5f;

	struct track_event e = track->events[track->current_event];
	/* f32 t = game->real_time; */
	/* f32 next_t = track->last_event_time + (f32)e.t / 1000.0f; */

	f32 next_frame_t = track->last_event_time + (f32)e.t / track->tempo_inverse_scale; /* 1200.0f; */

	/* u32 frame = game->frame_index; */
	/* u32 next_frame = track->last_event_frame + e.t / 15; */
	/* if (frame >= next_frame || (next_frame - frame) < 1) { */
	if (t >= next_frame_t) {
		if (e.a < 0x80) {
			if (track->last_event_status <= 0x8f) {
				struct sound *s = find_sound_by_tag(game, index, e.a);
				if (s)
					s->disposed = true;
			} else if (track->last_event_status <= 0x9f) {
				struct sound *s = push_tagged_sound(game, SAW, (u16)(440 * pow(2, ((f64)e.a - 69.0) / 12.0)), e.b / 127.0f * volume, index, e.a);
				if (s)
					s->echo = true;

				/* struct waveform *wave = game->saw_waves + (game->saw_wave_count++); */
				/* wave->expiration_t = 0; */
				/* wave->tag = e.a; */
				/* wave->freq = (u16)(440 * pow(2, ((f64)e.a - 69.0) / 12.0)); */
				/* wave->amp = e.b / 127.0f; */
			}
		} else {
			if (e.a < 0x8f) {
				struct sound *s = find_sound_by_tag(game, index, e.b);
				if (s)
					s->disposed = true;

					/* s->fadeout = true; */
					/* s->fadeout_begin = t; */
					/* s->fadeout_end = t + 0.01f; */

			} else if (e.a < 0x9f) {
				struct sound *s = push_tagged_sound(game, SAW, (u16)(440 * pow(2, ((f64)e.b - 69.0) / 12.0)), e.c / 127.0f * volume, index, e.b);
				if (s)
					s->echo = true;
				struct sound *noise = push_sound(game, WHITENOISE, 0, 0.5f);
				noise->fadeout = true;
				noise->fadeout_begin = noise->fadeout_end = t;
			}

			track->last_event_status = e.a;
		}

		track->current_event = (track->current_event + 1) % track->event_count;
		/* track->last_event_frame = next_frame; */
		track->last_event_time = next_frame_t;
	}

	SDL_UnlockAudioDevice(1);
}


static void
_fft(double complex buf[], double complex out[], int n, int step)
{
	if (step < n) {
		_fft(out, buf, n, step * 2);
		_fft(out + step, buf + step, n, step * 2);

		for (int i = 0; i < n; i += 2 * step) {
			double complex t = cexp(-(double complex)I * 3.14 * i / n) * out[i + step];
			buf[i / 2]     = out[i] + t;
			buf[(i + n)/2] = out[i] - t;
		}
	}
}

static void
fft(double complex buf[], double complex tmp[],  int n)
{
	for (int i = 0; i < n; i++)
		tmp[i] = buf[i];

	_fft(buf, tmp, n, 1);
}

static void
update_audio(struct game_state *game)
{

	SDL_LockAudioDevice(1);
	if (true) {
		for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
			struct entity *entity = game->entities + entity_index;
			for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
				struct entity_part *part = entity->parts + part_index;
				if (part->audio_gen > 0) {
					f32 amp = part->audio_gen * entity->z;
					if (entity->z < 1)
						amp *= entity->z;

					if (amp > 0.5f)
						amp = 0.5f;

					struct sound *s = push_sound(game, WHITENOISE, 0, amp);
					if (s)
						s->once = true;
				}
			}
		}
	}
	for (u32 particle_index = 0; particle_index < game->particle_count; ++particle_index) {
		struct particle *particle = game->particles + particle_index;
		/* struct entity_part *part = &particle->part; */
		if (particle->type == PARTICLE_EXPLOSION) {
			f32 ttl = particle->expiration_t - game->time;
			if (ttl > 0) {
				struct sound *s = push_sound(game, SINE, 70 + (u16)(30 * fmodf(ttl, 1)), ttl * ttl / 2);
				if (s)
					s->once = true;

				s = push_sound(game, SAW, 80 + (u16)(30 * fmodf(ttl, 1)), ttl * ttl / 2);
				if (s)
					s->once = true;

				s = push_sound(game, WHITENOISE, 0, fmodf(ttl, 1) / 2);
				if (s)
					s->once = true;
			}
		} else if (particle->type & PARTICLE_FIREBALL) {
			f32 elapsed = game->time - particle->spawn_t;
			struct sound *s = push_sound(game, SINE, 200 + (u16)(120 * elapsed), 0.5f);
			if (s)
				s->once = true;

			s = push_sound(game, WHITENOISE, 0, fmodf(elapsed, 1) / 2);
			if (s)
				s->once = true;

		}
	}

	SDL_UnlockAudioDevice(1);

	if (false) {
		static const s32 mix_length = ARRAY_COUNT(game->last_audio_mix);
		double complex buf[ARRAY_COUNT(game->last_audio_mix)];
		double complex tmp[ARRAY_COUNT(game->last_audio_mix)];
		for (u32 i = 0; i < mix_length; ++i)
			buf[i] =  game->last_audio_mix[i];

		fft(buf, tmp, mix_length);

		/* f64 length = (f64)AUDIO_SAMPLE_COUNT / AUDIO_FREQ; */
		u32 fft_length = AUDIO_SAMPLE_COUNT / 4;
		for (u32 i = 0; i < fft_length; ++i) {
			f32 v = 2 * (f32)(cabs(buf[i] / AUDIO_SAMPLE_COUNT));
			if (v > 1)
				v = 1;
			if (v < -1)
				v = -1;

			/* f32 v = game->last_audio_mix[i]; */
			f32 c = game->audio_mix_sample[i];
			game->audio_mix_sample[i] = v;
			game->audio_mix_sample[i] = c + (v - c * 0.1f);
		}
	} else {
		game->audio_mix_power = 0;
		for (u32 i = 0; i < AUDIO_SAMPLE_COUNT; ++i) {
			f32 v = game->last_audio_mix[i];
			f32 c = game->audio_mix_sample[i];
			game->audio_mix_power += (game->audio_mix_sample[i] = c + (v - c * 0.1f));
		}
		game->audio_mix_power /= AUDIO_SAMPLE_COUNT;
	}
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

static f32
make_lightning_to_point(__attribute__((unused)) struct game_state *game, SDL_Renderer *renderer, struct v2 from, struct v2 to, f32 z, f32 elapsed, f32 perturbation)
{
	struct v2 d = sub_v2(to, from);

	u32 count = 0;
	for (u32 i = 0; i < 8; ++i) {
		/* f32 t = elapsed * (f32)(i + 1) / 10.0f; */
		f32 t = elapsed * (f32)(i + 1);
		f32 r = fmodf(t, 1);
		u8 alpha = z < 1 ? (u8)(z * 0x80) : 0x80;
		struct color c = color((u8)(0xFF * (1 - r)), (u8)((1-r) * 0xFF), 0xFF, alpha);

		while (r < 1) {
			struct v2 p = add_v2(from, scale_v2(d, r));
			struct v2 tangent = normalize_v2(v2(d.y, -d.x));
			p = add_v2(p, scale_v2(tangent, sinf(r * 5 * 3.14f + t + perturbation * 3.14f) * fmodf(t, 25)));

			fill_cell_c(renderer, (s32)p.x, (s32)p.y, 8, 8, c);
			r += 0.01f + fmodf(t, 1) * 0.5f;

			++count;
		}
	}

	f32 power = (f32)count / 400;

	return power;
	/* e1->parts->audio_gen += power * 0.12f; */
}

static void
update_game(struct game_state *game,
            const struct input_state *input)
{
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

}

static void
render_audio_spectrum(struct game_state *game,
		      SDL_Renderer *renderer)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	/* SDL_LockAudioDevice(1); */

	static const s32 mix_length = ARRAY_COUNT(game->last_audio_mix);
	double complex buf[ARRAY_COUNT(game->last_audio_mix)];
	double complex tmp[ARRAY_COUNT(game->last_audio_mix)];
	for (u32 i = 0; i < mix_length; ++i)
		buf[i] =  game->last_audio_mix[i];

	fft(buf, tmp, mix_length);

	/* f64 length = (f64)AUDIO_SAMPLE_COUNT / AUDIO_FREQ; */
	u32 fft_length = AUDIO_SAMPLE_COUNT / 4;
	for (u32 i = 0; i < fft_length; ++i) {
		f32 v = 2 * (f32)(cabs(buf[i] / AUDIO_SAMPLE_COUNT));
		if (v > 1)
			v = 1;
		if (v < -1)
			v = -1;

		/* f32 v = game->last_audio_mix[i]; */
		f32 c = game->audio_mix_sample[i];
		game->audio_mix_sample[i] = v;
		game->audio_mix_sample[i] = c + (v - c * 0.1f);
	}

	f32 width = (f32)WINDOW_WIDTH / (f32)fft_length;
	for (s32 i = 0; i < (s32)fft_length; ++i) {
		f32 v = game->audio_mix_sample[i];
		s32 x = (s32)(width * (f32)i);
		s32 h = (s32)(v * WINDOW_HEIGHT / 20.0f) + 1;
		s32 w = (s32)(roundf(width)) + 1;

		for (s32 y = 0; y < 10; ++y) {
			f32 s = (f32)y / 9.0f;
			struct color c = color((u8)(s * s * s * 0xFF), (u8)(s * s * 0xFF), (u8)(s * 0xFF), 0xFF);

			fill_rect(renderer, x, WINDOW_HEIGHT / 2 - y * h, w, h, c);
		}
	}

	/* SDL_UnlockAudioDevice(1); */

	SDL_RenderPresent(renderer);
}

static void
render_entity_part(struct game_state *game,
		   SDL_Renderer *renderer,
		   const struct entity_part *part,
		   f32 z,
		   f32 scale)
{
	struct v2 part_p = part->p;
#if 0
	const struct entity_part *parent = entity->parts + part->parent_index;
	struct v2 parent_p = parent->p;
	struct v2 d = sub_v2(part_p, parent_p);
	u32 chain_count = (u32)(part->length / 20);
#endif

	u8 c = (u8)part->color;
	if (c == UINT8_MAX)
		return;

	if (z >= 1) {
#if 0
		for (u32 chain_index = 1; chain_index < chain_count; ++chain_index) {
			f32 r = (f32)chain_index / (f32)chain_count;
			struct v2 p = add_v2(parent_p, scale_v2(d, r));
			fill_cell(renderer, 3, (s32)p.x, (s32)p.y, 12, 12);
		}
#endif

		u8 alpha = 0xFF;


		u32 segment_1 = (u32)(part_p.y / TUNNEL_SEGMENT_THICKNESS);
		u32 segment_2 = segment_1 + 1;

		struct tunnel_segment s1 = game->tunnel_segments[(game->current_tunnel_segment - segment_1) & (TUNNEL_SEGMENT_COUNT - 1)];
		struct tunnel_segment s2 = game->tunnel_segments[(game->current_tunnel_segment - segment_2) & (TUNNEL_SEGMENT_COUNT - 1)];

		f32 left = part_p.x - part->width / 2;
		f32 right = part_p.x + part->width / 2;
		if (left < s1.left || right > (WINDOW_WIDTH - s1.right))
			c = 3;
		else if (left < s2.left || right > (WINDOW_WIDTH - s2.right))
			c = 3;

		fill_cell_(renderer, c, alpha, (s32)part_p.x, (s32)part_p.y, (s32)part->width, (s32)part->height);

		if (part->hurt > 0) {
			/* fill_rect(renderer, (s32)part_p.x, (s32)part_p.y, (s32)part->width, (s32)part->height, color(0xff, 0xff, 0xff, (u8)(part->hurt * 0xff))); */
			fill_cell_c(renderer, (s32)part_p.x, (s32)part_p.y, (s32)part->width, (s32)part->height, color(0xff, 0xff, 0xff, 0xff));
			/* fill_cell_(renderer, 3, (u8)(part->hurt * 0xff), (s32)part_p.x, (s32)part_p.y, (s32)part->width, (s32)part->height); */
		}

		/* if (part->content) { */
		/* 	fill_cell_(renderer, part->content, alpha, (s32)part_p.x, (s32)part_p.y, 10, 10); */
		/* 	draw_cell(renderer, 0, (s32)part_p.x, (s32)part_p.y, 10, 10); */
		/* } */

		/* if (part->accept) { */
		/* 	fill_cell_(renderer, part->accept, 200, (s32)part_p.x, (s32)(part_p.y - part->render_size / 2.0f), (s32)part->render_size, 10); */
		/* } */
	} else if (z > 0) {
		u8 max_alpha = (u8)(0xE0);
		u8 alpha = (u8)(max_alpha * z);

		SDL_RenderSetScale(renderer, z, z);
		special_fill_cell_(renderer, c, alpha, (s32)(part_p.x / z), (s32)(part_p.y / z), (s32)(part->width), (s32)(part->height));
		SDL_RenderSetScale(renderer, scale, scale);
	}
}

static void
swap_u8s(u8 *array, u32 i, u32 j)
{
	u8 tmp = array[i];
	array[i] = array[j];
	array[j] = tmp;
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

	/* f32 elapsed_t = game->time - game->level_begin_t; */

	struct v2 o = screen_center; /* v2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2); */
	f32 len_o = len_v2(o) + 100;

	f32 scale = 1 + game->audio_mix_power * 0.24f;

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_RenderSetScale(renderer, scale, scale);

	/* NOTE(omid): Render tunnel. */
	if (true) {
		f32 fade_progress = 1.0f;
		if (game->time < game->level_begin_t) {
			f32 tunnel_d = game->level_begin_t - game->tunnel_begin_t;
			fade_progress = (game->time - game->tunnel_begin_t) / tunnel_d;
		}
		fade_progress = 1.0f;

		u32 visible_count = WINDOW_HEIGHT / TUNNEL_SEGMENT_THICKNESS;

		for (u32 i = 0; i < visible_count; ++i) {
			u32 segment_index = (game->current_tunnel_segment - i) & (TUNNEL_SEGMENT_COUNT - 1);
			struct tunnel_segment segment = game->tunnel_segments[segment_index];
			u8 base_color = 0;
			/* struct color dark = DARK_COLORS[(u8)(game->current_level + 1) % ARRAY_COUNT(DARK_COLORS)]; */
			struct color accent = LIGHT_COLORS[(u8)(game->current_level + 1) % ARRAY_COUNT(LIGHT_COLORS)];
			u8 pows[3] = { 2, 2, 2 };
			/* if (accent.r > accent.g && accent.r > accent.b) { */
			/* 	pows[0] = 1; */
			/* 	if (accent.g > accent.b) { */
			/* 		pows[1] = 2; */
			/* 		pows[2] = 3; */
			/* 	} else { */
			/* 		pows[1] = 3; */
			/* 		pows[2] = 2; */
			/* 	} */
			/* } else if (accent.r > accent.g) { */
			/* 	pows[0] = 2; */
			/* 	pows[1] = 3; */
			/* 	pows[2] = 1; */
			/* } else if (accent.r > accent.b) { */
			/* 	pows[0] = 2; */
			/* 	pows[1] = 1; */
			/* 	pows[2] = 3; */
			/* } else if (accent.g > accent.b) { */
			/* 	pows[0] = 3; */
			/* 	pows[1] = 1; */
			/* 	pows[2] = 2; */
			/* } else { */
			/* 	pows[0] = 3; */
			/* 	pows[1] = 2; */
			/* 	pows[2] = 1; */
			/* } */

			f32 r = 0;
			f32 w = 20;
			while (r < segment.left) {
				w = ((f32)segment.left - r) / 4.0f;
				if (w < 20)
					w = 20;

				u8 max_alpha = (u8)(0xE0 * sqrtf(r / len_o));
				u8 alpha = max_alpha;
				if (game->time < game->level_begin_t)
					alpha = (u8)(max_alpha * fade_progress);

				struct v2 sp = v2(r, (f32)TUNNEL_SEGMENT_THICKNESS * (f32)i);
				struct color color = BASE_COLORS[base_color];
				color.a = alpha;
				/* render_rect(renderer, color, (s32)sp.x, (s32)sp.y, (s32)w, TUNNEL_SEGMENT_THICKNESS, false); */
				special_fill_cell_(renderer, base_color, alpha, (s32)sp.x, (s32)sp.y, (s32)w, TUNNEL_SEGMENT_THICKNESS);
				r += w;
			}

			if (true) {
				f32 v = fabsf(game->audio_mix_sample[segment_index % AUDIO_SAMPLE_COUNT]);
				s32 y = (s32)(TUNNEL_SEGMENT_THICKNESS * i);
				s32 h = TUNNEL_SEGMENT_THICKNESS; /* (s32)(v * WINDOW_HEIGHT / 20.0f) + 1; */
				f32 total_width = v * 50;
				f32 segment_width = total_width / 10;

				for (s32 x = 0; x < 10; ++x) {
					f32 s = (f32)x / 9.0f;
					/* struct color c = color((u8)(s * s * s * accent.r), (u8)(s * s * accent.g), (u8)(s * accent.b), 0xA0); */

					f32 sr = powf(s, pows[0]);
					f32 sg = powf(s, pows[1]);
					f32 sb = powf(s, pows[2]);
					struct color c = color((u8)(sr * accent.r), (u8)(sg * accent.g), (u8)(sb * accent.b), 0x80);
					fill_rect(renderer, (s32)(segment.left + (f32)x * segment_width), y, (s32)(segment_width), h, c);
				}
			}

			r = 0;
			while (r < segment.right) {
				w = (segment.right - r) / 4.0f;
				if (w < 20)
					w = 20;


				u8 max_alpha = (u8)(0xE0 * sqrtf(r / len_o));
				u8 alpha = max_alpha;
				if (game->time < game->level_begin_t)
					alpha = (u8)(max_alpha * fade_progress);
				alpha = 0xff;

				struct v2 sp = v2(WINDOW_WIDTH - r, (f32)TUNNEL_SEGMENT_THICKNESS * (f32)i);
				struct color color = BASE_COLORS[base_color];
				color.a = alpha;
				special_fill_cell_(renderer, base_color, alpha, (s32)sp.x, (s32)sp.y, (s32)w, TUNNEL_SEGMENT_THICKNESS);
				r += w;
			}

			if (true) {
				f32 v = fabsf(game->audio_mix_sample[segment_index % AUDIO_SAMPLE_COUNT]);
				s32 y = (s32)(TUNNEL_SEGMENT_THICKNESS * i);
				s32 h = TUNNEL_SEGMENT_THICKNESS; /* (s32)(v * WINDOW_HEIGHT / 20.0f) + 1; */
				f32 total_width = v * 50;
				f32 segment_width = total_width / 10;

				for (s32 x = 0; x < 10; ++x) {
					f32 s = (f32)x / 9.0f;
					/* struct color c = color((u8)(s * s * s * accent.r), (u8)(s * s * accent.g), (u8)(s * accent.b), 0x80); */
					f32 sr = powf(s, pows[0]);
					f32 sg = powf(s, pows[1]);
					f32 sb = powf(s, pows[2]);
					struct color c = color((u8)(sr * accent.r), (u8)(sg * accent.g), (u8)(sb * accent.b), 0x80);
					fill_rect(renderer, (s32)(WINDOW_WIDTH - segment.right - (f32)(x + 1) * segment_width), y, (s32)(segment_width), h, c);
				}
			}

		}
	}

	/* NOTE(omid): Render entities. */
	if (true) {
		for (u32 sort_list_index = 0; sort_list_index < game->entity_count; ++sort_list_index) {
			u32 entity_index = game->entity_index_by_z[sort_list_index];
			struct entity *entity = game->entities + entity_index;

			for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
				const struct entity_part *part = entity->parts + (entity->part_count - part_index - 1);
				f32 z = entity->z;
				render_entity_part(game, renderer, part, z, scale);
			}

			if (entity->type == ENTITY_LIGHTNING) {
				u32 from_entity_index, to_entity_index;
				if (find_entity_by_id(game, entity->from_id, &from_entity_index) &&
				    find_entity_by_id(game, entity->to_id, &to_entity_index)) {
					struct entity *e1 = game->entities + from_entity_index;
					struct entity *e2 = game->entities + to_entity_index;

					if (entity->from_part_index < e1->part_count &&
					    entity->to_part_index < e2->part_count) {
						struct entity_part *p1 = e1->parts + entity->from_part_index;
						struct entity_part *p2 = e2->parts + entity->to_part_index;

						f32 r = ((f32)entity->seed / (f32)RAND_MAX);
						f32 t = (game->time - entity->spawn_t) / 3 + 1.75f * 4;
						f32 ttl = entity->expiration_t - game->time;
						f32 z = ttl * 10;
						f32 power = make_lightning_to_point(game, renderer, p1->p, p2->p, z, t, r);
						entity->parts->audio_gen = power * 0.24f; /* * 0.12f; */
					}
				}
			}
		}

		for (u32 particle_index = 0; particle_index < game->particle_count; ++particle_index) {
			f32 ttl = game->particles[particle_index].expiration_t - game->time;
			if (ttl < 0)
				continue;

			struct entity_part *part = &game->particles[particle_index].part;
			struct v2 part_p = part->p;
			u8 c = (u8)part->color;
			if (c == UINT8_MAX)
				continue;

			struct color color = BASE_COLORS[c];
			if (ttl < 1)
				color.a = (u8)(ttl * 0xff);
			fill_rotated_rect(game, renderer, (s32)part_p.x, (s32)part_p.y, (s32)part->width, (s32)part->height, (f64)part->angle, color);

			/* if (ttl > 0 && ttl < 1) */
			/* 	render_entity_part(game, renderer, &game->particles[particle_index].part, ttl, scale); */
			/* else */
			/* 	render_entity_part(game, renderer, &game->particles[particle_index].part, 1, scale); */
		}
	}

	if (game->shield_active && game->shield_energy > 0) {
		if (find_entity_by_id(game, game->player_id, &game->player_index)) {
			struct entity *player = game->entities + game->player_index;
			if (player->part_count) {
				struct entity_part *p = player->parts;

				fill_cell_(renderer, 9, 0x80, (s32)p->p.x, (s32)p->p.y, p->width * 2, p->height * 2);
			}
		}
	}

	/* SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); */
	SDL_RenderSetScale(renderer, 1, 1);

	for (u32 entity_index = 0; entity_index < game->entity_count; ++entity_index) {
		const struct entity *entity = game->entities + entity_index;
		for (u32 part_index = 0; part_index < entity->part_count; ++part_index) {
			const struct entity_part *part = entity->parts + (entity->part_count - part_index - 1);
			if (part->hurt > 0) {
				u16 hp = part->hp / 2;
				s32 x = (s32)(part->p.x - (hp * 7) / 2);
				s32 y = (s32)(part->p.y - part->height / 2 - 9);
				for (u16 i = 0; i < hp; ++i)
					fill_rect(renderer, (s32)(x + i * 7), y, 5, 7, color(0xff, 0xff, 0xff, 0xff));
			}
		}
	}


	if (game->card_select_mode && game->card_select_start_t < game->time) {
		struct color c = BASE_COLORS[0];
		struct color selected_c = BASE_COLORS[1];
		u32 card_count = game->card_count;

		s32 outer_padding = 200;
		s32 total_width = WINDOW_WIDTH - 2 * outer_padding;
		s32 width = 200;
		s32 total_padding = total_width - width * (s32)card_count;
		s32 padding = total_padding / 4;
		s32 height = 300;
		s32 top = (WINDOW_HEIGHT - height) / 2;

		s32 x = outer_padding + padding;
		for (u32 i = 0; i < card_count; ++i) {
			u8 alpha = 0xff;
			f32 elapsed_since_start = game->time - game->card_select_start_t;
			if (game->did_select_card) {
				f32 ttl = (game->card_select_t + 1) - game->time;
				if (ttl < 0)
					ttl = 0;
				if (ttl > 1)
					ttl = 1;
				alpha = (u8)(0xff * ttl);
			} else if (elapsed_since_start < 0.5f) {
				alpha = (u8)(0xff * elapsed_since_start * 2);
			}
			c.a = alpha;
			selected_c.a = alpha;

			fill_rect(renderer, x, top, width, height, game->selected_card == i ? selected_c : c);

			if (game->did_select_card && game->selected_card == i) {
				f32 end = game->card_select_t + 1;
				f32 elapsed = (game->time - game->card_select_t);
				f32 progress = fmodf(elapsed / (end - game->card_select_t), 1);
				u16 step = ((u16)(progress * 100) / 10) * 10;

				fill_rect(renderer, x, top, width, height, color(0xff, 0xff, 0xff, alpha));

				struct sound *s = push_sound(game, SINE, step * 10, 0.5f);
				if (s)
					s->once = true;
			}

			if (!game->did_select_card || game->selected_card == i)
				draw_string(renderer, font, game->cards[i].name, x + width / 2, top + height / 2 - FONT_SIZE / 2, TEXT_ALIGN_CENTER, color(0xff, 0xff, 0xff, alpha));
			x += padding + width;
		}
	}

	if (find_entity_by_id(game, game->player_id, &game->player_index)) {
		s32 y = 5 + SMALL_FONT_SIZE;
		struct entity *player = game->entities + game->player_index;
		struct color c = color(0xff, 0xff, 0xff, 0xff);

		draw_string_f(renderer, small_font, 25, y, TEXT_ALIGN_LEFT, white, "SHIELD");
		y += SMALL_FONT_SIZE + 2;
		u16 shield = (u16)(32 * game->shield_energy);
		for (u16 i = 0; i < shield; ++i)
			fill_rect(renderer, 25 + i * 12, y, 10, SMALL_FONT_SIZE, c);
		y += SMALL_FONT_SIZE + 3;

		for (u32 part_index = 0; part_index < player->part_count; ++part_index) {
			struct entity_part *part = player->parts + part_index;
			if (part->name)
				draw_string_f(renderer, small_font, 25, y, TEXT_ALIGN_LEFT, white, "%s", part->name);
			y += SMALL_FONT_SIZE + 2;
			for (u16 i = 0; i < part->hp; ++i)
				fill_rect(renderer, 25 + i * 12, y, 10, SMALL_FONT_SIZE, c);
			y += SMALL_FONT_SIZE + 3;
		}
	}

	/* NOTE(omid): Render on-screen text. */

	/* draw_string(renderer, font, "LD48 - InvertedMinds", 5, 5, TEXT_ALIGN_LEFT, white); */
#if 0
	draw_string_f(renderer, small_font, 5, 5, TEXT_ALIGN_LEFT, white, "T: %f (%uX)", (f64)game->time, game->time_speed_up + 1);
#endif
	draw_string_f(renderer, small_font, WINDOW_WIDTH, WINDOW_HEIGHT - SMALL_FONT_SIZE, TEXT_ALIGN_RIGHT, white, "odyssjii");

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
				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, c, "GAME OVER", game->current_level + 1);
				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT - FONT_SIZE, TEXT_ALIGN_CENTER, c, "PRESS 'ENTER' TO TRY AGAIN");
			} else {
				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, c, "LEVEL %u", game->current_level + 1);

				if (game->level_instr) {
					draw_string_f(renderer, small_font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 -9 + FONT_SIZE, TEXT_ALIGN_CENTER, c, "%s", game->level_instr);
				}

				draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT - FONT_SIZE, TEXT_ALIGN_CENTER, c, "PRESS 'ENTER' TO SKIP");
			}
		}
	}

	if (game->game_over) {
		draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 9, TEXT_ALIGN_CENTER, white, "GAME OVER", game->current_level + 1);
		draw_string_f(renderer, font, WINDOW_WIDTH / 2, WINDOW_HEIGHT - FONT_SIZE, TEXT_ALIGN_CENTER, white, "PRESS 'ENTER' TO TRY AGAIN");
	}

	u32 score_delta = 0;
	if (game->visible_score < game->score)
		score_delta = (u32)(ceilf((f32)(game->score - game->visible_score) * 0.1f));
	else if (game->visible_score > game->score)
		game->visible_score = game->score;

	game->visible_score += score_delta;

	if (score_delta) {
		f32 z = 1 + (f32)score_delta / 100;
		if (z > 2)
			z = 2;
		SDL_RenderSetScale(renderer, z, z);
		draw_string_f(renderer, font, (s32)(WINDOW_WIDTH / 2 / z), (s32)((5 + SMALL_FONT_SIZE) / z), TEXT_ALIGN_CENTER, white, "%u", game->visible_score);
		SDL_RenderSetScale(renderer, 1, 1);
	} else {
		draw_string_f(renderer, font, WINDOW_WIDTH / 2, 5 + SMALL_FONT_SIZE, TEXT_ALIGN_CENTER, white, "%u", game->visible_score);
	}

	if (false) {
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
	}

	if (false) {
		s32 y = 5 + SMALL_FONT_SIZE;
		u32 visible_count = WINDOW_HEIGHT / TUNNEL_SEGMENT_THICKNESS;

		for (u32 i = 0; i < visible_count; ++i) {
			u32 segment_index = (game->current_tunnel_segment - i) & (TUNNEL_SEGMENT_COUNT - 1);
			struct tunnel_segment segment = game->tunnel_segments[segment_index];
			draw_string_f(renderer, small_font, 25, y, TEXT_ALIGN_LEFT, white, "SEGMENT: %u, %u", segment.left, segment.right);
			y += SMALL_FONT_SIZE;
		}
	}

	SDL_RenderPresent(renderer);
}

static void
mix_audio(void *state, Uint8 *stream, int len)
{
	struct game_state *game = state;

	u32 length = (u32)(len / 4);
	f32 *s = (f32 *)(void *)stream;

	if (game->game_over)
		play_track(game, 1);
	else if (game->card_select_mode)
		play_track(game, 2);
	else
		play_track(game, 0);

	f32 global_t = (f32)(game->played_audio_sample_count) / AUDIO_FREQ;
	u32 sound_index;

	sound_index = 0;
	while (sound_index < game->sound_count) {
		struct sound *sound = game->sounds + sound_index;
		if (sound->fadeout && sound->fadeout_end < global_t)
			sound->disposed = true;

		if (sound->disposed && sound->echo) {
			struct sound *echo = push_sound(game, sound->type, sound->wave.freq, sound->wave.amp * 0.5f);
			if (echo) {
				echo->echo = echo->wave.amp > 0.1f;
				echo->fadeout = true;
				echo->fadeout_end = global_t + 0.1f;
			}
		}

		if (sound->disposed) {
			game->sounds[sound_index] = game->sounds[--game->sound_count];
			continue;
		}

		if (sound->once)
			sound->disposed = true;

		++sound_index;
	}

	for (u32 i = 0; i < length; ++i) {
		f32 t = (f32)(game->played_audio_sample_count + i) / AUDIO_FREQ;

		f32 mix = 0;
		for (sound_index = 0; sound_index < game->sound_count; ++sound_index) {
			struct sound sound = game->sounds[sound_index];

			if (sound.play_begin > global_t)
				continue;

			f32 w = 0;
			switch (sound.type) {
			case SINE:
				w = sinf(2.0f * 3.14f * sound.wave.freq * t) * sound.wave.amp;
				break;

			case SAW:
				if (!IS_F32_ZERO(sound.wave.amp))
					w = fmodf(sound.wave.amp * sound.wave.freq * t, sound.wave.amp) - sound.wave.amp / 2;
				break;

			case WHITENOISE:
				w = sound.wave.amp * (2 * random_f32() - 1);
				break;
			}

			if (sound.fadeout && sound.fadeout_begin > global_t && (sound.fadeout_end > sound.fadeout_begin)) {
				f32 fade = (global_t - sound.fadeout_begin) / (sound.fadeout_end - sound.fadeout_begin);
				w *= fade;
			}

			mix += w;
		}

		if (mix < -1.0f)
			mix = -1.0f;
		else if (mix > 1.0f)
			mix = 1.0f;

		s[i] = mix;

		if (i < ARRAY_COUNT(game->last_audio_mix)) {
			f32 window = 1;
			/* if (i < 100) */
			/* 	window = (f32)i / 100.0f; */
			/* else if (i > (length - 100)) */
			/* 	window = (f32)(length - i) / 100.0f; */

			game->last_audio_mix[i] = mix * window * window;
		}
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
		input.start = key_states[SDL_SCANCODE_RETURN];
		input.action = key_states[SDL_SCANCODE_SPACE];

		input.speed_up = key_states[SDL_SCANCODE_PAGEUP];
		input.speed_down = key_states[SDL_SCANCODE_PAGEDOWN];

		input.dleft = (s8)(input.left - prev_input.left);
		input.dright = (s8)(input.right - prev_input.right);
		input.dup = (s8)(input.up - prev_input.up);
		input.ddown = (s8)(input.down - prev_input.down);
		input.dstart = (s8)(input.start - prev_input.start);
		input.daction = (s8)(input.action - prev_input.action);

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

		game->time = (f32)game->frame_index * (1.0f / 60);

		if (game->time > game->level_begin_t && game->skip_to_begin) {
			game->skip_to_begin = false;
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

static struct track *
load_track(struct game_state *game, const char *filename)
{
	size_t track_data_size;
	u8 *data = read_entire_file(filename, &track_data_size);
	u8 *p = data;
	u32 track_event_count = (u32)(track_data_size / 7);
	u32 track_index = game->track_count++;
	struct track *track = game->tracks + track_index;
	track->tempo_inverse_scale = 1200;
	track->event_count = track_event_count;
	track->events = malloc(sizeof(struct track_event) * track_event_count);
	for (u32 i = 0; i < track_event_count; ++i) {
		struct track_event e;
		memcpy(&e.t, p, 4);
		p += 4;
		e.a = *(p++);
		e.b = *(p++);
		e.c = *(p++);
		track->events[i] = e;
	}
	free(data);

	return track;
}

int
main()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;

	if (TTF_Init() < 0)
		return 2;

#if defined(__EMSCRIPTEN__)
	srand(emscripten_random() * RAND_MAX);
#else
	srand((u32)time(0));
#endif

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

	/* SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN); */

	SDL_GetRendererOutputSize(renderer, &renderer_w, &renderer_h);

	/* printf("Render Size: %u, %u\n", renderer_w, renderer_h); */

	default_scale = (f32)renderer_w / (f32)window_w;
	/* SDL_RenderSetScale(renderer, default_scale, default_scale); */

	font_name = "novem___.ttf";
	font = TTF_OpenFont(font_name, FONT_SIZE);
	small_font = TTF_OpenFont(font_name, SMALL_FONT_SIZE);

	global_game = (struct game_state *)malloc(sizeof(struct game_state));
	ZERO_STRUCT(*global_game);
	/* game->level_end_t = -5; */
	goto_level(global_game, 0);

	load_track(global_game, "track.imm");
	load_track(global_game, "track-2.imm");
	load_track(global_game, "track-3.imm")->tempo_inverse_scale = 600;
	load_track(global_game, "track-4.imm");

	ZERO_STRUCT(input);

	global_game->temp_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 128, 128);
	SDL_SetRenderTarget(renderer, global_game->temp_texture);
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_RenderClear(renderer);
	SDL_SetRenderTarget(renderer, 0);

	SDL_AudioSpec fmt = { 0 };
	fmt.freq = AUDIO_FREQ;
	fmt.format = AUDIO_F32;
	fmt.channels = 1;
	fmt.samples = AUDIO_SAMPLE_COUNT;
	fmt.callback = mix_audio;
	fmt.userdata = global_game;

	SDL_AudioSpec obt;

	if (SDL_OpenAudio(&fmt, &obt) < 0)
		return 3;

	audio = 1;

	SDL_PauseAudio(0);

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
