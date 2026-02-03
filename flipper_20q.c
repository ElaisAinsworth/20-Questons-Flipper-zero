#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* ================= CONFIG ================= */
#define MAX_QUESTIONS 20
#define POOL_SIZE 6
#define CONFIRM_THRESHOLD 6
#define MIN_QUESTIONS_BEFORE_GUESS 3
#define WIN_DISPLAY_MS 1500

/* ================= OBJECTS ================= */
typedef struct {
    const char* name;
    const char* category;
    int score;
    bool eliminated; // mark eliminated guesses
} Guess;

static Guess pool[POOL_SIZE] = {
    {"Flipper Zero", "Handheld multi-tool", 0, false},
    {"TV Remote", "Handheld IR device", 0, false},
    {"Car Key Fob", "Portable RF device", 0, false},
    {"RFID Card", "Access device", 0, false},
    {"Smartphone", "Smart handheld device", 0, false},
    {"Game Controller", "Interactive controller", 0, false},
};

/* ================= QUESTIONS ================= */
static const char* questions[MAX_QUESTIONS] = {
    "Is it electronic?",
    "Does it use batteries?",
    "Is it portable?",
    "Does it emit RF?",
    "Does it have buttons?",
    "Is it used for access?",
    "Is it handheld?",
    "Does it connect to a phone?",
    "Does it have a screen?",
    "Is it for gaming?",
    "Is it security related?",
    "Does it require charging?",
    "Is it used daily?",
    "Does it interact wirelessly?",
    "Is it programmable?",
    "Is it battery powered?",
    "Is it a common device?",
    "Is it a consumer gadget?",
    "Does it control something?",
    "Does it clone signals?"
};

/* ================= WEIGHTS ================= */
static const int8_t yes_weights[MAX_QUESTIONS][POOL_SIZE] = {
    {3,0,0,0,2,1},
    {2,1,1,1,3,0},
    {2,2,2,1,2,3},
    {2,0,3,0,1,0},
    {2,3,1,0,2,3},
    {1,0,2,3,0,1},
    {2,1,3,2,1,2},
    {1,0,0,0,3,1},
    {1,0,0,0,3,0},
    {0,0,0,0,0,3},
    {1,0,0,3,0,0},
    {1,0,0,0,2,0},
    {2,2,1,0,2,1},
    {1,0,1,0,1,2},
    {1,0,0,0,1,0},
    {1,1,1,0,2,1},
    {1,1,0,0,2,1},
    {1,0,0,0,2,1},
    {2,0,0,0,1,2},
    {1,0,1,0,0,0}
};

static const int8_t no_weights[MAX_QUESTIONS][POOL_SIZE] = {
    {-3,0,0,0,-2,-1},
    {-2,-1,-1,-1,-3,0},
    {-2,-2,-2,-1,-2,-3},
    {-2,0,-3,0,-1,0},
    {-2,-3,-1,0,-2,-3},
    {-1,0,-2,-3,0,-1},
    {-2,-1,-3,-2,-1,-2},
    {-1,0,0,0,-3,-1},
    {-1,0,0,0,-3,0},
    {0,0,0,0,0,-3},
    {-1,0,0,-3,0,0},
    {-1,0,0,0,-2,0},
    {-2,-2,-1,0,-2,-1},
    {-1,0,-1,0,-1,-2},
    {-1,0,0,0,-1,0},
    {-1,-1,-1,0,-2,-1},
    {-1,-1,0,0,-2,-1},
    {-1,0,0,0,-2,-1},
    {-2,0,0,0,-1,-2},
    {-1,0,-1,0,0,0}
};

/* ================= STATE ================= */
typedef struct {
    size_t q_index;
    bool finished;
    bool confirm_guess;
    bool win_display;
    uint32_t win_start;
    size_t best_guess;
    bool running;
} AppState;

/* ================= UTIL ================= */
static void reset_state(AppState* s){
    s->q_index = 0;
    s->finished = false;
    s->confirm_guess = false;
    s->win_display = false;
    s->win_start = 0;
    s->best_guess = 0;
    s->running = true;
    for(size_t i=0;i<POOL_SIZE;i++){
        pool[i].score = 0;
        pool[i].eliminated = false;
    }
}

static void update_scores(size_t q,bool yes){
    for(size_t i=0;i<POOL_SIZE;i++){
        if(!pool[i].eliminated)
            pool[i].score += yes ? yes_weights[q][i] : no_weights[q][i];
    }
}

static void find_best_guess(AppState* s){
    int max_score=-999;
    size_t best=0;
    for(size_t i=0;i<POOL_SIZE;i++){
        if(!pool[i].eliminated && pool[i].score>max_score){
            max_score = pool[i].score;
            best = i;
        }
    }
    s->best_guess = best;
    if(s->q_index >= MIN_QUESTIONS_BEFORE_GUESS && max_score >= CONFIRM_THRESHOLD)
        s->confirm_guess = true;
}

/* ================= UI ================= */
static void draw_callback(Canvas* canvas, void* ctx){
    AppState* s = ctx;
    canvas_clear(canvas);

    if(s->win_display){
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas,6,20,"I guessed it!");
        canvas_draw_str(canvas,6,40,"You win!");
        return;
    }

    if(s->confirm_guess){
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas,6,12,"Are you thinking of:");
        canvas_draw_str(canvas,6,30,pool[s->best_guess].name);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas,6,50,"<No        Yes>");
        return;
    }

    if(s->finished){
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas,6,12,"Out of guesses!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas,6,30,"Press BACK to restart");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas,6,12,"Flipper 20Q");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas,6,30,questions[s->q_index]);
    canvas_draw_str(canvas,6,50,"<No   ?   Yes>");
}

/* ================= INPUT ================= */
static void input_callback(InputEvent* event, void* ctx){
    AppState* s = ctx;
    if(event->type != InputTypeShort) return;

    if(s->win_display) return; // ignore input during win display

    if(s->confirm_guess){
        if(event->key == InputKeyRight){
            // Correct guess: show win and restart after delay
            s->win_display = true;
            s->win_start = furi_get_tick();
            pool[s->best_guess].eliminated = true;
            s->confirm_guess = false;
        } else if(event->key == InputKeyLeft){
            // Wrong guess: eliminate it and continue
            pool[s->best_guess].eliminated = true;
            s->confirm_guess = false;
            s->q_index++;
        }
        if(s->q_index >= MAX_QUESTIONS) s->finished = true;
        return;
    }

    if(s->finished){
        if(event->key == InputKeyBack) reset_state(s);
        return;
    }

    if(event->key == InputKeyLeft) update_scores(s->q_index,false);
    else if(event->key == InputKeyRight) update_scores(s->q_index,true);
    else if(event->key == InputKeyBack) s->running = false;

    s->q_index++;
    find_best_guess(s);

    if(s->q_index >= MAX_QUESTIONS) s->finished = true;
}

/* ================= MAIN ================= */
int32_t flipper_20q_app(void* p){
    UNUSED(p);
    srand(furi_get_tick());
    AppState state;
    reset_state(&state);

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, &state);
    view_port_input_callback_set(vp, input_callback, &state);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    while(state.running){
        view_port_update(vp);

        // handle win display timeout
        if(state.win_display){
            if(furi_get_tick() - state.win_start >= WIN_DISPLAY_MS){
                reset_state(&state);
            }
        }

        furi_delay_ms(50);

        // auto restart if all guesses eliminated
        if(!state.win_display && state.finished){
            bool all_elim = true;
            for(size_t i=0;i<POOL_SIZE;i++){
                if(!pool[i].eliminated){ all_elim=false; break;}
            }
            if(all_elim) reset_state(&state);
        }
    }

    gui_remove_view_port(gui,vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    return 0;
}
