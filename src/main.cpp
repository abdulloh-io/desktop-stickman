// ============================================================================
//  Alan Becker Desktop Stickmen — v2
//  Correct anatomical forward-kinematics animation, friendly social AI,
//  startup wave, video couch mode, typing buddy, autonomous work & props.
//  Pure Win32 + GDI+ + DWM layered overlay. No external DLLs.
// ============================================================================
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <mmdeviceapi.h>

#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <cstdint>

using namespace Gdiplus;

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif
#ifndef ULW_ALPHA
#define ULW_ALPHA 0x00000002
#endif

#define IDI_APPICON        101
#define WM_TRAYICON        (WM_USER + 10)
#define TIMER_FRAME        1
#define TIMER_WINSCAN      2
#define TIMER_SENSORS      3

// ---- Tray menu commands ----
#define CMD_ADD_STICKMAN   1001
#define CMD_RESET_SQUAD    1002
#define CMD_ADD_TCO        1003
#define CMD_REMOVE_ONE     1004
#define CMD_SPARRING       1005
#define CMD_CALL_HERE      1006
#define CMD_TOGGLE_GHOST   1007
#define CMD_TOGGLE_WINS    1008
#define CMD_SAY_HELLO      1010
#define CMD_DROP_PIZZA     1011
#define CMD_SPAWN_BALL     1012
#define CMD_TOGGLE_COUCH   1013
#define CMD_TOGGLE_DESK    1014
#define CMD_TOGGLE_AUTOTV  1015
#define CMD_TOGGLE_CALM    1016
#define CMD_TOGGLE_SPARAI  1017
#define CMD_SCALE_SMALL    1020
#define CMD_SCALE_NORMAL   1021
#define CMD_SCALE_LARGE    1022
#define CMD_CLEAR_TOYS     1023
#define CMD_TREAT_CAKE     1030
#define CMD_TREAT_MELON    1031
#define CMD_TREAT_POTION   1032
#define CMD_TREAT_CUBE     1033
#define CMD_GIVE_SWORD     1040
#define CMD_GIVE_PENCIL    1041
#define CMD_GIVE_GUITAR    1042
#define CMD_GIVE_NONE      1043
#define CMD_PARTY          1044
#define CMD_DUEL           1045
#define CMD_TOGGLE_PRANKS  1046
#define CMD_TOGGLE_PUSH    1047
#define CMD_TOGGLE_NIGHT   1048
#define CMD_TOGGLE_TEA     1049
#define CMD_SNOWMAN        1050
#define CMD_PAT_HEAD       1051
#define CMD_TREAT_PIZZA    1052
#define CMD_HIDE_ICON      1053
#define CMD_ICON_KICK      1054
#define CMD_BRING_FOLDER   1055
#define CMD_TAB_OOPS       1056
#define CMD_SEND_GUARD     1057
#define CMD_CURSOR_THEFT   1058
#define CMD_SHOW_DUEL      1059
#define CMD_NOTE           1060
#define CMD_TOGGLE_WEATHER 1061
#define CMD_WEATHER_AUTO   1062
#define CMD_WEATHER_RAIN   1063
#define CMD_WEATHER_SNOW   1064
#define CMD_BREAK_15       1065
#define CMD_BREAK_30       1066
#define CMD_BREAK_45       1067
#define CMD_BREAK_60       1068
#define CMD_AUTOSTART      1069
#define CMD_ASK_AI         1070
#define CMD_EXIT           1009

// ---- Global hotkey IDs ----
#define HK_SPAWN           2001
#define HK_SPAR            2002
#define HK_CALL            2003
#define HK_GHOST           2004
#define HK_QUIT            2005
#define HK_TCO             2006
#define HK_PIZZA           2007
#define HK_BALL            2008
#define HK_COUCH           2009
#define HK_HELLO           2010
#define HK_MODE            2011
#define HK_SCALE           2012
#define HK_TREAT           2013
#define HK_DUEL            2014
#define HK_PARTY           2015
#define HK_SNOWMAN         2016
#define HK_V4_FIRST        2017
#define HK_ICON_HIDE       2017
#define HK_BRING_FOLDER    2018
#define HK_WRITE_NOTE      2019
#define HK_DUEL_SHOW       2020
#define HK_ASK_AI          2021

static const float PI_F = 3.14159265f;
static const float TAU_F = 6.28318531f;

static inline float ClampF(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}
static inline float RandRange(float lo, float hi) {
    return lo + (hi - lo) * ((float)(rand() % 100000) / 100000.0f);
}
static inline int RandInt(int n) { return n > 0 ? rand() % n : 0; }
static inline float ApproachF(float v, float target, float rate, float dt) {
    return v + (target - v) * ClampF(rate * dt, 0.0f, 1.0f);
}
static inline float Osc(float t, float freq) { return std::sin(t * freq); }
static inline float Bounce(float t, float freq) { return std::fabs(std::sin(t * freq)); }

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};
static inline Vec2 V2(float x, float y) { return {x, y}; }
static inline Vec2 Add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
static inline Vec2 Sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
static inline Vec2 Mul(Vec2 a, float s) { return {a.x * s, a.y * s}; }
static inline Vec2 LerpV(Vec2 a, Vec2 b, float t) { return {LerpF(a.x, b.x, t), LerpF(a.y, b.y, t)}; }
static inline float Len(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

// Direction helpers. Limb angles are measured from "straight down" (+y) and
// rotate toward "forward" (+x). Torso/head angles are measured from "straight
// up" (-y). Everything below is forward kinematics, so joints can never pop
// into impossible positions the way a naive 2-bone IK solver does.
static inline Vec2 DirDown(float ang) { return V2(std::sin(ang), std::cos(ang)); }
static inline Vec2 DirUp(float ang)   { return V2(std::sin(ang), -std::cos(ang)); }

// ----------------------------------------------------------------------------
// Character roster
// ----------------------------------------------------------------------------
enum class CharType {
    TSC_ORANGE = 0, // The Second Coming — doodles / draws things
    RED        = 1, // builds & spars
    BLUE       = 2, // drinks potions, reads
    GREEN      = 3, // music & fishing
    YELLOW     = 4, // mini laptop programmer
    TCO_BLACK  = 5  // The Chosen One — fire
};

// ----------------------------------------------------------------------------
// Activity state machine (v2: mostly friendly, fighting is rare & optional)
// ----------------------------------------------------------------------------
enum class Act {
    IDLE = 0,
    WALK,
    RUN,
    AIRBORNE,
    WALL_CLIMB,
    SIT_EDGE,      // sits on a window's top bar, legs dangling inside
    PEEK_WINDOW,   // lies on a window and peers into it
    GRABBED,
    WAVE_HELLO,    // greeting wave at the user
    CHEER,         // both arms up, little jumps
    HIGH_FIVE,     // friendly slap with a buddy
    POINT_AT,      // pokes/points at the cursor or a window
    SPARRING,      // rare, cooldown gated or manual
    WATCH_TV,      // sitting on the cozy couch at the bottom of the screen
    EAT_POPCORN,   // hand-to-mouth while sitting on the couch
    MINI_LAPTOP,   // types along with the user
    SKETCH,        // draws doodles on the desktop with a pencil
    BUILD,         // stacks little blocks / workbench work
    MUSIC,         // plays a tiny guitar, spawns notes
    FISH,          // fishes off a window edge
    READ,          // sits and reads a book
    CAMPFIRE,      // tends the AFK campfire
    NAP,           // sleeps on the job (AFK)
    EAT_SNACK,     // eats a dropped pizza slice
    PLAY_BALL,     // chases & kicks the bouncy ball
    FLINCH,        // reacted to a boop / loud hit
    TRAVEL,        // walking to a prop location before performing
    // ---- v3: Actual-Shorts style group scenes ----
    CARD_GAME,     // card / chess match at the corner table
    WATCH_GAME,    // spectator: clapping around the table
    COOK,          // Blue stirs the cauldron
    QUEUE,         // waiting in line for food, then eating
    LAB,           // Yellow tinkers with a mini console (may go BOOM)
    ARMWRESTLE,    // two elbows on the window frame, others cheer
    TUGWAR,        // rope pull, both lean back
    HANDSHAKE,     // friendly finish of a duel
    SPY_CHAT,      // leans on the messenger edge with a magnifier
    LAUGH,         // rolling on the floor laughing at the feed
    FACEPALM,      // second-hand embarrassment
    YAWN,          // the video (or the work) is boring
    DANCE,         // bobbing to the music detected on the audio output
    CURSOR_PLAY,   // reacts to the user's cursor, waves at it
    HANG_CURSOR,   // hangs from the cursor like from a bar
    PETTED,        // user stroked the head: eyes closed, hearts
    ASK_ATTENTION, // sits near the cursor holding a heart / food sign
    TEA_RUN,       // brings a drawn mug + "stretch break" banner
    CRACK_PRANK,   // runs into the screen edge, then "fixes" the fake crack
    MUSTACHE,      // draws a face on the wallpaper, then erases it
    BOX_KICK,      // kicks a drawn cube around (never a real icon)
    SQUISH,        // squeezed by a moving window, pops out and shakes a fist
    EXTINGUISH,    // CPU is hot: fire extinguisher on the top bezel
    IN_VIDEO,      // "climbed inside" the video window (drawn dim & clipped)
    PUSH_WINDOW,   // shoves a background window a few pixels, then it eases back
    AIR_TYPE,      // cheers / ghost-types next to your editor
    // ---- v4: cursor games, sign language, safe mischief with windows ----
    CHASE_CURSOR,  // pounces at the pointer when you tease it
    RIDE_CURSOR,   // stands on the pointer and rides it like a scooter
    FIGHT_CURSOR,  // boxes the pointer after you annoyed him
    CURSOR_THEFT,  // grabs a *drawn copy* of the pointer and tries to lug it away
    SPEAK,         // holds up a notepad and "types" a message for you
    SEND_GUARD,    // sits next to the chat's Send button and nods on Enter
    HIDE_ICON,     // hides behind a drawn shortcut tile, only the eyes show
    ICON_KICK,     // dribbles a drawn icon around like a football
    BRING_FOLDER,  // brings you a drawn folder ("here you go")
    TAB_OOPS,      // "accidentally" knocked a drawn tab off the window, panics
    DRAG_COUCH,    // drags the whole couch into the corner before sitting
    PLUG_IN,       // battery is low: runs to the corner with a drawn plug
    UMBRELLA,      // it is raining outside (weather API, opt-in): tiny umbrella
    PASS_BALL      // passes the ball to a buddy instead of just kicking it
};

// Is this a "standing on the ground" pose where feet must be planted?
static bool ActPlantsFeet(Act a) {
    switch (a) {
        case Act::IDLE: case Act::WALK: case Act::RUN: case Act::WAVE_HELLO:
        case Act::CHEER: case Act::HIGH_FIVE: case Act::POINT_AT: case Act::SPARRING:
        case Act::TRAVEL: case Act::EAT_SNACK: case Act::PLAY_BALL:
        case Act::CAMPFIRE: case Act::CARD_GAME: case Act::WATCH_GAME:
        case Act::COOK: case Act::QUEUE: case Act::LAB: case Act::ARMWRESTLE:
        case Act::TUGWAR: case Act::HANDSHAKE: case Act::LAUGH: case Act::FACEPALM:
        case Act::YAWN: case Act::DANCE: case Act::CURSOR_PLAY: case Act::ASK_ATTENTION:
        case Act::TEA_RUN: case Act::CRACK_PRANK: case Act::MUSTACHE: case Act::BOX_KICK:
        case Act::EXTINGUISH: case Act::AIR_TYPE: case Act::SQUISH:
        case Act::CHASE_CURSOR: case Act::FIGHT_CURSOR: case Act::SPEAK: case Act::ICON_KICK:
        case Act::BRING_FOLDER: case Act::TAB_OOPS: case Act::DRAG_COUCH: case Act::PLUG_IN:
        case Act::UMBRELLA: case Act::PASS_BALL: case Act::HIDE_ICON:
        case Act::CURSOR_THEFT: return true;
        default: return false;
    }
}
static bool ActIsSeated(Act a) {
    switch (a) {
        case Act::WATCH_TV: case Act::EAT_POPCORN: case Act::MINI_LAPTOP:
        case Act::READ: case Act::SIT_EDGE: case Act::NAP: case Act::CARD_GAME:
        case Act::ARMWRESTLE: case Act::SPY_CHAT: case Act::QUEUE: case Act::LAB:
        case Act::PETTED: case Act::TEA_RUN: case Act::YAWN: case Act::SEND_GUARD:
            return true;
        default: return false;
    }
}

// ----------------------------------------------------------------------------
// Skeleton pose (world-space joints, produced by forward kinematics)
// ----------------------------------------------------------------------------
struct Pose {
    Vec2 head, neck, hip;
    Vec2 elbowL, handL, elbowR, handR;
    Vec2 kneeL, footL, kneeR, footR;
};

// Art-directable angle set, in figure-local units (y+ is down, x+ is forward)
struct FigAngles {
    float spine   = 0.05f; // torso lean from vertical, + = lean forward
    float headA   = 0.00f; // head direction from "up", + = look forward/up-down
    float upArmL  = -0.10f, foreArmL = 0.34f;
    float upArmR  =  0.10f, foreArmR = 0.30f;
    float thighL  = -0.12f, shinL    = -0.02f;
    float thighR  =  0.12f, shinR    =  0.05f;
    float hipRaise = 0.0f; // + = stand taller, - = crouch/sit
    float lat      = 0.0f; // horizontal hip shift
    float hop      = 0.0f; // whole figure vertical offset (+ is up)
};

// Base proportions — sleek Alan Becker style (~64 units tall, thin limbs)
struct Proportions {
    float hipH = 25.5f;
    float spine = 21.5f;
    float headR = 8.6f;
    float uArm = 12.8f, lArm = 12.2f;
    float uLeg = 13.0f, lLeg = 12.5f;
};
static const Proportions kProp = {};

// ----------------------------------------------------------------------------
// Props in the world
// ----------------------------------------------------------------------------
struct Ball {
    float x = 0, y = 0, vx = 0, vy = 0, r = 11.0f, spin = 0, rot = 0;
    float life = 42.0f;
    int kickerId = -1;
    float kickCool = 0.0f;
};
struct Snack {
    float x = 0, y = 0, vx = 0, vy = 0;
    float life = 30.0f;
    float bites = 3.0f;
    int ownerId = -1;
    int kind = 0;      // 0 pizza, 1 cake, 2 watermelon, 3 potion, 4 mc cube
    bool shared = false;
    bool onFloor = false;
};

struct DesktopPlatform {
    HWND hwnd = nullptr;
    RECT rect = {0, 0, 0, 0}; // overlay-local coordinates
    bool isActive = false;
    bool isMedia = false;
    int kind = 0;             // 0 app, 1 media, 2 messenger, 3 social feed, 4 editor/office
    int zIndex = 0;
    float dx = 0.0f;
    float dy = 0.0f;
};

struct MonitorFloor {
    int left = 0;
    int right = 1920;
    int top = 0;
    int bottom = 1080;
    int floorY = 1040;
};

struct Particle {
    float x, y, vx, vy, size, life, maxLife;
    BYTE r, g, b;
    int kind; // 0 dust, 1 ring, 2 star, 3 note, 4 flame, 5 code spark, 6 Zzz, 7 crumb, 8 splash
};

// A little world object the stickmen interact with (table, cauldron, fake crack...).
enum PropKind {
    PR_TABLE = 0,   // card / chess / armwrestle table
    PR_CAULDRON,    // Blue's cooking pot
    PR_BOX,         // drawn cube kicked around (never a real icon)
    PR_CRACK,       // fake cracked screen + tape repair
    PR_PILLOW,      // sleep spot
    PR_PANEL,       // Yellow's mini console
    PR_SIGN,        // banner with a message
    PR_FEED,        // "screen content" the couch reacts to (unused visual)
    PR_ICON,        // v4: a *drawn* shortcut tile they kick around (never a real icon)
    PR_FOLDER,      // v4: a drawn folder brought to the user
    PR_TAB,         // v4: a fake browser tab knocked off a window, then taped back
    PR_MAX
};

struct WorldProp {
    bool alive = false;
    int kind = 0;
    float x = 0, y = 0;
    float w = 0, h = 0;
    float life = 0, maxLife = 1;
    int a = -1, b = -1;       // owning stickman ids
    float phase = 0, vx = 0, vy = 0, rot = 0, spin = 0;
    int data = 0;             // kind-specific (0 cards 1 chess, crack stage...)
};

struct Stickman {
    int id = 0;
    CharType type = CharType::TSC_ORANGE;
    BYTE r = 255, g = 122, b = 0;
    bool hollowHead = false;

    float x = 400.0f, y = 400.0f; // ground contact point (feet), overlay-local
    float vx = 0.0f, vy = 0.0f;
    int facing = 1;

    Act act = Act::AIRBORNE;
    Act nextAct = Act::IDLE;      // for TRAVEL: what to do once we arrive
    float actTimer = 0.0f;
    float animTime = 0.0f;
    float phase = 0.0f;          // walking cycle phase
    float flipAngle = 0.0f;
    bool doingFlip = false;

    bool grounded = false;
    bool poseInitialized = false;
    float squash = 0.0f, squashVel = 0.0f;   // squash & stretch spring
    float landImpact = 0.0f;

    HWND standingHwnd = nullptr;
    HWND climbingHwnd = nullptr;
    int climbSide = 0;

    int partnerId = -1;          // sparring / high-five buddy
    float travelTargetX = 0.0f;
    bool travelFloor = false;
    int seatIndex = -1;          // couch seat slot
    float propT = 0.0f;          // generic per-activity timer (bites, blocks…)
    float keyAnim = 0.0f;        // mini-laptop keystroke dip
    int blocks = 0;
    float gaze = 0.0f;           // head turn toward cursor (-1..1)
    bool pendingHello = false;
    float socialCooldown = 0.0f;
    float sparCooldown = 25.0f;
    bool hasSword = false;
    float swordTimer = 0.0f;
    float grabAngle = 0.0f, grabAngVel = 0.0f;
    int doodleShape = 0;         // 0 scribble, 1 cat, 2 pig, 3 flower, 4 smiley+moustache
    std::vector<Vec2> doodle;    // world-space doodle polyline (SKETCH)
    float doodleFade = 0.0f;
    float fishT = 0.0f, bobY = 0.0f;
    int snackBite = 0;
    float laugh = 0.0f;
    // --- v3 state ---
    float pet = 0.0f;            // >0 while the user strokes the head
    float soot = 0.0f;           // black face after the lab boom
    float actPhase = 0.0f;       // generic progress (armwrestle / tug / cook)
    float blink = 0.0f;          // eye timer
    int   carry = 0;             // 0 none, 1 mug, 2 box, 3 magnifier, 4 extinguisher, 5 ladle,
                                 // 6 tape, 7 marker, 8 folder, 9 plug, 10 umbrella, 11 hammer,
                                 // 12 eraser, 13 stolen cursor (drawn copy!)
    wchar_t speech[160] = {0};    // v4: text typed onto the raised notepad
    float speechT = 0.0f;        // seconds since the note went up
    float speechSeen = 0.0f;     // how many characters are revealed so far
    float mood = 0.75f;          // personal mood: low -> moping, high -> clowning
    int   weapon = 0;            // 0 none, 1 diamond sword, 2 stick, 3 pencil, 4 guitar
    bool  hidden = false;        // hiding behind a prop / window: only the head peeks
    float floatT = 0.0f;         // generic clock for the v4 acts
    int   rides = 0;             // >0 while glued to the pointer (riding / stealing)
    int   propIndex = -1;        // world prop this stickman is tied to
    int   audienceOf = -1;       // watching this stickman's show
    float prankCool = 40.0f;     // cooldown between mischief gags
    float workCool = 0.0f;       // "user is working" reaction cooldown
    bool  inVideo = false;       // drawn as if inside the video window
    float inVideoT = 0.0f;
    HWND  pushHwnd = nullptr;    // window being shoved
    Vec2  pushOrigin = {0, 0};
    float pushT = 0.0f;
    float pushDir = 1.0f;
    float seatOff = 0.0f;        // offset from the tied prop (seats, queue lines)
    HWND travelWin = nullptr;    // TRAVEL destination is on top of this window
    Pose pose;

    Vec2 Anchor() const { return V2(x, y - 32.0f); }
};

// ----------------------------------------------------------------------------
// Global app state
// ----------------------------------------------------------------------------
struct AppState {
    HWND hwnd = nullptr;
    HINSTANCE hInst = nullptr;
    ULONG_PTR gdiplusToken = 0;
    NOTIFYICONDATAW nid = {};

    int originX = 0, originY = 0;
    int width = 1920, height = 1080;

    HDC screenDC = nullptr;
    HDC memDC = nullptr;
    HBITMAP dib = nullptr;
    HBITMAP oldBmp = nullptr;
    uint32_t* pixels = nullptr;

    RECT prevDirty = {0, 0, 0, 0};
    bool hasPrevDirty = false;

    std::vector<MonitorFloor> monitors;
    std::vector<DesktopPlatform> platforms;
    std::unordered_map<HWND, RECT> prevWinRects;
    HWND foregroundWin = nullptr;
    RECT mediaWinRect = {0, 0, 0, 0};
    bool mediaWinValid = false;

    std::vector<Stickman> stickmen;
    std::vector<Particle> particles;
    std::vector<Ball> balls;
    std::vector<Snack> snacks;
    WorldProp props[12];
    int nextStickmanId = 1;

    // --- settings ---
    bool ghostMode = false;
    bool interactWindows = true;
    bool calmMode = false;        // Calm = chill activities, Active = parkour
    bool sparAI = false;          // rare spontaneous sparring
    bool autoCouch = true;        // detect video watching
    bool autoDesk = true;         // detect user typing
    int  scaleIdx = 1;            // 0 small, 1 normal, 2 large
    float scale = 1.0f;

    // --- sensing ---
    bool couchOn = false;         // currently active (auto or manual)
    float couchHold = 0.0f;
    float couchX = 0.0f, couchY = 0.0f, couchW = 0.0f;
    int couchSeatsTaken[6] = {-1,-1,-1,-1,-1,-1};
    bool deskOn = false;
    float deskHold = 0.0f;
    float deskX = 0.0f, deskY = 0.0f;
    int deskOwnerId = -1;
    float typingRate = 0.0f;      // 0..1 keys pressure
    float typingPulse = 0.0f;     // spikes on each key
    bool keyPrev[256] = {false};
    bool mousePrevDown = false;
    float audioPeak = 0.0f, audioEnv = 0.0f;
    bool audioOk = false;
    float idleSeconds = 0.0f;
    bool afk = false;
    float afkHold = 0.0f;

    // --- v3 sensing ---
    int   fgKind = 0;          // 0 desktop/app, 1 media, 2 messenger, 3 social feed, 4 editor/office
    float cpuLoad = 0.0f;      // 0..1 system load
    float hotHold = 0.0f;      // for how long the CPU has been busy
    float beatPhase = 0.0f;    // pseudo beat clock from the audio envelope
    float audioBeat = 0.0f;    // spikes on a loud onset
    float workStreak = 0.0f;   // continuous working time (minutes)
    float feedScore = 0.0f;    // vertical cursor wiggle = scrolling a feed
    float cursorPlay = 0.0f;   // the user is waving the cursor near someone
    int   nightHour = -1;
    bool  nightMode = false;   // late night -> pyjama caps, more naps
    bool  winter = false;      // Dec..Feb -> snowman
    // --- v3 toggles ---
    bool  pranksOn = true;     // harmless mischief gags
    bool  deepseekOn = true;   // DeepSeek API live replies
    char  deepseekKey[128] = ""; // loaded from deepseek_key.txt if present
    std::atomic<bool> aiBusy{false};
    std::mutex aiMutex;
    bool  aiHasReply = false;
    float aiAutoCool = 0.0f;
    int   aiTargetId = -1;
    wchar_t aiReply[160] = {};
    wchar_t fgTitle[128] = {};
    bool  pushWindows = false; // opt-in: shove a background window a few px (self-restoring)
    bool  autoNight = true;
    bool  nightForced = false; // used when autoNight is off
    bool  teaReminders = true;
    float teaCooldown = 0.0f;
    float prankTimer = 26.0f;
    float directorT = 0.0f;
    float heatGlow = 0.0f;     // red strip on the top bezel
    // --- v4 ---
    float mood = 0.75f;        // shared squad mood (0..1)
    float breakMinutes = 45.0f;
    bool  weatherOn = false;   // react to the weather (calendar + tray override)
    int   weatherMode = 0;     // 0 auto (by month), 1 rain, 2 snow
    bool  batteryLow = false;  // the laptop is under 15%
    bool  raining = false, snowing = false;
    float outsideTemp = 99.0f; // 99 = unknown
    float weatherT = 999.0f;   // time until the next refresh
    float weatherFail = 0.0f;
    int   batteryPct = -1;     // -1 = desktop / unknown
    bool  onBattery = false;
    float batteryT = 0.0f;
    float cursorTease = 0.0f;  // the pointer is being waved around them
    float pokeCount = 0.0f;    // how often the user poked them lately
    float guardT = 0.0f;       // "send button" scene clock
    float guardX = 0.0f, guardY = 0.0f;  // where the Send button would be
    float couchGoalX = 0.0f;             // v4: where they are dragging the couch
    float weatherCool = 40.0f; // prank clock for the weather acts
    float plugCool = 30.0f;
    float theftCool = 30.0f;
    int   fgChat = 0;          // active window is a messenger (1) etc.
    Vec2  ghostCursor = {0, 0}; // the *drawn* cursor someone stole
    float keepOutL = -1.0f, keepOutR = -1.0f; // "user is reading here" band
    ULONGLONG prevIdleTick = 0;
    FILETIME prevIdle{}, prevKernel{}, prevUser{};
    bool haveCpuSample = false;

    int grabbedId = -1;
    POINT prevMouse = {0, 0};
    Vec2 mouseVel = {0.0f, 0.0f};
    Vec2 cursor = {0.0f, 0.0f};
    DWORD lastTick = 0;
    int topmostCounter = 0;
    float globalTime = 0.0f;
    float helloTimer = 0.0f;
    float touchTimer = 0.0f;      // time since the user last touched a stickman
    float lastSwingX = 0.0f;
    float swingSign = 0.0f;
    float swingCount = 0.0f;
    int furthestBallKicker = -1;
} g_app;

static void SpawnParticle(float x, float y, float vx, float vy, float size, float life,
                          BYTE r, BYTE g, BYTE b, int kind = 0);
static void SpawnBurst(float x, float y, BYTE r, BYTE g, BYTE b, int count, int kind = 0);
static void SpawnStickman(CharType type, float x, float y, bool hello = false);
static void ResetSquad();
static void StartSparringMatch();
static void CallAllToCursor();
static void SayHelloAll();
static void DropSnackAtCursor(int kind = 0);
static void SpawnBallAtCursor();
static void SetScaleIdx(int idx);
static int FloorYAt(float x);
static void RecomputeCouchAnchor();
static Stickman* FindStickmanById(int id);
static void LeaveSeat(Stickman& s);
static void SetCouchMode(bool on);
static void SetDeskMode(bool on);
static void ReleaseGrab();
static void GoSitOnCouch(Stickman& s);
static void StartTeaBreak();
static void TryPrank();
static void SayText(Stickman& s, const wchar_t* text);
static void SayNote(Stickman& s, int reason);
static bool AnyoneSpeaking();
static void StartNoteHere(Stickman& s, int reason);
static void StartCursorHide(Stickman& s);
static void StartIconGame(Stickman& s);
static void StartBring(Stickman& s);
static void StartTabOops(Stickman& s);
static void StartPlugRun(Stickman& s);
static void StartSendGuard(Stickman& s);
static void StartUmbrella(Stickman& s);
static void StartPassBall(Stickman& s);
static void StartShowDuel();
static Stickman* PickFreeGuy(int prefer);
static void directorsTick(float dt);
static void SceneFX(Stickman& s, float dt);
static void TryGroupScene(Stickman& s);
static void StartArmWrestle(Stickman& a, Stickman& b);
static void StartTugWar(Stickman& a, Stickman& b);
static void EndSparWithHandshake(Stickman& s);
static void ReleaseProp(Stickman& s);
static Vec2 DoodlePoint(int shape, float u, float ax, float ay, float rr, float facing);
static void GoToDesk(Stickman& s);
static void SetAct(Stickman& s, Act a);
static void BeginTravel(Stickman& s, Act dest, float targetX);

// ============================================================================
// Setup helpers
// ============================================================================
static void EnableDpiAwareness() {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFn)(DPI_AWARENESS_CONTEXT);
        auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            reinterpret_cast<void*>(GetProcAddress(hUser32, "SetProcessDpiAwarenessContext")));
        if (fn) {
            if (fn((DPI_AWARENESS_CONTEXT)-4)) return; // PER_MONITOR_AWARE_V2
        }
        typedef BOOL(WINAPI* SetProcessDPIAwareFn)();
        auto fnOld = reinterpret_cast<SetProcessDPIAwareFn>(
            reinterpret_cast<void*>(GetProcAddress(hUser32, "SetProcessDPIAware")));
        if (fnOld) fnOld();
    }
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM) {
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        MonitorFloor mf;
        mf.left   = mi.rcMonitor.left   - g_app.originX;
        mf.right  = mi.rcMonitor.right  - g_app.originX;
        mf.top    = mi.rcMonitor.top    - g_app.originY;
        mf.bottom = mi.rcMonitor.bottom - g_app.originY;
        mf.floorY = mi.rcWork.bottom    - g_app.originY;
        if (mf.floorY >= mf.bottom) mf.floorY = mf.bottom - 2;
        g_app.monitors.push_back(mf);
    }
    return TRUE;
}

static int FloorYAt(float x) {
    for (const auto& m : g_app.monitors) {
        if (x >= (float)m.left && x <= (float)m.right) return m.floorY;
    }
    return g_app.monitors.empty() ? (g_app.height - 48) : g_app.monitors[0].floorY;
}

static void RebuildBackbuffer() {
    if (g_app.memDC) {
        if (g_app.oldBmp) SelectObject(g_app.memDC, g_app.oldBmp);
        if (g_app.dib) DeleteObject(g_app.dib);
        DeleteDC(g_app.memDC);
        g_app.memDC = nullptr;
        g_app.dib = nullptr;
        g_app.pixels = nullptr;
    }
    if (!g_app.screenDC) g_app.screenDC = GetDC(nullptr);

    g_app.originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_app.originY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_app.width   = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_app.height  = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (g_app.width <= 0)  g_app.width  = GetSystemMetrics(SM_CXSCREEN);
    if (g_app.height <= 0) g_app.height = GetSystemMetrics(SM_CYSCREEN);

    g_app.monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);
    if (g_app.monitors.empty()) {
        RECT wa = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        MonitorFloor mf;
        mf.left = 0; mf.right = g_app.width; mf.top = 0;
        mf.bottom = g_app.height; mf.floorY = wa.bottom - g_app.originY;
        g_app.monitors.push_back(mf);
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = g_app.width;
    bmi.bmiHeader.biHeight      = -g_app.height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_app.memDC = CreateCompatibleDC(g_app.screenDC);
    void* bits = nullptr;
    g_app.dib = CreateDIBSection(g_app.screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    g_app.oldBmp = (HBITMAP)SelectObject(g_app.memDC, g_app.dib);
    g_app.pixels = static_cast<uint32_t*>(bits);
    if (g_app.pixels) {
        std::memset(g_app.pixels, 0, (size_t)g_app.width * (size_t)g_app.height * 4);
    }
    g_app.hasPrevDirty = false;

    RecomputeCouchAnchor();

    if (g_app.hwnd) {
        SetWindowPos(g_app.hwnd, HWND_TOPMOST,
                     g_app.originX, g_app.originY, g_app.width, g_app.height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

// ============================================================================
// Desktop window scanning + media detection
// ============================================================================
static bool WideContainsCI(const wchar_t* hay, const wchar_t* needle) {
    if (!hay || !needle || !*needle) return false;
    size_t hl = wcslen(hay), nl = wcslen(needle);
    if (nl > hl) return false;
    for (size_t i = 0; i + nl <= hl; ++i) {
        size_t j = 0;
        for (; j < nl; ++j) {
            if (towlower(hay[i + j]) != towlower(needle[j])) break;
        }
        if (j == nl) return true;
    }
    return false;
}

// Titles (latin + russian) that mean "the user is watching a video"
static const wchar_t* kMediaTitles[] = {
    L"YouTube", L"You tube", L"Netflix", L"Twitch", L"Kick.com", L"Disney",
    L"HBO", L"Prime Video", L"Apple TV", L"Vimeo", L"Rutube", L"Streamable",
    L"Video", L"Watch", L"Player", L"PlayerTV", L"VLC media player",
    L"PotPlayer", L"MPC-HC", L"MPC-BE", L"mpv", L"Mpv", L"GOM Player",
    L"KMPlayer", L"5KPlayer", L"Plex", L"Stremio", L"IV", L"Смотреть",
    L"Кинопоиск", L"Видео", L"Фильм", L"Сериал", L"Трейлер", L"Прямой эфир",
    L"Kinopoisk", L"Ivi", L"Wink", L"Okko", L"Steam"
};
// Messengers: they sneak to the window edge and "read" the chat with you
static const wchar_t* kChatTitles[] = {
    L"Telegram", L"Discord", L"WhatsApp", L"Viber", L"Skype", L"Signal",
    L"ICQ", L"Slack", L"Mattermost", L"Teams", L"Zoom", L"Звонок",
    L"Чат", L"Переписка",
    L"Mail", L"Outlook", L"Почта"
};
static const wchar_t* kChatClasses[] = {
    L"TelegramDesktopGUI", L"Discord", L"Qt5152QWindowIcon", L"Chrome_WidgetWin",
    L"WhatsApp", L"Skype", L"Teams"
};
// Feeds: vertical cursor wiggle there is treated as "scrolling memes"
static const wchar_t* kFeedTitles[] = {
    L"VK", L"ВКонтакте", L"TikTok", L"Reddit",
    L"Twitter", L"X.com", L"Instagram", L"Pinterest", L"9GAG", L"Lurkmore",
    L"Pikabu", L"Пикабу", L"Habr", L"Steam", L"Forge",
    L"Shorts", L"Boosty", L"Twitch"
};
static const wchar_t* kFeedClasses[] = { L"" };
// Editors / offices: they stay out of the middle and "cheer" for your typing
static const wchar_t* kEditorTitles[] = {
    L"Notepad", L"Блокнот", L"Word", L"Docs", L"Google Docs",
    L"Visual Studio Code", L"- Visual Studio", L"PyCharm", L"IntelliJ", L"CLion",
    L"Sublime Text", L"Notepad++", L"Vim", L"Neovim", L"LaTeX", L"PowerPoint",
    L"Excel", L"Obsidian", L"Typora", L"LibreOffice"
};
static const wchar_t* kEditorClasses[] = { L"Reditz", L"WordProcessor", L"OpusApp" };

static const wchar_t* kMediaClasses[] = {
    L"PotPlayer64", L"PotPlayer76", L"mpv", L"MPV", L"EVR543WindowClass",
    L"DirectShow9", L"WMPlayerApp", L"Qt67QWindowIcon", L"Citra",
    L"VLC video window", L"mxrtpvideo", L"VirtualDub", L"FLVMediaPlayer"
};

static bool MatchesAny(const wchar_t* title, const wchar_t* cls,
                       const wchar_t* const* titles, const wchar_t* const* classes) {
    for (const wchar_t* c = *classes; c && *c; ++c) {
        if (wcsstr(cls, c) != nullptr) return true;
    }
    for (const wchar_t* t = *titles; t && *t; ++t) {
        if (WideContainsCI(title, t)) return true;
    }
    return false;
}

struct EnumWinCtx {
    std::vector<DesktopPlatform> list;
    int zCounter = 0;
};

static BOOL CALLBACK EnumDesktopWindowsProc(HWND hwnd, LPARAM lParam) {
    if (hwnd == g_app.hwnd) return TRUE;
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;
    if ((exStyle & WS_EX_TRANSPARENT) && (exStyle & WS_EX_LAYERED)) return TRUE;

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
        return TRUE;
    }

    wchar_t clsName[96] = {};
    GetClassNameW(hwnd, clsName, 95);
    if (wcscmp(clsName, L"Progman") == 0 ||
        wcscmp(clsName, L"WorkerW") == 0 ||
        wcscmp(clsName, L"Shell_TrayWnd") == 0 ||
        wcscmp(clsName, L"Shell_SecondaryTrayWnd") == 0 ||
        wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0) {
        return TRUE;
    }

    RECT r = {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
        if (!GetWindowRect(hwnd, &r)) return TRUE;
    }
    r.left   -= g_app.originX;
    r.right  -= g_app.originX;
    r.top    -= g_app.originY;
    r.bottom -= g_app.originY;

    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (w < 140 || h < 80) return TRUE;

    bool isMedia = false;
    int kind = 0;
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 255);
    for (const wchar_t* c : kMediaClasses) {
        if (wcsstr(clsName, c) != nullptr) { isMedia = true; break; }
    }
    if (!isMedia) {
        for (const wchar_t* k : kMediaTitles) {
            if (WideContainsCI(title, k)) { isMedia = true; break; }
        }
    }
    if (isMedia) {
        kind = 1;
    } else if (MatchesAny(title, clsName, kChatTitles, kChatClasses)) {
        kind = 2;
    } else if (MatchesAny(title, clsName, kFeedTitles, kFeedClasses)) {
        kind = 3;
    } else if (MatchesAny(title, clsName, kEditorTitles, kEditorClasses)) {
        kind = 4;
    }
    // A borderless window that fills a whole monitor is basically always video/game
    if (!isMedia && r.top <= 4 && r.bottom >= g_app.height - 4 && w >= g_app.width - 8) {
        if (wcslen(title) > 0) { isMedia = true; kind = 1; }
    }

    auto* ctx = reinterpret_cast<EnumWinCtx*>(lParam);
    DesktopPlatform dp;
    dp.hwnd = hwnd;
    dp.rect = r;
    dp.isActive = (hwnd == g_app.foregroundWin);
    dp.isMedia = isMedia;
    dp.kind = kind;
    dp.zIndex = ctx->zCounter++;
    if (isMedia && dp.isActive) {
        g_app.mediaWinRect = r;
        g_app.mediaWinValid = true;
    }

    auto it = g_app.prevWinRects.find(hwnd);
    if (it != g_app.prevWinRects.end()) {
        dp.dx = (float)(r.left - it->second.left);
        dp.dy = (float)(r.top  - it->second.top);
    }
    ctx->list.push_back(dp);
    return TRUE;
}

static void ScanDesktopWindows() {
    g_app.foregroundWin = GetForegroundWindow();
    g_app.mediaWinValid = false;
    if (!g_app.interactWindows) {
        g_app.platforms.clear();
        g_app.prevWinRects.clear();
        return;
    }
    EnumWinCtx ctx;
    EnumWindows(EnumDesktopWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    g_app.prevWinRects.clear();
    for (const auto& p : ctx.list) g_app.prevWinRects[p.hwnd] = p.rect;
    g_app.platforms = std::move(ctx.list);

    // What kind of window does the user actually have in front of them?
    int kind = 0;
    for (const auto& p : g_app.platforms) {
        if (p.isActive) { kind = p.kind; break; }
    }
    g_app.fgKind = kind;

    // "Don't wander into the middle of what they are reading" band.
    g_app.keepOutL = g_app.keepOutR = -1.0f;
    if (kind == 3 || kind == 4 || kind == 2) {
        for (const auto& p : g_app.platforms) {
            if (!p.isActive) continue;
            int ww = p.rect.right - p.rect.left;
            int hh = p.rect.bottom - p.rect.top;
            if (ww < 320 || hh < 220) break;
            float shrink = (float)ww * 0.22f;
            g_app.keepOutL = (float)p.rect.left + shrink;
            g_app.keepOutR = (float)p.rect.right - shrink;
            break;
        }
    }
}

static bool InKeepOut(float x) {
    return g_app.keepOutL >= 0.0f && x > g_app.keepOutL && x < g_app.keepOutR;
}
// Push a walking target out of the reading band (they loop around it instead).
static float RouteAround(float from, float to) {
    if (!InKeepOut(to)) return to;
    float mid = (g_app.keepOutL + g_app.keepOutR) * 0.5f;
    return (from < mid) ? g_app.keepOutL - 26.0f : g_app.keepOutR + 26.0f;
}

static const DesktopPlatform* FindPlatformByHwnd(HWND h) {
    if (!h) return nullptr;
    for (const auto& p : g_app.platforms) {
        if (p.hwnd == h) return &p;
    }
    return nullptr;
}
static bool IsOccludedByHigherWindow(float x, float y, int maxZIndex) {
    for (const auto& p : g_app.platforms) {
        if (p.zIndex >= maxZIndex) break;
        if (x >= p.rect.left && x <= p.rect.right &&
            y >= p.rect.top  && y <= p.rect.bottom) return true;
    }
    return false;
}

// ============================================================================
// Tiny world props (tables, cauldrons, fake cracks, pillows, banners...)
// ============================================================================
static int AddProp(int kind, float x, float y, float life, int a, int b, int data) {
    int freeSlot = -1;
    float oldest = 1e9f;
    for (int i = 0; i < (int)(sizeof(g_app.props) / sizeof(g_app.props[0])); ++i) {
        WorldProp& w = g_app.props[i];
        if (!w.alive) { freeSlot = i; break; }
        if (a >= 0 && w.kind == kind && (w.a == a || w.b == a)) return i;   // reuse ours
        if (w.life < oldest) { oldest = w.life; freeSlot = i; }
    }
    if (freeSlot < 0) return -1;
    WorldProp& w = g_app.props[freeSlot];
    w.alive = true;
    w.kind = kind;
    w.x = x; w.y = y;
    w.w = 0; w.h = 0;
    w.life = life; w.maxLife = life;
    w.a = a; w.b = b;
    w.phase = 0; w.vx = 0; w.vy = 0; w.rot = 0; w.spin = 0;
    w.data = data;
    return freeSlot;
}

static WorldProp* PropAt(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(g_app.props) / sizeof(g_app.props[0]))) return nullptr;
    WorldProp& w = g_app.props[idx];
    return w.alive ? &w : nullptr;
}

static WorldProp* FindProp(int kind, int ownerId) {
    for (auto& w : g_app.props) {
        if (w.alive && w.kind == kind && (w.a == ownerId || w.b == ownerId)) return &w;
    }
    return nullptr;
}

static void KillProp(WorldProp& w) {
    w.alive = false;
    for (auto& s : g_app.stickmen) {
        if (s.propIndex >= 0 && &g_app.props[s.propIndex] == &w) s.propIndex = -1;
    }
}

static void UpdateProps(float dt) {
    for (auto& w : g_app.props) {
        if (!w.alive) continue;
        w.life -= dt;
        w.phase += dt;
        if ((w.kind == PR_ICON || w.kind == PR_TAB) && w.data == 3) {
            // v4: somebody is holding it, the act drives its position by hand
            w.vx = 0.0f; w.vy = 0.0f; w.rot *= std::pow(0.02f, dt);
        } else if (w.kind == PR_BOX || w.kind == PR_ICON || w.kind == PR_TAB) {
            // a drawn cube / shortcut tile / fake tab with toy physics
            float grav = (w.kind == PR_TAB) ? 340.0f : 1350.0f;
            w.vy += grav * dt;
            if (w.kind == PR_TAB) { w.vx += std::sin(w.phase * 4.0f) * 60.0f * dt; w.rot += 2.2f * dt; }
            w.x += w.vx * dt;
            w.y += w.vy * dt;
            w.rot += w.spin * dt;
            w.spin *= std::pow(0.7f, dt);
            float fl = (float)FloorYAt(w.x) - ((w.kind == PR_TAB) ? 4.0f : 8.0f);
            if (w.y > fl) {
                w.y = fl;
                if (w.vy > 60.0f) SpawnBurst(w.x, w.y + 8.0f, 210, 215, 225, 3, 0);
                w.vy = -w.vy * ((w.kind == PR_TAB) ? 0.24f : 0.5f);
                w.vx *= 0.86f;
                w.spin = w.vx / 10.0f;
            }
            if (w.x < 14.0f) { w.x = 14.0f; w.vx = std::fabs(w.vx) * 0.7f; }
            if (w.x > (float)g_app.width - 14.0f) { w.x = (float)g_app.width - 14.0f; w.vx = -std::fabs(w.vx) * 0.7f; }
            if (w.kind == PR_TAB) { w.vx *= std::pow(0.35f, dt); w.vy *= std::pow(0.5f, dt); }
        }
        // the brew is done: a pie appears where the cauldron stood
        if (w.kind == PR_CAULDRON && w.life <= 3.0f && w.data == 0) {
            w.data = 1;
            AddProp(PR_SIGN, w.x + 46.0f * g_app.scale, w.y, 9.0f, w.a, w.b, 0); // 0 = pie
            for (auto& s2 : g_app.stickmen) {
                if (s2.propIndex >= 0 && &g_app.props[s2.propIndex] == &w) {
                    s2.act = Act::CHEER;
                    s2.actTimer = RandRange(1.2f, 2.2f);
                }
            }
        }
        // the snowman is finished -> it simply stands there for a while
        if (w.kind == PR_PILLOW && w.data == 1 && w.life <= 9.0f) {
            w.data = 2;
            w.life = 45.0f;
            w.maxLife = 45.0f;
            for (auto& s2 : g_app.stickmen) {
                if (s2.propIndex >= 0 && &g_app.props[s2.propIndex] == &w) {
                    s2.propIndex = -1;
                    s2.act = Act::CHEER;
                    s2.actTimer = RandRange(1.4f, 2.6f);
                    s2.animTime = 0.0f;
                }
            }
            SpawnBurst(w.x, w.y - 60.0f * g_app.scale, 235, 245, 252, 10, 0);
        }
        if (w.life <= 0.0f) {
            if (w.kind == PR_TABLE || w.kind == PR_CAULDRON || w.kind == PR_PANEL) {
                for (auto& s : g_app.stickmen) {
                    if (s.propIndex >= 0 && &g_app.props[s.propIndex] == &w &&
                        (s.act == Act::CARD_GAME || s.act == Act::COOK || s.act == Act::LAB ||
                         s.act == Act::QUEUE || s.act == Act::ARMWRESTLE)) {
                        s.propIndex = -1;
                        s.act = Act::IDLE;
                        s.actTimer = RandRange(0.5f, 1.4f);
                    }
                }
            }
            KillProp(w);
        }
    }
}

// ============================================================================
// WASAPI peak meter (used as an extra hint for "video is playing")
// MinGW's <endpointvolume.h> only forward declares IAudioMeterInformation, so
// we declare the vtable ourselves and pass explicit GUIDs (no __uuidof).
// ============================================================================
struct IAudioMeterInfoWin : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* pfMaxLevel) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT* pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 u32ChannelCount, UINT* afPeakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD* pdwHardwareSupportMask) = 0;
};

static const GUID CLSID_MMDeviceEnumerator_Local =
    {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID IID_IMMDeviceEnumerator_Local =
    {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID IID_IAudioMeterInfo_Local =
    {0xC02216F6, 0x8C67, 0x4B5B, {0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64}};

static IMMDeviceEnumerator* g_mmEnum = nullptr;
static IMMDevice* g_mmDevice = nullptr;
static IAudioMeterInfoWin* g_mmMeter = nullptr;

static void AudioMeterEnsure() {
    if (g_mmMeter) return;
    if (!g_mmEnum) {
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator_Local, nullptr, CLSCTX_ALL,
                                      IID_IMMDeviceEnumerator_Local, reinterpret_cast<void**>(&g_mmEnum));
        if (FAILED(hr) || !g_mmEnum) { g_app.audioOk = false; return; }
    }
    if (!g_mmDevice) {
        if (FAILED(g_mmEnum->GetDefaultAudioEndpoint(eRender, eConsole, &g_mmDevice)) || !g_mmDevice) {
            g_app.audioOk = false;
            return;
        }
    }
    IUnknown* unk = nullptr;
    if (FAILED(g_mmDevice->Activate(IID_IAudioMeterInfo_Local, CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void**>(&unk))) || !unk) {
        g_app.audioOk = false;
        return;
    }
    g_mmMeter = static_cast<IAudioMeterInfoWin*>(unk);
    g_app.audioOk = true;
}

static void AudioMeterSample() {
    AudioMeterEnsure();
    if (!g_mmMeter) return;
    float peak = 0.0f;
    if (SUCCEEDED(g_mmMeter->GetPeakValue(&peak))) {
        g_app.audioPeak = ClampF(peak, 0.0f, 1.0f);
    }
}

static void AudioMeterRelease() {
    if (g_mmMeter) { g_mmMeter->Release(); g_mmMeter = nullptr; }
    if (g_mmDevice) { g_mmDevice->Release(); g_mmDevice = nullptr; }
    if (g_mmEnum) { g_mmEnum->Release(); g_mmEnum = nullptr; }
}

// ============================================================================
// Couch geometry (bottom of the screen, derived from stickman proportions)
// ============================================================================
static void RecomputeCouchAnchor() {
    float floorY = (float)(g_app.monitors.empty() ? g_app.height - 48 : g_app.monitors[0].floorY);
    int n = 0;
    for (const auto& s : g_app.stickmen) {
        if (s.seatIndex >= 0) ++n;
    }
    if (n < 2) n = 2;
    float sc = g_app.scale;
    float seatSpacing = 62.0f * sc;
    float armW = 15.0f * sc;
    float w = seatSpacing * (float)n + armW * 2.0f + 16.0f * sc;
    if (w > (float)g_app.width * 0.72f) w = (float)g_app.width * 0.72f;
    g_app.couchW = w;
    g_app.couchX = (float)g_app.width * 0.5f;
    g_app.couchY = floorY;
}

static int CouchSeatCount() {
    int n = (int)((g_app.couchW / (62.0f * std::max(0.5f, g_app.scale))) - 1.6f);
    if (n < 2) n = 2;
    if (n > 6) n = 6;
    return n;
}
static float CouchSeatX(int seat) {
    int n = CouchSeatCount();
    if (seat < 0) seat = 0;
    if (seat >= n) seat = n - 1;
    float spacing = 62.0f * g_app.scale;
    return g_app.couchX + ((float)seat - (float)(n - 1) * 0.5f) * spacing;
}
static float CouchSeatTopY() {
    // Seat surface: seated hip height = shin length, so the stickman's feet
    // land exactly on the floor line without any IK.
    return g_app.couchY - (kProp.lLeg - 1.0f) * g_app.scale;
}
static int ClaimCouchSeat(int id) {
    int n = CouchSeatCount();
    for (int i = 0; i < n; ++i) {
        if (g_app.couchSeatsTaken[i] == id) return i;
    }
    for (int i = 0; i < n; ++i) {
        if (g_app.couchSeatsTaken[i] == -1 || FindStickmanById(g_app.couchSeatsTaken[i]) == nullptr) {
            g_app.couchSeatsTaken[i] = id;
            return i;
        }
    }
    return -1;
}
static void ReleaseCouchSeat(int id) {
    for (int i = 0; i < 6; ++i) {
        if (g_app.couchSeatsTaken[i] == id) g_app.couchSeatsTaken[i] = -1;
    }
}

static Stickman* FindStickmanById(int id) {
    for (auto& s : g_app.stickmen) if (s.id == id) return &s;
    return nullptr;
}

// ============================================================================
// Particles, spawning, props
// ============================================================================
static void SpawnParticle(float x, float y, float vx, float vy, float size, float life,
                          BYTE r, BYTE g, BYTE b, int kind) {
    if (g_app.particles.size() > 300) return;
    g_app.particles.push_back({x, y, vx, vy, size, life, life, r, g, b, kind});
}

static void SpawnBurst(float x, float y, BYTE r, BYTE g, BYTE b, int count, int kind) {
    for (int i = 0; i < count; ++i) {
        float ang = RandRange(0.0f, TAU_F);
        float spd = RandRange(40.0f, 190.0f);
        SpawnParticle(x, y, std::cos(ang) * spd, std::sin(ang) * spd - 35.0f,
                      RandRange(2.5f, 5.5f), RandRange(0.25f, 0.55f), r, g, b, kind);
    }
}

static void ConfigureStickmanStyle(Stickman& s, CharType type) {
    s.type = type;
    s.hollowHead = false;
    switch (type) {
        case CharType::TSC_ORANGE: s.r = 255; s.g = 122; s.b = 12; s.hollowHead = true; break;
        case CharType::RED:        s.r = 229; s.g = 37;  s.b = 33; break;
        case CharType::BLUE:       s.r = 30;  s.g = 144; s.b = 245; break;
        case CharType::GREEN:      s.r = 46;  s.g = 204; s.b = 64;  break;
        case CharType::YELLOW:     s.r = 255; s.g = 196; s.b = 12;  break;
        case CharType::TCO_BLACK:  s.r = 30;  s.g = 32;  s.b = 38;  s.hollowHead = true; break;
    }
}

static CharType PickRandomType() {
    CharType pool[5] = {CharType::TSC_ORANGE, CharType::RED, CharType::BLUE,
                        CharType::GREEN, CharType::YELLOW};
    return pool[RandInt(5)];
}

static void SpawnStickman(CharType type, float x, float y, bool hello) {
    if (g_app.stickmen.size() >= 14) return;
    Stickman s;
    s.id = g_app.nextStickmanId++;
    ConfigureStickmanStyle(s, type);
    s.x = ClampF(x, 30.0f, (float)g_app.width - 30.0f);
    s.y = y;
    s.vx = RandRange(-90.0f, 90.0f);
    s.vy = -60.0f;
    s.facing = (s.vx >= 0.0f) ? 1 : -1;
    s.act = Act::AIRBORNE;
    s.actTimer = 1.0f;
    s.doingFlip = (RandInt(3) == 0);
    s.pendingHello = hello;
    s.socialCooldown = RandRange(1.5f, 4.0f);
    s.sparCooldown = RandRange(60.0f, 160.0f);
    SpawnBurst(x, y - 34.0f, s.r, s.g, s.b, 10, 2);
    g_app.stickmen.push_back(s);
    RecomputeCouchAnchor();
}

static void ResetSquad() {
    for (auto& s : g_app.stickmen) ReleaseCouchSeat(s.id);
    g_app.stickmen.clear();
    g_app.grabbedId = -1;
    g_app.deskOwnerId = -1;
    for (int i = 0; i < 6; ++i) g_app.couchSeatsTaken[i] = -1;
    float cx = (float)g_app.width * 0.5f;
    for (const auto& p : g_app.platforms) {
        if (p.isActive && p.rect.top > 90) { cx = (float)(p.rect.left + p.rect.right) * 0.5f; break; }
    }
    CharType squad[5] = {CharType::TSC_ORANGE, CharType::RED, CharType::BLUE,
                         CharType::GREEN, CharType::YELLOW};
    for (int i = 0; i < 5; ++i) {
        SpawnStickman(squad[i], cx + (i - 2) * 70.0f, 80.0f + (float)i * 16.0f, true);
    }
    g_app.helloTimer = 5.4f;
    RecomputeCouchAnchor();
}

// Everyone lands, turns to the user and waves "hi!"
static void SayHelloAll() {
    g_app.helloTimer = 4.6f;
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED) continue;
        s.pendingHello = true;
        if (s.grounded) {
            s.pendingHello = false;
            s.act = Act::WAVE_HELLO;
            s.actTimer = RandRange(2.6f, 4.2f);
            s.animTime = 0.0f;
            s.vx = 0.0f;
        }
    }
}

static void StartSparringMatch() {
    if (g_app.stickmen.size() < 2) {
        SpawnStickman(CharType::RED, (float)g_app.width * 0.5f + 80.0f, 130.0f, false);
    }
    if (g_app.stickmen.size() < 2) return;
    int a = RandInt((int)g_app.stickmen.size());
    int b = (a + 1 + RandInt((int)g_app.stickmen.size() - 1)) % (int)g_app.stickmen.size();
    Stickman& sa = g_app.stickmen[a];
    Stickman& sb = g_app.stickmen[b];
    LeaveSeat(sa);
    LeaveSeat(sb);
    sa.act = Act::SPARRING;
    sb.act = Act::SPARRING;
    sa.partnerId = sb.id;
    sb.partnerId = sa.id;
    sa.actTimer = 5.0f;
    sb.actTimer = 5.0f;
    sa.hasSword = (RandInt(2) == 0);
    sb.hasSword = sa.hasSword;
    sa.swordTimer = 5.0f;
    sb.swordTimer = 5.0f;
    sa.sparCooldown = RandRange(150.0f, 260.0f);
    sb.sparCooldown = sa.sparCooldown;
    SpawnParticle((sa.x + sb.x) * 0.5f, (sa.y + sb.y) * 0.5f - 34.0f, 0, 0, 14.0f, 0.35f, 255, 230, 90, 1);
}

static void CallAllToCursor() {
    POINT pt = {};
    GetCursorPos(&pt);
    float tx = (float)(pt.x - g_app.originX);
    float ty = (float)(pt.y - g_app.originY);
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED) continue;
        LeaveSeat(s);
        float dx = tx - s.x;
        s.facing = (dx >= 0.0f) ? 1 : -1;
        s.vx = ClampF(dx * 1.3f, -520.0f, 520.0f);
        float dy = ty - s.y;
        s.vy = ClampF(-430.0f + std::min(0.0f, dy * 0.6f), -760.0f, -260.0f);
        s.grounded = false;
        s.standingHwnd = nullptr;
        s.act = Act::AIRBORNE;
        s.doingFlip = (std::fabs(dx) > 190.0f);
        s.pendingHello = true;   // they greet you where they land
        s.squashVel = -4.0f;      // stretch as they launch
        SpawnBurst(s.x, s.y - 4.0f, 220, 225, 235, 5, 0);
    }
    g_app.helloTimer = 4.2f;
}

static void DropSnackAtCursor(int kind) {
    if (g_app.snacks.size() >= 6) g_app.snacks.erase(g_app.snacks.begin());
    Snack sn;
    sn.kind = kind;
    sn.bites = (kind == 1) ? 4.0f : (kind == 2) ? 5.0f : (kind == 3) ? 2.0f : 3.0f;
    sn.x = g_app.cursor.x;
    sn.y = std::min(g_app.cursor.y, (float)FloorYAt(g_app.cursor.x) - 60.0f);
    sn.vx = RandRange(-40.0f, 40.0f);
    sn.vy = 30.0f;
    sn.life = 40.0f;
    g_app.snacks.push_back(sn);
    // Nearest free stickman runs to it
    Stickman* best = nullptr;
    float bestD = 1e9f;
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED || s.act == Act::SPARRING) continue;
        float d = std::fabs(s.x - sn.x) + std::fabs(s.y - sn.y) * 0.6f;
        if (d < bestD) { bestD = d; best = &s; }
    }
    if (best) {
        LeaveSeat(*best);
        best->nextAct = Act::EAT_SNACK;
        best->travelTargetX = sn.x;
        best->act = Act::TRAVEL;
        best->actTimer = 9.0f;
        best->snackBite = 0;
    }
}

static void SpawnBallAtCursor() {
    if (g_app.balls.size() >= 4) g_app.balls.erase(g_app.balls.begin());
    Ball b;
    b.x = g_app.cursor.x;
    b.y = std::min(g_app.cursor.y, (float)FloorYAt(g_app.cursor.x) - 40.0f);
    b.vx = RandRange(-220.0f, 220.0f);
    b.vy = -160.0f;
    b.r = 11.0f * g_app.scale;
    g_app.balls.push_back(b);
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED) continue;
        if (std::fabs(s.x - b.x) < 340.0f && s.grounded) {
            LeaveSeat(s);
            s.act = Act::PLAY_BALL;
            s.actTimer = RandRange(6.0f, 11.0f);
            break;
        }
    }
}

static void SetScaleIdx(int idx) {
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    g_app.scaleIdx = idx;
    float scales[3] = {0.78f, 1.0f, 1.32f};
    g_app.scale = scales[g_app.scaleIdx];
    RecomputeCouchAnchor();
}

// Free a stickman from couch/desk slots when it needs to move somewhere
static void LeaveSeat(Stickman& s) {
    if (s.seatIndex >= 0) { ReleaseCouchSeat(s.id); s.seatIndex = -1; }
    if (g_app.deskOwnerId == s.id) g_app.deskOwnerId = -1;
    s.actTimer = 0.0f;
}

// ============================================================================
// Forward-kinematics pose builder (v2).
// Every limb is driven by two ABSOLUTE angles, so elbows/knees can only ever
// sit where a real skeleton puts them: elbows bend forward, knees bend back.
// No endpoint interpolation => no "winged elbow" artefacts while walking.
// ============================================================================
static const float kSitHipRaise = -(kProp.hipH - kProp.lLeg); // seat height = shin length

static FigAngles BaseFig() {
    FigAngles a;
    a.spine = 0.05f;  a.headA = 0.0f;
    a.upArmL = -0.09f; a.foreArmL = 0.30f;
    a.upArmR =  0.11f; a.foreArmR = 0.26f;
    a.thighL = -0.11f; a.shinL = -0.03f;
    a.thighR =  0.12f; a.shinR =  0.04f;
    a.hipRaise = 0.0f; a.lat = 0.0f; a.hop = 0.0f;
    return a;
}

static FigAngles ComputeAngles(const Stickman& s, Vec2 cursorLocal) {
    FigAngles a = BaseFig();
    const float t = s.animTime;
    const float ph = s.phase;
    const int f = s.facing;
    (void)f;

    switch (s.act) {
        case Act::IDLE: {
            float br = Osc(t, 2.1f);
            a.spine = 0.04f + br * 0.02f;
            a.hipRaise = br * 0.5f;
            a.upArmL = -0.09f + br * 0.04f;  a.foreArmL = 0.30f + br * 0.06f;
            a.upArmR =  0.11f - br * 0.04f;  a.foreArmR = 0.26f + br * 0.06f;
            // idle shuffles the weight from foot to foot
            float sh = Osc(t, 0.9f) * 0.10f;
            a.thighL = -0.11f + sh; a.shinL = -0.03f + sh;
            a.thighR =  0.12f + sh; a.shinR =  0.04f + sh;
            break;
        }

        case Act::WALK:
        case Act::RUN:
        case Act::TRAVEL: {
            bool run = (s.act == Act::RUN) ||
                       (s.act == Act::TRAVEL && std::fabs(s.vx) > 175.0f);
            float amp    = run ? 1.05f : 0.62f;
            float legAmp = run ? 0.95f : 0.58f;
            float armFlex= run ? 1.35f : 0.75f;
            float kneeAmp= run ? 1.25f : 1.0f;
            float s1 = std::sin(ph), s2 = std::sin(ph + PI_F);
            float c1 = std::cos(ph), c2 = std::cos(ph + PI_F);
            a.spine = run ? 0.34f : 0.10f;
            a.hipRaise = (run ? 2.6f : 1.3f) * std::fabs(std::sin(ph)) - (run ? 1.2f : 0.5f);
            a.headA = run ? 0.16f : 0.05f;
            a.upArmL = s1 * amp * 0.85f - 0.02f;
            a.foreArmL = a.upArmL + armFlex - 0.28f * s1;
            a.upArmR = s2 * amp * 0.85f - 0.02f;
            a.foreArmR = a.upArmR + armFlex - 0.28f * s2;
            a.thighL = s1 * legAmp;
            a.shinL  = a.thighL - ClampF(0.18f - c1 * 0.75f, 0.0f, 1.9f) * kneeAmp;
            a.thighR = s2 * legAmp;
            a.shinR  = a.thighR - ClampF(0.18f - c2 * 0.75f, 0.0f, 1.9f) * kneeAmp;
            break;
        }

        case Act::AIRBORNE: {
            if (s.doingFlip) {
                a.spine = 0.90f;
                a.upArmL = 1.50f; a.foreArmL = 0.50f;
                a.upArmR = 1.60f; a.foreArmR = 0.60f;
                a.thighL = 1.50f; a.shinL = 2.90f;   // knees tucked to the chest
                a.thighR = 1.40f; a.shinR = 2.80f;
            } else {
                float ar = ClampF(-s.vy / 520.0f, -1.0f, 1.0f); // +1 rising, -1 falling
                a.spine = 0.10f - ar * 0.12f;
                a.headA = -0.10f;
                a.upArmL =  2.20f + ar * 0.50f; a.foreArmL =  1.60f + ar * 0.70f;
                a.upArmR = -1.90f - ar * 0.50f; a.foreArmR = -1.30f - ar * 0.70f;
                a.thighL = 0.75f - ar * 0.20f;  a.shinL = 0.30f - ar * 0.30f;
                a.thighR = -0.55f + ar * 0.20f; a.shinR = -1.10f + ar * 0.40f;
            }
            break;
        }

        case Act::WALL_CLIMB: {
            float cyc = t * 9.5f;
            a.spine = 0.42f;
            a.hipRaise = -3.0f;
            a.headA = 0.10f;
            a.upArmL = 2.62f + std::sin(cyc) * 0.30f;
            a.foreArmL = 2.30f + std::sin(cyc) * 0.35f;
            a.upArmR = 2.62f - std::sin(cyc) * 0.30f;
            a.foreArmR = 2.30f - std::sin(cyc) * 0.35f;
            a.thighL = 1.05f + std::sin(cyc) * 0.22f;
            a.shinL  = 0.10f + std::sin(cyc) * 0.30f;
            a.thighR = 0.95f - std::sin(cyc) * 0.22f;
            a.shinR  = 0.05f - std::sin(cyc) * 0.30f;
            break;
        }

        case Act::SIT_EDGE: {
            // Butt on the window's top bar, legs dangling down the window face
            a.hipRaise = -22.0f;
            a.spine = 0.12f + Osc(t, 1.6f) * 0.03f;
            a.headA = 0.20f;
            a.upArmL = 1.25f; a.foreArmL = 1.05f + Osc(t, 2.2f) * 0.06f;
            a.upArmR = 1.40f; a.foreArmR = 1.22f - Osc(t, 2.2f) * 0.06f;
            float dangle = Osc(t, 2.6f) * 0.34f;
            a.thighL = 1.05f; a.shinL = 0.15f + dangle;
            a.thighR = 1.12f; a.shinR = 0.22f + dangle * 0.75f;
            break;
        }

        case Act::PEEK_WINDOW: {
            // Lying prone on the window's top bar, peering into the window
            a.hipRaise = -19.5f;
            a.spine = 1.30f;
            a.headA = 0.55f;
            a.lat = -6.0f;
            a.upArmL = 1.35f; a.foreArmL = 1.20f;
            a.upArmR = 1.55f; a.foreArmR = 1.45f;
            a.thighL = -1.35f; a.shinL = -1.05f;
            a.thighR = -1.28f; a.shinR = -0.95f;
            float kick = Osc(t, 3.1f);
            a.shinL += kick * 0.22f; a.shinR -= kick * 0.22f;
            break;
        }

        case Act::GRABBED: {
            // Dangling from the cursor: everything flails, the body swings
            float sw = s.grabAngle;
            a.spine = 0.05f + sw * 0.5f;
            a.headA = -sw * 0.8f;
            a.hipRaise = -2.0f;
            a.upArmL =  2.35f + Osc(t, 9.0f) * 0.35f; a.foreArmL =  2.70f;
            a.upArmR = -2.20f - Osc(t, 9.0f) * 0.35f; a.foreArmR = -2.60f;
            float flail = t * 8.0f;
            a.thighL = 0.30f + std::sin(flail) * 0.45f;
            a.shinL  = -0.45f + std::sin(flail + 1.1f) * 0.55f;
            a.thighR = -0.20f + std::sin(flail + 2.4f) * 0.45f;
            a.shinR  =  0.35f + std::sin(flail + 3.5f) * 0.55f;
            break;
        }

        case Act::WAVE_HELLO: {
            float w = Osc(t, 8.5f);
            a.spine = 0.05f + 0.03f * Osc(t, 4.2f);
            a.hipRaise = 0.30f * Bounce(t, 4.2f);
            a.headA = 0.10f + 0.05f * w;
            a.upArmL = -0.16f; a.foreArmL = 0.40f;      // relaxed arm
            a.upArmR =  2.42f + 0.07f * w;               // waving arm up
            a.foreArmR = 2.72f + 0.52f * w;
            a.thighL = -0.07f; a.shinL = -0.02f;
            a.thighR =  0.07f; a.shinR =  0.03f;
            break;
        }

        case Act::CHEER: {
            float b = Bounce(t, 6.5f);
            a.spine = -0.05f + 0.04f * Osc(t, 6.5f);
            a.hipRaise = 2.0f * b;
            a.hop = 5.0f * b;
            a.headA = -0.22f;
            a.upArmL = -2.62f + 0.12f * Osc(t, 6.5f); a.foreArmL = -2.95f;
            a.upArmR =  2.62f - 0.12f * Osc(t, 6.5f); a.foreArmR =  2.95f;
            a.thighL = -0.22f; a.shinL = -0.10f;
            a.thighR =  0.22f; a.shinR =  0.10f;
            break;
        }

        case Act::HIGH_FIVE: {
            float reach = ClampF(1.0f - s.actTimer * 0.35f, 0.0f, 1.0f);
            a.spine = 0.12f;
            a.hop = Bounce(t, 9.0f) * (2.0f + 4.0f * reach);
            a.headA = 0.12f;
            a.upArmL = -0.10f; a.foreArmL = 0.30f;
            a.upArmR = LerpF(1.30f, 2.30f, reach);
            a.foreArmR = LerpF(1.10f, 2.45f, reach);
            a.thighL = -0.20f; a.shinL = -0.10f;
            a.thighR =  0.24f; a.shinR =  0.06f;
            break;
        }

        case Act::POINT_AT: {
            a.spine = 0.10f;
            a.headA = 0.28f;
            a.upArmR = 1.35f + Osc(t, 2.4f) * 0.05f;
            a.foreArmR = 1.42f;
            a.upArmL = -0.15f; a.foreArmL = 0.50f;
            break;
        }

        case Act::SPARRING:
        case Act::FIGHT_CURSOR: {
            float p = t * 9.0f;
            float jab = ClampF(std::sin(p * 1.35f + 0.6f), 0.0f, 1.0f);
            float kickPhase = ClampF(std::sin(p * 0.55f - 1.2f), 0.0f, 1.0f);
            a.spine = 0.20f + jab * 0.14f - kickPhase * 0.36f;
            a.hipRaise = -2.0f + kickPhase * 1.0f;
            a.lat = -2.0f;
            a.headA = 0.05f + jab * 0.10f;
            a.upArmL = 1.15f; a.foreArmL = 2.55f;                       // guard hand
            a.upArmR = 1.30f + jab * 0.15f; a.foreArmR = 2.35f - jab * 0.95f;
            a.thighL = -0.42f - jab * 0.03f; a.shinL = -0.75f;          // back leg
            a.thighR = 0.30f + kickPhase * 1.15f;
            a.shinR  = 0.05f - kickPhase * 1.05f + jab * 0.30f;
            a.hop = kickPhase * 2.0f;
            break;
        }

        case Act::FLINCH: {
            float j = Bounce(t, 13.0f);
            a.spine = -0.28f;
            a.hipRaise = -3.0f;
            a.hop = 3.0f * j;
            a.headA = -0.30f;
            a.upArmL = -1.90f + Osc(t, 13.0f) * 0.30f; a.foreArmL = -1.20f;
            a.upArmR =  2.05f - Osc(t, 13.0f) * 0.30f; a.foreArmR =  1.35f;
            a.thighL = -0.30f; a.shinL = -0.70f;
            a.thighR =  0.35f; a.shinR = -0.20f;
            break;
        }

        case Act::WATCH_TV:
        case Act::EAT_POPCORN: {
            float chomp = (s.act == Act::EAT_POPCORN)
                ? ClampF(std::sin(s.propT * 3.1f) * 1.6f, 0.0f, 1.0f) : 0.0f;
            a.hipRaise = kSitHipRaise + Osc(t, 1.3f) * 0.25f;
            a.spine = -0.10f + 0.05f * Osc(t, 1.4f) + chomp * 0.06f;
            a.headA = 0.42f - 0.30f * chomp + Osc(t, 0.8f) * 0.05f;
            a.upArmL = 1.95f; a.foreArmL = 1.70f;                     // arm along the backrest
            a.upArmR = LerpF(1.00f, 2.05f, chomp);
            a.foreArmR = LerpF(0.85f, -1.51f, chomp);                 // hand to the mouth
            float nod = Osc(t, 1.1f) * 0.05f;
            a.thighL = 1.55f; a.shinL = 0.02f + nod;
            a.thighR = 1.50f; a.shinR = -0.04f - nod;
            break;
        }

        case Act::MINI_LAPTOP: {
            float dip = s.keyAnim;
            a.hipRaise = kSitHipRaise - 0.4f + Osc(t, 1.2f) * 0.2f;
            a.spine = 0.42f + Osc(t, 0.9f) * 0.04f;
            a.headA = 0.30f + dip * 0.06f;
            a.upArmL = 1.18f - dip * 0.10f; a.foreArmL = 1.02f - dip * 0.16f;
            a.upArmR = 1.34f - dip * 0.16f; a.foreArmR = 1.18f - dip * 0.10f;
            a.thighL = 1.52f; a.shinL = 0.05f;
            a.thighR = 1.46f; a.shinR = 0.00f;
            break;
        }

        case Act::SKETCH: {
            float draw = Osc(s.propT, 7.0f);
            a.hipRaise = -15.5f;
            a.spine = 0.55f + draw * 0.03f;
            a.headA = 0.35f;
            a.upArmL = 0.95f; a.foreArmL = 0.62f;                     // far arm for balance
            a.upArmR = 1.15f + draw * 0.10f;
            a.foreArmR = 0.80f + draw * 0.22f;                        // pencil hand
            a.thighL = 1.25f; a.shinL = -0.55f;
            a.thighR = 1.05f; a.shinR = -0.35f;
            break;
        }

        case Act::BUILD: {
            float lift = ClampF(std::sin(s.propT * 2.4f) * 1.4f, 0.0f, 1.0f);
            a.hipRaise = -1.5f - lift * 1.0f;
            a.spine = 0.16f - lift * 0.10f;
            a.headA = 0.20f - lift * 0.35f;
            a.upArmL = LerpF(1.10f, 2.40f, lift); a.foreArmL = LerpF(0.95f, 2.62f, lift);
            a.upArmR = LerpF(1.25f, 2.50f, lift); a.foreArmR = LerpF(1.05f, 2.70f, lift);
            a.thighL = -0.18f; a.shinL = -0.06f;
            a.thighR =  0.20f; a.shinR =  0.05f;
            break;
        }

        case Act::MUSIC: {
            float strum = Osc(t, 8.0f);
            a.hipRaise = -1.2f + Bounce(t, 4.0f) * 0.6f;
            a.spine = 0.16f - strum * 0.03f;
            a.headA = -0.15f + Osc(t, 2.0f) * 0.06f;
            a.upArmL = 1.70f; a.foreArmL = 1.35f + strum * 0.05f;     // fretting hand
            a.upArmR = 0.80f + strum * 0.10f; a.foreArmR = 0.55f + strum * 0.35f;
            a.thighL = -0.20f; a.shinL = -0.05f;
            a.thighR =  0.22f + strum * 0.05f; a.shinR = 0.05f;
            break;
        }

        case Act::FISH: {
            float jerk = ClampF(-Osc(s.fishT, 1.6f) - 0.72f, 0.0f, 1.0f);
            a.hipRaise = -2.0f - jerk * 1.2f;
            a.spine = 0.22f + jerk * 0.20f;
            a.headA = 0.30f + jerk * 0.20f;
            a.upArmL = 1.85f; a.foreArmL = 1.95f;
            a.upArmR = 1.70f - jerk * 0.9f; a.foreArmR = 1.60f - jerk * 1.1f;
            a.thighL = -0.30f; a.shinL = -0.20f;
            a.thighR =  0.10f; a.shinR =  0.05f;
            break;
        }

        case Act::READ: {
            float page = ClampF(std::sin(s.propT * 1.1f + 1.0f) * 3.0f, 0.0f, 1.0f);
            a.hipRaise = -20.5f;
            a.spine = 0.35f + Osc(t, 0.7f) * 0.03f;
            a.headA = 0.45f - page * 0.35f;
            a.upArmL = 1.00f + page * 0.5f; a.foreArmL = 1.62f - page * 0.9f;
            a.upArmR = 1.12f; a.foreArmR = 1.72f;
            a.thighL = 1.42f; a.shinL = 1.52f;
            a.thighR = 1.36f; a.shinR = 1.46f;
            break;
        }

        case Act::CAMPFIRE: {
            float poke = ClampF(std::sin(s.propT * 2.0f) * 1.8f, 0.0f, 1.0f);
            a.hipRaise = -12.0f;
            a.spine = 0.40f + poke * 0.10f;
            a.headA = 0.35f;
            a.upArmL = 1.05f; a.foreArmL = 0.85f;
            a.upArmR = 1.20f + poke * 0.25f; a.foreArmR = 0.95f + poke * 0.20f;
            a.thighL = 1.30f; a.shinL = 0.20f;      // front foot planted
            a.thighR = -0.10f; a.shinR = -1.40f;    // back knee on the ground
            break;
        }

        case Act::NAP: {
            float br = Osc(t, 1.5f);
            a.hipRaise = -21.5f;
            a.lat = -4.0f;
            a.spine = 1.45f + br * 0.015f;
            a.headA = 0.10f - br * 0.04f;
            a.upArmL = 2.20f; a.foreArmL = -2.60f;   // folded arms as a pillow
            a.upArmR = 1.60f + br * 0.05f; a.foreArmR = 1.50f;
            a.thighL = -1.50f; a.shinL = -1.45f;
            a.thighR = -1.44f; a.shinR = -1.40f + br * 0.05f;
            break;
        }

        case Act::EAT_SNACK: {
            float bite = ClampF(std::sin(s.propT * 4.4f) * 1.7f, 0.0f, 1.0f);
            a.spine = 0.10f + bite * 0.16f;
            a.headA = 0.10f - bite * 0.35f;
            a.hipRaise = -bite * 0.8f;
            a.upArmR = LerpF(1.05f, 2.10f, bite);
            a.foreArmR = LerpF(0.80f, -1.45f, bite);
            a.upArmL = -0.20f; a.foreArmL = 0.55f;
            a.thighL = -0.14f; a.shinL = -0.05f;
            a.thighR =  0.16f; a.shinR =  0.06f;
            break;
        }

        case Act::PLAY_BALL:
        case Act::PASS_BALL: {
            float wind = ClampF(-std::cos(s.phase * 2.0f), 0.0f, 1.0f);
            a.spine = 0.20f + wind * 0.10f;
            a.headA = 0.30f;
            a.hipRaise = -1.0f;
            a.upArmL = 1.60f + wind * 0.30f; a.foreArmL = 1.30f;
            a.upArmR = -1.20f - wind * 0.30f; a.foreArmR = -0.90f;
            a.thighL = -0.20f; a.shinL = -0.10f;
            a.thighR = 0.30f + wind * 1.10f; a.shinR = 0.05f - wind * 0.85f;
            break;
        }

        // ------------------------------------------------------------ v3 acts
        case Act::CARD_GAME: {
            float slam = ClampF(-std::cos(s.actPhase * 1.3f) - 0.75f, 0.0f, 1.0f);
            a.hipRaise = kSitHipRaise - 0.5f;
            a.spine = 0.30f + slam * 0.14f;
            a.headA = 0.30f - slam * 0.30f;
            a.upArmL = 1.10f; a.foreArmL = 1.05f;
            a.upArmR = LerpF(1.05f, 1.55f, slam); a.foreArmR = LerpF(0.95f, 0.55f, slam);
            a.thighL = 1.42f; a.shinL = 0.30f;
            a.thighR = 1.36f; a.shinR = 0.24f;
            break;
        }

        case Act::WATCH_GAME: {
            // clapping: hands meet in front of the chest, then spring apart
            float clap = 0.5f + 0.5f * std::sin(t * 6.4f);
            a.spine = 0.10f + clap * 0.05f;
            a.hipRaise = Bounce(t, 3.2f) * 0.8f;
            a.headA = 0.18f;
            a.upArmL = 1.12f - clap * 0.18f;  a.foreArmL = 1.05f + clap * 0.55f;
            a.upArmR = 1.28f + clap * 0.18f;  a.foreArmR = 1.15f - clap * 0.55f;
            break;
        }

        case Act::COOK: {
            float st = s.actPhase * 2.4f;
            a.hipRaise = -1.5f;
            a.spine = 0.28f + std::cos(st) * 0.04f;
            a.headA = 0.34f;
            a.upArmL = 1.20f; a.foreArmL = 0.90f;
            a.upArmR = 1.26f + std::sin(st) * 0.10f;
            a.foreArmR = 0.72f + std::cos(st) * 0.22f;
            a.thighL = -0.16f; a.shinL = -0.06f;
            a.thighR =  0.18f; a.shinR =  0.04f;
            break;
        }

        case Act::QUEUE: {
            float rub = ClampF(std::sin(s.propT * 1.5f) * 1.5f, 0.0f, 1.0f);
            a.spine = 0.06f;
            a.headA = 0.16f - rub * 0.30f;
            a.upArmL = 1.28f; a.foreArmL = 0.60f + rub * 0.5f;   // hands on the tummy
            a.upArmR = 1.34f; a.foreArmR = 0.55f + rub * 0.5f;
            a.thighL = -0.10f + rub * 0.05f; a.thighR = 0.12f - rub * 0.05f;
            a.lat = Osc(t, 0.7f) * 0.6f;
            break;
        }

        case Act::LAB: {
            float tweak = Osc(t, 5.4f);
            a.hipRaise = kSitHipRaise - 1.0f;
            a.spine = 0.46f + tweak * 0.03f;
            a.headA = 0.36f;
            a.upArmL = 1.05f + tweak * 0.06f; a.foreArmL = 0.86f;
            a.upArmR = 1.18f - tweak * 0.08f; a.foreArmR = 0.92f;
            a.thighL = 1.44f; a.shinL = 0.20f;
            a.thighR = 1.38f; a.shinR = 0.16f;
            break;
        }

        case Act::ARMWRESTLE: {
            float prog = std::sin(s.actPhase * 3.2f);
            a.hipRaise = kSitHipRaise - 1.5f;
            a.spine = 0.34f + prog * 0.12f * (float)s.facing * -1.0f;
            a.lat = -prog * 2.4f;
            a.headA = 0.24f;
            a.upArmL = 1.30f; a.foreArmL = 0.30f;                     // free hand on the table
            a.upArmR = 1.36f - prog * 0.20f; a.foreArmR = -0.10f - prog * 0.35f;
            a.thighL = 1.36f; a.shinL = 0.30f;
            a.thighR = 1.30f; a.shinR = 0.26f;
            break;
        }

        case Act::TUGWAR: {
            float pull = 0.5f + 0.5f * std::sin(s.actPhase * 2.2f);
            a.hipRaise = -19.0f;                    // sitting on the floor, leaning back
            a.spine = -0.55f - pull * 0.25f;
            a.headA = -0.30f;
            a.lat = -1.5f - pull * 2.0f;
            a.upArmL = 1.62f + pull * 0.10f; a.foreArmL = 1.55f;
            a.upArmR = 1.70f + pull * 0.10f; a.foreArmR = 1.60f;
            a.thighL = 1.20f; a.shinL = 0.55f + pull * 0.2f;
            a.thighR = 1.14f; a.shinR = 0.50f + pull * 0.2f;
            break;
        }

        case Act::HANDSHAKE: {
            float pump = Osc(t, 5.0f);
            a.hipRaise = -1.0f;
            a.spine = 0.26f - pump * 0.04f;
            a.headA = 0.20f;
            a.upArmL = -0.20f; a.foreArmL = 0.40f;
            a.upArmR = 1.42f; a.foreArmR = 1.28f + pump * 0.10f;      // arm out, gripping
            a.thighL = -0.12f; a.shinL = -0.03f;
            a.thighR =  0.16f; a.shinR =  0.05f;
            break;
        }

        case Act::SPY_CHAT: {
            float lift = Osc(t, 1.1f) * 0.05f;
            a.hipRaise = -1.5f;
            a.spine = 0.34f;
            a.headA = 0.10f + lift;
            a.upArmR = 1.62f; a.foreArmR = -1.62f + lift;             // magnifier to the eye
            a.upArmL = 1.20f; a.foreArmL = 1.05f;                     // hand on the window frame
            a.thighL = -0.20f; a.shinL = -0.08f;
            a.thighR =  0.14f; a.shinR =  0.06f;
            break;
        }

        case Act::LAUGH: {
            float sh = std::sin(t * 11.0f);
            a.hipRaise = -17.0f;                    // on the floor, holding the belly
            a.spine = 0.95f + sh * 0.10f;
            a.headA = 0.55f;
            a.lat = -3.0f + sh * 1.5f;
            a.hop = std::fabs(sh) * 1.5f;
            a.upArmL = 0.90f; a.foreArmL = 0.30f + sh * 0.15f;
            a.upArmR = 1.05f; a.foreArmR = 0.20f - sh * 0.15f;
            a.thighL = 1.30f; a.shinL = 0.50f;
            a.thighR = 1.24f; a.shinR = 0.44f;
            break;
        }

        case Act::FACEPALM: {
            float down = ClampF(1.0f - s.actTimer * 0.7f, 0.0f, 1.0f);
            a.spine = 0.20f + down * 0.25f;
            a.headA = 0.30f + down * 0.25f;
            a.hipRaise = -1.0f;
            a.upArmR = LerpF(1.10f, 1.72f, down);
            a.foreArmR = LerpF(1.00f, -1.85f, down);                   // palm onto the face
            a.upArmL = -0.10f; a.foreArmL = 0.55f;
            break;
        }

        case Act::YAWN: {
            float y = ClampF(std::sin(s.propT * 1.6f) * 1.7f, 0.0f, 1.0f);
            a.spine = -0.10f - y * 0.14f;
            a.headA = -0.45f - y * 0.35f;
            a.hipRaise = -y * 1.0f;
            a.upArmL = -0.20f; a.foreArmL = 0.40f;
            a.upArmR = 1.70f + y * 0.12f; a.foreArmR = -1.35f;        // hand over the mouth
            a.thighL = -0.14f; a.shinL = -0.05f;
            a.thighR =  0.16f; a.shinR =  0.05f;
            break;
        }

        case Act::DANCE: {
            float b = std::sin(s.actPhase * 2.0f);
            float sw = std::cos(s.actPhase * 1.0f);
            a.hipRaise = Bounce(t, 4.6f) * 1.6f;
            a.spine = 0.10f + b * 0.08f;
            a.lat = sw * 2.2f;
            a.headA = -0.10f + b * 0.14f;
            a.upArmL = -1.90f - b * 0.55f; a.foreArmL = -2.30f - b * 0.35f;
            a.upArmR =  1.90f + b * 0.55f; a.foreArmR =  2.30f + b * 0.35f;
            a.thighL = -0.16f + sw * 0.10f; a.shinL = -0.10f + sw * 0.30f;
            a.thighR =  0.20f + sw * 0.10f; a.shinR =  0.08f - sw * 0.30f;
            break;
        }

        case Act::CURSOR_PLAY: {
            float wig = Osc(t, 6.0f);
            a.spine = 0.14f;
            a.headA = 0.20f + wig * 0.05f;
            a.upArmR = 1.30f + wig * 0.10f; a.foreArmR = 1.20f + wig * 0.18f;
            a.upArmL = -0.18f; a.foreArmL = 0.55f;
            break;
        }

        case Act::HANG_CURSOR: {
            float sw = s.grabAngle;
            a.spine = 0.02f + sw * 0.55f;
            a.headA = -sw * 0.5f;
            a.upArmL = -2.72f + sw * 0.20f; a.foreArmL = -2.98f;
            a.upArmR =  2.72f + sw * 0.20f; a.foreArmR =  2.98f;
            a.thighL = 0.30f + sw * 0.4f; a.shinL = 0.55f;
            a.thighR = 0.16f + sw * 0.4f; a.shinR = 0.40f;
            a.hipRaise = -1.0f;
            break;
        }

        case Act::PETTED: {
            float lean = Osc(t, 1.5f);
            a.spine = 0.06f + lean * 0.03f;
            a.headA = -0.18f + lean * 0.05f;              // chin up, eyes closed
            a.hipRaise = 0.3f;
            a.upArmL = -0.14f; a.foreArmL = 0.20f;
            a.upArmR =  0.14f; a.foreArmR = 0.24f;
            a.lat = lean * 0.8f;
            a.thighL = -0.08f; a.shinL = -0.02f;
            a.thighR =  0.10f; a.shinR =  0.03f;
            break;
        }

        case Act::ASK_ATTENTION: {
            float w = Osc(t, 3.6f);
            a.spine = 0.10f;
            a.headA = 0.22f + w * 0.06f;
            a.upArmL = 1.35f; a.foreArmL = -1.85f;         // holds the little sign up
            a.upArmR = -0.10f + w * 0.30f; a.foreArmR = 0.35f;
            a.thighL = -0.10f; a.shinL = -0.03f;
            a.thighR =  0.12f; a.shinR =  0.04f;
            break;
        }

        case Act::TEA_RUN:
        case Act::BRING_FOLDER:
        case Act::DRAG_COUCH: {
            a.spine = 0.14f;
            a.headA = 0.26f;
            a.upArmL = 1.26f; a.foreArmL = 0.86f;          // carrying the tray flat
            a.upArmR = 1.30f; a.foreArmR = 0.84f;
            a.thighL = std::sin(s.phase) * 0.40f;
            a.shinL  = a.thighL - 0.30f;
            a.thighR = std::sin(s.phase + PI_F) * 0.40f;
            a.shinR  = a.thighR - 0.30f;
            break;
        }

        case Act::CRACK_PRANK: {
            float hit = ClampF(1.0f - s.actTimer * 0.6f, 0.0f, 1.0f);
            // 1 = hammer raised over the head, 0 = the strike landed (and he starts taping)
            float wind = ClampF((s.actTimer - 4.6f) / 2.4f, 0.0f, 1.0f);
            a.spine = 0.95f * hit + 0.2f - 0.16f * wind;
            a.headA = -0.22f + 0.12f * wind;            // chin up, staring at it
            a.hipRaise = -2.0f * hit;
            a.lat = hit * 3.0f;
            a.upArmL = 1.30f; a.foreArmL = 1.20f;
            a.upArmR = 1.36f + wind * 1.06f; a.foreArmR = 1.30f - wind * 0.62f;
            a.thighL = -0.30f; a.shinL = -0.20f;
            a.thighR =  0.40f; a.shinR = -0.30f * hit;
            break;
        }

        case Act::MUSTACHE: {
            float draw = Osc(s.propT, 6.2f);
            a.hipRaise = -15.0f;
            a.spine = 0.50f;
            a.headA = 0.40f;
            a.upArmL = 1.00f; a.foreArmL = 0.65f;
            a.upArmR = 1.18f + draw * 0.08f; a.foreArmR = 0.85f + draw * 0.2f;
            a.thighL = 1.22f; a.shinL = -0.50f;
            a.thighR = 1.00f; a.shinR = -0.30f;
            break;
        }

        case Act::BOX_KICK:
        case Act::ICON_KICK: {
            float wind = ClampF(-std::cos(s.actPhase * 1.8f), 0.0f, 1.0f);
            a.spine = 0.24f + wind * 0.12f;
            a.headA = 0.32f;
            a.hipRaise = -1.0f;
            a.upArmL = 1.70f + wind * 0.30f; a.foreArmL = 1.40f;
            a.upArmR = -1.30f - wind * 0.30f; a.foreArmR = -1.00f;
            a.thighL = -0.22f; a.shinL = -0.12f;
            a.thighR = 0.30f + wind * 1.25f; a.shinR = 0.05f - wind * 0.95f;
            break;
        }

        case Act::SQUISH: {
            float push = ClampF(s.actTimer * 1.4f, 0.0f, 1.0f);
            a.spine = 1.45f * push + 0.1f;              // flattened against the glass
            a.headA = 0.55f;
            a.hipRaise = -3.0f;
            a.lat = -3.0f * push;
            a.upArmL = 1.80f; a.foreArmL = 2.10f;
            a.upArmR = 1.85f; a.foreArmR = 2.15f;
            a.thighL = 0.90f; a.shinL = 1.30f;
            a.thighR = 0.80f; a.shinR = 1.20f;
            break;
        }

        case Act::EXTINGUISH: {
            float sweep = Osc(t, 2.4f);
            a.spine = -0.10f;
            a.headA = -0.28f;
            a.hipRaise = -1.5f;
            a.lat = -1.5f;
            a.upArmL = 1.40f; a.foreArmL = 1.10f;
            a.upArmR = 1.30f + sweep * 0.12f; a.foreArmR = 0.85f + sweep * 0.20f;
            a.thighL = -0.34f; a.shinL = -0.10f;
            a.thighR =  0.24f; a.shinR =  0.06f;
            break;
        }

        case Act::IN_VIDEO: {
            float w = Osc(t, 2.6f);
            a.spine = 0.05f + w * 0.04f;
            a.headA = 0.24f;
            a.upArmL = -0.30f + w * 0.2f; a.foreArmL = 0.4f;
            a.upArmR =  1.40f + w * 0.3f; a.foreArmR = 1.10f;
            break;
        }

        case Act::PUSH_WINDOW: {
            float strain = Osc(t, 7.5f);
            a.spine = 0.62f + strain * 0.03f;
            a.headA = 0.30f;
            a.hipRaise = -4.0f;
            a.lat = -2.0f;
            a.upArmL = 1.57f; a.foreArmL = 1.55f;
            a.upArmR = 1.58f; a.foreArmR = 1.56f;
            a.thighL = -0.62f + strain * 0.05f; a.shinL = -0.20f;
            a.thighR = -0.10f; a.shinR = 0.10f;
            break;
        }

        case Act::AIR_TYPE: {
            float dip = s.keyAnim;
            a.spine = 0.22f;
            a.headA = 0.30f + dip * 0.05f;
            a.hipRaise = -0.6f;
            a.upArmL = 1.30f - dip * 0.10f; a.foreArmL = 1.05f - dip * 0.20f;
            a.upArmR = 1.42f - dip * 0.14f; a.foreArmR = 1.10f - dip * 0.06f;
            a.thighL = -0.14f; a.shinL = -0.04f;
            a.thighR =  0.16f; a.shinR =  0.05f;
            break;
        }
        // ------------------------------------------------------------ v4 acts
        case Act::CHASE_CURSOR: {
            float lunge = ClampF(-std::cos(s.actPhase * 2.6f) - 0.35f, 0.0f, 1.0f);
            a.spine = 0.20f + lunge * 0.40f;
            a.headA = 0.10f + lunge * 0.10f;
            a.hipRaise = -1.0f - lunge * 1.5f;
            a.upArmL = 1.35f + lunge * 0.30f; a.foreArmL = 0.55f - lunge * 0.30f;
            a.upArmR = 1.45f + lunge * 0.35f; a.foreArmR = 0.45f - lunge * 0.25f;
            a.thighL = 0.30f + lunge * 0.55f; a.shinL = 0.10f;
            a.thighR = -0.30f - lunge * 0.25f; a.shinR = -0.20f;
            break;
        }

        case Act::RIDE_CURSOR: {
            float wob = Osc(t, 6.2f) * 0.05f;
            a.spine = 0.46f + wob;
            a.headA = -0.06f;
            a.hipRaise = -2.0f;
            a.lat = -1.5f;
            a.upArmL = -0.55f + wob * 2.0f; a.foreArmL = 0.65f;
            a.upArmR = -0.45f - wob * 2.0f; a.foreArmR = 0.55f;
            a.thighL = 0.62f; a.shinL = -0.55f;
            a.thighR = -0.42f; a.shinR = 0.95f;
            a.hop = 1.2f + std::fabs(Osc(t, 6.2f)) * 1.2f;
            break;
        }

        case Act::CURSOR_THEFT: {
            float strain = Osc(t, 5.2f) * 0.5f + 0.5f;
            a.spine = -0.24f - strain * 0.06f;
            a.headA = 0.34f;
            a.hipRaise = -2.0f - strain * 1.5f;
            a.lat = -1.5f;
            a.upArmL = 1.02f; a.foreArmL = 1.18f;
            a.upArmR = 1.08f; a.foreArmR = 1.24f;
            a.thighL = 0.52f; a.shinL = 0.26f;
            a.thighR = 0.44f; a.shinR = 0.22f;
            break;
        }

        case Act::SPEAK: {
            float nod = Osc(t, 2.2f) * 0.05f;
            float pts = ClampF(std::sin(s.speechT * 3.1f), 0.0f, 1.0f);
            a.spine = 0.10f;
            a.headA = 0.16f + nod;
            a.hipRaise = -0.4f;
            a.upArmL = 1.86f; a.foreArmL = 1.34f;                  // holds the pad up
            a.upArmR = LerpF(1.10f, 1.45f, pts); a.foreArmR = LerpF(0.95f, 0.35f, pts);
            a.thighL = -0.12f; a.shinL = -0.03f;
            a.thighR =  0.14f; a.shinR =  0.04f;
            break;
        }

        case Act::SEND_GUARD: {
            float nod = ClampF(g_app.typingPulse * 1.2f - 0.4f, 0.0f, 1.0f);
            a.hipRaise = kSitHipRaise - 1.0f;
            a.spine = 0.20f - nod * 0.06f;
            a.headA = 0.26f + nod * 0.22f;
            a.upArmL = 1.48f; a.foreArmL = 1.42f;                  // elbows on knees
            a.upArmR = 1.52f; a.foreArmR = 1.36f;
            a.thighL = 1.44f; a.shinL = 0.36f;
            a.thighR = 1.40f; a.shinR = 0.30f;
            break;
        }

        case Act::HIDE_ICON: {
            float peek = Osc(t, 0.9f);
            a.spine = 0.56f;
            a.headA = -0.24f + peek * 0.10f;
            a.hipRaise = -8.0f;
            a.upArmL = 1.78f; a.foreArmL = -1.92f;                 // hands clapped over the eyes
            a.upArmR = 1.84f; a.foreArmR = -1.98f;
            a.thighL = 1.58f; a.shinL = 1.16f;
            a.thighR = 1.52f; a.shinR = 1.10f;
            break;
        }

        case Act::TAB_OOPS: {
            float shock = ClampF(s.actTimer * 0.6f, 0.0f, 1.0f);
            a.spine = 0.06f - shock * 0.08f;
            a.headA = -0.30f;
            a.hipRaise = -4.0f - shock * 2.0f;
            a.upArmL = 2.02f; a.foreArmL = -2.38f;                 // hands on the head
            a.upArmR = 2.06f; a.foreArmR = -2.42f;
            a.thighL = 0.58f; a.shinL = 0.92f;
            a.thighR = 0.52f; a.shinR = 0.86f;
            break;
        }

        case Act::UMBRELLA: {
            float gust = Osc(t, 1.6f) * 0.06f;
            a.spine = 0.02f + gust;
            a.headA = -0.20f;
            a.hipRaise = -0.6f;
            a.upArmR = -2.42f + gust; a.foreArmR = -0.22f;         // right arm straight up, umbrella
            a.upArmL = 0.46f; a.foreArmL = 1.06f;
            a.thighL = -0.10f; a.shinL = -0.02f;
            a.thighR =  0.12f; a.shinR =  0.03f;
            break;
        }

        case Act::PLUG_IN: {
            float run = t * 9.2f;
            float s1 = std::sin(run), s2 = std::sin(run + PI_F);
            a.spine = 0.40f;
            a.headA = 0.14f;
            a.hipRaise = 2.0f * std::fabs(s1) - 1.0f;
            a.upArmL = -0.85f + s1 * 0.5f; a.foreArmL = 0.80f;
            a.upArmR = 1.34f + s2 * 0.18f; a.foreArmR = 0.42f;      // plug held forward
            a.thighL = s1 * 0.92f; a.shinL = s1 * 0.92f - 0.75f;
            a.thighR = s2 * 0.92f; a.shinR = s2 * 0.92f - 0.75f;
            break;
        }

    }


    // --- gaze: always aware of the pointer, and of whatever is moving ---
    if (s.act != Act::GRABBED && s.act != Act::AIRBORNE) {
        Vec2 headHint = V2(s.x + s.facing * 3.0f, s.y - 46.0f * g_app.scale);
        float dx = (cursorLocal.x - headHint.x) * (float)s.facing;
        float dy = (cursorLocal.y - headHint.y);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 1.0f && dist < 460.0f) {
            float want = std::atan2(dx, -dy) - a.spine;
            float w = ClampF(1.0f - dist / 460.0f, 0.0f, 1.0f);
            a.headA += ClampF(want, -1.05f, 1.05f) * 0.55f * w;
        }
    }
    return a;
}

// Build the world-space pose: local FK -> plant feet -> squash -> mirror -> scale
static Pose ComputePose(const Stickman& s, Vec2 cursorLocal, float& outHop) {
    FigAngles a = ComputeAngles(s, cursorLocal);
    const Proportions& P = kProp;
    bool plant = ActPlantsFeet(s.act) && s.grounded;

    Vec2 hip = V2(a.lat, -(P.hipH + a.hipRaise));
    Vec2 neck = Add(hip, Mul(DirUp(a.spine), P.spine));
    Vec2 head = Add(neck, Mul(DirUp(a.spine + a.headA), P.headR + 1.6f));
    Vec2 eL = Add(neck, Mul(DirDown(a.upArmL), P.uArm));
    Vec2 hL = Add(eL,   Mul(DirDown(a.foreArmL), P.lArm));
    Vec2 eR = Add(neck, Mul(DirDown(a.upArmR), P.uArm));
    Vec2 hR = Add(eR,   Mul(DirDown(a.foreArmR), P.lArm));
    Vec2 kL = Add(hip,  Mul(DirDown(a.thighL), P.uLeg));
    Vec2 fL = Add(kL,   Mul(DirDown(a.shinL), P.lLeg));
    Vec2 kR = Add(hip,  Mul(DirDown(a.thighR), P.uLeg));
    Vec2 fR = Add(kR,   Mul(DirDown(a.shinR), P.lLeg));

    Pose p;
    p.head = head; p.neck = neck; p.hip = hip;
    p.elbowL = eL; p.handL = hL; p.elbowR = eR; p.handR = hR;
    p.kneeL = kL; p.footL = fL;  p.kneeR = kR;  p.footR = fR;

    // Ground the lowest foot so there is never any floating / foot sliding.
    if (plant) {
        float dy = -std::max(p.footL.y, p.footR.y);
        if (std::fabs(dy) < 12.0f) {
            p.head.y += dy; p.neck.y += dy; p.hip.y += dy;
            p.elbowL.y += dy; p.handL.y += dy; p.elbowR.y += dy; p.handR.y += dy;
            p.kneeL.y += dy; p.footL.y += dy; p.kneeR.y += dy; p.footR.y += dy;
        }
    }

    // Squash & stretch (impact = flatten, launch = elongate)
    float sq = ClampF(s.squash, -0.35f, 0.40f);
    if (std::fabs(sq) > 0.001f) {
        float ky = 1.0f - sq, kx = 1.0f + sq * 0.45f;
        auto sqz = [&](Vec2& v) { v.x *= kx; v.y *= ky; };
        sqz(p.head); sqz(p.neck); sqz(p.hip);
        sqz(p.elbowL); sqz(p.handL); sqz(p.elbowR); sqz(p.handR);
        sqz(p.kneeL); sqz(p.footL); sqz(p.kneeR); sqz(p.footR);
    }

    // Parkour spin around the body centre
    if (s.doingFlip && s.act == Act::AIRBORNE) {
        Vec2 c = V2(0.0f, -30.0f);
        float ca = std::cos(s.flipAngle), sa = std::sin(s.flipAngle);
        auto rot = [&](Vec2& v) {
            float x = v.x - c.x, y = v.y - c.y;
            v = V2(c.x + x * ca - y * sa, c.y + x * sa + y * ca);
        };
        rot(p.head); rot(p.neck); rot(p.hip);
        rot(p.elbowL); rot(p.handL); rot(p.elbowR); rot(p.handR);
        rot(p.kneeL); rot(p.footL); rot(p.kneeR); rot(p.footR);
    }

    float sc = g_app.scale;
    float fh = (float)s.facing;
    auto toWorld = [&](Vec2 v) { return V2(s.x + v.x * fh * sc, s.y + v.y * sc - a.hop * sc); };
    p.head = toWorld(p.head); p.neck = toWorld(p.neck); p.hip = toWorld(p.hip);
    p.elbowL = toWorld(p.elbowL); p.handL = toWorld(p.handL);
    p.elbowR = toWorld(p.elbowR); p.handR = toWorld(p.handR);
    p.kneeL = toWorld(p.kneeL); p.footL = toWorld(p.footL);
    p.kneeR = toWorld(p.kneeR); p.footR = toWorld(p.footR);

    // Hanging from the cursor: the palms (not the neck) stick to the pointer
    if (s.act == Act::HANG_CURSOR) {
        Vec2 palmMid = Mul(Add(p.handL, p.handR), 0.5f);
        Vec2 want = cursorLocal;
        Vec2 off = V2((want.x - palmMid.x) / (fh * sc), (want.y - palmMid.y) / sc + a.hop);
        p.head = Add(p.head, off); p.neck = Add(p.neck, off); p.hip = Add(p.hip, off);
        p.elbowL = Add(p.elbowL, off); p.handL = Add(p.handL, off);
        p.elbowR = Add(p.elbowR, off); p.handR = Add(p.handR, off);
        p.kneeL = Add(p.kneeL, off); p.footL = Add(p.footL, off);
        p.kneeR = Add(p.kneeR, off); p.footR = Add(p.footR, off);
    }

    // While grabbed, the neck is welded to the cursor
    if (s.act == Act::GRABBED) {
        Vec2 off = Sub(V2(s.x, s.y), p.neck);
        p.head = Add(p.head, off); p.neck = Add(p.neck, off); p.hip = Add(p.hip, off);
        p.elbowL = Add(p.elbowL, off); p.handL = Add(p.handL, off);
        p.elbowR = Add(p.elbowR, off); p.handR = Add(p.handR, off);
        p.kneeL = Add(p.kneeL, off); p.footL = Add(p.footL, off);
        p.kneeR = Add(p.kneeR, off); p.footR = Add(p.footR, off);
    }
    outHop = a.hop * sc;
    return p;
}

// ============================================================================
// Sensing: what is the user actually doing right now?
// ============================================================================
static bool swingActive(float speed) { return std::fabs(speed) > 130.0f; }
static float swingNow() { return g_app.mouseVel.x; }

static void UpdateSensors(float dt) {
    // --- keystrokes (polled, no hooks, no injected input, nothing logged) ---
    static const int kKeys[] = {
        VK_SPACE, VK_BACK, VK_RETURN, VK_DELETE, VK_OEM_PERIOD, VK_OEM_COMMA,
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
    };
    int held = 0; bool pressed = false;
    for (int vk : kKeys) {
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down) ++held;
        if (down && !g_app.keyPrev[vk]) pressed = true;
        g_app.keyPrev[vk] = down;
    }
    float heldF = ClampF((float)held / 3.0f, 0.0f, 1.0f);
    g_app.typingRate = ApproachF(g_app.typingRate, heldF, 7.0f, dt);
    if (pressed) {
        g_app.typingPulse = 1.0f;
        g_app.typingRate = std::max(g_app.typingRate, 0.45f);
    } else {
        g_app.typingPulse = ApproachF(g_app.typingPulse, 0.0f, 8.0f, dt);
    }

    // --- user idling (AFK campfire / nap) ---
    LASTINPUTINFO lii = {};
    lii.cbSize = sizeof(lii);
    if (GetLastInputInfo(&lii)) {
        g_app.idleSeconds = (float)(GetTickCount() - lii.dwTime) / 1000.0f;
    }
    bool afkNow = (g_app.idleSeconds > 95.0f) && !g_app.couchOn;
    if (afkNow) g_app.afkHold += dt; else g_app.afkHold -= dt * 0.5f;
    g_app.afk = g_app.afkHold > 2.0f;

    // --- audio peak of the default output device ---
    static float audioAcc = 0.0f;
    audioAcc += dt;
    if (audioAcc > 0.09f) { audioAcc = 0.0f; AudioMeterSample(); }
    float up = (g_app.audioPeak > g_app.audioEnv) ? 16.0f : 2.6f;
    g_app.audioEnv = ApproachF(g_app.audioEnv, g_app.audioPeak, up, dt);

    // --- "is a video playing?" ---
    bool media = false;
    for (const auto& p : g_app.platforms) {
        if (!p.isMedia) continue;
        bool big = (p.rect.right - p.rect.left) > 520 && (p.rect.bottom - p.rect.top) > 300;
        if (p.isActive && big) media = true;
    }
    bool audioBusy = g_app.audioEnv > 0.055f;

    if (g_app.autoCouch) {
        if (media) g_app.couchHold += dt * (audioBusy ? 1.6f : 0.85f);
        else       g_app.couchHold -= dt * 0.35f;
        g_app.couchHold = ClampF(g_app.couchHold, -3.0f, 3.0f);
        bool want = g_app.couchHold > 0.9f;
        if (want && !g_app.couchOn) SetCouchMode(true);
        else if (!want && g_app.couchOn && g_app.couchHold < -0.6f) SetCouchMode(false);
    }

    if (g_app.autoDesk) {
        if (g_app.typingRate > 0.30f) g_app.deskHold += dt * 1.4f;
        else                          g_app.deskHold -= dt * 0.30f;
        g_app.deskHold = ClampF(g_app.deskHold, -3.0f, 3.0f);
        bool want = g_app.deskHold > 0.8f;
        if (want && !g_app.deskOn) SetDeskMode(true);
        else if (!want && g_app.deskOn && g_app.deskHold < -0.5f) SetDeskMode(false);
    }

    // Release a grab if the user pressed the mouse elsewhere / ghost mode on
    if (g_app.grabbedId != -1) {
        bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!lmb) ReleaseGrab();
    }

    // ---------------------------------------------------------------- CPU
    {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            if (g_app.haveCpuSample) {
                auto diff = [](const FILETIME& a, const FILETIME& b) {
                    ULARGE_INTEGER ua, ub;
                    ua.LowPart = a.dwLowDateTime; ua.HighPart = a.dwHighDateTime;
                    ub.LowPart = b.dwLowDateTime; ub.HighPart = b.dwHighDateTime;
                    return (float)(ua.QuadPart - ub.QuadPart);
                };
                float dIdle = diff(idle, g_app.prevIdle);
                float dKern = diff(kernel, g_app.prevKernel);
                float dUser = diff(user, g_app.prevUser);
                float busy = dKern + dUser;
                if (busy > 1.0f) g_app.cpuLoad = ClampF(1.0f - dIdle / busy, 0.0f, 1.0f);
            }
            g_app.prevIdle = idle; g_app.prevKernel = kernel; g_app.prevUser = user;
            g_app.haveCpuSample = true;
        }
    }
    bool busyNow = g_app.cpuLoad > 0.86f && !g_app.afk;
    g_app.hotHold = busyNow ? g_app.hotHold + dt : g_app.hotHold - dt * 1.5f;
    g_app.hotHold = ClampF(g_app.hotHold, 0.0f, 12.0f);
    {
        bool spraying = false;
        for (const auto& s2 : g_app.stickmen) if (s2.act == Act::EXTINGUISH) { spraying = true; break; }
        float target = ClampF((g_app.hotHold - 1.5f) / 3.0f, 0.0f, 1.0f);
        if (spraying) { target = 0.0f; g_app.hotHold = ApproachF(g_app.hotHold, 0.0f, 6.0f, dt); }
        g_app.heatGlow = ApproachF(g_app.heatGlow, target, spraying ? 9.0f : 3.0f, dt);
    }

    // ------------------------------------------------- clock: day / night
    {
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        g_app.nightHour = (int)st.wHour;
        bool night = (st.wHour >= 23 || st.wHour < 6);
        g_app.nightMode = g_app.autoNight ? night : g_app.nightForced;
        g_app.winter = (st.wMonth == 12 || st.wMonth == 1 || st.wMonth == 2);
    }

    // ------------------------------------------------ audio: beat / onset
    static float prevEnv = 0.0f;
    if (g_app.audioEnv - prevEnv > 0.11f) {
        g_app.audioBeat = 1.0f;
        if (g_app.audioEnv > 0.16f) {
            for (auto& s : g_app.stickmen) {
                if (s.act == Act::DANCE || s.act == Act::MUSIC || s.act == Act::WATCH_TV || s.act == Act::EAT_POPCORN) {
                    s.actPhase += 0.55f;
                }
            }
        }
    }
    prevEnv = g_app.audioEnv;
    g_app.audioBeat = ApproachF(g_app.audioBeat, 0.0f, 6.0f, dt);
    g_app.beatPhase += dt * (1.9f + 2.4f * g_app.audioEnv) * PI_F;

    // ------------------------------------- "user is scrolling a feed" guess
    float vy = g_app.mouseVel.y;
    bool vertical = (vy > 260.0f || vy < -260.0f);
    if (vertical && (g_app.fgKind == 3 || g_app.fgKind == 2 || (g_app.fgKind == 1 && g_app.mediaWinValid))) {
        g_app.feedScore = ClampF(g_app.feedScore + dt * 1.3f, 0.0f, 2.0f);
    } else {
        g_app.feedScore = ApproachF(g_app.feedScore, 0.0f, 0.8f, dt);
    }

    // -------------------------------------------- "user is working" streak
    if (!g_app.afk) {
        g_app.workStreak += dt;
    } else if (g_app.idleSeconds > 300.0f) {
        g_app.workStreak = 0.0f;   // they took a break, so did we
    }
    if (g_app.teaCooldown > 0.0f) g_app.teaCooldown -= dt;
    if (g_app.teaReminders && g_app.workStreak > g_app.breakMinutes * 60.0f &&
        g_app.teaCooldown <= 0.0f && !g_app.afk) {
        StartTeaBreak();
        g_app.teaCooldown = 18.0f * 60.0f;
    }
    if (g_app.prankTimer > 0.0f) g_app.prankTimer -= dt;
    if (g_app.pranksOn && g_app.prankTimer <= 0.0f && !g_app.afk && !g_app.couchOn) {
        TryPrank();
        g_app.prankTimer = RandRange(38.0f, 88.0f);
    }

    // ------------------------------------------- v4: DeepSeek async reply delivery
    if (g_app.aiAutoCool > 0.0f) g_app.aiAutoCool -= dt;
    if (g_app.aiHasReply) {
        wchar_t rep[160] = {};
        int tid = -1;
        {
            std::lock_guard<std::mutex> lk(g_app.aiMutex);
            if (g_app.aiHasReply) {
                wcscpy_s(rep, g_app.aiReply);
                tid = g_app.aiTargetId;
                g_app.aiHasReply = false;
            }
        }
        if (rep[0]) {
            Stickman* tgt = FindStickmanById(tid);
            if (!tgt && !g_app.stickmen.empty()) tgt = &g_app.stickmen[0];
            std::wstring text(rep);
            std::wstring actTag;
            if (text.rfind(L"[ACT:", 0) == 0) {
                size_t end = text.find(L']');
                if (end != std::wstring::npos && end < 20) {
                    actTag = text.substr(5, end - 5);
                    text = text.substr(end + 1);
                    while (!text.empty() && text[0] == L' ') text.erase(0, 1);
                }
            }
            if (tgt && !text.empty()) SayText(*tgt, text.c_str());
            if (tgt && !actTag.empty()) {
                if (actTag == L"WAVE") SetAct(*tgt, Act::WAVE_HELLO);
                else if (actTag == L"TEA") StartTeaBreak();
                else if (actTag == L"DANCE") SetAct(*tgt, Act::DANCE);
                else if (actTag == L"DUEL") StartShowDuel();
                else if (actTag == L"NAP") SetAct(*tgt, Act::NAP);
                else if (actTag == L"BALL") StartPassBall(*tgt);
                else if (actTag == L"THEFT") { tgt->act = Act::CURSOR_THEFT; tgt->actTimer = 7.0f; tgt->carry = 13; }
                else if (actTag == L"COUCH") { g_app.couchOn = true; }
                else if (actTag == L"FOLDER") StartBring(*tgt);
                else if (actTag == L"LAUGH") SetAct(*tgt, Act::LAUGH);
                else if (actTag == L"CHEER") SetAct(*tgt, Act::CHEER);
            }
        }
    }

    // ------------------------------------------- v4: battery (real, local)
    if (g_app.batteryT <= 0.0f) {
        g_app.batteryT = 5.0f;
        SYSTEM_POWER_STATUS ps = {};
        if (GetSystemPowerStatus(&ps) && ps.BatteryFlag != 255) {
            g_app.onBattery = (ps.SystemStatusFlag == 0);
            g_app.batteryPct = (ps.BatteryLifePercent <= 100) ? (int)ps.BatteryLifePercent : -1;
            bool low = g_app.onBattery && g_app.batteryPct >= 0 && g_app.batteryPct <= 15;
            if (low && !g_app.batteryLow) {
                g_app.batteryLow = true;
                Stickman* who = PickFreeGuy(1);
                if (who) StartNoteHere(*who, 15);     // "heavy..." -> go find the charger
                for (auto& o : g_app.stickmen) {
                    if (o.act == Act::IDLE && RandInt(2) == 0) { o.act = Act::RUN; o.actTimer = 1.6f; }
                }
            } else if (!low) {
                g_app.batteryLow = false;
            }
        } else {
            g_app.batteryPct = -1;      // desktop / no battery: nothing to worry about
            g_app.onBattery = false;
        }
    } else g_app.batteryT -= dt;

    // ----------------------------- v4: weather, read off the calendar + sky
    // No internet call: the season decides what they expect outside, and the
    // tray can force rain or snow. They react to that, not to your webcam.
    if (g_app.weatherT <= 0.0f) {
        g_app.weatherT = 240.0f;
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        int m = (int)st.wMonth;
        bool snow = (m == 12 || m == 1 || m == 2);
        bool wet = (m == 3 || m == 4 || m == 10 || m == 11);
        if (g_app.weatherOn) {                 // tray-forced weather wins
            if (g_app.weatherMode == 1) { g_app.raining = true; g_app.snowing = false; }
            else if (g_app.weatherMode == 2) { g_app.raining = false; g_app.snowing = true; }
            else if (g_app.weatherMode == 3) { g_app.raining = false; g_app.snowing = false; }
            else {                             // auto: what the month suggests
                g_app.snowing = (snow && RandInt(2) == 0);
                g_app.raining = (!g_app.snowing) && (wet || m == 1 || m == 2 || m == 3) && RandInt(3) == 0;
            }
            if (g_app.raining || g_app.snowing) g_app.weatherCool = 0.0f;
        } else {
            g_app.raining = false;
            g_app.snowing = false;
        }
        g_app.outsideTemp = snow ? -6.0f : (wet ? 7.0f : 18.0f);
        (void)g_app.weatherFail;
    } else g_app.weatherT -= dt;
    if ((g_app.raining || g_app.snowing) && !g_app.afk) {
        g_app.weatherCool -= dt;
        if (g_app.weatherCool <= 0.0f) {
            g_app.weatherCool = RandRange(55.0f, 130.0f);
            Stickman* who = PickFreeGuy(0);
            if (who && who->act != Act::UMBRELLA) StartUmbrella(*who);
        }
        // a few drawn drops / flakes right above them, so you can see what they see
        if (g_app.raining && RandInt(2) == 0) {
            for (int i = 0; i < 3; ++i) {
                float rx = RandRange(0.0f, (float)g_app.width);
                SpawnParticle(rx, -6.0f, RandRange(-16, 16), RandRange(420, 620),
                              RandRange(1.2f, 2.0f), RandRange(0.9f, 1.6f), 140, 195, 250, 9);
            }
        }
        if (g_app.snowing) {
            for (int i = 0; i < 2; ++i) {
                SpawnParticle(RandRange(0.0f, (float)g_app.width), -6.0f, RandRange(-24, 24),
                              RandRange(28, 64), RandRange(2.0f, 4.2f), RandRange(2.4f, 4.6f),
                              245, 250, 255, 0);
            }
        }
    }

    // ------------------------------------------ v4: a hot PC needs some help
    if (g_app.plugCool > 0.0f) g_app.plugCool -= dt;
    if (g_app.heatGlow > 0.62f && g_app.plugCool <= 0.0f && !g_app.afk) {
        bool already = false;
        for (auto& o : g_app.stickmen) if (o.act == Act::PLUG_IN) already = true;
        if (!already) {
            Stickman* who = PickFreeGuy(2);
            if (who) { StartPlugRun(*who); g_app.plugCool = 90.0f; }
        }
    }

    // ----------------------------------------------- v4: drag the couch closer
    if (g_app.couchOn && g_app.mediaWinValid && g_app.couchGoalX == 0.0f && RandInt(600) == 0) {
        float mcx = (float)(g_app.mediaWinRect.left + g_app.mediaWinRect.right) * 0.5f;
        if (std::fabs(mcx - g_app.couchX) > 220.0f * g_app.scale) {
            Stickman* who = PickFreeGuy(0);
            if (who) {
                float dir = (mcx > g_app.couchX) ? 1.0f : -1.0f;
                g_app.couchGoalX = mcx;
                who->blocks = 0;
                BeginTravel(*who, Act::DRAG_COUCH, g_app.couchX - dir * 110.0f * g_app.scale);
                who->travelFloor = true;
                who->actTimer = 6.0f + std::fabs(mcx - g_app.couchX) / 80.0f;
                if (!AnyoneSpeaking()) SayNote(*who, 15);
            }
        }
    }

    // ------------------------------ v4: they guard your "Send" button a bit
    g_app.fgChat = g_app.fgKind;
    if (g_app.fgChat == 2 && g_app.typingRate > 0.35f && RandInt(1500) == 0) {
        Stickman* who = PickFreeGuy(2);
        if (who) StartSendGuard(*who);
    }

    // ------------------------------------------------------- head strokes
    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    for (auto& s : g_app.stickmen) {
        if (s.pet > 0.0f) s.pet = ApproachF(s.pet, 0.0f, 0.55f, dt);
        if (s.soot > 0.0f) s.soot = ApproachF(s.soot, 0.0f, 0.25f, dt);
        if (s.act == Act::GRABBED || !s.grounded) continue;
        float hx = s.pose.head.x, hy = s.pose.head.y;
        float dx = g_app.cursor.x - hx, dy = g_app.cursor.y - hy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < 34.0f * g_app.scale && !lmb && !g_app.ghostMode) {
            float swing = std::fabs(g_app.mouseVel.x);
            if (swing > 200.0f) {
                s.pet = ClampF(s.pet + dt * 2.2f, 0.0f, 1.6f);
                if (s.act != Act::PETTED) {
                    s.act = Act::PETTED;
                    s.actTimer = RandRange(2.0f, 3.4f);
                    s.animTime = 0.0f;
                }
                if (RandInt(7) == 0) {
                    SpawnParticle(hx + RandRange(-8, 8), hy - 12.0f, RandRange(-14, 14), -52.0f,
                                  4.0f, 0.8f, 255, 120, 170, 9);
                }
                g_app.cursorPlay = 1.0f;
            } else if (swing > 900.0f) {
                // the cursor zips past -> they hang from it like from a bar
                s.act = Act::HANG_CURSOR;
                s.actTimer = 1.4f;
                s.animTime = 0.0f;
                g_app.cursorPlay = 1.0f;
            }
        } else if (d < 130.0f * g_app.scale && swingActive(swingNow()) && !lmb) {
            if (s.act == Act::IDLE && RandInt(90) == 0) {
                s.act = Act::CURSOR_PLAY;
                s.actTimer = RandRange(2.0f, 3.4f);
                s.facing = (dx >= 0.0f) ? 1 : -1;
            }
        }

        // ------------------------------------------------- v4: pointer games
        // "tease" = the pointer is darting around a guy who is doing nothing
        float speed = Len(g_app.mouseVel);
        if (d < 150.0f * g_app.scale && speed > 260.0f && !lmb && s.grounded) {
            g_app.cursorTease = ClampF(g_app.cursorTease + dt * 1.5f, 0.0f, 3.0f);
        }
        if (s.act == Act::IDLE || s.act == Act::WALK || s.act == Act::TRAVEL) {
            // 1) chase: they lunge at the pointer when it is waved in their face
            if (g_app.cursorTease > 1.15f && d < 120.0f * g_app.scale && s.grounded &&
                (s.act != Act::TRAVEL) && RandInt(60) == 0) {
                s.act = Act::CHASE_CURSOR;
                s.actTimer = RandRange(2.6f, 4.6f);
                s.animTime = 0.0f; s.actPhase = 0.0f;
                s.facing = (dx >= 0.0f) ? 1 : -1;
                s.squashVel = -1.6f;
                g_app.cursorTease = 0.0f;
            }
            // 2) ride: the pointer hovers low and still -> he hops on it like a scooter
            float below = g_app.cursor.y - (s.y - 40.0f * g_app.scale);
            if (s.act == Act::IDLE && d < 60.0f * g_app.scale && speed < 60.0f &&
                below > 6.0f * g_app.scale && below < 40.0f * g_app.scale && RandInt(340) == 0) {
                s.act = Act::RIDE_CURSOR;
                s.actTimer = RandRange(4.0f, 9.0f);
                s.animTime = 0.0f;
                s.rides = 1;
                s.squashVel = -2.2f;
                s.speechT = 5.9f;   // no bubble yet
                SpawnBurst(s.x, s.y - 10.0f * g_app.scale, 235, 240, 246, 4, 0);
            }
            // 3) theft: rarely he grabs a DRAWN COPY of the pointer and tries to steal it
            if (s.act == Act::IDLE && g_app.cursorTease > 2.1f && RandInt(40) == 0) {
                s.act = Act::CURSOR_THEFT;
                s.actTimer = 7.0f;
                s.animTime = 0.0f; s.floatT = 0.0f; s.blocks = 0;
                s.rides = 2;
                s.carry = 13;              // a *drawn* copy of the arrow, not your real cursor
                g_app.ghostCursor = g_app.cursor;      // the real one is never moved
                g_app.cursorTease = 0.0f;
                SayNote(s, 15);
            }
        }
    }
    g_app.cursorTease = ApproachF(g_app.cursorTease, 0.0f, 0.55f, dt);
    if (g_app.pokeCount > 0.0f) g_app.pokeCount = ApproachF(g_app.pokeCount, 0.0f, 0.22f, dt);
    g_app.cursorPlay = ApproachF(g_app.cursorPlay, 0.0f, 1.6f, dt);

    // --------------------------------------------------- v4: mood of the squad
    {
        float target = 0.55f;
        if (g_app.afk) target = 0.35f;
        if (g_app.couchOn || g_app.deskOn) target += 0.12f;
        if (!g_app.snacks.empty() || !g_app.balls.empty()) target += 0.12f;
        if (g_app.nightMode) target -= 0.05f;
        g_app.mood = ApproachF(g_app.mood, ClampF(target, 0.0f, 1.0f), 0.06f, dt);
        for (auto& s2 : g_app.stickmen) {
            s2.mood = ApproachF(s2.mood, g_app.mood, 0.25f, dt);
            if (s2.speech[0]) {
                int total = (int)wcslen(s2.speech);
                if (s2.speechSeen < (float)total) {
                    s2.speechSeen = std::min((float)total, s2.speechSeen + dt * 18.0f);
                    s2.speechT = 0.25f;
                } else {
                    s2.speechT += dt;
                }
                if (s2.speechSeen >= (float)total && s2.speechT > 7.0f) s2.speech[0] = 0;
            }
        }
        // a mopey guy asks for attention in writing
        if (!g_app.afk && g_app.touchTimer > 60.0f && RandInt(1400) == 0) {
            for (auto& s2 : g_app.stickmen) {
                if (s2.act != Act::IDLE || !s2.grounded || s2.speech[0]) continue;
                if (s2.mood > 0.42f) continue;
                s2.act = Act::SPEAK;
                s2.actTimer = RandRange(4.5f, 6.5f);
                SayNote(s2, RandInt(2) == 0 ? 8 : 18);
                break;
            }
        }
    }

    // ------------------------------------------- they notice you, not vice versa
    g_app.touchTimer += dt;
    if (g_app.touchTimer > 150.0f && !g_app.afk && !g_app.couchOn && RandInt(400) == 0) {
        Stickman* lonely = nullptr;
        for (auto& s : g_app.stickmen) {
            if (s.act == Act::IDLE && s.grounded) { lonely = &s; break; }
        }
        if (lonely) {
            lonely->act = Act::ASK_ATTENTION;
            lonely->actTimer = RandRange(5.0f, 8.0f);
            lonely->animTime = 0.0f;
            g_app.touchTimer = 60.0f;
        }
    }
}

// ============================================================================
// Activity selection
// ============================================================================
static float ActDuration(Act a, bool& autoHold) {
    autoHold = false;
    switch (a) {
        case Act::WATCH_TV:    autoHold = true; return RandRange(18.0f, 40.0f);
        case Act::MINI_LAPTOP: autoHold = true; return RandRange(20.0f, 45.0f);
        case Act::NAP:         autoHold = true; return RandRange(18.0f, 32.0f);
        case Act::CAMPFIRE:    autoHold = true; return RandRange(14.0f, 26.0f);
        case Act::EAT_SNACK:   autoHold = true; return 12.0f;
        case Act::PLAY_BALL:   autoHold = true; return RandRange(7.0f, 12.0f);
        case Act::SKETCH:   return RandRange(6.0f, 10.0f);
        case Act::BUILD:    return RandRange(6.0f, 10.0f);
        case Act::MUSIC:    return RandRange(7.0f, 13.0f);
        case Act::FISH:     return RandRange(7.0f, 13.0f);
        case Act::READ:     return RandRange(8.0f, 15.0f);
        case Act::SIT_EDGE: return RandRange(4.0f, 8.0f);
        case Act::PEEK_WINDOW: return RandRange(3.5f, 6.5f);
        case Act::WAVE_HELLO: return RandRange(2.4f, 3.8f);
        case Act::CHEER:    return RandRange(1.8f, 2.8f);
        case Act::HIGH_FIVE: return 2.2f;
        case Act::POINT_AT: return RandRange(1.8f, 2.6f);
        case Act::CARD_GAME:   return RandRange(22.0f, 40.0f);
        case Act::WATCH_GAME:  return RandRange(16.0f, 30.0f);
        case Act::COOK:        return RandRange(16.0f, 26.0f);
        case Act::QUEUE:       return RandRange(12.0f, 20.0f);
        case Act::LAB:          return RandRange(12.0f, 22.0f);
        case Act::ARMWRESTLE:  return 12.0f;
        case Act::TUGWAR:      return 11.0f;
        case Act::HANDSHAKE:   return 2.4f;
        case Act::SPY_CHAT:    return RandRange(8.0f, 15.0f);
        case Act::LAUGH:       return RandRange(3.5f, 6.0f);
        case Act::FACEPALM:    return RandRange(2.2f, 3.6f);
        case Act::YAWN:        return RandRange(2.6f, 5.4f);
        case Act::DANCE:       return RandRange(8.0f, 16.0f);
        case Act::CURSOR_PLAY: return RandRange(2.6f, 4.6f);
        case Act::HANG_CURSOR: return 2.2f;
        case Act::PETTED:      return RandRange(2.6f, 4.6f);
        case Act::ASK_ATTENTION: return RandRange(6.0f, 10.0f);
        case Act::TEA_RUN:     return 18.0f;
        case Act::CRACK_PRANK: return 7.0f;
        case Act::MUSTACHE:    return RandRange(8.0f, 12.0f);
        case Act::BOX_KICK:    return RandRange(7.0f, 12.0f);
        case Act::SQUISH:      return 2.2f;
        case Act::EXTINGUISH:  return RandRange(7.0f, 13.0f);
        case Act::IN_VIDEO:    return RandRange(6.0f, 10.0f);
        case Act::PUSH_WINDOW: return 5.0f;
        case Act::AIR_TYPE:    return RandRange(8.0f, 16.0f);
        // ---- v4 ----
        case Act::CHASE_CURSOR: return RandRange(2.6f, 4.6f);
        case Act::RIDE_CURSOR:  return RandRange(4.0f, 9.0f);
        case Act::FIGHT_CURSOR: return RandRange(4.0f, 7.5f);
        case Act::CURSOR_THEFT: return 7.0f;
        case Act::SPEAK:        return RandRange(5.5f, 8.5f);
        case Act::SEND_GUARD:   return RandRange(12.0f, 22.0f);
        case Act::HIDE_ICON:    return RandRange(6.0f, 12.0f);
        case Act::ICON_KICK:   return RandRange(7.0f, 12.0f);
        case Act::BRING_FOLDER: return RandRange(9.0f, 14.0f);
        case Act::TAB_OOPS:     return 6.0f;
        case Act::DRAG_COUCH:   return 5.0f;
        case Act::PLUG_IN:      return 10.0f;
        case Act::UMBRELLA:     return RandRange(14.0f, 26.0f);
        case Act::PASS_BALL:   return RandRange(9.0f, 15.0f);
        case Act::IDLE:     return RandRange(1.6f, 3.6f);
        case Act::WALK:     return RandRange(1.6f, 4.0f);
        case Act::RUN:      return RandRange(0.9f, 2.2f);
        default: return RandRange(1.5f, 3.0f);
    }
}

static void SetAct(Stickman& s, Act a) {
    if (s.act != a) {
        bool keepsProp = (a == Act::CARD_GAME || a == Act::WATCH_GAME || a == Act::COOK ||
                          a == Act::QUEUE || a == Act::LAB || a == Act::ARMWRESTLE ||
                          a == Act::TUGWAR || a == Act::MUSTACHE || a == Act::BOX_KICK);
        if (!keepsProp) ReleaseProp(s);
        if (a != Act::WATCH_TV && a != Act::EAT_POPCORN && s.seatIndex >= 0) {
            ReleaseCouchSeat(s.id);
            s.seatIndex = -1;
        }
        if (a != Act::MINI_LAPTOP && g_app.deskOwnerId == s.id) g_app.deskOwnerId = -1;
    }
    s.act = a;
    s.animTime = 0.0f;
    bool hold = false;
    s.actTimer = ActDuration(a, hold);
    if (a == Act::SKETCH || a == Act::BUILD || a == Act::READ || a == Act::CAMPFIRE) s.propT = 0.0f;
}

static bool ActNeedsFloor(Act a) {
    switch (a) {
        case Act::WATCH_TV: case Act::EAT_POPCORN: case Act::MINI_LAPTOP: case Act::NAP:
        case Act::CAMPFIRE: case Act::SKETCH: case Act::BUILD: case Act::MUSIC:
        case Act::READ: case Act::EAT_SNACK: case Act::PLAY_BALL: return true;
        default: return false;
    }
}

static void BeginTravel(Stickman& s, Act dest, float targetX) {
    LeaveSeat(s);
    s.nextAct = dest;
    s.travelTargetX = ClampF(targetX, 30.0f, (float)g_app.width - 30.0f);
    s.travelFloor = ActNeedsFloor(dest);
    s.act = Act::TRAVEL;
    s.actTimer = 9.5f;
    s.animTime = 0.0f;
    if (s.travelFloor && s.standingHwnd) {
        // Hop off the window and land on the desktop floor instead
        s.grounded = false;
        s.standingHwnd = nullptr;
        s.vy = -40.0f;
        s.squashVel = -1.6f;
    }
}

static void GoSitOnCouch(Stickman& s) {
    int seat = -1;
    int n = CouchSeatCount();
    for (int i = 0; i < n; ++i) {
        if (g_app.couchSeatsTaken[i] == s.id) { seat = i; break; }
    }
    if (seat < 0) seat = ClaimCouchSeat(s.id);
    if (seat < 0) { SetAct(s, Act::IDLE); return; }
    s.seatIndex = seat;
    BeginTravel(s, Act::WATCH_TV, CouchSeatX(seat));
}

static void GoToDesk(Stickman& s) {
    float sc = g_app.scale;
    float floorY = (float)FloorYAt(s.x);
    g_app.deskX = ClampF(s.x + s.facing * 30.0f * sc, 60.0f, (float)g_app.width - 60.0f);
    g_app.deskY = floorY;
    g_app.deskOwnerId = s.id;
    BeginTravel(s, Act::MINI_LAPTOP, g_app.deskX);
}

// Where should this character hang out? Keeps them in their usual seats.
static void PickAutoActivity(Stickman& s) {
    // Already standing on somebody's window? Play with it before hopping down.
    if (s.standingHwnd && !g_app.couchOn && !g_app.afk) {
        const DesktopPlatform* wp = FindPlatformByHwnd(s.standingHwnd);
        int r = RandInt(100);
        if (wp && r < 16) {
            float mid = (float)(wp->rect.left + wp->rect.right) * 0.5f;
            float ledge = 26.0f * g_app.scale;
            bool leftSide = (s.x < mid);
            s.x = leftSide ? (float)wp->rect.left + ledge : (float)wp->rect.right - ledge;
            s.facing = leftSide ? -1 : 1;
            SetAct(s, Act::SIT_EDGE);
            return;
        }
        if (wp && r < 30) {
            SetAct(s, Act::PEEK_WINDOW);
            s.facing = (r < 23) ? -1 : 1;
            return;
        }
        if (wp && r < 44) {
            s.grounded = false;
            s.standingHwnd = nullptr;
            s.act = Act::AIRBORNE;
            s.animTime = 0.0f;
            s.vy = -250.0f;
            s.vx = s.facing * RandRange(-150.0f, 150.0f);
            s.doingFlip = (RandInt(3) == 0);
            s.squashVel = -2.0f;
            return;
        }
    }

    if (g_app.afk) {
        // Nobody is watching the PC: build a campfire or take a nap
        if (RandInt(2) == 0) {
            float fx = (float)g_app.width * 0.5f + RandRange(-140.0f, 140.0f);
            BeginTravel(s, Act::CAMPFIRE, fx);
        } else {
            BeginTravel(s, Act::NAP, s.x + RandRange(-90.0f, 90.0f));
        }
        return;
    }

    if (g_app.couchOn) {
        if (s.seatIndex < 0) GoSitOnCouch(s);
        else BeginTravel(s, Act::WATCH_TV, CouchSeatX(s.seatIndex));
        return;
    }

    if (!g_app.balls.empty() && RandInt(100) < 55) {
        s.act = Act::PLAY_BALL;
        s.actTimer = RandRange(7.0f, 12.0f);
        s.animTime = 0.0f;
        return;
    }

    int roll = RandInt(100);
    int activeBudget = g_app.calmMode ? 35 : 72;

    if (roll < 12) {
        // parkour: hop onto the user's active window
        const DesktopPlatform* activeWin = nullptr;
        for (const auto& p : g_app.platforms) {
            if (p.isActive && p.rect.top > 80 && p.rect.top < g_app.height - 90) { activeWin = &p; break; }
        }
        if (activeWin) {
            float targetX = ClampF(s.x, (float)activeWin->rect.left + 40.0f, (float)activeWin->rect.right - 40.0f);
            float dx = targetX - s.x;
            float dy = (float)activeWin->rect.top - s.y;
            s.facing = (dx >= 0.0f) ? 1 : -1;
            if (std::fabs(dx) < 300.0f && dy < -20.0f && dy > -460.0f) {
                s.vy = -std::sqrt(2.0f * 1280.0f * (std::fabs(dy) + 62.0f));
                s.vx = ClampF(dx * 1.15f, -380.0f, 380.0f);
                s.grounded = false;
                s.standingHwnd = nullptr;
                s.act = Act::AIRBORNE;
                s.doingFlip = (dy < -170.0f);
                s.squashVel = -2.2f;
                s.actTimer = 2.0f;
                SpawnBurst(s.x, s.y, 220, 225, 235, 6, 0);
                return;
            }
            BeginTravel(s, Act::RUN, targetX);
            return;
        }
    }

    if (roll < activeBudget) {
        // roaming: walk / run / flip / idle
        int r2 = RandInt(100);
        if (r2 < 42) { SetAct(s, Act::WALK); s.facing = (RandInt(2) == 0) ? 1 : -1; }
        else if (r2 < 68) { SetAct(s, Act::RUN); s.facing = (RandInt(2) == 0) ? 1 : -1; }
        else if (r2 < 82) {
            s.vy = RandRange(-500.0f, -370.0f);
            s.vx = s.facing * RandRange(120.0f, 240.0f);
            s.grounded = false;
            s.standingHwnd = nullptr;
            s.act = Act::AIRBORNE;
            s.doingFlip = (RandInt(2) == 0);
            s.actTimer = 1.8f;
            s.squashVel = -2.4f;
            SpawnBurst(s.x, s.y, 210, 215, 225, 6, 0);
        } else if (r2 < 92) { SetAct(s, Act::CHEER); }
        else { SetAct(s, Act::IDLE); }
        return;
    }

    // "job" activities, biased per character personality
    Act job = Act::IDLE;
    switch (s.type) {
        case CharType::TSC_ORANGE: job = (RandInt(4) == 0) ? Act::BUILD : Act::SKETCH; break;
        case CharType::RED:        job = (RandInt(3) == 0) ? Act::MUSIC  : Act::BUILD;  break;
        case CharType::BLUE:       job = (RandInt(3) == 0) ? Act::READ   : Act::SKETCH; break;
        case CharType::GREEN:      job = (RandInt(2) == 0) ? Act::MUSIC  : Act::FISH;   break;
        case CharType::YELLOW:     job = (RandInt(4) == 0) ? Act::READ   : Act::MINI_LAPTOP; break;
        case CharType::TCO_BLACK:  job = (RandInt(2) == 0) ? Act::CAMPFIRE : Act::SKETCH; break;
    }
    if (job == Act::MINI_LAPTOP) {
        if (g_app.deskOwnerId == -1) { GoToDesk(s); return; }
        job = Act::READ;
    }
    if (job == Act::FISH) {
        // fish off the closest window edge or the screen edge
        const DesktopPlatform* best = nullptr;
        float bd = 1e9f;
        for (const auto& p : g_app.platforms) {
            if (p.rect.top < 60) continue;
            float ex = (std::fabs(p.rect.left - s.x) < std::fabs(p.rect.right - s.x))
                ? (float)p.rect.left : (float)p.rect.right;
            float d = std::fabs(ex - s.x);
            if (d < bd) { bd = d; best = &p; }
        }
        float fx = best ? (bd < 320.0f ? ((std::fabs((float)best->rect.left - s.x) < std::fabs((float)best->rect.right - s.x))
                        ? (float)best->rect.left - 20.0f * g_app.scale
                        : (float)best->rect.right + 20.0f * g_app.scale)
                        : s.x + s.facing * RandRange(120.0f, 260.0f))
                        : s.x + s.facing * RandRange(120.0f, 260.0f);
        BeginTravel(s, Act::FISH, fx);
        return;
    }
    if (job == Act::SKETCH || job == Act::BUILD || job == Act::READ || job == Act::MUSIC ||
        job == Act::CAMPFIRE) {
        BeginTravel(s, job, RouteAround(s.x, ClampF(s.x + RandRange(-70.0f, 70.0f), 40.0f, (float)g_app.width - 40.0f)));
        return;
    }
    SetAct(s, job);
}

// Friendly social behaviour: waving, high fives, laughing. Fighting is the rare exception.
static void TrySocial(Stickman& s) {
    if (s.socialCooldown > 0.0f) return;
    if (s.act == Act::GRABBED || s.act == Act::SPARRING || s.act == Act::AIRBORNE) return;
    for (auto& o : g_app.stickmen) {
        if (o.id == s.id) continue;
        if (o.act == Act::GRABBED || o.act == Act::SPARRING || !o.grounded || !s.grounded) continue;
        float dx = o.x - s.x;
        if (std::fabs(dx) > 150.0f || std::fabs(o.y - s.y) > 26.0f) continue;
        int roll = RandInt(100);
        s.facing = (dx >= 0.0f) ? 1 : -1;
        o.facing = (dx >= 0.0f) ? -1 : 1;
        if (roll < 88) {
            SetAct(s, Act::POINT_AT);
            s.facing = (dx >= 0.0f) ? 1 : -1;
            s.socialCooldown = RandRange(5.0f, 12.0f);
        } else if (g_app.sparAI && !g_app.calmMode &&
                   s.sparCooldown <= 0.0f && o.sparCooldown <= 0.0f) {
            s.act = Act::SPARRING; o.act = Act::SPARRING;
            s.actTimer = 4.6f; o.actTimer = 4.6f;
            s.partnerId = o.id; o.partnerId = s.id;
            s.sparCooldown = RandRange(150.0f, 300.0f);
            o.sparCooldown = s.sparCooldown;
            s.hasSword = (s.type == CharType::RED || s.type == CharType::TSC_ORANGE);
            o.hasSword = s.hasSword;
            s.swordTimer = 4.6f; o.swordTimer = 4.6f;
            SpawnParticle((s.x + o.x) * 0.5f, (s.y + o.y) * 0.5f - 34.0f, 0, 0,
                          12.0f, 0.3f, 255, 235, 110, 1);
        } else {
            s.socialCooldown = RandRange(3.0f, 9.0f);
        }
        return;
    }
}

// ============================================================================
// Mode switches (couch / mini desk) and mouse release
// ============================================================================
static void SetCouchMode(bool on) {
    if (on == g_app.couchOn) return;
    g_app.couchOn = on;
    if (on) {
        for (int i = 0; i < 6; ++i) g_app.couchSeatsTaken[i] = -1;
        RecomputeCouchAnchor();
        for (auto& s : g_app.stickmen) {
            if (s.act == Act::GRABBED) continue;
            GoSitOnCouch(s);
        }
    } else {
        for (auto& s : g_app.stickmen) {
            if (s.seatIndex >= 0) { ReleaseCouchSeat(s.id); s.seatIndex = -1; }
            if (s.act == Act::WATCH_TV || s.act == Act::EAT_POPCORN) {
                s.act = Act::IDLE;
                s.actTimer = RandRange(0.6f, 1.4f);
                s.vx = (RandInt(2) == 0) ? -70.0f : 70.0f;
            }
        }
    }
}

static void SetDeskMode(bool on) {
    if (on == g_app.deskOn) return;
    g_app.deskOn = on;
    if (on) {
        if (g_app.deskOwnerId == -1) {
            Stickman* pick = nullptr;
            for (auto& s : g_app.stickmen) {
                if (s.act == Act::GRABBED || s.act == Act::SPARRING) continue;
                if (s.type == CharType::YELLOW) { pick = &s; break; }
                if (!pick) pick = &s;
            }
            if (pick) GoToDesk(*pick);
        }
    } else {
        if (g_app.deskOwnerId != -1) {
            if (Stickman* s = FindStickmanById(g_app.deskOwnerId)) {
                s->act = Act::IDLE;
                s->actTimer = RandRange(0.5f, 1.2f);
            }
            g_app.deskOwnerId = -1;
        }
    }
}

static void ReleaseGrab() {
    if (g_app.grabbedId == -1) return;
    if (Stickman* s = FindStickmanById(g_app.grabbedId)) {
        s->act = Act::AIRBORNE;
        s->vx = ClampF(g_app.mouseVel.x * 0.82f, -1150.0f, 1150.0f);
        s->vy = ClampF(g_app.mouseVel.y * 0.82f - 110.0f, -1150.0f, 950.0f);
        s->y += 50.0f * g_app.scale;      // neck anchor -> feet anchor
        s->facing = (s->vx >= 0.0f) ? 1 : -1;
        s->doingFlip = (Len(V2(s->vx, s->vy)) > 360.0f);
        s->squashVel = -2.0f;
        s->grounded = false;
        s->pendingHello = false;
        // Friend-to-friend greeting on flick disabled.
    }
    g_app.grabbedId = -1;
    if (g_app.hwnd) ReleaseCapture();
}

// ============================================================================
// v3 scenes: party corner, cooking line, lab, matches, pranks
// ============================================================================
static bool ActIsScene(Act a) {
    switch (a) {
        case Act::CARD_GAME: case Act::WATCH_GAME: case Act::COOK: case Act::QUEUE:
        case Act::LAB: case Act::ARMWRESTLE: case Act::TUGWAR: case Act::MUSTACHE:
        case Act::BOX_KICK: case Act::SPY_CHAT: case Act::DANCE: case Act::EXTINGUISH:
        case Act::ASK_ATTENTION: case Act::AIR_TYPE: case Act::IN_VIDEO: case Act::PETTED:
        case Act::CURSOR_PLAY: case Act::HANDSHAKE: case Act::LAUGH: case Act::FACEPALM:
        case Act::YAWN: case Act::CRACK_PRANK: case Act::TEA_RUN: case Act::SQUISH:
        case Act::CHASE_CURSOR: case Act::RIDE_CURSOR: case Act::FIGHT_CURSOR:
        case Act::CURSOR_THEFT: case Act::SPEAK: case Act::SEND_GUARD: case Act::HIDE_ICON:
        case Act::ICON_KICK: case Act::BRING_FOLDER: case Act::TAB_OOPS: case Act::DRAG_COUCH:
        case Act::PLUG_IN: case Act::UMBRELLA: case Act::PASS_BALL:
            return true;
        default: return false;
    }
}

static void ReleaseProp(Stickman& s) {
    if (s.propIndex >= 0) {
        WorldProp* w = PropAt(s.propIndex);
        if (w) {
            if (w->a == s.id) w->a = -1;
            if (w->b == s.id) w->b = -1;
        }
        s.propIndex = -1;
    }
    s.seatOff = 0.0f;
    s.audienceOf = -1;
}

// Is there room on the floor at [x-w/2, x+w/2]? Avoid other props and the
// band right under the window the user is reading.
static bool SpotFree(float x, float w) {
    if (x - w < 30.0f || x + w > (float)g_app.width - 30.0f) return false;
    if (InKeepOut(x)) return false;
    for (const auto& p : g_app.props) {
        if (!p.alive) continue;
        if (std::fabs(p.x - x) < (w + 60.0f)) return false;
    }
    return true;
}

// A cozy corner: the far side of the taskbar, never the middle of the screen.
static float PickSceneSpot(float spread) {
    float floorGuess = (float)g_app.height;
    (void)floorGuess;
    for (int attempt = 0; attempt < 24; ++attempt) {
        bool right = (attempt % 2 == 0);
        float margin = (right ? (float)g_app.width - 90.0f : 90.0f);
        float x = margin + (right ? -1.0f : 1.0f) * RandRange(0.0f, spread);
        if (SpotFree(x, 120.0f)) return x;
    }
    return -1.0f;
}

static void SeatAt(Stickman& s, Act a, int propIdx, float off, int facing) {
    WorldProp* w = PropAt(propIdx);
    s.propIndex = propIdx;
    s.seatOff = off;
    s.facing = facing;
    if (w) {
        BeginTravel(s, a, w->x + off);
        s.nextAct = a;
    }
}

// ---- party corner: cards or chess, with an audience ----
static void StartPartyCorner() {
    float sc = g_app.scale;
    int players = 0;
    Stickman* pl[2] = {nullptr, nullptr};
    for (auto& s : g_app.stickmen) {
        if (players >= 2) break;
        if (!s.grounded || s.act == Act::GRABBED || s.propIndex >= 0) continue;
        if (s.act == Act::SPARRING) continue;
        pl[players++] = &s;
    }
    if (players < 2) return;
    float x = PickSceneSpot(150.0f);
    if (x < 0.0f) return;
    float y = (float)FloorYAt(x);
    int data = (RandInt(2) == 0) ? 0 : 1;   // 0 cards, 1 chess
    int idx = AddProp(PR_TABLE, x, y, RandRange(30.0f, 46.0f), pl[0]->id, pl[1]->id, data);
    SeatAt(*pl[0], Act::CARD_GAME, idx, -32.0f * sc, 1);
    SeatAt(*pl[1], Act::CARD_GAME, idx,  32.0f * sc, -1);
    pl[0]->partnerId = pl[1]->id;
    pl[1]->partnerId = pl[0]->id;
    pl[0]->actPhase = 0.0f; pl[1]->actPhase = 0.0f;
    int seat = 0;
    for (auto& s : g_app.stickmen) {
        if (s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
        if (std::fabs(s.x - x) > 460.0f) continue;
        ++seat;
        BeginTravel(s, Act::WATCH_GAME, x + (seat % 2 == 0 ? -1.0f : 1.0f) * (78.0f + 26.0f * seat) * sc);
        s.audienceOf = pl[0]->id;
        if (seat >= 2) break;
    }
    SpawnBurst(x, y - 10.0f * sc, 255, 220, 120, 6, 2);
}

// ---- Blue's cooking line ----
static void StartCookLine() {
    float sc = g_app.scale;
    Stickman* cook = nullptr;
    for (auto& s : g_app.stickmen) {
        if (s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
        if (s.type == CharType::BLUE) { cook = &s; break; }
        if (!cook) cook = &s;
    }
    if (!cook) return;
    float x = PickSceneSpot(180.0f);
    if (x < 0.0f) return;
    float y = (float)FloorYAt(x);
    int idx = AddProp(PR_CAULDRON, x, y, RandRange(22.0f, 30.0f), cook->id, -1, 0);
    SeatAt(*cook, Act::COOK, idx, -6.0f * sc, 1);
    cook->actPhase = 0.0f;
    int n = 0;
    for (auto& s : g_app.stickmen) {
        if (&s == cook || s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
        if (std::fabs(s.x - x) > 420.0f) continue;
        ++n;
        BeginTravel(s, Act::QUEUE, x + (18.0f + 30.0f * n) * sc);
        s.audienceOf = cook->id;
        if (n >= 3) break;
    }
}

// ---- Yellow's experiments (may go boom) ----
static void StartLabScene() {
    float sc = g_app.scale;
    Stickman* lab = nullptr;
    for (auto& s : g_app.stickmen) {
        if (s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
        if (s.type == CharType::YELLOW) { lab = &s; break; }
        if (!lab) lab = &s;
    }
    if (!lab) return;
    float x = PickSceneSpot(200.0f);
    if (x < 0.0f) return;
    float y = (float)FloorYAt(x);
    int idx = AddProp(PR_PANEL, x, y, RandRange(16.0f, 26.0f), lab->id, -1, 0);
    SeatAt(*lab, Act::LAB, idx, -14.0f * sc, 1);
    lab->actPhase = 0.0f;
}

// ---- two-man matches on the desktop ----
static void StartArmWrestle(Stickman& a, Stickman& b) {
    float sc = g_app.scale;
    float x = (a.x + b.x) * 0.5f;
    if (!SpotFree(x, 110.0f)) x = PickSceneSpot(140.0f);
    if (x < 0.0f) return;
    float y = (float)FloorYAt(x);
    int idx = AddProp(PR_TABLE, x, y, 16.0f, a.id, b.id, 2);  // 2 = wrestle table
    a.propIndex = idx; b.propIndex = idx;
    a.seatOff = -26.0f * sc; b.seatOff = 26.0f * sc;
    a.act = Act::ARMWRESTLE; b.act = Act::ARMWRESTLE;
    a.actTimer = 12.0f; b.actTimer = 12.0f;
    a.animTime = b.animTime = 0.0f;
    a.actPhase = b.actPhase = 0.0f;
    a.partnerId = b.id; b.partnerId = a.id;
    a.facing = 1; b.facing = -1;
    a.socialCooldown = b.socialCooldown = 14.0f;
    g_app.props[idx].data = RandInt(2);      // who wins
    g_app.props[idx].y = y;
    a.x = x + a.seatOff; b.x = x + b.seatOff;
    SpawnParticle(x, y - 30.0f * sc, 0, 0, 12.0f, 0.3f, 255, 240, 160, 1);
}

static void StartTugWar(Stickman& a, Stickman& b) {
    float sc = g_app.scale;
    float x = (a.x + b.x) * 0.5f;
    if (!SpotFree(x, 190.0f)) x = PickSceneSpot(140.0f);
    if (x < 0.0f) return;
    float y = (float)FloorYAt(x);
    int idx = AddProp(PR_TABLE, x, y, 14.0f, a.id, b.id, 3);  // 3 = rope, no table top
    a.propIndex = idx; b.propIndex = idx;
    a.act = Act::TUGWAR; b.act = Act::TUGWAR;
    a.actTimer = 11.0f; b.actTimer = 11.0f;
    a.animTime = b.animTime = 0.0f;
    a.actPhase = b.actPhase = 0.0f;
    a.partnerId = b.id; b.partnerId = a.id;
    a.facing = 1; b.facing = -1;
    a.x = x - 44.0f * sc; b.x = x + 44.0f * sc;
    a.y = y; b.y = y;
    a.grounded = b.grounded = true;
    a.standingHwnd = b.standingHwnd = nullptr;
    g_app.props[idx].data = RandInt(2);
    SpawnParticle(x, y - 24.0f * sc, 0, 0, 11.0f, 0.3f, 255, 240, 160, 1);
}

// A duel should end like in the cartoons: a handshake, not a KO.
static void EndSparWithHandshake(Stickman& s) {
    Stickman* p = FindStickmanById(s.partnerId);
    s.hasSword = false;
    s.swordTimer = 0.0f;
    s.act = Act::HANDSHAKE;
    s.actTimer = 2.4f;
    s.animTime = 0.0f;
    s.socialCooldown = RandRange(8.0f, 18.0f);
    s.sparCooldown = RandRange(150.0f, 320.0f);
    if (p) {
        p->hasSword = false;
        p->swordTimer = 0.0f;
        p->act = Act::HANDSHAKE;
        p->actTimer = 2.4f;
        p->animTime = 0.0f;
        p->socialCooldown = s.socialCooldown;
        p->sparCooldown = s.sparCooldown;
        p->partnerId = s.id;
        float mx = (s.x + p->x) * 0.5f;
        float my = (s.y + p->y) * 0.5f - 34.0f * g_app.scale;
        SpawnBurst(mx, my, 180, 240, 180, 7, 2);
        // and a little "friends again" banner
        AddProp(PR_SIGN, mx, (float)FloorYAt(mx), 6.5f, s.id, p->id, 1);   // 1 = heart
    }
}

// ---- sneaking a look at the chat you have open ----
static void TrySpyChat(Stickman& s) {
    const DesktopPlatform* win = nullptr;
    float bd = 1e9f;
    for (const auto& p : g_app.platforms) {
        if (p.kind != 2 && p.kind != 3) continue;
        if (p.rect.top < 70 || p.rect.bottom > g_app.height - 20) continue;
        float d = std::fabs((float)(p.rect.left + p.rect.right) * 0.5f - s.x);
        if (d < bd) { bd = d; win = &p; }
    }
    if (!win) return;
    bool useLeft = (std::fabs((float)win->rect.left - s.x) < std::fabs((float)win->rect.right - s.x));
    float ex = useLeft ? (float)win->rect.left - 20.0f * g_app.scale
                       : (float)win->rect.right + 20.0f * g_app.scale;
    float targetY = (float)win->rect.top;
    float dy = targetY - s.y;
    if (dy < -30.0f && dy > -430.0f && std::fabs(ex - s.x) < 320.0f && s.grounded) {
        s.vy = -std::sqrt(2.0f * 1280.0f * (std::fabs(dy) + 60.0f));
        s.vx = ClampF((ex - s.x) * 1.1f, -360.0f, 360.0f);
        s.grounded = false;
        s.standingHwnd = nullptr;
        s.act = Act::AIRBORNE;
        s.travelWin = win->hwnd;
        s.nextAct = Act::SPY_CHAT;
        s.travelTargetX = ex;
        s.travelFloor = false;
        s.actTimer = 2.2f;
        s.doingFlip = (dy < -160.0f);
    } else {
        BeginTravel(s, Act::SPY_CHAT, ClampF(ex, 30.0f, (float)g_app.width - 30.0f));
        s.travelWin = nullptr;
    }
}

// ---- tea / break reminder ----
static void StartTeaBreak() {
    float sc = g_app.scale;
    Stickman* who = nullptr;
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED || s.propIndex >= 0) continue;
        if (!who) who = &s;
        if (s.type == CharType::GREEN) { who = &s; break; }
    }
    if (!who) return;
    int idx = AddProp(PR_SIGN, (float)g_app.width * 0.5f, (float)g_app.height - 150.0f * sc,
                      26.0f, who->id, -1, 2);   // 2 = "stretch break" banner
    (void)idx;
    BeginTravel(*who, Act::TEA_RUN, ClampF(g_app.cursor.x + 60.0f * sc, 40.0f, (float)g_app.width - 40.0f));
    who->carry = 1;              // the mug is already in his hand
    who->nextAct = Act::TEA_RUN;
    who->travelFloor = true;
    for (auto& s : g_app.stickmen) {
        if (&s == who) continue;
        if (s.act == Act::IDLE || s.act == Act::WALK) { s.act = Act::YAWN; s.actTimer = RandRange(2.4f, 4.6f); s.propT = 0.0f; }
    }
}

// ---- v4: a free pair of hands, on request ----
static Stickman* PickFreeGuy(int prefer) {   // 1 = joker, 2 = the calm one, 0 = anyone
    Stickman* who = nullptr;
    for (auto& s : g_app.stickmen) {
        if (!s.grounded || s.act == Act::GRABBED || s.propIndex >= 0) continue;
        if (s.act == Act::SPARRING || s.act == Act::NAP || s.act == Act::CAMPFIRE) continue;
        if (ActIsScene(s.act)) continue;
        if (!who) who = &s;
        if (prefer == 1 && (s.type == CharType::TSC_ORANGE || s.type == CharType::RED)) { who = &s; break; }
        if (prefer == 2 && s.type == CharType::GREEN) { who = &s; break; }
    }
    return who;
}

// he "hides" a desktop icon: a DRAWN shortcut tile that he buries under the
// taskbar and puts back a few seconds later. Your real icons are never touched.
static void StartCursorHide(Stickman& s) {
    float sc = g_app.scale;
    float x = ClampF(s.x + s.facing * RandRange(28.0f, 90.0f), 46.0f, (float)g_app.width - 46.0f);
    float y = (float)FloorYAt(x) - 40.0f * sc;
    int idx = AddProp(PR_ICON, x, y, RandRange(12.0f, 18.0f), s.id, -1, 3);
    if (idx < 0) return;
    g_app.props[idx].w = x;                 // where it belongs (we put it back)
    g_app.props[idx].h = y;
    s.propIndex = idx;
    s.blocks = 0;
    s.hidden = true;
    s.prankCool = RandRange(40.0f, 70.0f);
    s.act = Act::HIDE_ICON;
    s.actTimer = RandRange(6.0f, 12.0f);
    s.animTime = 0.0f; s.propT = 0.0f;
    s.facing = (x >= s.x) ? 1 : -1;
}

// kick a drawn shortcut tile across the desktop and chase it
static void StartIconGame(Stickman& s) {
    float sc = g_app.scale;
    float x = ClampF(s.x + s.facing * RandRange(40.0f, 130.0f), 40.0f, (float)g_app.width - 40.0f);
    int idx = AddProp(PR_ICON, x, (float)FloorYAt(x) - 80.0f, RandRange(10.0f, 15.0f), s.id, -1, 0);
    if (idx < 0) return;
    g_app.props[idx].vx = s.facing * 60.0f;
    s.prankCool = RandRange(40.0f, 75.0f);
    SeatAt(s, Act::ICON_KICK, idx, -20.0f * sc, s.facing);
}

// helpful: fetch a folder (a drawn one) and carry it over to the pointer
static void StartBring(Stickman& s) {
    float sc = g_app.scale;
    float x = (RandInt(2) == 0) ? 52.0f * sc : (float)g_app.width - 52.0f * sc;
    x = ClampF(x, 30.0f, (float)g_app.width - 30.0f);
    int idx = AddProp(PR_FOLDER, x, (float)FloorYAt(x), 26.0f, s.id, -1, 0);
    if (idx < 0) return;
    s.blocks = 0;
    s.carry = 0;
    SeatAt(s, Act::BRING_FOLDER, idx, (x > s.x ? -24.0f : 24.0f) * sc, (x >= s.x) ? 1 : -1);
}

// mischief: knock a browser tab off the top of a window, then tape it back.
// The tab is a DRAWN one; no window and no real tab is touched.
static void StartTabOops(Stickman& s) {
    float sc = g_app.scale;
    float topY = 26.0f * sc;
    float x = ClampF(s.x + s.facing * RandRange(30.0f, 120.0f), 60.0f, (float)g_app.width - 60.0f);
    if (g_app.interactWindows) {
        for (const auto& p : g_app.platforms) {
            if (!p.isActive) continue;
            if (p.rect.bottom - p.rect.top < 200) continue;
            topY = (float)p.rect.top + 6.0f * sc;
            x = ClampF((float)(p.rect.left + p.rect.right) * 0.5f + RandRange(-160.0f, 160.0f),
                       (float)p.rect.left + 40.0f, (float)p.rect.right - 40.0f);
            break;
        }
    }
    int idx = AddProp(PR_TAB, x, topY, RandRange(16.0f, 22.0f), s.id, -1, 3);
    if (idx < 0) return;
    g_app.props[idx].w = x;                 // anchor: where the tab belongs
    g_app.props[idx].h = topY;
    s.propIndex = idx;
    s.blocks = 0;
    s.prankCool = RandRange(50.0f, 90.0f);
    s.act = Act::TAB_OOPS;
    s.actTimer = 6.0f;
    s.animTime = 0.0f; s.propT = 0.0f;
    s.x = LerpF(s.x, x - s.facing * 40.0f * sc, 0.6f);
    s.y = (float)FloorYAt(s.x);
    s.facing = (x >= s.x) ? 1 : -1;
}

// the PC is working itself to death / the laptop is on 4%: run to the socket
static void StartPlugRun(Stickman& s) {
    float sc = g_app.scale;
    float socketX = (s.x < (float)g_app.width * 0.5f) ? 24.0f * sc : (float)g_app.width - 24.0f * sc;
    BeginTravel(s, Act::PLUG_IN, socketX);
    s.travelFloor = true;
    s.carry = 9;
    s.blocks = 0;
    s.actTimer = RandRange(8.0f, 11.0f);
    if (!AnyoneSpeaking()) SayNote(s, 13);
}

// sit in front of the Send button and guard it (drawn, of course)
static void StartSendGuard(Stickman& s) {
    float sc = g_app.scale;
    float gx = ClampF(g_app.cursor.x + 90.0f * sc, 60.0f, (float)g_app.width - 60.0f);
    float gy = (float)FloorYAt(gx);
    if (g_app.interactWindows) {
        for (const auto& p : g_app.platforms) {
            if (!p.isActive || p.kind != 2) continue;
            if (p.rect.right - p.rect.left < 260 || p.rect.bottom - p.rect.top < 200) continue;
            gx = ClampF((float)p.rect.right - 58.0f * sc, 40.0f, (float)g_app.width - 40.0f);
            if ((float)p.rect.bottom + 40.0f * sc < (float)g_app.height - 8.0f)
                gy = std::min(gy, (float)p.rect.bottom - 4.0f * sc);
            break;
        }
    }
    g_app.guardX = gx;
    g_app.guardY = gy;
    g_app.guardT = 0.0f;
    s.blocks = 0;
    BeginTravel(s, Act::SEND_GUARD, gx);
    s.travelFloor = false;
    s.workCool = RandRange(60.0f, 140.0f);
    if (!AnyoneSpeaking()) SayNote(s, 9);
}

// rain or snow outside: haul an umbrella out and stand by the "window"
static void StartUmbrella(Stickman& s) {
    float x = PickSceneSpot(200.0f);
    if (x < 0.0f) x = ClampF(s.x, 90.0f, (float)g_app.width - 90.0f);
    BeginTravel(s, Act::UMBRELLA, x);
    s.travelFloor = true;
    s.carry = 10;
    s.actTimer = RandRange(14.0f, 26.0f);
    if (!AnyoneSpeaking()) SayNote(s, g_app.raining ? 14 : 8);
}

// two of them pass a ball back and forth
static void StartPassBall(Stickman& s) {
    if (g_app.balls.empty()) {
        Ball b;
        b.x = s.x; b.y = s.y - 70.0f * g_app.scale;
        b.vx = -s.facing * 60.0f; b.vy = -120.0f;
        b.r = 11.0f * g_app.scale;
        b.life = 34.0f;
        g_app.balls.push_back(b);
    }
    Stickman* pal = nullptr;
    for (auto& o : g_app.stickmen) {
        if (o.id == s.id || !o.grounded) continue;
        if (o.act == Act::GRABBED || o.act == Act::SPARRING || o.propIndex >= 0) continue;
        if (std::fabs(o.x - s.x) < 140.0f) continue;
        if (!pal) pal = &o;
    }
    if (!pal) return;
    s.partnerId = pal->id;
    pal->partnerId = s.id;
    s.act = Act::PASS_BALL; s.actTimer = RandRange(9.0f, 15.0f); s.animTime = 0.0f;
    pal->act = Act::PASS_BALL; pal->actTimer = RandRange(9.0f, 15.0f); pal->animTime = 0.0f;
    pal->facing = (s.x >= pal->x) ? 1 : -1;
    s.facing = (pal->x >= s.x) ? 1 : -1;
}

// a staged sword fight for the crowd: sparkler swords, bows, confetti
static void StartShowDuel() {
    if (g_app.stickmen.size() < 2) {
        SpawnStickman(CharType::RED, (float)g_app.width * 0.5f + 80.0f, 130.0f, false);
    }
    if (g_app.stickmen.size() < 2) return;
    Stickman* a = PickFreeGuy(1);
    if (!a) a = &g_app.stickmen[0];
    Stickman* b = nullptr;
    for (auto& o : g_app.stickmen) {
        if (o.id == a->id || !o.grounded || o.act == Act::GRABBED) continue;
        if (!b) b = &o;
    }
    if (!b) {
        SpawnStickman(CharType::GREEN, a->x + 120.0f, a->y - 10.0f, false);
        if (!g_app.stickmen.empty()) b = &g_app.stickmen.back();
    }
    if (!b || a == b) return;
    LeaveSeat(*a); LeaveSeat(*b);
    a->act = b->act = Act::SPARRING;
    a->partnerId = b->id; b->partnerId = a->id;
    a->actTimer = b->actTimer = 12.0f;
    a->animTime = b->animTime = 0.0f;
    a->hasSword = b->hasSword = true;
    a->weapon = b->weapon = 1;                  // diamond swords, drawn
    a->swordTimer = b->swordTimer = 14.0f;
    a->sparCooldown = RandRange(60.0f, 110.0f);
    b->sparCooldown = a->sparCooldown;
    float mid = (a->x + b->x) * 0.5f;
    AddProp(PR_SIGN, mid, (float)FloorYAt(mid) - 120.0f * g_app.scale, 13.0f, -1, -1, 3);
    for (auto& o : g_app.stickmen) {
        if (o.id == a->id || o.id == b->id) continue;
        if (!o.grounded || o.act == Act::GRABBED || o.propIndex >= 0) continue;
        o.act = Act::WATCH_GAME;
        o.actTimer = RandRange(12.0f, 18.0f);
        o.animTime = 0.0f;
    }
    SpawnBurst(mid, (float)FloorYAt(mid) - 60.0f * g_app.scale, 255, 230, 90, 12, 2);
}

// ---- harmless mischief, picked at random ----
static void TryPrank() {
    float sc = g_app.scale;
    Stickman* who = nullptr;
    for (auto& s : g_app.stickmen) {
        if (!s.grounded || s.act == Act::GRABBED || s.propIndex >= 0 || s.act == Act::SPARRING) continue;
        if (s.prankCool > 0.0f) continue;
        if (!who) who = &s;
        if (s.type == CharType::TSC_ORANGE || s.type == CharType::RED) { who = &s; break; }
    }
    if (!who) return;
    who->prankCool = RandRange(35.0f, 80.0f);

    int roll = RandInt(100);
    if (roll < 10) { StartCursorHide(*who); return; }        // v4: hide a (drawn) icon
    if (roll < 20) { StartIconGame(*who); return; }          // v4: kick a (drawn) shortcut
    if (roll < 27) { StartTabOops(*who); return; }           // v4: knock a fake tab off
    if (roll < 34) {
        // "run into the edge of the monitor" + tape repair
        float side = (who->x < (float)g_app.width * 0.5f) ? 18.0f : (float)g_app.width - 18.0f;
        BeginTravel(*who, Act::CRACK_PRANK, side);
        who->travelFloor = true;
        who->actTimer = 8.0f;
    } else if (roll < 62) {
        // draw a cube and kick it around (a drawn one, never a real icon)
        float x = ClampF(who->x + who->facing * RandRange(40.0f, 130.0f), 40.0f, (float)g_app.width - 40.0f);
        int idx = AddProp(PR_BOX, x, (float)FloorYAt(x) - 90.0f, RandRange(14.0f, 20.0f), who->id, -1, 0);
        if (idx >= 0) {
            g_app.props[idx].vx = who->facing * 90.0f;
            SeatAt(*who, Act::BOX_KICK, idx, -22.0f * sc, who->facing);
        }
    } else if (roll < 84) {
        // doodle a face on the wallpaper, then erase it
        float x = PickSceneSpot(220.0f);
        if (x < 0.0f) x = ClampF(who->x, 90.0f, (float)g_app.width - 90.0f);
        BeginTravel(*who, Act::MUSTACHE, x);
        who->travelFloor = true;
        who->carry = 7;
        who->doodleShape = 4;   // a smiley with a moustache
        who->doodle.clear();
        who->doodleFade = 12.0f;
    } else {
        // shove a BACKGROUND window a few pixels and let it slide back
        if (!g_app.pushWindows) return;
        for (const auto& p : g_app.platforms) {
            if (p.isActive) continue;
            if (p.rect.top <= 4 || p.rect.bottom >= g_app.height - 4) continue;   // maximized: hands off
            if (p.rect.right - p.rect.left < 260) continue;
            if (p.zIndex < 2) continue;
            who->pushHwnd = p.hwnd;
            who->pushOrigin = V2((float)p.rect.left, (float)p.rect.top);
            who->pushT = 0.0f;
            who->pushDir = (who->x < (float)(p.rect.left + p.rect.right) * 0.5f) ? -1.0f : 1.0f;
            BeginTravel(*who, Act::PUSH_WINDOW,
                        who->pushDir < 0.0f ? (float)p.rect.left - 22.0f * sc : (float)p.rect.right + 22.0f * sc);
            who->travelFloor = false;
            return;
        }
    }
}

// ---- occasional group scene instead of solo wandering ----
static void TryGroupScene(Stickman& s) {
    int roll = RandInt(100);
    if (roll < 8) { StartPassBall(s); return; }                       // v4: a game of catch
    if (roll < 14 && g_app.fgKind == 2) { StartSendGuard(s); return; } // v4: guard your Send button
    if (roll < 26) { StartPartyCorner(); return; }
    if (roll < 44) { StartCookLine(); return; }
    if (roll < 56) { StartLabScene(); return; }
    if (roll < 70) {
        Stickman* other = nullptr;
        for (auto& o : g_app.stickmen) {
            if (o.id == s.id || !o.grounded || o.propIndex >= 0) continue;
            if (o.act == Act::GRABBED || o.act == Act::SPARRING) continue;
            if (std::fabs(o.x - s.x) > 340.0f) continue;
            if (!other) other = &o;
        }
        if (other) {
            if (RandInt(2) == 0) StartArmWrestle(s, *other);
            else StartTugWar(s, *other);
            return;
        }
    }
    if (roll < 82 && (g_app.fgKind == 2 || g_app.fgKind == 3)) { TrySpyChat(s); return; }
    if (roll < 92 && g_app.winter) {
        float x = PickSceneSpot(120.0f);
        if (x > 0.0f) {
            int idx = AddProp(PR_PILLOW, x, (float)FloorYAt(x), 26.0f, s.id, -1, 1); // 1 = snowman
            if (idx >= 0) SeatAt(s, Act::BUILD, idx, 26.0f * g_app.scale, -1);
            return;
        }
    }
    SetAct(s, Act::IDLE);
}

// per-frame behaviour for the v3 scene acts
static void SceneFX(Stickman& s, float dt) {
    float sc = g_app.scale;
    WorldProp* w = PropAt(s.propIndex);

    switch (s.act) {
        case Act::CARD_GAME: {
            s.actPhase += dt * (1.0f + 0.5f * Osc(s.animTime, 0.7f));
            if (w) s.x = LerpF(s.x, w->x + s.seatOff, ClampF(dt * 7.0f, 0.0f, 1.0f));
            s.facing = (s.seatOff <= 0.0f) ? 1 : -1;
            if (RandInt(140) == 0) {
                s.squashVel = -0.9f;
                SpawnParticle(s.pose.handR.x, s.pose.handR.y - 6.0f, RandRange(-30, 30), -60.0f,
                              2.6f, 0.5f, 250, 245, 235, 0);
            }
            break;
        }
        case Act::WATCH_GAME: {
            if (w) {
                float side = (s.x < w->x) ? -1.0f : 1.0f;
                s.facing = (side > 0.0f) ? -1 : 1;
                s.x = LerpF(s.x, w->x + side * (86.0f + 20.0f * (float)(s.id % 3)), ClampF(dt * 5.0f, 0.0f, 1.0f));
            }
            if (RandInt(70) == 0) s.squashVel = -1.0f;
            break;
        }
        case Act::COOK: {
            s.actPhase += dt * 1.6f;
            if (w) {
                w->phase += dt;
                s.x = LerpF(s.x, w->x - 8.0f * sc, ClampF(dt * 6.0f, 0.0f, 1.0f));
                s.facing = 1;
                if (RandInt(14) == 0) {
                    SpawnParticle(w->x + RandRange(-9, 9) * sc, w->y - 26.0f * sc, RandRange(-14, 14),
                                  RandRange(-70, -34), RandRange(2.6f, 5.0f), 0.85f, 235, 235, 235, 0);
                }
            }
            break;
        }
        case Act::QUEUE: {
            if (w) {
                s.x = LerpF(s.x, w->x + 34.0f * sc + s.seatOff, ClampF(dt * 4.0f, 0.0f, 1.0f));
                s.facing = (s.x >= w->x) ? -1 : 1;
            }
            break;
        }
        case Act::LAB: {
            s.actPhase += dt;
            if (w) {
                s.x = LerpF(s.x, w->x - 16.0f * sc, ClampF(dt * 6.0f, 0.0f, 1.0f));
                s.facing = 1;
                if (RandInt(40) == 0) {
                    SpawnParticle(w->x + RandRange(-10, 10) * sc, w->y - 22.0f * sc, RandRange(-30, 30),
                                  RandRange(-80, -30), RandRange(2.0f, 3.4f), 0.6f, 130, 245, 255, 5);
                }
            }
            // a small harmless boom: soot on the face, black smoke, everyone jumps
            if (w && s.actPhase > RandRange(5.0f, 14.0f) && w->data == 0) {
                w->data = 1;
                s.soot = 1.0f;
                s.squashVel = -3.2f;
                s.act = Act::FLINCH;
                s.actTimer = 1.2f;
                s.animTime = 0.0f;
                float bx = w->x, by = w->y - 24.0f * sc;
                for (int i = 0; i < 14; ++i) {
                    SpawnParticle(bx, by, RandRange(-70, 70), RandRange(-140, -20),
                                  RandRange(4.0f, 10.0f), RandRange(0.7f, 1.5f), 54, 52, 56, 0);
                }
                SpawnParticle(bx, by, 0, 0, 13.0f, 0.35f, 255, 200, 90, 1);
                for (auto& o : g_app.stickmen) {
                    if (o.id == s.id || !o.grounded) continue;
                    if (std::fabs(o.x - s.x) < 260.0f) {
                        o.squashVel = -2.2f;
                        if (RandInt(3) == 0) { o.act = Act::LAUGH; o.actTimer = RandRange(3.0f, 5.0f); }
                    }
                }
            }
            break;
        }
        case Act::ARMWRESTLE:
        case Act::TUGWAR: {
            Stickman* p = FindStickmanById(s.partnerId);
            s.actPhase += dt * 2.2f;
            bool winning = (w && (w->data == 0) == (w->a == s.id));
            if (w) {
                float off = s.seatOff;
                if (s.act == Act::TUGWAR) off = (s.seatOff == 0.0f)
                    ? ((s.x < w->x) ? -44.0f * sc : 44.0f * sc) : s.seatOff;
                float drag = winning ? 6.0f * sc : -5.0f * sc;
                s.x = LerpF(s.x, w->x + off + drag, ClampF(dt * 3.0f, 0.0f, 1.0f));
                if (p) {
                    // keep the two in a fixed relationship
                    float gap = (s.act == Act::TUGWAR) ? 88.0f * sc : 52.0f * sc;
                    float want = s.x + ((p->x >= s.x) ? gap : -gap);
                    p->x = LerpF(p->x, want, ClampF(dt * 3.0f, 0.0f, 1.0f));
                    p->facing = (p->x >= s.x) ? -1 : 1;
                    p->actPhase = s.actPhase;
                    s.facing = (p->x >= s.x) ? 1 : -1;
                }
                if (s.actTimer < 3.0f && RandInt(24) == 0) {
                    float mx = (s.x + (p ? p->x : s.x)) * 0.5f;
                    SpawnBurst(mx, w->y - 40.0f * sc, 255, 236, 150, 3, 2);
                }
            }
            if (s.actTimer <= 0.35f && s.actTimer - dt <= 0.35f) {
                // done: winner cheers, loser buries the face in the hand, then handshake
                if (w && p) {
                    bool iWin = (w->data == 0) == (w->a == s.id);
                    Stickman& winner = iWin ? s : *p;
                    Stickman& loser  = iWin ? *p : s;
                    winner.act = Act::CHEER; winner.actTimer = 2.2f; winner.animTime = 0.0f;
                    loser.act = Act::FACEPALM; loser.actTimer = 2.6f; loser.animTime = 0.0f;
                    ReleaseProp(winner);
                    if (&loser != &winner) ReleaseProp(loser);
                    for (auto& o : g_app.stickmen) {
                        if (o.audienceOf != -1) { o.act = Act::CHEER; o.actTimer = 2.0f; o.audienceOf = -1; }
                    }
                }
            }
            break;
        }
        case Act::SPY_CHAT: {
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            if (s.standingHwnd) {
                const DesktopPlatform* wp = FindPlatformByHwnd(s.standingHwnd);
                if (wp) s.x = LerpF(s.x, (s.x < (float)(wp->rect.left + wp->rect.right) * 0.5f)
                                    ? (float)wp->rect.left - 20.0f * sc
                                    : (float)wp->rect.right + 20.0f * sc, ClampF(dt * 4.0f, 0.0f, 1.0f));
            }
            if (g_app.typingPulse > 0.3f && RandInt(9) == 0) {
                SpawnParticle(s.pose.handR.x, s.pose.handR.y - 8.0f, RandRange(-20, 20), -50.0f,
                              2.4f, 0.5f, 190, 240, 255, 2);
            }
            break;
        }
        case Act::DANCE: {
            s.actPhase += dt * (2.4f + 3.2f * g_app.audioEnv);
            if (RandInt(90) == 0) s.squashVel = -1.2f;
            break;
        }
        case Act::EXTINGUISH: {
            s.actPhase += dt;
            if (RandInt(2) == 0) {
                Vec2 h = s.pose.handR;
                SpawnParticle(h.x + 6.0f * sc, h.y - 6.0f * sc, RandRange(60, 190) * (float)s.facing,
                              RandRange(-40, -8), RandRange(5.0f, 12.0f), RandRange(0.35f, 0.7f),
                              245, 248, 252, 0);
            }
            break;
        }
        case Act::ASK_ATTENTION: {
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            if (s.actTimer > 0.0f) {
                float want = ClampF(g_app.cursor.x - s.facing * 70.0f * sc, 30.0f, (float)g_app.width - 30.0f);
                s.x = LerpF(s.x, want, ClampF(dt * 1.2f, 0.0f, 1.0f));
            }
            break;
        }
        case Act::TEA_RUN: {
            if (s.blocks == 0) {
                float want = ClampF(g_app.cursor.x + 56.0f * sc, 30.0f, (float)g_app.width - 30.0f);
                float dx = want - s.x;
                s.facing = (dx >= 0.0f) ? 1 : -1;
                s.vx = std::fabs(dx) > 20.0f ? s.facing * 130.0f : 0.0f;
                s.blocks = (std::fabs(dx) < 26.0f) ? 1 : 0;
                if (s.blocks == 1) {
                    s.act = Act::POINT_AT;
                    s.actTimer = 4.0f;
                    SpawnParticle(s.pose.handR.x, s.pose.handR.y - 8.0f, 0, -34.0f, 6.0f, 1.2f, 190, 240, 255, 3);
                }
            }
            break;
        }
        case Act::CRACK_PRANK: {
            if (s.actTimer >= 4.6f) s.carry = 11;      // the (drawn) hammer, not a real click
            if (w == nullptr && s.actTimer < 4.6f && s.actTimer - dt >= 4.6f) {
                float ex = (s.x < (float)g_app.width * 0.5f) ? 3.0f : (float)g_app.width - 3.0f;
                int idx = AddProp(PR_CRACK, ex, s.y - 46.0f * sc, 7.0f, s.id, -1, 0);
                (void)idx;
                s.squashVel = 2.6f;
                s.vx = 0.0f;
                SpawnBurst(ex, s.y - 40.0f * sc, 250, 250, 255, 8, 2);
                s.carry = 6;   // tape
            }
            break;
        }
        case Act::BOX_KICK: {
            WorldProp* box = (w && w->kind == PR_BOX) ? w : nullptr;
            if (!box) { SetAct(s, Act::IDLE); break; }
            s.actPhase += dt * 3.0f;
            float dx = box->x - s.x;
            if (std::fabs(dx) > 24.0f * sc) {
                s.facing = (dx >= 0.0f) ? 1 : -1;
                s.vx = s.facing * 170.0f;
            } else {
                s.vx *= std::pow(0.002f, dt);
                box->vx += s.facing * 330.0f;
                box->vy -= 250.0f;
                box->spin = box->vx / 10.0f;
                s.squashVel = -1.4f;
                SpawnBurst(box->x, box->y, 240, 240, 240, 3, 0);
            }
            break;
        }
        case Act::MUSTACHE: {
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            if (s.doodle.size() < 120 && RandInt(2) == 0) {
                float u = (float)s.doodle.size() * 0.22f;
                float ax = s.x + s.facing * 30.0f * sc;
                float ay = s.y - 40.0f * sc;
                float rr = 20.0f * sc;
                float ang = u;
                float nx, ny;
                if (u < 6.2f) { nx = ax + std::cos(ang) * rr; ny = ay + std::sin(ang) * rr * 0.9f; }
                else {   // moustache sweep under the "nose"
                    float v = u - 6.2f;
                    nx = ax + std::cos(v * 2.2f + 3.4f) * rr * 0.55f;
                    ny = ay + rr * 0.45f + std::sin(v * 2.2f) * 3.0f * sc;
                }
                s.doodle.push_back(V2(nx, ny));
            }
            if (s.doodleFade <= 0.0f) s.doodleFade = 12.0f;
            break;
        }
        case Act::IN_VIDEO: {
            s.actPhase += dt;
            break;
        }
        case Act::PUSH_WINDOW: {
            s.pushT += dt;
            float prog = ClampF(s.pushT / 3.4f, 0.0f, 1.0f);
            float shove = std::sin(prog * PI_F) * 7.0f * s.pushDir;
            bool userDragging = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (userDragging) { s.pushT = 3.5f; }
            if (s.pushHwnd && IsWindow(s.pushHwnd) && !userDragging) {
                RECT rc = {};
                if (GetWindowRect(s.pushHwnd, &rc)) {
                    float wantL = s.pushOrigin.x + shove;
                    // hands off while the user is dragging anything
                    if (std::fabs((float)rc.left - wantL) < 22.0f) {
                        SetWindowPos(s.pushHwnd, nullptr, (int)(s.pushOrigin.x + shove),
                                     (int)s.pushOrigin.y, 0, 0,
                                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    } else {
                        s.pushHwnd = nullptr;
                    }
                }
            }
            if (prog >= 1.0f) {
                if (s.pushHwnd) {
                    SetWindowPos(s.pushHwnd, nullptr, (int)s.pushOrigin.x, (int)s.pushOrigin.y, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    s.pushHwnd = nullptr;
                }
                s.act = Act::WALK;
                s.actTimer = RandRange(1.0f, 2.2f);
                s.squashVel = -1.2f;
            }
            break;
        }
        case Act::SQUISH: {
            // vibrate against the frame, then stumble away shaking a fist
            s.x += std::sin(s.propT * 46.0f) * 22.0f * dt;
            s.propT += dt;
            if (s.propT > 1.25f && s.blocks == 0) {
                s.blocks = 1;
                s.standingHwnd = nullptr;
                s.grounded = false;
                s.vy = -300.0f;
                s.vx = -s.facing * 130.0f;
                s.act = Act::AIRBORNE;
                s.actTimer = 1.0f;
                s.squashVel = -2.0f;
                SpawnBurst(s.x, s.y - 8.0f * sc, 220, 226, 236, 6, 0);
            }
            break;
        }
        case Act::PETTED: {
            if (RandInt(26) == 0) {
                SpawnParticle(s.pose.head.x + RandRange(-8, 8), s.pose.head.y - 12.0f,
                              RandRange(-12, 12), -46.0f, 4.0f, 0.9f, 255, 120, 170, 9);
            }
            break;
        }
        case Act::CURSOR_PLAY:
        case Act::HANG_CURSOR: {
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            if (s.act == Act::HANG_CURSOR) {
                s.x = g_app.cursor.x;
                s.y = LerpF(s.y, g_app.cursor.y + 34.0f * sc, ClampF(dt * 9.0f, 0.0f, 1.0f));
                s.grounded = true;
                s.vx = 0.0f; s.vy = 0.0f;
                s.grabAngVel += (-g_app.mouseVel.x * 0.0035f - 11.0f * std::sin(s.grabAngle)) * dt;
                s.grabAngVel *= std::pow(0.2f, dt);
                s.grabAngle = ClampF(s.grabAngle + s.grabAngVel * dt, -0.9f, 0.9f);
            }
            break;
        }
        case Act::AIR_TYPE: {
            if (g_app.typingPulse > 0.3f) {
                s.keyAnim = std::max(s.keyAnim, g_app.typingPulse);
                if (RandInt(6) == 0) s.squashVel = -0.6f;
            }
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            break;
        }
        case Act::LAUGH: {
            if (RandInt(18) == 0) {
                SpawnParticle(s.pose.head.x + s.facing * 6.0f, s.pose.head.y - 6.0f, s.facing * 20.0f,
                              -46.0f, 2.6f, 0.6f, 150, 200, 255, 0);
            }
            break;
        }

        // ================================ v4: games with the mouse pointer ===
        case Act::CHASE_CURSOR: {
            s.actPhase += dt * 2.6f;
            float dxc = g_app.cursor.x - s.x;
            s.facing = (dxc >= 0.0f) ? 1 : -1;
            float lunge = ClampF(-std::cos(s.actPhase * 2.6f) - 0.30f, 0.0f, 1.0f);
            s.vx = ClampF(dxc * (0.8f + 2.4f * lunge), -470.0f, 470.0f);
            if (lunge > 0.72f && std::fabs(dxc) < 34.0f * sc) {
                s.squashVel = -2.8f;
                SpawnBurst(g_app.cursor.x, g_app.cursor.y, 255, 244, 150, 5, 2);
                if (RandInt(5) == 0 && !AnyoneSpeaking()) SayNote(s, 13);
            }
            if (s.grounded && g_app.cursor.y < (float)FloorYAt(s.x) - 190.0f * sc && lunge > 0.5f)
                s.squashVel = -3.6f;                    // he jumps for a raised pointer
            break;
        }

        case Act::RIDE_CURSOR: {
            // he stands on a *drawn* copy of the arrow; the real pointer is untouched
            s.x = LerpF(s.x, g_app.cursor.x, ClampF(dt * 15.0f, 0.0f, 1.0f));
            s.y = LerpF(s.y, g_app.cursor.y + 26.0f * sc, ClampF(dt * 17.0f, 0.0f, 1.0f));
            s.grounded = true; s.standingHwnd = nullptr;
            s.vx = 0.0f; s.vy = 0.0f;
            float sp = Len(g_app.mouseVel);
            if (std::fabs(g_app.mouseVel.x) > 30.0f) s.facing = (g_app.mouseVel.x > 0.0f) ? 1 : -1;
            if (sp > 800.0f && RandInt(2) == 0) {
                SpawnParticle(s.x - s.facing * 12.0f * sc, s.y + 3.0f, -s.facing * 120.0f, -34.0f,
                              RandRange(2.0f, 4.0f), 0.4f, 235, 242, 250, 0);
            }
            if (sp > 400.0f && RandInt(40) == 0 && !AnyoneSpeaking()) SayNote(s, 11);
            if (g_app.ghostMode || sp > 2500.0f) {      // the ride ends abruptly
                s.rides = 0; s.hidden = false;
                s.act = Act::AIRBORNE; s.grounded = false; s.vy = -60.0f; s.actTimer = 1.0f;
                s.squashVel = -2.0f;
                SpawnBurst(s.x, s.y - 20.0f * sc, 220, 228, 240, 5, 0);
            }
            break;
        }

        case Act::CURSOR_THEFT: {
            s.floatT += dt;
            Vec2 hand = s.pose.handR;
            g_app.ghostCursor.x = LerpF(g_app.ghostCursor.x, hand.x + s.facing * 5.0f * sc,
                                        ClampF(dt * 11.0f, 0.0f, 1.0f));
            g_app.ghostCursor.y = LerpF(g_app.ghostCursor.y, hand.y + 3.0f * sc,
                                        ClampF(dt * 11.0f, 0.0f, 1.0f));
            if (s.floatT > 0.7f && s.floatT < 2.0f && RandInt(10) == 0) {
                SpawnParticle(g_app.ghostCursor.x + RandRange(-6, 6), g_app.ghostCursor.y,
                              -s.facing * 70.0f, RandRange(-40, 10), 2.4f, 0.4f, 255, 236, 120, 0);
                s.squashVel = -0.8f;
            }
            if (s.floatT > 2.0f && s.blocks == 0) {     // give it back, politely
                s.blocks = 1;
                g_app.ghostCursor = g_app.cursor;
                s.carry = 0; s.rides = 0;
                if (!AnyoneSpeaking()) SayNote(s, 4);
                SpawnBurst(g_app.cursor.x, g_app.cursor.y, 250, 252, 240, 7, 1);
                s.actTimer = std::min(s.actTimer, 1.2f);
            }
            break;
        }

        case Act::FIGHT_CURSOR: {
            s.actPhase += dt * 5.2f;
            float dxc = g_app.cursor.x - s.x;
            float dyc = g_app.cursor.y - (s.y - 46.0f * sc);
            float reach = Len(V2(dxc, dyc));
            s.facing = (dxc >= 0.0f) ? 1 : -1;
            s.vx = ClampF(dxc * 0.8f, -230.0f, 230.0f);
            if (std::sin(s.actPhase) > 0.80f && reach < 66.0f * sc) {
                ++s.blocks;
                SpawnBurst(g_app.cursor.x, g_app.cursor.y, 255, 226, 90, 4, 2);
                s.squashVel = -0.9f;
            }
            if (Len(g_app.mouseVel) > 700.0f && reach < 48.0f * sc && RandInt(8) == 0) {
                s.squashVel = -2.2f;                                        // the pointer jabs back
                s.vx = -s.facing * 150.0f;
                SpawnBurst(s.pose.head.x, s.pose.head.y, 250, 120, 90, 4, 0);
                if (RandInt(3) == 0 && !AnyoneSpeaking()) SayNote(s, 12);
            }
            if (s.weapon != 0 && RandInt(24) == 0) {
                SpawnParticle(s.pose.handR.x + s.facing * 10.0f * sc, s.pose.handR.y - 10.0f * sc,
                              s.facing * 40.0f, -30.0f, 3.0f, 0.3f, 210, 250, 255, 0);
            }
            if (s.actTimer < 1.0f && s.blocks > 0 && RandInt(3) == 0) {
                s.act = Act::CHEER; s.actTimer = RandRange(1.4f, 2.6f); s.animTime = 0.0f;
                SpawnBurst(s.x, s.y - 60.0f * sc, 255, 235, 120, 8, 2);
                if (!AnyoneSpeaking()) SayNote(s, 11);
            }
            break;
        }

        // ========================================== v4: notes and small talk ===
        case Act::SPEAK: {
            s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
            if (s.speechT > 2.6f && s.blocks == 0) { s.blocks = 1; s.squashVel = -0.7f; }
            break;
        }

        case Act::SEND_GUARD: {
            g_app.guardT += dt;
            s.x = LerpF(s.x, g_app.guardX, ClampF(dt * 6.0f, 0.0f, 1.0f));
            s.y = g_app.guardY;
            s.grounded = true; s.standingHwnd = nullptr;
            s.vx = 0.0f; s.vy = 0.0f;
            s.facing = -1;
            float dd = Len(V2(g_app.cursor.x - g_app.guardX, g_app.cursor.y - (g_app.guardY - 30.0f * sc)));
            if (dd < 52.0f * sc) {
                if (s.blocks == 0) {
                    s.blocks = 1;
                    if (!AnyoneSpeaking()) SayNote(s, 10);
                }
                s.squashVel = -1.5f;
                if (RandInt(4) == 0)
                    SpawnBurst(g_app.cursor.x, g_app.cursor.y, 255, 210, 90, 3, 1);
            } else if (dd > 130.0f * sc) {
                s.blocks = 0;
            }
            if (g_app.typingRate > 0.30f && RandInt(22) == 0) {
                s.squashVel = -0.8f;
                if (s.blocks == 0 && !AnyoneSpeaking()) SayNote(s, 9);
            }
            if (g_app.interactWindows && g_app.fgKind != 2 && g_app.guardT > 2.5f) s.actTimer = 0.05f;
            break;
        }

        // ==================================== v4: mischief with drawn things ===
        case Act::HIDE_ICON: {
            WorldProp* p = PropAt(s.propIndex);
            if (p) {
                p->data = 3;                            // held: no toy physics
                if (s.actTimer > 1.8f) {
                    p->x = LerpF(p->x, p->w, ClampF(dt * 3.0f, 0.0f, 1.0f));
                    p->y = LerpF(p->y, (float)g_app.height + 70.0f, ClampF(dt * 1.1f, 0.0f, 1.0f));
                    s.hidden = true;
                    if (RandInt(50) == 0 && !AnyoneSpeaking()) SayNote(s, 20);
                } else {
                    p->x = LerpF(p->x, p->w, ClampF(dt * 4.5f, 0.0f, 1.0f));
                    p->y = LerpF(p->y, p->h, ClampF(dt * 4.5f, 0.0f, 1.0f));
                    if (s.blocks == 0) {
                        s.blocks = 1;
                        s.hidden = false;
                        p->data = 0;
                        SpawnBurst(p->w, p->h, 255, 250, 200, 7, 2);
                        if (!AnyoneSpeaking()) SayNote(s, 21);
                        s.actTimer = std::min(s.actTimer, 1.2f);
                    }
                }
                s.x = LerpF(s.x, p->w - 15.0f * sc, ClampF(dt * 4.0f, 0.0f, 1.0f));
                s.y = LerpF(s.y, (float)FloorYAt(s.x), ClampF(dt * 8.0f, 0.0f, 1.0f));
                s.facing = 1;
            }
            break;
        }

        case Act::ICON_KICK: {
            WorldProp* p = PropAt(s.propIndex);
            if (!p) break;
            float dxp = p->x - s.x;
            s.facing = (dxp >= 0.0f) ? 1 : -1;
            s.vx = ClampF(dxp * 1.1f, -300.0f, 300.0f);
            if (std::sin(s.animTime * 3.4f) > 0.93f && std::fabs(dxp) < 42.0f * sc) {
                p->vx = s.facing * RandRange(200.0f, 360.0f);
                p->vy = -RandRange(120.0f, 260.0f);
                p->spin = s.facing * RandRange(1.6f, 4.0f);
                s.squashVel = -1.0f;
                SpawnBurst(p->x, p->y, 255, 240, 150, 3, 0);
                if (RandInt(6) == 0 && !AnyoneSpeaking()) SayNote(s, 11);
            }
            break;
        }

        case Act::BRING_FOLDER: {
            WorldProp* p = PropAt(s.propIndex);
            if (s.blocks == 0 && p) {
                s.facing = (p->x >= s.x) ? 1 : -1;
                if (std::fabs(p->x - s.x) < 42.0f * sc) {
                    s.blocks = 1;
                    s.carry = 8;                        // he picks the drawn folder up
                    float fx = p->x, fy = p->y;
                    KillProp(*p);
                    s.propIndex = -1;
                    SpawnBurst(fx, fy - 8.0f * sc, 250, 226, 120, 4, 2);
                    BeginTravel(s, Act::BRING_FOLDER, g_app.cursor.x);
                    s.travelFloor = true;
                    s.actTimer = RandRange(4.5f, 8.0f);
                }
            } else if (s.blocks == 1) {
                s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
                s.vx = ClampF((g_app.cursor.x - s.x) * 0.9f, -250.0f, 250.0f);
                if (std::fabs(g_app.cursor.x - s.x) < 38.0f * sc) {
                    s.blocks = 2;
                    float px = ClampF(g_app.cursor.x, 40.0f, (float)g_app.width - 40.0f);
                    AddProp(PR_FOLDER, px, (float)FloorYAt(px), 11.0f, -1, -1, 0);
                    SpawnBurst(px, (float)FloorYAt(px) - 10.0f * sc, 250, 226, 120, 8, 2);
                    if (!AnyoneSpeaking()) SayNote(s, 6);
                    s.carry = 0;
                    s.actTimer = std::min(s.actTimer, 2.0f);
                }
            }
            break;
        }

        case Act::TAB_OOPS: {
            WorldProp* p = PropAt(s.propIndex);
            if (!p) break;
            s.facing = (p->x >= s.x) ? 1 : -1;
            s.x = LerpF(s.x, p->x - s.facing * 26.0f * sc, ClampF(dt * 3.0f, 0.0f, 1.0f));
            s.y = LerpF(s.y, (float)FloorYAt(s.x), ClampF(dt * 8.0f, 0.0f, 1.0f));
            if (s.blocks == 0) {
                s.blocks = 1;
                p->data = 0;                            // let go: it tumbles down
                p->vx = s.facing * RandRange(40.0f, 110.0f);
                p->vy = 20.0f;
                p->spin = s.facing * 1.4f;
                if (!AnyoneSpeaking()) SayNote(s, 4);
                SpawnBurst(p->x, p->y, 240, 242, 250, 5, 0);
                s.squashVel = -1.6f;
            }
            if (s.propT > 2.4f && s.blocks == 1) { s.blocks = 2; s.carry = 6; }   // TAPE!
            if (s.propT > 4.0f && s.blocks == 2) {
                s.blocks = 3;
                s.carry = 0;
                p->data = 3;
                p->x = p->w; p->y = p->h; p->vx = 0.0f; p->vy = 0.0f; p->spin = 0.0f;
                SpawnBurst(p->x, p->y, 250, 250, 210, 7, 2);
                if (!AnyoneSpeaking()) SayNote(s, 5);
                s.actTimer = std::min(s.actTimer, 1.4f);
            }
            break;
        }

        case Act::DRAG_COUCH: {
            float sc2 = g_app.scale;
            float target = ClampF(g_app.couchGoalX, 130.0f, (float)g_app.width - 130.0f);
            float dir = (target > g_app.couchX) ? 1.0f : -1.0f;
            s.facing = (dir > 0.0f) ? 1 : -1;
            g_app.couchX = LerpF(g_app.couchX, target, ClampF(dt * 0.9f, 0.0f, 1.0f));
            RecomputeCouchAnchor();
            s.x = LerpF(s.x, g_app.couchX - dir * 96.0f * sc2, ClampF(dt * 8.0f, 0.0f, 1.0f));
            s.y = (float)FloorYAt(s.x);
            s.vx = 0.0f; s.vy = 0.0f;
            if (std::fabs(g_app.couchX - target) > 2.0f * sc2 && RandInt(6) == 0) {
                SpawnParticle(s.x - s.facing * 8.0f * sc2, s.y + 2.0f, -s.facing * 40.0f, -20.0f,
                              2.4f, 0.4f, 220, 210, 190, 0);
                s.squashVel = -0.5f;
            }
            if (std::fabs(g_app.couchX - target) < 3.0f * sc2 && s.blocks == 0) {
                s.blocks = 1;
                g_app.couchGoalX = 0.0f;
                if (!AnyoneSpeaking()) SayNote(s, 17);
                s.actTimer = std::min(s.actTimer, 1.4f);
            }
            break;
        }

        case Act::PLUG_IN: {
            float socketX = (s.x < (float)g_app.width * 0.5f)
                                ? 16.0f * sc : (float)g_app.width - 16.0f * sc;
            float dxs = socketX - s.x;
            if (s.blocks == 0) {
                s.facing = (dxs >= 0.0f) ? 1 : -1;
                s.vx = ClampF(dxs * 1.3f, -320.0f, 320.0f);
                if (std::fabs(dxs) < 30.0f * sc) {
                    s.blocks = 1;
                    s.carry = 9;
                    s.actTimer = std::min(s.actTimer, 5.0f);
                    if (!AnyoneSpeaking()) SayNote(s, 13);
                }
            } else {
                s.vx = 0.0f;
                s.facing = (socketX >= s.x) ? 1 : -1;
                if (RandInt(7) == 0) {
                    SpawnParticle(socketX, s.y - 30.0f * sc, RandRange(-50, 50), RandRange(-90, -20),
                                  RandRange(2.0f, 4.0f), 0.35f, 255, 235, 120, 5);
                }
                if (s.blocks == 1 && s.propT > 2.2f) {
                    s.blocks = 2;
                    g_app.hotHold = 0.0f;               // fresh electrons: the PC chills out
                    g_app.mood = ClampF(g_app.mood + 0.15f, 0.0f, 1.0f);
                    SpawnBurst(socketX, s.y - 30.0f * sc, 255, 240, 140, 12, 2);
                    for (auto& o : g_app.stickmen) {
                        if (o.id == s.id || !o.grounded || o.act != Act::IDLE) continue;
                        o.act = Act::CHEER;
                        o.actTimer = RandRange(1.4f, 2.6f);
                    }
                }
            }
            break;
        }

        // ==================================== v4: weather and team sports =====
        case Act::UMBRELLA: {
            if (std::fabs(g_app.mouseVel.x) > 40.0f) s.facing = (g_app.mouseVel.x > 0.0f) ? 1 : -1;
            s.carry = 10;
            if (g_app.raining && RandInt(2) == 0) {
                for (int i = 0; i < 2; ++i) {
                    float rx = s.x + RandRange(-70, 70) * sc;
                    SpawnParticle(rx, s.y - 118.0f * sc, RandRange(-12, 12), RandRange(300, 460),
                                  RandRange(1.4f, 2.2f), 0.55f, 140, 195, 250, 9);
                }
            }
            if (g_app.snowing && RandInt(5) == 0) {
                SpawnParticle(s.x + RandRange(-90, 90) * sc, s.y - 130.0f * sc, RandRange(-24, 24),
                              RandRange(26, 60), 4.0f, 1.7f, 240, 248, 255, 0);
            }
            if (!g_app.raining && !g_app.snowing && s.actTimer > 1.5f) s.actTimer = 1.5f;
            break;
        }

        case Act::PASS_BALL: {
            Ball* ball = nullptr;
            float best = 1e9f;
            for (auto& b : g_app.balls) {
                float d = Len(V2(b.x - s.x, b.y - (s.y - 40.0f * sc)));
                if (d < best) { best = d; ball = &b; }
            }
            if (!ball || g_app.balls.empty()) { s.actTimer = 0.05f; break; }
            Stickman* pal = FindStickmanById(s.partnerId);
            Vec2 tgt = pal ? V2(pal->x, pal->y - 70.0f * sc) : V2(g_app.cursor.x, g_app.cursor.y - 40.0f * sc);
            s.facing = (ball->x >= s.x) ? 1 : -1;
            s.vx = ClampF((ball->x - s.x) * 1.1f, -200.0f, 200.0f);
            float dh = Len(V2(ball->x - s.pose.handR.x, ball->y - s.pose.handR.y));
            if (dh < 46.0f * sc && ball->kickCool <= 0.0f) {
                ball->kickCool = 1.1f;
                ball->kickerId = s.id;
                float dx = tgt.x - ball->x;
                ball->vx = ClampF(dx * 1.5f, -430.0f, 430.0f);
                ball->vy = -RandRange(250.0f, 380.0f);
                ball->spin = ball->vx / ball->r;
                s.squashVel = -1.3f;
                SpawnBurst(ball->x, ball->y, 255, 200, 90, 3, 0);
                if (pal) {
                    if (pal->propIndex < 0 && pal->act != Act::GRABBED) {
                        pal->act = Act::PASS_BALL;
                        pal->actTimer = std::max(pal->actTimer, 6.0f);
                        pal->partnerId = s.id;
                        pal->facing = (s.x >= pal->x) ? 1 : -1;
                    }
                }
                if (RandInt(7) == 0 && !AnyoneSpeaking()) SayNote(s, 19);
            }
            break;
        }
        default: break;
    }

    // snowman building (uses the BUILD act with a pillow-slot prop)
    if (s.act == Act::BUILD && w && w->kind == PR_PILLOW && w->data == 1) {
        if (s.propT > (float)s.blocks * 1.8f + 1.2f && s.blocks < 3) {
            ++s.blocks;
            s.squashVel = 1.1f;
            SpawnBurst(w->x, w->y - 12.0f - (float)s.blocks * 14.0f * sc, 250, 252, 255, 4, 0);
        }
        if (s.blocks >= 3 && w->data == 1) { w->data = 2; w->life = 20.0f; w->maxLife = 20.0f; }
        s.facing = -1;
        s.x = LerpF(s.x, w->x + 26.0f * sc, ClampF(dt * 5.0f, 0.0f, 1.0f));
    }
}

// One director tick per second: keeps the squad busy with shared scenes and
// makes sure they never set up shop in the middle of the user's work area.
static void directorsTick(float dt) {
    (void)dt;
    g_app.directorT -= dt;
    if (g_app.directorT > 0.0f) return;
    g_app.directorT = g_app.calmMode ? 5.5f : 3.6f;

    if (g_app.afk || g_app.couchOn) return;

    // music on (and no video) -> someone jams, others dance on the taskbar
    if (g_app.audioEnv > 0.12f && g_app.fgKind != 1) {
        for (auto& s : g_app.stickmen) {
            if (s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
            if (s.act == Act::DANCE || s.act == Act::MUSIC) continue;
            if (s.act == Act::IDLE || s.act == Act::WALK) {
                if (s.type == CharType::GREEN) { BeginTravel(s, Act::MUSIC, s.x + s.facing * 20.0f); }
                else if (RandInt(2) == 0) { BeginTravel(s, Act::DANCE, RouteAround(s.x, s.x + RandRange(-60, 60))); }
            }
        }
    }

    // user is reading/memeing: react from the edges, do not walk over the text
    if (g_app.feedScore > 0.9f) {
        for (auto& s : g_app.stickmen) {
            if (s.propIndex >= 0 || !s.grounded || s.act == Act::GRABBED) continue;
            if (InKeepOut(s.x)) continue;
            if (RandInt(3) != 0) continue;
            int r = RandInt(100);
            s.act = (r < 45) ? Act::LAUGH : (r < 70) ? Act::FACEPALM : Act::CURSOR_PLAY;
            s.actTimer = RandRange(2.4f, 5.0f);
            s.animTime = 0.0f;
        }
    }

    // typing in an editor: somebody cheers for you / ghost-types next to the caret
    if ((g_app.fgKind == 4 || g_app.fgKind == 2) && g_app.typingRate > 0.25f) {
        for (auto& s : g_app.stickmen) {
            if (s.workCool > 0.0f || s.propIndex >= 0) continue;
            if (s.act == Act::MINI_LAPTOP || s.act == Act::AIR_TYPE) continue;
            if (!s.grounded) continue;
            if (std::fabs(s.x - g_app.cursor.x) > 460.0f) continue;
            s.workCool = RandRange(25.0f, 60.0f);
            s.act = (RandInt(2) == 0) ? Act::AIR_TYPE : Act::CHEER;
            s.actTimer = RandRange(6.0f, 12.0f);
            break;
        }
    }

    // overheating PC: Yellow grabs the extinguisher
    if (g_app.heatGlow > 0.45f) {
        bool already = false;
        for (auto& s : g_app.stickmen) if (s.act == Act::EXTINGUISH) already = true;
        if (!already) {
            for (auto& s : g_app.stickmen) {
                if (!s.grounded || s.propIndex >= 0 || s.act == Act::GRABBED) continue;
                s.carry = 4;                 // he grabs the extinguisher and walks up to the hot spot
                BeginTravel(s, Act::EXTINGUISH,
                            RouteAround(s.x, (float)g_app.width * 0.5f + RandRange(-260.0f, 260.0f)));
                s.travelFloor = true;
                break;
            }
        }
    }

    // late night: fewer stunts, more sleeping
    if (g_app.nightMode && RandInt(3) == 0) {
        for (auto& s : g_app.stickmen) {
            if (!s.grounded || s.propIndex >= 0 || s.act == Act::GRABBED) continue;
            if (s.act == Act::NAP || s.act == Act::CAMPFIRE) continue;
            if (RandInt(4) != 0) continue;
            float x = s.x + s.facing * RandRange(-60.0f, 60.0f);
            int idx = AddProp(PR_PILLOW, x, (float)FloorYAt(x), RandRange(20.0f, 34.0f), s.id, -1, 0);
            SeatAt(s, Act::NAP, idx, 0.0f, s.facing);
            break;
        }
    }

    // shared scenes, only when the squad is idle enough to host one
    int freeMen = 0;
    for (auto& s : g_app.stickmen) {
        if (s.grounded && s.propIndex < 0 && s.act != Act::GRABBED && s.act != Act::SPARRING) ++freeMen;
    }
    if (freeMen >= 2 && RandInt(100) < (g_app.calmMode ? 26 : 46)) {
        for (auto& s : g_app.stickmen) {
            if (s.grounded && s.propIndex < 0 && s.act != Act::GRABBED && s.act != Act::SPARRING) {
                TryGroupScene(s);
                break;
            }
        }
    }
}

// ============================================================================
// Props: bouncy ball + dropped snacks
// ============================================================================
static void UpdateBalls(float dt) {
    for (size_t i = 0; i < g_app.balls.size();) {
        Ball& b = g_app.balls[i];
        b.life -= dt;
        if (b.kickCool > 0.0f) b.kickCool -= dt;
        if (b.life <= 0.0f) {
            g_app.balls[i] = g_app.balls.back();
            g_app.balls.pop_back();
            continue;
        }
        b.vy += 1480.0f * dt;
        b.vx *= std::pow(0.55f, dt);
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.rot += b.spin * dt;
        b.spin *= std::pow(0.6f, dt);
        float floorLine = (float)FloorYAt(b.x) - b.r;
        if (b.y >= floorLine) {
            b.y = floorLine;
            if (b.vy > 55.0f) {
                SpawnBurst(b.x, b.y + b.r * 0.8f, 200, 205, 215, 3, 0);
            }
            b.vy = -b.vy * 0.62f;
            b.vx *= 0.90f;
            if (std::fabs(b.vy) < 45.0f) b.vy = 0.0f;
            b.spin = b.vx / b.r;
        }
        if (b.x < b.r + 3.0f) { b.x = b.r + 3.0f; b.vx = std::fabs(b.vx) * 0.72f; }
        if (b.x > (float)g_app.width - b.r - 3.0f) {
            b.x = (float)g_app.width - b.r - 3.0f; b.vx = -std::fabs(b.vx) * 0.72f;
        }
        ++i;
    }
}

static void UpdateSnacks(float dt) {
    for (size_t i = 0; i < g_app.snacks.size();) {
        Snack& sn = g_app.snacks[i];
        sn.life -= dt;
        if (sn.life <= 0.0f || sn.bites <= 0.0f) {
            if (sn.bites <= 0.0f) SpawnBurst(sn.x, sn.y - 4.0f, 255, 214, 120, 6, 7);
            g_app.snacks[i] = g_app.snacks.back();
            g_app.snacks.pop_back();
            continue;
        }
        if (!sn.onFloor) {
            sn.vy += 1350.0f * dt;
            sn.x += sn.vx * dt;
            sn.y += sn.vy * dt;
            float fl = (float)FloorYAt(sn.x) - 4.0f;
            if (sn.y >= fl) { sn.y = fl; sn.onFloor = true; sn.vx = 0.0f; sn.vy = 0.0f; }
        }
        ++i;
    }
}

// ============================================================================
// Main simulation tick
// ============================================================================
static void UpdateSimulation(float dt) {
    g_app.globalTime += dt;

    POINT cur = {};
    GetCursorPos(&cur);
    Vec2 cursorLocal = V2((float)(cur.x - g_app.originX), (float)(cur.y - g_app.originY));
    g_app.cursor = cursorLocal;
    Vec2 rawVel = V2((float)(cur.x - g_app.prevMouse.x) / std::max(dt, 0.004f),
                     (float)(cur.y - g_app.prevMouse.y) / std::max(dt, 0.004f));
    g_app.mouseVel = LerpV(g_app.mouseVel, rawVel, 0.45f);
    g_app.prevMouse = cur;

    // "the user is waving the cursor around" = the sign of the horizontal speed flips
    float sx = (g_app.mouseVel.x > 60.0f) ? 1.0f : (g_app.mouseVel.x < -60.0f ? -1.0f : 0.0f);
    if (sx != 0.0f && g_app.swingSign != 0.0f && sx != g_app.swingSign) {
        g_app.swingCount += 1.0f;
        g_app.cursorPlay = ClampF(g_app.cursorPlay + 0.5f, 0.0f, 1.5f);
    }
    if (sx != 0.0f) g_app.swingSign = sx;
    g_app.swingCount = ApproachF(g_app.swingCount, 0.0f, 1.4f, dt);
    directorsTick(dt);

    UpdateSensors(dt);
    if (g_app.helloTimer > 0.0f) g_app.helloTimer -= dt;
    UpdateBalls(dt);
    UpdateSnacks(dt);
    UpdateProps(dt);

    const float gravity = 1280.0f;

    for (auto& s : g_app.stickmen) {
        s.animTime += dt;
        s.propT += dt;
        s.fishT += dt;
        if (s.socialCooldown > 0.0f) s.socialCooldown -= dt;
        if (s.sparCooldown > 0.0f) s.sparCooldown -= dt;
        if (s.prankCool > 0.0f) s.prankCool -= dt;
        if (s.workCool > 0.0f) s.workCool -= dt;
        if (s.inVideo) s.inVideoT += dt;
        if (s.act == Act::HANG_CURSOR) {
            if (g_app.ghostMode || Len(g_app.mouseVel) > 2600.0f) {
                s.act = Act::AIRBORNE;
                s.grounded = false;
                s.vy = -120.0f;
            }
        }
        if (s.laugh > 0.0f) s.laugh -= dt;
        s.keyAnim = ApproachF(s.keyAnim, 0.0f, 10.0f, dt);
        if (s.hasSword) {
            s.swordTimer -= dt;
            if (s.swordTimer <= 0.0f) s.hasSword = false;
        }
        // squash & stretch spring
        s.squashVel += (-s.squash * 230.0f - s.squashVel * 14.0f) * dt;
        s.squash = ClampF(s.squash + s.squashVel * dt, -0.34f, 0.40f);
        if (std::fabs(s.squash) < 0.002f) { s.squash = 0.0f; s.squashVel *= 0.5f; }

        if (s.doodleFade > 0.0f) {
            s.doodleFade -= dt;
            if (s.doodleFade <= 0.0f) s.doodle.clear();
        }

        // -------- grabbed by the user --------
        if (s.act == Act::GRABBED) {
            if (g_app.grabbedId != s.id) {
                s.act = Act::AIRBORNE;
            } else {
                s.x = cursorLocal.x;
                s.y = cursorLocal.y;
                s.grabAngVel += (-g_app.mouseVel.x * 0.0045f - 13.0f * std::sin(s.grabAngle)) * dt;
                s.grabAngVel *= std::pow(0.12f, dt);
                s.grabAngle = ClampF(s.grabAngle + s.grabAngVel * dt, -1.25f, 1.25f);
                s.grounded = false;
                s.standingHwnd = nullptr;
                s.facing = (g_app.mouseVel.x < -40.0f) ? -1 : (g_app.mouseVel.x > 40.0f ? 1 : s.facing);
            }
        }
        // -------- climbing a window edge --------
        else if (s.act == Act::WALL_CLIMB) {
            const DesktopPlatform* wp = FindPlatformByHwnd(s.climbingHwnd);
            if (!wp || s.actTimer <= 0.0f) {
                s.act = Act::AIRBORNE;
                s.vy = -330.0f;
                s.vx = (float)(-s.climbSide * 130.0f);
                s.climbingHwnd = nullptr;
            } else {
                s.actTimer -= dt;
                float edgeX = (s.climbSide < 0) ? (float)wp->rect.left : (float)wp->rect.right;
                s.x = edgeX + s.climbSide * 6.0f * g_app.scale;
                s.y -= 128.0f * g_app.scale * dt;
                s.facing = (s.climbSide < 0) ? 1 : -1;
                if (s.y <= (float)wp->rect.top + 3.0f) {
                    s.y = (float)wp->rect.top;
                    s.x = edgeX - s.climbSide * 26.0f * g_app.scale;
                    s.vx = (float)(-s.climbSide * 90.0f);
                    s.vy = -150.0f;
                    s.act = Act::AIRBORNE;
                    s.climbingHwnd = nullptr;
                    SpawnBurst(s.x, s.y, 220, 225, 235, 5, 0);
                }
            }
        }
        // -------- grounded / airborne body physics --------
        else {
            float prevY = s.y;

            // ride the window you are standing on
            if (s.grounded && s.standingHwnd) {
                const DesktopPlatform* wp = FindPlatformByHwnd(s.standingHwnd);
                if (!wp) {
                    s.grounded = false;
                    s.standingHwnd = nullptr;
                } else {
                    s.x += wp->dx;
                    s.y = (float)wp->rect.top;
                    if (std::fabs(wp->dx) > 34.0f || std::fabs(wp->dy) > 34.0f) {
                        s.grounded = false;
                        s.standingHwnd = nullptr;
                        s.vx = ClampF(wp->dx * 8.0f, -520.0f, 520.0f);
                        s.vy = -360.0f;
                        s.act = Act::AIRBORNE;
                        s.doingFlip = true;
                        s.squashVel = -2.5f;
                    } else if (s.x < wp->rect.left - 8.0f || s.x > wp->rect.right + 8.0f) {
                        bool edgy = (s.act == Act::SIT_EDGE || s.act == Act::PEEK_WINDOW ||
                                     s.act == Act::FISH || s.act == Act::ARMWRESTLE ||
                                     s.act == Act::SPY_CHAT);
                        if (edgy && std::fabs(wp->dx) > 4.0f) {
                            // the edge slid away with him -> squeezed flat against it
                            s.act = Act::SQUISH;
                            s.actTimer = 2.0f;
                            s.animTime = 0.0f;
                            s.squash = 0.36f;
                            s.squashVel = 1.8f;
                            s.facing = (s.x < wp->rect.left) ? -1 : 1;
                            SpawnBurst(s.x, s.y - 16.0f * g_app.scale, 225, 230, 240, 5, 0);
                        } else {
                            s.grounded = false;
                            s.standingHwnd = nullptr;
                            s.act = Act::AIRBORNE;
                        }
                    }
                }
            }

            // ---------- anchored "job" acts lock the body to the prop ----------
            bool locked = false;
            if (s.grounded) {
                if (s.act == Act::WATCH_TV || s.act == Act::EAT_POPCORN) {
                    locked = true;
                    int seat = (s.seatIndex >= 0 && s.seatIndex < CouchSeatCount() &&
                                g_app.couchSeatsTaken[s.seatIndex] == s.id)
                                   ? s.seatIndex : ClaimCouchSeat(s.id);
                    if (seat < 0) { s.act = Act::IDLE; s.actTimer = 1.0f; locked = false; }
                    else {
                        s.seatIndex = seat;
                        RecomputeCouchAnchor();
                        float sx = CouchSeatX(seat);
                        s.x = LerpF(s.x, sx, ClampF(dt * 8.0f, 0.0f, 1.0f));
                        s.y = (float)FloorYAt(s.x);
                        s.grounded = true;
                        s.standingHwnd = nullptr;
                        s.vx = 0.0f; s.vy = 0.0f;
                        float lookX = g_app.mediaWinValid
                            ? (float)(g_app.mediaWinRect.left + g_app.mediaWinRect.right) * 0.5f
                            : g_app.couchX;
                        s.facing = (lookX >= s.x) ? 1 : -1;
                        // popcorn munching every now and then
                        if (s.act == Act::WATCH_TV && RandInt(240) == 0) { s.act = Act::EAT_POPCORN; s.propT = 0.0f; }
                        else if (s.act == Act::EAT_POPCORN && s.propT > RandRange(2.2f, 4.4f)) { s.act = Act::WATCH_TV; }
                        if (s.actTimer > 0.0f) s.actTimer -= dt;
                        // react to a loud beat in the video
                        if (g_app.audioEnv > 0.45f && RandInt(40) == 0) {
                            s.squashVel = -1.8f;
                            SpawnBurst(s.x, s.y - 40.0f * g_app.scale, 255, 240, 160, 3, 2);
                        }
                        // a boring silent video makes them yawn
                        if (g_app.audioEnv < 0.02f && RandInt(700) == 0) {
                            s.act = Act::YAWN;
                            s.actTimer = RandRange(3.0f, 5.0f);
                            s.propT = 0.0f;
                            s.squashVel = 0.6f;
                        }
                        // they hit the player buttons themselves (drawn, never a real click)
                        if (g_app.mediaWinValid && RandInt(1400) == 0) {
                            AddProp(PR_FEED, ClampF(g_app.couchX + RandRange(-140, 140), 60.0f, (float)g_app.width - 60.0f),
                                    (float)g_app.mediaWinRect.bottom - 34.0f * g_app.scale,
                                    3.2f, s.id, -1, RandInt(3));
                            s.act = Act::POINT_AT;
                            s.actTimer = 2.4f;
                            s.animTime = 0.0f;
                        }
                        // and once in a while one of them climbs INSIDE the video
                        if (g_app.mediaWinValid && !s.inVideo && RandInt(2200) == 0) {
                            s.inVideo = true;
                            s.inVideoT = 0.0f;
                            s.act = Act::IN_VIDEO;
                            s.actTimer = RandRange(6.0f, 10.0f);
                            s.animTime = 0.0f;
                            s.seatIndex = -1;
                            ReleaseCouchSeat(s.id);
                            s.y = (float)g_app.mediaWinRect.bottom - 6.0f * g_app.scale;
                            s.x = RandRange((float)g_app.mediaWinRect.left + 70.0f,
                                           (float)g_app.mediaWinRect.right - 70.0f);
                            s.grounded = true;
                            s.standingHwnd = nullptr;
                            s.vx = 0.0f; s.vy = 0.0f;
                            SpawnBurst(s.x, s.y - 30.0f, 180, 220, 255, 8, 2);
                        }
                        if (s.actTimer <= 0.0f || !g_app.couchOn) {
                            if (g_app.couchOn) s.actTimer = RandRange(14.0f, 30.0f);
                            else { ReleaseCouchSeat(s.id); s.seatIndex = -1; s.act = Act::IDLE; s.actTimer = 0.6f; }
                        }
                    }
                } else if (s.act == Act::MINI_LAPTOP) {
                    locked = true;
                    g_app.deskOwnerId = s.id;
                    s.x = LerpF(s.x, g_app.deskX, ClampF(dt * 8.0f, 0.0f, 1.0f));
                    s.y = g_app.deskY;
                    s.vx = 0.0f; s.vy = 0.0f;
                    s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
                    if (g_app.typingPulse > 0.35f) {
                        s.keyAnim = std::max(s.keyAnim, g_app.typingPulse);
                        if (RandInt(3) == 0) {
                            Vec2 h = s.pose.handR;
                            SpawnParticle(h.x + RandRange(-4, 4), h.y - 6.0f, RandRange(-18, 18),
                                          RandRange(-90, -45), RandRange(2.4f, 4.0f), RandRange(0.5f, 0.9f),
                                          120, 245, 255, 5);
                        }
                    }
                    if (s.actTimer > 0.0f) s.actTimer -= dt;
                    if (!g_app.deskOn) {
                        g_app.deskOwnerId = -1;
                        s.act = Act::IDLE;
                        s.actTimer = RandRange(0.4f, 1.0f);
                    } else if (s.actTimer <= 0.0f && g_app.typingRate < 0.25f) {
                        g_app.deskOwnerId = -1;
                        s.act = Act::IDLE;
                        s.actTimer = RandRange(0.6f, 1.4f);
                    } else if (s.actTimer <= 0.0f) {
                        s.actTimer = RandRange(18.0f, 40.0f);
                    }
                } else if (s.act == Act::EAT_SNACK) {
                    Snack* sn = nullptr;
                    for (auto& cand : g_app.snacks) {
                        if (cand.ownerId == s.id || (cand.bites > 0.0f && std::fabs(cand.x - s.x) < 40.0f)) {
                            sn = &cand; break;
                        }
                    }
                    if (!sn) { s.act = Act::IDLE; s.actTimer = 1.0f; }
                    else {
                        locked = true;
                        sn->ownerId = s.id;
                        s.x = LerpF(s.x, sn->x - s.facing * 14.0f * g_app.scale, ClampF(dt * 6.0f, 0.0f, 1.0f));
                        s.facing = (sn->x >= s.x) ? 1 : -1;
                        s.vx = 0.0f; s.vy = 0.0f;
                        if (std::fabs(sn->x - s.x) < 34.0f * g_app.scale) {
                            s.act = Act::EAT_SNACK;
                            sn->bites -= dt * 0.85f;
                            if (RandInt(24) == 0) {
                                SpawnParticle(s.pose.handR.x, s.pose.handR.y, RandRange(-30, 30),
                                              RandRange(-40, 10), RandRange(1.6f, 2.8f), 0.5f,
                                              255, 214, 120, 7);
                            }
                            if (sn->bites <= 0.0f) {
                                s.act = Act::CHEER;
                                s.actTimer = 1.6f;
                                s.animTime = 0.0f;
                                s.squashVel = -2.0f;
                                s.socialCooldown = 3.0f;
                            }
                        } else {
                            float dx = sn->x - s.x;
                            s.facing = (dx >= 0.0f) ? 1 : -1;
                            s.vx = s.facing * 150.0f;
                            // safety net: give up if the snack is unreachable
                            if (s.actTimer > 0.0f) s.actTimer -= dt;
                            else { s.act = Act::IDLE; s.actTimer = RandRange(0.6f, 1.4f); }
                        }
                    }
                } else if (s.act == Act::PLAY_BALL) {
                    Ball* nearBall = nullptr;
                    float bd = 1e9f;
                    for (auto& b : g_app.balls) {
                        float d = std::fabs(b.x - s.x);
                        if (d < bd && b.kickCool <= 0.0f) { bd = d; nearBall = &b; }
                    }
                    if (!nearBall) {
                        s.act = Act::IDLE;
                        s.actTimer = RandRange(0.8f, 1.6f);
                    } else {
                        locked = true;
                        float dx = nearBall->x - s.x;
                        s.facing = (dx >= 0.0f) ? 1 : -1;
                        if (std::fabs(dx) > 26.0f * g_app.scale) {
                            s.vx = s.facing * (std::fabs(dx) > 130.0f ? 235.0f : 140.0f);
                            s.act = Act::PLAY_BALL;
                            if (s.actTimer <= 0.0f) s.actTimer = 4.0f;
                        } else {
                            s.vx *= std::pow(0.001f, dt);
                            nearBall->vx += s.facing * 430.0f;
                            nearBall->vy -= 330.0f;
                            nearBall->spin = nearBall->vx / nearBall->r;
                            nearBall->kickCool = 0.55f;
                            s.squashVel = -1.8f;
                            s.actTimer = RandRange(3.0f, 7.0f);
                            SpawnBurst(nearBall->x, nearBall->y, 255, 235, 120, 4, 2);
                        }
                        if (s.actTimer <= 0.0f) {
                            s.act = Act::IDLE;
                            s.actTimer = 1.0f;
                        }
                    }
                } else if (ActIsScene(s.act) || s.act == Act::SKETCH || s.act == Act::BUILD ||
                           s.act == Act::READ || s.act == Act::MUSIC || s.act == Act::FISH ||
                           s.act == Act::CAMPFIRE || s.act == Act::NAP) {
                    locked = true;
                    s.vx *= std::pow(0.0005f, dt);
                    s.x += s.vx * dt;
                    if (s.actTimer > 0.0f) s.actTimer -= dt;
                    else {
                        Act done = s.act;
                        // leaving a v4 scene: put everything back the way it was
                        if (done == Act::HIDE_ICON || done == Act::TAB_OOPS) {
                            WorldProp* bp = PropAt(s.propIndex);
                            if (bp) { bp->data = 0; bp->x = bp->w; bp->y = bp->h; bp->vx = bp->vy = 0.0f; }
                        }
                        ReleaseProp(s);
                        s.carry = 0;
                        s.hidden = false;
                        s.rides = 0;
                        if (done == Act::TEA_RUN) g_app.workStreak = 0.0f;
                        if (done == Act::IN_VIDEO) s.inVideo = false;
                        // hanging from the cursor or inside the video: let go and fall down
                        if (done == Act::IN_VIDEO || done == Act::HANG_CURSOR ||
                            (done == Act::PUSH_WINDOW && !s.grounded)) {
                            s.grounded = false;
                            s.standingHwnd = nullptr;
                            s.vy = 60.0f;
                            s.squashVel = -1.5f;
                            SpawnBurst(s.x, s.y - 18.0f * g_app.scale, 190, 225, 255, 5, 2);
                        }
                        s.act = Act::IDLE;
                        s.actTimer = RandRange(0.8f, 2.0f);
                    }
                    SceneFX(s, dt);
                    if (s.act == Act::MUSIC && RandInt(22) == 0) {
                        SpawnParticle(s.x + RandRange(-12, 12) * g_app.scale,
                                      s.y - 52.0f * g_app.scale, RandRange(-26, 26), -52.0f,
                                      6.5f, 0.95f, 90, 240, 120, 3);
                    }
                    if (s.act == Act::NAP && RandInt(46) == 0) {
                        SpawnParticle(s.pose.head.x + 6.0f, s.pose.head.y - 4.0f, 14.0f, -26.0f,
                                      5.0f, 1.5f, 200, 215, 255, 6);
                    }
                    if (s.act == Act::FISH) {
                        float jerk = ClampF(-std::cos(s.fishT * 1.6f) - 0.86f, 0.0f, 1.0f);
                        if (jerk > 0.0f && RandInt(6) == 0) {
                            SpawnBurst(s.x + s.facing * 26.0f * g_app.scale, s.y + 12.0f * g_app.scale,
                                       140, 210, 255, 4, 8);
                        }
                    }
                    if (s.act == Act::BUILD) {
                        // a block gets placed every couple of seconds
                        if (s.propT > (float)s.blocks * 2.1f + 1.4f && s.blocks < 6) {
                            ++s.blocks;
                            s.squashVel = 1.2f;
                            SpawnBurst(s.pose.handR.x, s.pose.handR.y + 6.0f, 230, 235, 245, 3, 0);
                        }
                    }
                    if (s.act == Act::SKETCH) {
                        // he draws a little pet on the desktop: cat, pig, flower...
                        float ax = s.x + s.facing * 24.0f * g_app.scale;
                        float ay = s.y - 22.0f * g_app.scale;
                        float rr = 13.0f * g_app.scale;
                        if (s.doodle.empty()) {
                            s.doodleShape = 1 + RandInt(3);
                            s.doodle.push_back(DoodlePoint(s.doodleShape, 0.0f, ax, ay, rr,
                                                           (float)s.facing));
                        }
                        float uMax = (s.doodleShape == 1) ? 6.6f : (s.doodleShape == 2) ? 5.8f : 5.6f;
                        if ((float)s.doodle.size() < uMax * 9.0f && RandInt(2) == 0) {
                            float u = (float)s.doodle.size() / 9.0f;
                            s.doodle.push_back(DoodlePoint(s.doodleShape, u, ax, ay, rr,
                                                           (float)s.facing));
                            if (RandInt(14) == 0) {
                                SpawnParticle(s.pose.handR.x, s.pose.handR.y + 4.0f, RandRange(-14, 14),
                                              RandRange(-30, -6), 1.8f, 0.4f, 230, 232, 236, 0);
                            }
                        }
                        // the drawing is done -> friends come over to look at it
                        if ((float)s.doodle.size() >= uMax * 9.0f && s.blocks == 0) {
                            s.blocks = 1;
                            SpawnBurst(ax, ay, s.r, s.g, s.b, 8, 2);
                            for (auto& o : g_app.stickmen) {
                                if (o.id == s.id || !o.grounded) continue;
                                if (std::fabs(o.x - s.x) > 300.0f) continue;
                                if (o.act == Act::GRABBED || o.act == Act::SPARRING) continue;
                                o.act = (RandInt(2) == 0) ? Act::CHEER : Act::POINT_AT;
                                o.actTimer = RandRange(1.8f, 3.2f);
                                o.animTime = 0.0f;
                                o.facing = (s.x >= o.x) ? 1 : -1;
                            }
                        }
                        if (s.doodleFade <= 0.0f) s.doodleFade = 11.0f;
                    }
                } else if (s.act == Act::TRAVEL) {
                    float dx = s.travelTargetX - s.x;
                    if (std::fabs(dx) < 9.0f) {
                        s.x = s.travelTargetX;
                        if (s.travelFloor) s.y = (float)FloorYAt(s.x);
                        if (s.travelWin) {
                            const DesktopPlatform* wp = FindPlatformByHwnd(s.travelWin);
                            if (wp) {
                                s.y = (float)wp->rect.top;
                                s.standingHwnd = wp->hwnd;
                                s.grounded = true;
                            }
                            s.travelWin = nullptr;
                        }
                        s.vx = 0.0f;
                        bool hold = false;
                        s.act = s.nextAct;
                        s.animTime = 0.0f;
                        s.phase = 0.0f;
                        s.propT = 0.0f;
                        s.blocks = 0;
                        s.actTimer = ActDuration(s.nextAct, hold);
                        if (s.act == Act::MINI_LAPTOP) g_app.deskOwnerId = s.id;
                        if (s.act == Act::WATCH_TV) g_app.couchHold = 2.0f;
                        s.doodleFade = (s.act == Act::SKETCH) ? 9.0f : s.doodleFade;
                    } else {
                        s.facing = (dx >= 0.0f) ? 1 : -1;
                        s.vx = s.facing * (std::fabs(dx) > 190.0f ? 250.0f : 130.0f);
                        if (s.actTimer > 0.0f) s.actTimer -= dt;
                        else { s.act = Act::IDLE; s.actTimer = 1.0f; }
                    }
                } else if (s.act == Act::HIGH_FIVE) {
                    Stickman* o = FindStickmanById(s.partnerId);
                    float dx = o ? (o->x - s.x) : 0.0f;
                    if (!o) { s.act = Act::IDLE; s.actTimer = 1.0f; }
                    else {
                        s.facing = (dx >= 0.0f) ? 1 : -1;
                        if (std::fabs(dx) > 44.0f) s.vx = s.facing * 90.0f;
                        else {
                            s.vx *= std::pow(0.001f, dt);
                            if (s.laugh <= 0.0f && std::fabs(dx) < 60.0f) {
                                s.laugh = 1.0f;
                                float mx = (s.x + o->x) * 0.5f;
                                float my = (s.y + o->y) * 0.5f - 58.0f * g_app.scale;
                                SpawnBurst(mx, my, 255, 240, 150, 8, 2);
                                SpawnParticle(mx, my, 0, 0, 9.0f, 0.26f, 255, 255, 255, 1);
                                s.squashVel = -1.4f;
                            }
                        }
                        if (s.actTimer > 0.0f) s.actTimer -= dt;
                        else { s.act = Act::IDLE; s.actTimer = RandRange(0.8f, 1.8f); }
                    }
                } else if (s.act == Act::SPARRING) {
                    Stickman* partner = FindStickmanById(s.partnerId);
                    if (!partner || partner->act != Act::SPARRING || s.actTimer <= 0.0f) {
                        // ...then it ends the cartoon way: a handshake, and they walk apart
                        if (partner && partner->act == Act::SPARRING) {
                            EndSparWithHandshake(s);
                        } else {
                            s.act = Act::IDLE;
                            s.actTimer = RandRange(1.0f, 2.0f);
                            s.partnerId = -1;
                            s.hasSword = false;
                            s.socialCooldown = RandRange(10.0f, 20.0f);
                            s.vx = (RandInt(2) == 0) ? -110.0f : 110.0f;
                        }
                    } else {
                        float dx = partner->x - s.x;
                        s.facing = (dx >= 0.0f) ? 1 : -1;
                        if (std::fabs(dx) > 44.0f) s.vx = s.facing * 200.0f;
                        else if (std::fabs(dx) < 26.0f) s.vx = -s.facing * 110.0f;
                        else s.vx *= std::pow(0.002f, dt);
                        if (std::fabs(dx) < 60.0f && RandInt(20) == 0) {
                            float mx = (s.x + partner->x) * 0.5f;
                            float my = (s.y + partner->y) * 0.5f - 40.0f * g_app.scale;
                            SpawnBurst(mx, my, 255, 235, 110, 4, 2);
                        }
                        s.actTimer -= dt;
                    }
                } else {
                    // plain ground behaviour: walk / run / idle
                    if (s.act == Act::WALK) s.vx = s.facing * 118.0f;
                    // never stroll through the middle of what the user is reading
                    if (s.grounded && !s.standingHwnd &&
                        InKeepOut(s.x + s.facing * 30.0f) &&
                        (g_app.fgKind == 2 || g_app.fgKind == 3 || g_app.fgKind == 4) &&
                        (s.act == Act::WALK || s.act == Act::RUN)) {
                        s.facing = -s.facing;
                        s.vx = s.facing * 108.0f;
                        s.squashVel = -0.6f;
                    }
                    else if (s.act == Act::RUN) s.vx = s.facing * 250.0f;
                    else if (s.act == Act::WAVE_HELLO || s.act == Act::CHEER ||
                             s.act == Act::POINT_AT || s.act == Act::FLINCH) {
                        s.vx *= std::pow(0.0008f, dt);
                    } else {
                        s.vx *= std::pow(0.002f, dt);
                    }
                    if (s.actTimer > 0.0f) {
                        s.actTimer -= dt;
                    } else {
                        TrySocial(s);
                        if (s.actTimer <= 0.0f) PickAutoActivity(s);
                    }
                }
            }

            // head/foot cycle: advance the walk phase from the ACTUAL speed so the
            // planted foot never slides across the floor
            if (s.grounded) {
                float sp = std::fabs(s.vx);
                if (s.act == Act::WALK || s.act == Act::RUN || s.act == Act::TRAVEL || s.act == Act::PLAY_BALL) {
                    s.phase += sp * dt * 0.050f;
                    if (sp < 12.0f) s.phase += dt * 1.2f;
                } else {
                    s.phase += dt * 2.0f;
                }
                if (s.act == Act::WALK || s.act == Act::RUN || s.act == Act::TRAVEL) {
                    if (RandInt(90) == 0) {
                        SpawnParticle(s.x - s.facing * 6.0f, s.y - 2.0f, -s.facing * 22.0f, -14.0f,
                                      2.6f, 0.28f, 205, 210, 220, 0);
                    }
                }
            } else {
                s.phase += dt * 3.0f;
            }

            if (!s.grounded && s.act != Act::TRAVEL && !locked) {
                if (s.act == Act::WALK || s.act == Act::RUN || s.act == Act::IDLE ||
                    s.act == Act::WATCH_TV || s.act == Act::EAT_POPCORN || s.act == Act::MINI_LAPTOP) {
                    s.act = Act::AIRBORNE;
                    s.animTime = 0.0f;
                }
            }

            if (!s.grounded) {
                float gScale = (s.type == CharType::TCO_BLACK && s.vy > 80.0f) ? 0.6f : 1.0f;
                s.vy += gravity * gScale * dt;
                if (s.act == Act::TRAVEL) {
                    float dx = s.travelTargetX - s.x;
                    s.vx = ClampF(dx * 1.6f, -320.0f, 320.0f);
                    if (std::fabs(dx) < 10.0f && s.vy > 0.0f) s.vx *= 0.4f;
                }
                if (s.doingFlip) s.flipAngle += s.facing * 12.0f * dt;
            }

            s.x += s.vx * dt;
            s.y += s.vy * dt;

            if (s.x < 22.0f) {
                s.x = 22.0f;
                s.vx = std::fabs(s.vx) * 0.6f;
                s.facing = 1;
                if (s.act == Act::TRAVEL) s.travelTargetX = ClampF(s.travelTargetX, 30.0f, (float)g_app.width - 30.0f);
            } else if (s.x > (float)g_app.width - 22.0f) {
                s.x = (float)g_app.width - 22.0f;
                s.vx = -std::fabs(s.vx) * 0.6f;
                s.facing = -1;
            }
            if (s.y < 26.0f) {
                s.y = 26.0f;
                if (s.vy < 0.0f) s.vy = 40.0f;
            }

            // ---------- landing on window title bars / grabbing a window edge ----------
            if (!s.grounded) {
                for (const auto& p : g_app.platforms) {
                    if (p.rect.top < 55) continue;
                    if (s.vy >= 0.0f &&
                        s.x >= p.rect.left + 8.0f && s.x <= p.rect.right - 8.0f &&
                        prevY <= (float)p.rect.top + 8.0f && s.y >= (float)p.rect.top) {
                        if (!IsOccludedByHigherWindow(s.x, (float)p.rect.top - 3.0f, p.zIndex)) {
                            float impact = s.vy;
                            s.y = (float)p.rect.top;
                            s.vy = 0.0f;
                            s.grounded = true;
                            s.standingHwnd = p.hwnd;
                            s.doingFlip = false;
                            s.squashVel += ClampF(impact / 900.0f, 0.0f, 3.4f);
                            if (s.act != Act::TRAVEL) {
                                if (s.act == Act::AIRBORNE) { s.act = Act::IDLE; s.actTimer = RandRange(1.0f, 2.4f); }
                                if (s.pendingHello) {
                                    s.pendingHello = false;
                                    s.act = Act::WAVE_HELLO;
                                    s.actTimer = RandRange(2.6f, 4.0f);
                                    s.animTime = 0.0f;
                                    s.facing = (cursorLocal.x >= s.x) ? 1 : -1;
                                }
                            }
                            SpawnBurst(s.x, s.y, 220, 225, 235, 5, 0);
                            break;
                        }
                    }
                    if (p.isActive && s.vy > -170.0f && s.act != Act::TRAVEL && !s.travelFloor &&
                        s.y > p.rect.top + 45.0f && s.y < p.rect.bottom - 20.0f) {
                        if (std::fabs(s.x - (float)p.rect.left) < 14.0f * g_app.scale && s.vx > 20.0f) {
                            if (!IsOccludedByHigherWindow((float)p.rect.left - 3.0f, s.y - 30.0f, p.zIndex)) {
                                s.act = Act::WALL_CLIMB;
                                s.climbingHwnd = p.hwnd;
                                s.climbSide = -1;
                                s.actTimer = 4.0f;
                                s.vx = 0.0f; s.vy = 0.0f;
                                break;
                            }
                        } else if (std::fabs(s.x - (float)p.rect.right) < 14.0f * g_app.scale && s.vx < -20.0f) {
                            if (!IsOccludedByHigherWindow((float)p.rect.right + 3.0f, s.y - 30.0f, p.zIndex)) {
                                s.act = Act::WALL_CLIMB;
                                s.climbingHwnd = p.hwnd;
                                s.climbSide = 1;
                                s.actTimer = 4.0f;
                                s.vx = 0.0f; s.vy = 0.0f;
                                break;
                            }
                        }
                    }
                }
            }

            // ---------- taskbar / bottom of the screen ----------
            int floorY = FloorYAt(s.x);
            if (s.y >= (float)floorY) {
                float impact = s.vy;
                s.y = (float)floorY;
                if (impact > 480.0f) {
                    SpawnBurst(s.x, s.y, 220, 225, 235, 8, 0);
                    SpawnParticle(s.x, s.y - 4.0f, 0, 0, 9.0f, 0.24f, s.r, s.g, s.b, 1);
                }
                s.vy = 0.0f;
                if (!s.grounded) {
                    s.grounded = true;
                    s.standingHwnd = nullptr;
                    s.doingFlip = false;
                    s.squashVel += ClampF(impact / 850.0f, 0.0f, 3.8f);
                    if (s.act == Act::AIRBORNE) { s.act = Act::IDLE; s.actTimer = RandRange(0.9f, 2.2f); }
                    if (s.pendingHello) {
                        s.pendingHello = false;
                        s.act = Act::WAVE_HELLO;
                        s.actTimer = RandRange(2.8f, 4.4f);
                        s.animTime = 0.0f;
                        s.facing = (cursorLocal.x >= s.x) ? 1 : -1;
                    }
                }
            } else if (s.grounded && !s.standingHwnd && s.y < (float)floorY - 2.0f && !locked) {
                s.grounded = false;
            }
        }

        // ---------- resolve pose ----------
        float hop = 0.0f;
        Pose target = ComputePose(s, cursorLocal, hop);
        (void)hop;
        if (!s.poseInitialized) {
            s.pose = target;
            s.poseInitialized = true;
        } else {
            float blend = (s.act == Act::GRABBED || s.doingFlip) ? 0.62f
                        : (ActIsSeated(s.act) || s.act == Act::WATCH_TV || s.act == Act::EAT_POPCORN ||
                           s.act == Act::MINI_LAPTOP || s.act == Act::NAP || s.act == Act::SKETCH ||
                           s.act == Act::READ || s.act == Act::CAMPFIRE)
                              ? ClampF(dt * 9.0f, 0.08f, 0.34f)
                              : ClampF(dt * 24.0f, 0.20f, 0.62f);
            s.pose.head   = LerpV(s.pose.head,   target.head,   blend);
            s.pose.neck   = LerpV(s.pose.neck,   target.neck,   blend);
            s.pose.hip    = LerpV(s.pose.hip,    target.hip,    blend);
            s.pose.elbowL = LerpV(s.pose.elbowL, target.elbowL, blend);
            s.pose.handL  = LerpV(s.pose.handL,  target.handL,  blend);
            s.pose.elbowR = LerpV(s.pose.elbowR, target.elbowR, blend);
            s.pose.handR  = LerpV(s.pose.handR,  target.handR,  blend);
            s.pose.kneeL  = LerpV(s.pose.kneeL,  target.kneeL,  blend);
            s.pose.footL  = LerpV(s.pose.footL,  target.footL,  blend);
            s.pose.kneeR  = LerpV(s.pose.kneeR,  target.kneeR,  blend);
            s.pose.footR  = LerpV(s.pose.footR,  target.footR,  blend);
        }
    }

    // -------- particles --------
    for (size_t i = 0; i < g_app.particles.size();) {
        Particle& p = g_app.particles[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            g_app.particles[i] = g_app.particles.back();
            g_app.particles.pop_back();
            continue;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.kind == 0 || p.kind == 7) p.vy += 330.0f * dt;
        else if (p.kind == 8) p.vy += 180.0f * dt;
        else if (p.kind == 2 || p.kind == 4) p.vy -= 90.0f * dt;
        else if (p.kind == 3 || p.kind == 5 || p.kind == 6) { p.vy -= 26.0f * dt; p.vx *= std::pow(0.3f, dt); }
        if (p.kind == 1) p.size += 82.0f * dt;
        ++i;
    }
}

// ============================================================================
// Rendering (premultiplied ARGB straight into the layered-window bitmap)
// ============================================================================
static void FillRoundRect(Graphics& g, const SolidBrush& br, REAL x, REAL y, REAL w, REAL h, REAL r) {
    if (w <= 0.0f || h <= 0.0f) return;
    r = std::min(r, std::min(w, h) * 0.5f);
    GraphicsPath path;
    path.AddArc(x, y, r * 2.0f, r * 2.0f, 180.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y, r * 2.0f, r * 2.0f, 270.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 0.0f, 90.0f);
    path.AddArc(x, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 90.0f, 90.0f);
    path.CloseFigure();
    g.FillPath(&br, &path);
}

static void ExpandRect(RECT& r, int x, int y, int pad, bool& initialized) {
    int l = x - pad, t = y - pad, ri = x + pad, b = y + pad;
    if (!initialized) { r = {l, t, ri, b}; initialized = true; }
    else {
        if (l  < r.left)  r.left  = l;
        if (t  < r.top)   r.top   = t;
        if (ri > r.right) r.right = ri;
        if (b  > r.bottom) r.bottom = b;
    }
}

static void ClampRectToScreen(RECT& r) {
    r.left   = std::max(0L, std::min((LONG)g_app.width,  r.left));
    r.top    = std::max(0L, std::min((LONG)g_app.height, r.top));
    r.right  = std::max(0L, std::min((LONG)g_app.width,  r.right));
    r.bottom = std::max(0L, std::min((LONG)g_app.height, r.bottom));
}

static void ClearRectInBuffer(const RECT& r) {
    if (!g_app.pixels || r.right <= r.left || r.bottom <= r.top) return;
    size_t rowBytes = (size_t)(r.right - r.left) * 4;
    for (int y = r.top; y < r.bottom; ++y) {
        std::memset(g_app.pixels + (size_t)y * (size_t)g_app.width + (size_t)r.left, 0, rowBytes);
    }
}

// ---------------- props ----------------
static void DrawCouch(Graphics& g) {
    float sc = g_app.scale;
    float seatY = CouchSeatTopY();
    float floorY = g_app.couchY;
    float halfW = g_app.couchW * 0.5f;
    float x0 = g_app.couchX - halfW;
    float backTop = seatY - 30.0f * sc;
    float armTop = seatY - 12.0f * sc;
    float armW = 16.0f * sc;
    Color body(255, 96, 62, 128);
    Color bodyDark(255, 70, 44, 96);
    SolidBrush brBody(body), brDark(bodyDark);
    Pen outline(Color(120, 24, 16, 34), 2.0f * sc);

    // back rest
    FillRoundRect(g, brDark, x0 + armW * 0.4f, backTop, g_app.couchW - armW * 0.8f,
                  seatY - backTop + 8.0f * sc, 10.0f * sc);
    // seat
    FillRoundRect(g, brBody, x0, seatY, g_app.couchW, (floorY - seatY) + 5.0f * sc, 7.0f * sc);
    // cushions
    int n = CouchSeatCount();
    float spacing = (g_app.couchW - armW * 2.0f) / (float)n;
    for (int i = 0; i < n; ++i) {
        float cxp = x0 + armW + spacing * (float)i + 3.0f * sc;
        FillRoundRect(g, brBody, cxp, seatY - 3.0f * sc, spacing - 6.0f * sc, 9.0f * sc, 4.0f * sc);
    }
    // armrests
    FillRoundRect(g, brBody, x0, armTop, armW, (floorY - armTop) + 5.0f * sc, 6.0f * sc);
    FillRoundRect(g, brBody, x0 + g_app.couchW - armW, armTop, armW, (floorY - armTop) + 5.0f * sc, 6.0f * sc);
    // feet
    SolidBrush brFoot(Color(255, 52, 34, 22));
    g.FillRectangle(&brFoot, x0 + 6.0f * sc, floorY - 2.0f * sc, 7.0f * sc, 4.0f * sc);
    g.FillRectangle(&brFoot, x0 + g_app.couchW - 13.0f * sc, floorY - 2.0f * sc, 7.0f * sc, 4.0f * sc);

    // popcorn box + soda on the middle of the couch
    float px = g_app.couchX;
    float py = seatY - 2.0f * sc;
    PointF popPts[4] = {
        {px - 9.0f * sc, py - 20.0f * sc}, {px + 9.0f * sc, py - 20.0f * sc},
        {px + 7.0f * sc, py},              {px - 7.0f * sc, py}
    };
    SolidBrush brPaper(Color(245, 246, 244, 238));
    g.FillPolygon(&brPaper, popPts, 4);
    Pen stripe(Color(220, 226, 60, 58), 2.2f * sc);
    for (int i = -1; i <= 1; ++i) {
        g.DrawLine(&stripe, px + (float)i * 5.5f * sc, py - 20.0f * sc, px + (float)i * 4.2f * sc, py);
    }
    SolidBrush brCorn(Color(250, 255, 240, 190));
    for (int i = 0; i < 7; ++i) {
        float ang = (float)i / 7.0f * TAU_F;
        float kx = px + std::cos(ang) * 7.0f * sc;
        float ky = py - 21.0f * sc + std::sin(ang) * 2.4f * sc;
        g.FillEllipse(&brCorn, kx - 2.6f * sc, ky - 2.6f * sc, 5.2f * sc, 5.2f * sc);
    }
    // soda cup
    float sx = px + 22.0f * sc;
    PointF cupPts[4] = {
        {sx - 5.5f * sc, py - 17.0f * sc}, {sx + 5.5f * sc, py - 17.0f * sc},
        {sx + 4.0f * sc, py},              {sx - 4.0f * sc, py}
    };
    SolidBrush brCup(Color(235, 226, 74, 60));
    g.FillPolygon(&brCup, cupPts, 4);
    Pen straw(Color(230, 245, 245, 245), 1.8f * sc);
    g.DrawLine(&straw, sx + 1.5f * sc, py - 17.0f * sc, sx + 6.0f * sc, py - 30.0f * sc);
}

static void DrawMiniDesk(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    Vec2 mid = LerpV(s.pose.handL, s.pose.handR, 0.5f);
    float f = (float)s.facing;
    float topY = mid.y + 4.0f * sc;
    float floorY = (float)FloorYAt(s.x);
    float deskCx = mid.x + f * 6.0f * sc;
    float deskW = 62.0f * sc;
    float deskH = 7.0f * sc;

    SolidBrush brWood(Color(235, 176, 122, 74));
    SolidBrush brLeg(Color(225, 128, 88, 52));
    FillRoundRect(g, brWood, deskCx - deskW * 0.5f, topY - deskH, deskW, deskH, 2.5f * sc);
    g.FillRectangle(&brLeg, deskCx - deskW * 0.5f + 4.0f * sc, topY, 4.0f * sc, floorY - topY);
    g.FillRectangle(&brLeg, deskCx + deskW * 0.5f - 8.0f * sc, topY, 4.0f * sc, floorY - topY);

    // laptop: base on the desk + tilted glowing lid
    float baseW = 30.0f * sc, baseH = 4.0f * sc;
    float bx = deskCx + f * 2.0f * sc;
    SolidBrush brBase(Color(250, 58, 64, 76));
    FillRoundRect(g, brBase, bx - baseW * 0.5f, topY - deskH - baseH, baseW, baseH, 1.6f * sc);
    float lidW = baseW * 0.92f, lidH = 22.0f * sc;
    GraphicsPath lid;
    REAL lx = bx - lidW * 0.5f + f * 3.0f * sc;
    REAL ly = topY - deskH - baseH - lidH;
    lid.AddLine(lx, ly + lidH, lx, ly);
    lid.AddLine(lx, ly, lx + f * 5.0f * sc, ly + 3.0f * sc);
    SolidBrush brGlow(Color(185, 96, 226, 255));
    SolidBrush brLid(Color(240, 40, 46, 58));
    float scrX = std::min(lx, lx + f * 5.0f * sc);
    g.FillRectangle(&brLid, scrX - 1.5f * sc, ly - 1.0f * sc, lidW + 3.0f * sc, lidH + 2.0f * sc);
    g.FillRectangle(&brGlow, scrX, ly, lidW, lidH);
    Pen codeLine(Color(210, 250, 255, 255), 1.2f * sc);
    for (int i = 0; i < 4; ++i) {
        float yy = ly + 4.0f * sc + (float)i * 4.6f * sc;
        float len = (7.0f + (float)((i * 5 + (int)(g_app.globalTime * 2.0f)) % 9)) * sc;
        g.DrawLine(&codeLine, scrX + 3.0f * sc, yy, scrX + 3.0f * sc + len, yy);
    }
}

static void DrawBlocks(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    float floorY = (float)FloorYAt(s.x);
    float bx = s.x - s.facing * 30.0f * sc;
    Color cols[4] = {Color(245, 240, 92, 66), Color(245, 66, 150, 240),
                     Color(245, 96, 210, 120), Color(245, 250, 200, 70)};
    int placed = ClampF((float)s.blocks, 0.0f, 6.0f) + 0.5f;
    for (int i = 0; i < placed; ++i) {
        SolidBrush br(cols[i % 4]);
        float sz = (14.0f - (float)i * 0.6f) * sc;
        float jx = ((i % 2) == 0) ? -1.5f * sc : 1.5f * sc;
        g.FillRectangle(&br, bx + jx - sz * 0.5f, floorY - sz * (float)(i + 1), sz, sz - 1.0f * sc);
        Pen pk(Color(150, 30, 30, 40), 1.2f * sc);
        g.DrawRectangle(&pk, bx + jx - sz * 0.5f, floorY - sz * (float)(i + 1), sz, sz - 1.0f * sc);
    }
}

static void DrawBook(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    Vec2 m = LerpV(s.pose.handL, s.pose.handR, 0.5f);
    SolidBrush brPages(Color(245, 250, 248, 240));
    SolidBrush brCover(Color(240, 120, 60, 200));
    float w = 15.0f * sc, h = 11.0f * sc;
    PointF l[3] = {{m.x - w, m.y - h * 0.35f}, {m.x, m.y + h * 0.25f}, {m.x, m.y - h}};
    PointF r[3] = {{m.x + w, m.y - h * 0.35f}, {m.x, m.y + h * 0.25f}, {m.x, m.y - h}};
    g.FillPolygon(&brCover, l, 3);
    g.FillPolygon(&brCover, r, 3);
    PointF pl[3] = {{m.x - w * 0.85f, m.y - h * 0.30f}, {m.x, m.y + h * 0.10f}, {m.x, m.y - h * 0.86f}};
    PointF pr[3] = {{m.x + w * 0.85f, m.y - h * 0.30f}, {m.x, m.y + h * 0.10f}, {m.x, m.y - h * 0.86f}};
    g.FillPolygon(&brPages, pl, 3);
    g.FillPolygon(&brPages, pr, 3);
}

static void DrawGuitar(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    Vec2 body = Add(s.pose.handR, Mul(Sub(s.pose.hip, s.pose.handR), 0.25f));
    Vec2 neckH = s.pose.handL;
    SolidBrush brBody(Color(245, 196, 96, 40));
    Pen outline(Color(180, 90, 44, 16), 1.6f * sc);
    g.FillEllipse(&brBody, body.x - 10.0f * sc, body.y - 8.0f * sc, 20.0f * sc, 16.0f * sc);
    g.DrawEllipse(&outline, body.x - 10.0f * sc, body.y - 8.0f * sc, 20.0f * sc, 16.0f * sc);
    SolidBrush hole(Color(220, 60, 34, 14));
    g.FillEllipse(&hole, body.x - 3.0f * sc, body.y - 2.6f * sc, 6.0f * sc, 5.2f * sc);
    Pen neck(Color(240, 120, 72, 40), 3.4f * sc);
    g.DrawLine(&neck, body.x, body.y, neckH.x, neckH.y);
    Pen string(Color(150, 250, 245, 235), 0.8f * sc);
    g.DrawLine(&string, body.x - 2.0f * sc, body.y - 1.0f * sc, neckH.x, neckH.y);
}

static void DrawFishing(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    Vec2 h = s.pose.handR;
    Vec2 rodTip = V2(h.x + s.facing * 30.0f * sc, h.y - 16.0f * sc);
    Pen rod(Color(240, 122, 82, 48), 2.4f * sc);
    rod.SetStartCap(LineCapRound);
    rod.SetEndCap(LineCapRound);
    g.DrawLine(&rod, h.x, h.y, rodTip.x, rodTip.y);
    float bobY = s.y + 20.0f * sc + std::sin(s.fishT * 2.2f) * 2.4f * sc;
    Pen line(Color(150, 240, 245, 250), 1.0f * sc);
    float bxp0 = rodTip.x + s.facing * 3.0f * sc;
    Vec2 mid = V2((rodTip.x + bxp0) * 0.5f + s.facing * 4.0f * sc, (rodTip.y + bobY) * 0.5f);
    PointF pts[3] = { {rodTip.x, rodTip.y}, {mid.x, mid.y}, {bxp0, bobY} };
    g.DrawCurve(&line, pts, 3, 0.35f);
    SolidBrush bobBer(Color(245, 240, 60, 52));
    SolidBrush bobWhi(Color(245, 250, 250, 250));
    float bxp = bxp0;
    g.FillEllipse(&bobWhi, bxp - 3.4f * sc, bobY - 3.4f * sc, 6.8f * sc, 6.8f * sc);
    g.FillEllipse(&bobBer, bxp - 3.4f * sc, bobY - 0.4f * sc, 6.8f * sc, 3.8f * sc);
}

static void DrawCampfire(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    float fx = s.x + s.facing * 42.0f * sc;
    float fy = (float)FloorYAt(fx);
    Pen log(Color(240, 118, 78, 44), 4.2f * sc);
    log.SetStartCap(LineCapRound);
    log.SetEndCap(LineCapRound);
    g.DrawLine(&log, fx - 13.0f * sc, fy - 2.0f * sc, fx + 13.0f * sc, fy - 7.0f * sc);
    g.DrawLine(&log, fx + 13.0f * sc, fy - 2.0f * sc, fx - 13.0f * sc, fy - 7.0f * sc);
    float fl = g_app.globalTime * 7.0f;
    for (int i = 0; i < 3; ++i) {
        float k = 1.0f - (float)i * 0.28f;
        float wob = std::sin(fl + (float)i * 1.7f) * 2.2f * sc;
        Color c = (i == 0) ? Color(200, 255, 140, 30)
                : (i == 1) ? Color(190, 255, 206, 70) : Color(160, 255, 240, 130);
        SolidBrush br(c);
        float hgt = (20.0f + std::sin(fl * 1.4f + (float)i) * 4.0f) * sc * k;
        PointF tri[3] = { {fx - 8.0f * sc * k + wob, fy - 6.0f * sc},
                          {fx + 8.0f * sc * k + wob, fy - 6.0f * sc},
                          {fx + wob * 1.6f, fy - 6.0f * sc - hgt} };
        g.FillPolygon(&br, tri, 3);
    }
}

static void DrawPencilAndDoodle(Graphics& g, const Stickman& s) {
    float sc = g_app.scale;
    if (!s.doodle.empty()) {
        float alpha = ClampF(s.doodleFade / 3.0f, 0.0f, 1.0f);
        Pen dp(Color((BYTE)(200 * alpha), s.r, s.g, s.b), 2.2f * sc);
        dp.SetStartCap(LineCapRound);
        dp.SetEndCap(LineCapRound);
        dp.SetLineJoin(LineJoinRound);
        for (size_t i = 1; i < s.doodle.size(); ++i) {
            g.DrawLine(&dp, s.doodle[i - 1].x, s.doodle[i - 1].y, s.doodle[i].x, s.doodle[i].y);
        }
    }
    if (s.act == Act::SKETCH) {
        Vec2 h = s.pose.handR;
        Vec2 tip = V2(h.x + s.facing * 3.0f * sc, h.y + 9.0f * sc);
        Pen pencil(Color(245, 250, 206, 60), 3.0f * sc);
        pencil.SetStartCap(LineCapRound);
        g.DrawLine(&pencil, h.x - s.facing * 4.0f * sc, h.y - 11.0f * sc, tip.x, tip.y);
        Pen graphite(Color(220, 60, 60, 66), 1.6f * sc);
        g.DrawLine(&graphite, tip.x, tip.y, tip.x + s.facing * 1.6f * sc, tip.y + 2.2f * sc);
    }
}

static void DrawSnack(Graphics& g, const Snack& sn) {
    float sc = g_app.scale;
    float a = ClampF(sn.life / 3.0f, 0.0f, 1.0f);
    float biteK = ClampF(sn.bites / 3.0f, 0.25f, 1.0f);
    if (sn.kind != 0) {
        float k = 11.0f * sc * biteK;
        if (sn.kind == 1) {            // cake with a cherry
            SolidBrush sponge(Color((BYTE)(245 * a), 250, 226, 180));
            SolidBrush cream(Color((BYTE)(245 * a), 252, 246, 240));
            g.FillRectangle(&sponge, sn.x - k, sn.y - k * 0.7f, k * 2.0f, k * 0.9f);
            g.FillRectangle(&cream, sn.x - k * 1.05f, sn.y - k * 1.05f, k * 2.1f, k * 0.45f);
            SolidBrush cher(Color((BYTE)(240 * a), 224, 56, 62));
            g.FillEllipse(&cher, sn.x - 2.6f * sc, sn.y - k * 1.5f, 5.2f * sc, 5.2f * sc);
        } else if (sn.kind == 2) {     // watermelon wedge
            SolidBrush rind(Color((BYTE)(245 * a), 92, 186, 96));
            SolidBrush flesh(Color((BYTE)(245 * a), 240, 88, 104));
            PointF tri[3] = { {sn.x - k, sn.y - k * 0.5f}, {sn.x + k, sn.y - k * 0.5f}, {sn.x, sn.y + k * 0.6f} };
            g.FillPolygon(&rind, tri, 3);
            PointF t2[3] = { {sn.x - k * 0.8f, sn.y - k * 0.28f}, {sn.x + k * 0.8f, sn.y - k * 0.28f},
                             {sn.x, sn.y + k * 0.42f} };
            g.FillPolygon(&flesh, t2, 3);
            SolidBrush seed(Color((BYTE)(230 * a), 40, 34, 36));
            g.FillEllipse(&seed, sn.x - 3.0f * sc, sn.y - 1.6f * sc, 2.0f * sc, 2.6f * sc);
            g.FillEllipse(&seed, sn.x + 1.4f * sc, sn.y - 3.0f * sc, 2.0f * sc, 2.6f * sc);
        } else if (sn.kind == 3) {     // bubbling potion
            SolidBrush glass(Color((BYTE)(215 * a), 190, 240, 255));
            SolidBrush brew(Color((BYTE)(245 * a), 120, 240, 150));
            g.FillEllipse(&glass, sn.x - k * 0.7f, sn.y - k * 1.1f, k * 1.4f, k * 1.3f);
            g.FillEllipse(&brew, sn.x - k * 0.55f, sn.y - k * 0.75f, k * 1.1f, k * 0.85f);
            Pen neck(Color((BYTE)(230 * a), 200, 230, 250), 2.4f * sc);
            g.DrawLine(&neck, sn.x, sn.y - k * 1.35f, sn.x, sn.y - k * 0.95f);
            SolidBrush cork(Color((BYTE)(240 * a), 190, 130, 70));
            g.FillRectangle(&cork, sn.x - 2.6f * sc, sn.y - k * 1.7f, 5.2f * sc, 3.4f * sc);
        } else {                        // a Minecraft-ish cube
            float c = k * 0.9f;
            SolidBrush face(Color((BYTE)(245 * a), 118, 180, 84));
            SolidBrush side(Color((BYTE)(245 * a), 96, 148, 70));
            SolidBrush dirt(Color((BYTE)(245 * a), 132, 94, 60));
            g.FillRectangle(&face, sn.x - c, sn.y - c * 2.0f, c * 2.0f, c * 2.0f);
            g.FillRectangle(&dirt, sn.x - c, sn.y - c * 0.7f, c * 2.0f, c * 0.7f);
            Pen grid(Color((BYTE)(150 * a), 60, 96, 44), 1.0f * sc);
            g.DrawLine(&grid, sn.x, sn.y - c * 2.0f, sn.x, sn.y - c * 0.7f);
            g.DrawLine(&grid, sn.x - c, sn.y - c * 1.35f, sn.x + c, sn.y - c * 1.35f);
            (void)side;
        }
        return;
    }
    PointF tri[3] = {
        {sn.x - 11.0f * sc * biteK, sn.y - 15.0f * sc * biteK},
        {sn.x + 11.0f * sc * biteK, sn.y - 15.0f * sc * biteK},
        {sn.x, sn.y}
    };
    SolidBrush cheese(Color((BYTE)(245 * a), 250, 202, 78));
    g.FillPolygon(&cheese, tri, 3);
    Pen crust(Color((BYTE)(245 * a), 224, 158, 74), 4.0f * sc);
    crust.SetStartCap(LineCapRound);
    crust.SetEndCap(LineCapRound);
    g.DrawLine(&crust, tri[0].X, tri[0].Y, tri[1].X, tri[1].Y);
    SolidBrush pep(Color((BYTE)(235 * a), 208, 58, 48));
    g.FillEllipse(&pep, sn.x - 5.0f * sc, sn.y - 12.0f * sc, 4.6f * sc, 4.6f * sc);
    g.FillEllipse(&pep, sn.x + 1.4f * sc, sn.y - 7.0f * sc, 4.0f * sc, 4.0f * sc);
}

static void DrawBallProp(Graphics& g, const Ball& b) {
    float a = ClampF(b.life / 2.5f, 0.0f, 1.0f);
    SolidBrush br(Color((BYTE)(245 * a), 250, 90, 70));
    g.FillEllipse(&br, b.x - b.r, b.y - b.r, b.r * 2.0f, b.r * 2.0f);
    Pen stripe(Color((BYTE)(230 * a), 250, 240, 240), 3.0f);
    stripe.SetStartCap(LineCapRound);
    float ca = std::cos(b.rot), sa = std::sin(b.rot);
    PointF pts[3] = {
        {b.x - b.r * ca, b.y - b.r * sa},
        {b.x + b.r * 0.4f * sa, b.y - b.r * 0.4f * ca},
        {b.x + b.r * ca, b.y + b.r * sa}
    };
    g.DrawCurve(&stripe, pts, 3, 0.4f);
    Pen outline(Color((BYTE)(150 * a), 40, 30, 34), 1.6f);
    g.DrawEllipse(&outline, b.x - b.r, b.y - b.r, b.r * 2.0f, b.r * 2.0f);
}

static void DrawSword(Graphics& g, Vec2 hand, Vec2 elbow, bool diamond, float sc,
                      BYTE alpha = 255, float angOff = 0.0f) {
    Vec2 dir = Sub(hand, elbow);
    float len = Len(dir);
    dir = (len > 0.01f) ? Mul(dir, 1.0f / len) : V2(1.0f, -0.5f);
    if (angOff != 0.0f) {
        float ca = std::cos(angOff), sa = std::sin(angOff);
        dir = V2(dir.x * ca - dir.y * sa, dir.x * sa + dir.y * ca);
    }
    Vec2 perp = V2(-dir.y, dir.x);
    Pen hiltPen(Color((BYTE)(alpha * 0.7f), 120, 78, 36), 3.6f * sc);
    hiltPen.SetStartCap(LineCapRound);
    hiltPen.SetEndCap(LineCapRound);
    Color bladeCol = diamond ? Color(alpha, 72, 238, 245) : Color(alpha, 215, 222, 232);
    Pen bladePen(bladeCol, 4.0f * sc);
    bladePen.SetEndCap(LineCapTriangle);
    Vec2 guard = Add(hand, Mul(dir, 4.0f * sc));
    Vec2 tip = Add(hand, Mul(dir, 28.0f * sc));
    Vec2 pommel = Sub(hand, Mul(dir, 5.0f * sc));
    Vec2 g1 = Add(guard, Mul(perp, 6.5f * sc));
    Vec2 g2 = Sub(guard, Mul(perp, 6.5f * sc));
    g.DrawLine(&bladePen, guard.x, guard.y, tip.x, tip.y);
    g.DrawLine(&hiltPen, pommel.x, pommel.y, guard.x, guard.y);
    g.DrawLine(&hiltPen, g1.x, g1.y, g2.x, g2.y);
}

// ============================================================================
// v3 prop artwork (everything here is drawn by the overlay; no real file,
// window content or input device is ever touched)
// ============================================================================
static void StrokeRoundRectF(Graphics& g, const Pen& pen, REAL x, REAL y, REAL w, REAL h, REAL r) {
    if (w <= 0.0f || h <= 0.0f) return;
    r = std::min(r, std::min(w, h) * 0.5f);
    GraphicsPath path;
    path.AddArc(x, y, r * 2.0f, r * 2.0f, 180.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y, r * 2.0f, r * 2.0f, 270.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 0.0f, 90.0f);
    path.AddArc(x, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 90.0f, 90.0f);
    path.CloseFigure();
    g.DrawPath(&pen, &path);
}

static void DrawWorldProp(Graphics& g, const WorldProp& w, float sc) {
    switch (w.kind) {
        case PR_TABLE: {
            float topY = w.y - 17.0f * sc;
            if (w.data == 3) {                       // tug of war: rope + red knot
                Stickman* pa = FindStickmanById(w.a);
                Stickman* pb = FindStickmanById(w.b);
                if (pa && pb) {
                    Pen rope(Color(240, 224, 190, 120), 2.6f * sc);
                    rope.SetStartCap(LineCapRound);
                    rope.SetEndCap(LineCapRound);
                    g.DrawLine(&rope, pa->pose.handR.x, pa->pose.handR.y, pb->pose.handR.x, pb->pose.handR.y);
                    float mx = (pa->pose.handR.x + pb->pose.handR.x) * 0.5f;
                    float my = (pa->pose.handR.y + pb->pose.handR.y) * 0.5f;
                    SolidBrush knot(Color(245, 226, 60, 54));
                    g.FillEllipse(&knot, mx - 3.8f * sc, my - 3.8f * sc, 7.6f * sc, 7.6f * sc);
                }
                break;
            }
            SolidBrush top(Color(240, 176, 122, 74));
            SolidBrush leg(Color(230, 128, 88, 52));
            float tw = 74.0f * sc;
            FillRoundRect(g, top, w.x - tw * 0.5f, topY, tw, 6.0f * sc, 2.0f * sc);
            g.FillRectangle(&leg, w.x - tw * 0.5f + 5.0f * sc, topY + 6.0f * sc,
                            4.5f * sc, w.y - topY - 6.0f * sc);
            g.FillRectangle(&leg, w.x + tw * 0.5f - 9.5f * sc, topY + 6.0f * sc,
                            4.5f * sc, w.y - topY - 6.0f * sc);
            if (w.data == 0) {                        // a fan of cards
                SolidBrush card(Color(248, 250, 248, 240));
                Pen edge(Color(200, 60, 62, 70), 1.0f * sc);
                for (int i = 0; i < 4; ++i) {
                    float cxp = w.x - 20.0f * sc + (float)i * 9.0f * sc;
                    float ry = topY - 4.0f * sc - (float)(i % 2) * 1.6f * sc;
                    g.FillRectangle(&card, cxp, ry, 7.5f * sc, 10.0f * sc);
                    g.DrawRectangle(&edge, cxp, ry, 7.5f * sc, 10.0f * sc);
                }
                SolidBrush pip(Color(230, 226, 60, 58));
                g.FillEllipse(&pip, w.x + 16.0f * sc, topY - 4.5f * sc, 4.2f * sc, 4.2f * sc);
            } else if (w.data == 1) {                 // chess / checkers board
                float cell = 7.2f * sc;
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 8; ++c) {
                        SolidBrush sq(((r + c) % 2 == 0) ? Color(245, 240, 232, 214)
                                                         : Color(245, 96, 78, 66));
                        g.FillRectangle(&sq, w.x - 4.0f * cell + (float)c * cell,
                                        topY - 2.4f * cell + (float)r * cell * 0.62f,
                                        cell, cell * 0.62f);
                    }
                }
            } else {                                  // armwrestle pad
                SolidBrush pad(Color(235, 120, 88, 60));
                FillRoundRect(g, pad, w.x - 11.0f * sc, topY - 3.2f * sc, 22.0f * sc, 4.0f * sc, 1.6f * sc);
            }
            break;
        }

        case PR_CAULDRON: {
            float py = w.y - 3.0f * sc;
            SolidBrush pot(Color(245, 58, 56, 64));
            PointF body[6] = {
                {w.x - 17.0f * sc, py - 16.0f * sc}, {w.x + 17.0f * sc, py - 16.0f * sc},
                {w.x + 20.0f * sc, py - 7.0f * sc},  {w.x + 12.0f * sc, py},
                {w.x - 12.0f * sc, py},               {w.x - 20.0f * sc, py - 7.0f * sc}
            };
            g.FillPolygon(&pot, body, 6);
            SolidBrush brew(Color(235, 120, 240, 130));
            g.FillEllipse(&brew, w.x - 16.0f * sc, py - 19.5f * sc, 32.0f * sc, 7.0f * sc);
            Pen rim(Color(235, 96, 96, 108), 2.2f * sc);
            g.DrawEllipse(&rim, w.x - 17.0f * sc, py - 20.5f * sc, 34.0f * sc, 7.5f * sc);
            SolidBrush flame(Color(190, 255, 168, 60));
            float fl = std::sin(w.phase * 9.0f) * 1.8f * sc;
            PointF tri[3] = { {w.x - 8.0f * sc, py + 1.0f * sc}, {w.x + 8.0f * sc, py + 1.0f * sc},
                              {w.x + fl, py + 10.0f * sc} };
            g.FillPolygon(&flame, tri, 3);
            break;
        }

        case PR_BOX: {                                // a drawn cube, kicked around
            float k = 10.0f * sc;
            float ca = std::cos(w.rot), sa = std::sin(w.rot);
            auto rot2 = [&](float x, float y) { return V2(w.x + x * ca - y * sa, w.y + x * sa + y * ca); };
            Vec2 p0 = rot2(-k, -k), p1 = rot2(k, -k), p2 = rot2(k, k), p3 = rot2(-k, k);
            PointF face[4] = { {p0.x, p0.y}, {p1.x, p1.y}, {p2.x, p2.y}, {p3.x, p3.y} };
            SolidBrush box(Color(240, 226, 168, 106));
            g.FillPolygon(&box, face, 4);
            Pen bx(Color(220, 150, 104, 56), 1.6f * sc);
            g.DrawPolygon(&bx, face, 4);
            Pen tape(Color(200, 250, 236, 190), 3.2f * sc);
            tape.SetStartCap(LineCapRound);
            tape.SetEndCap(LineCapRound);
            g.DrawLine(&tape, p3.x, p3.y, p1.x, p1.y);
            break;
        }

        case PR_CRACK: {                              // fake cracked glass, then tape
            float a = ClampF(w.life / 1.6f, 0.0f, 1.0f);
            float side = (w.x < (float)g_app.width * 0.5f) ? 1.0f : -1.0f;
            Pen glass(Color((BYTE)(205 * a), 250, 252, 255), 1.7f);
            glass.SetStartCap(LineCapRound);
            for (int i = -3; i <= 3; ++i) {
                float len = (16.0f + (float)(7 - std::abs(i)) * 6.0f) * sc;
                float ang = (float)i * 0.34f;
                g.DrawLine(&glass, w.x, w.y, w.x + std::cos(ang) * len * side, w.y + std::sin(ang) * len);
            }
            for (int ring = 1; ring <= 2; ++ring) {
                Pen arc(Color((BYTE)(150 * a), 245, 250, 255), 1.2f);
                float r = (float)ring * 9.0f * sc;
                g.DrawArc(&arc, w.x - r, w.y - r, r * 2.0f, r * 2.0f, side > 0.0f ? 250.0f : 70.0f, 140.0f);
            }
            if (w.life < 4.2f) {
                float ta = ClampF((4.2f - w.life) / 0.7f, 0.0f, 1.0f) * ClampF(w.life / 1.2f, 0.0f, 1.0f);
                SolidBrush tape(Color((BYTE)(215 * ta), 236, 230, 196));
                Pen te(Color((BYTE)(160 * ta), 190, 184, 150), 1.0f);
                for (int i = 0; i < 2; ++i) {
                    float yy = w.y + (i == 0 ? -17.0f : 12.0f) * sc;
                    PointF q[4] = { {w.x - 20.0f * sc, yy - 5.0f * sc}, {w.x + 21.0f * sc, yy + 2.0f * sc},
                                    {w.x + 21.0f * sc, yy + 10.0f * sc}, {w.x - 20.0f * sc, yy + 3.0f * sc} };
                    g.FillPolygon(&tape, q, 4);
                    g.DrawLine(&te, q[0].X, q[0].Y, q[1].X, q[1].Y);
                    g.DrawLine(&te, q[3].X, q[3].Y, q[2].X, q[2].Y);
                }
            }
            break;
        }

        case PR_PILLOW: {
            float k = sc;
            if (w.data == 1 || w.data == 2) {          // snowman (1 = still packing)
                SolidBrush snow(Color(238, 248, 251, 255));
                Pen edge(Color(130, 190, 205, 220), 1.2f * k);
                float r1 = 14.0f * k, r2 = 10.0f * k, r3 = 7.0f * k;
                g.FillEllipse(&snow, w.x - r1, w.y - r1 * 2.0f, r1 * 2.0f, r1 * 2.0f);
                g.DrawEllipse(&edge, w.x - r1, w.y - r1 * 2.0f, r1 * 2.0f, r1 * 2.0f);
                if (w.data == 2) {
                    g.FillEllipse(&snow, w.x - r2, w.y - r1 * 2.0f - r2 * 1.6f, r2 * 2.0f, r2 * 2.0f);
                    float hy = w.y - r1 * 2.0f - r2 * 1.6f - r3 * 1.5f;
                    g.FillEllipse(&snow, w.x - r3, hy - r3, r3 * 2.0f, r3 * 2.0f);
                    SolidBrush nose(Color(245, 250, 140, 40));
                    PointF car[3] = { {w.x + r3 * 0.3f, hy - 1.2f * k}, {w.x + r3 * 2.2f, hy + 0.6f * k},
                                      {w.x + r3 * 0.3f, hy + 2.6f * k} };
                    g.FillPolygon(&nose, car, 3);
                    SolidBrush coal(Color(240, 44, 44, 50));
                    g.FillEllipse(&coal, w.x - 2.6f * k, hy - 3.6f * k, 2.2f * k, 2.2f * k);
                    g.FillEllipse(&coal, w.x + 1.2f * k, hy - 3.6f * k, 2.2f * k, 2.2f * k);
                    Pen arm(Color(230, 120, 78, 40), 1.6f * k);
                    arm.SetStartCap(LineCapRound);
                    arm.SetEndCap(LineCapRound);
                    g.DrawLine(&arm, w.x - r2, w.y - r1 * 2.0f - r2 * 0.8f, w.x - r2 - 11.0f * k, hy);
                    g.DrawLine(&arm, w.x + r2, w.y - r1 * 2.0f - r2 * 0.8f, w.x + r2 + 11.0f * k, hy - 4.0f * k);
                }
            } else {                                   // a pillow for a nap
                SolidBrush p(Color(240, 178, 132, 214));
                FillRoundRect(g, p, w.x - 17.0f * k, w.y - 9.0f * k, 34.0f * k, 10.0f * k, 4.5f * k);
                Pen seam(Color(150, 214, 176, 244), 1.0f * k);
                g.DrawLine(&seam, w.x - 12.0f * k, w.y - 4.4f * k, w.x + 12.0f * k, w.y - 4.4f * k);
            }
            break;
        }

        case PR_PANEL: {                               // Yellow's mini console
            float k = sc;
            SolidBrush box(Color(245, 44, 50, 62));
            FillRoundRect(g, box, w.x - 20.0f * k, w.y - 30.0f * k, 40.0f * k, 24.0f * k, 3.0f * k);
            SolidBrush scr(Color(220, 90, 230, 255));
            g.FillRectangle(&scr, w.x - 16.0f * k, w.y - 26.0f * k, 22.0f * k, 12.0f * k);
            Pen line(Color(230, 250, 255, 255), 1.0f * k);
            for (int i = 0; i < 3; ++i) {
                float yy = w.y - 24.5f * k + (float)i * 3.6f * k;
                float len = (6.0f + (float)((i + (int)(w.phase * 2.0f)) % 7)) * k;
                g.DrawLine(&line, w.x - 14.5f * k, yy, w.x - 14.5f * k + len, yy);
            }
            for (int i = 0; i < 3; ++i) {
                bool on = (((int)(w.phase * 4.0f) + i) % 3) == 0;
                SolidBrush led(i == 0 ? Color(230, 255, 90, 80)
                                : (i == 1 ? Color(230, on ? 240 : 90, 200, 110)
                                          : Color(230, on ? 230 : 80, 140, 250)));
                g.FillEllipse(&led, w.x + 8.0f * k + (float)i * 3.4f * k, w.y - 12.0f * k, 2.4f * k, 2.4f * k);
            }
            break;
        }

        case PR_SIGN: {                                // a hand-painted note
            float k = sc;
            float a = ClampF(w.life / 1.2f, 0.0f, 1.0f) * ClampF((w.maxLife - w.life) / 0.5f, 0.0f, 1.0f);
            if (a < 0.01f) a = 1.0f;
            float by = w.y - 44.0f * k + std::sin(w.phase * 1.7f) * 1.6f * k;
            Pen stick(Color((BYTE)(220 * a), 150, 104, 60), 2.6f * k);
            stick.SetStartCap(LineCapRound);
            g.DrawLine(&stick, w.x, by + 4.0f * k, w.x, w.y - 2.0f * k);
            SolidBrush board(Color((BYTE)(242 * a), 248, 244, 232));
            Pen edge(Color((BYTE)(200 * a), 120, 92, 58), 1.6f * k);
            FillRoundRect(g, board, w.x - 34.0f * k, by - 17.0f * k, 68.0f * k, 24.0f * k, 3.0f * k);
            StrokeRoundRectF(g, edge, w.x - 34.0f * k, by - 17.0f * k, 68.0f * k, 24.0f * k, 3.0f * k);
            if (w.data == 3) {                                // the duel banner
                SolidBrush board2(Color((BYTE)(235 * a), 46, 44, 52));
                Pen rope2(Color((BYTE)(220 * a), 250, 208, 88), 2.2f * k);
                FillRoundRect(g, board2, w.x - 30.0f * k, by - 15.0f * k, 60.0f * k, 22.0f * k, 3.0f * k);
                g.DrawLine(&rope2, w.x - 30.0f * k, by - 15.0f * k, w.x + 30.0f * k, by + 7.0f * k);
                g.DrawLine(&rope2, w.x - 30.0f * k, by + 7.0f * k, w.x + 30.0f * k, by - 15.0f * k);
                FontFamily fam(L"Segoe UI");
                Font font(&fam, 12.0f * k, FontStyleBold, UnitPixel);
                SolidBrush txt(Color((BYTE)(250 * a), 250, 246, 236));
                StringFormat sf;
                sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(L"VS", 2, &font, RectF(w.x - 30.0f * k, by - 15.0f * k, 60.0f * k, 22.0f * k), &sf, &txt);
            } else if (w.data == 2) {
                SolidBrush cup(Color((BYTE)(240 * a), 96, 176, 236));
                g.FillRectangle(&cup, w.x - 28.0f * k, by - 10.0f * k, 11.0f * k, 10.0f * k);
                Pen handle(Color((BYTE)(230 * a), 96, 176, 236), 1.8f * k);
                g.DrawArc(&handle, w.x - 19.5f * k, by - 9.0f * k, 8.0f * k, 8.0f * k, -70.0f, 190.0f);
                FontFamily fam(L"Segoe UI");
                Font font(&fam, 8.6f * k, FontStyleBold, UnitPixel);
                SolidBrush txt(Color((BYTE)(240 * a), 58, 52, 44));
                StringFormat sf;
                sf.SetAlignment(StringAlignmentNear);
                sf.SetLineAlignment(StringAlignmentCenter);
                // "Пора размяться!"
                g.DrawString(L"\u041f\u043e\u0440\u0430 \u0440\u0430\u0437\u043c\u044f\u0442\u044c\u0441\u044f!", 18,
                             &font, RectF(w.x - 15.0f * k, by - 15.0f * k, 47.0f * k, 20.0f * k), &sf, &txt);
            } else if (w.data == 1) {
                SolidBrush heart(Color((BYTE)(240 * a), 240, 92, 120));
                g.FillEllipse(&heart, w.x - 11.0f * k, by - 11.0f * k, 9.5f * k, 9.5f * k);
                g.FillEllipse(&heart, w.x + 1.5f * k, by - 11.0f * k, 9.5f * k, 9.5f * k);
                PointF tri[3] = { {w.x - 9.5f * k, by - 4.0f * k}, {w.x + 11.0f * k, by - 4.0f * k},
                                  {w.x + 0.8f * k, by + 5.0f * k} };
                g.FillPolygon(&heart, tri, 3);
            } else {
                SolidBrush pie(Color((BYTE)(240 * a), 250, 200, 90));
                g.FillEllipse(&pie, w.x - 11.0f * k, by - 10.0f * k, 22.0f * k, 13.0f * k);
                SolidBrush jam(Color((BYTE)(240 * a), 224, 62, 62));
                g.FillEllipse(&jam, w.x - 4.0f * k, by - 11.0f * k, 8.0f * k, 5.0f * k);
            }
            break;
        }

        case PR_ICON: {                            // a *drawn* shortcut tile (a real icon is never touched)
            float k = sc, w0 = 26.0f * k, h0 = 21.0f * k;
            g.TranslateTransform(w.x, w.y);
            g.RotateTransform(w.rot * 57.29578f);
            SolidBrush paper(Color(245, 249, 249, 246));
            Pen edge(Color(215, 150, 156, 168), 1.4f * k);
            g.FillRectangle(&paper, -w0 * 0.5f, -h0 * 0.5f, w0, h0);
            g.DrawRectangle(&edge, -w0 * 0.5f, -h0 * 0.5f, w0, h0);
            SolidBrush pic(Color(240, 110, 176, 240));
            g.FillRectangle(&pic, -w0 * 0.38f, -h0 * 0.34f, w0 * 0.76f, h0 * 0.5f);
            SolidBrush hill(Color(240, 96, 190, 120));
            PointF tri[3] = { {w0 * 0.30f, h0 * 0.14f}, {w0 * 0.02f, -h0 * 0.16f}, {-w0 * 0.30f, h0 * 0.14f} };
            g.FillPolygon(&hill, tri, 3);
            SolidBrush sun(Color(245, 250, 208, 60));
            g.FillEllipse(&sun, w0 * 0.10f, -h0 * 0.30f, 5.0f * k, 5.0f * k);
            // the little "shortcut" arrow in the corner
            SolidBrush arrow(Color(245, 86, 190, 96));
            PointF ar[3] = { {-w0 * 0.5f, h0 * 0.5f}, {-w0 * 0.5f, h0 * 0.16f}, {-w0 * 0.16f, h0 * 0.5f} };
            g.FillPolygon(&arrow, ar, 3);
            g.ResetTransform();
            break;
        }

        case PR_FOLDER: {                          // a drawn folder, delivered to you
            float k = sc;
            SolidBrush back(Color(245, 246, 190, 60));
            SolidBrush front(Color(235, 252, 214, 96));
            Pen edge(Color(200, 186, 138, 24), 1.3f * k);
            g.FillRectangle(&back, w.x - 17.0f * k, w.y - 15.0f * k, 34.0f * k, 15.0f * k);
            PointF lid[4] = { {w.x - 17.0f * k, w.y - 15.0f * k}, {w.x - 6.0f * k, w.y - 15.0f * k},
                              {w.x - 3.0f * k, w.y - 19.0f * k}, {w.x - 20.0f * k, w.y - 19.0f * k} };
            g.FillPolygon(&back, lid, 4);
            g.DrawPolygon(&edge, lid, 4);
            g.FillRectangle(&front, w.x - 18.0f * k, w.y - 12.0f * k, 36.0f * k, 12.0f * k);
            g.DrawRectangle(&edge, w.x - 18.0f * k, w.y - 12.0f * k, 36.0f * k, 12.0f * k);
            break;
        }

        case PR_TAB: {                             // a fake browser tab that fell off
            float k = sc;
            float a = ClampF(w.life / 1.4f, 0.0f, 1.0f);
            g.TranslateTransform(w.x, w.y);
            g.RotateTransform(w.rot * 22.0f);
            SolidBrush tab(Color((BYTE)(235 * a), 236, 240, 248));
            StrokeRoundRectF(g, Pen(Color((BYTE)(200 * a), 150, 158, 172), 1.3f * k),
                             -22.0f * k, -8.0f * k, 44.0f * k, 16.0f * k, 4.0f * k);
            g.FillRectangle(&tab, -22.0f * k, -4.0f * k, 44.0f * k, 12.0f * k);
            SolidBrush fav(Color((BYTE)(235 * a), 96, 160, 240));
            g.FillRectangle(&fav, -18.0f * k, -1.5f * k, 6.0f * k, 6.0f * k);
            Pen xpen(Color((BYTE)(220 * a), 90, 96, 108), 1.5f * k);
            g.DrawLine(&xpen, 13.0f * k, -1.0f * k, 18.0f * k, 4.0f * k);
            g.DrawLine(&xpen, 18.0f * k, -1.0f * k, 13.0f * k, 4.0f * k);
            g.ResetTransform();
            break;
        }
        case PR_FEED: {                                // a "pressed" player button
            float k = sc;
            float a = ClampF(w.life / 0.6f, 0.0f, 1.0f) * 0.92f;
            SolidBrush bg(Color((BYTE)(215 * a), 20, 22, 28));
            Pen ring(Color((BYTE)(235 * a), 250, 250, 250), 1.6f * k);
            FillRoundRect(g, bg, w.x - 15.0f * k, w.y - 11.0f * k, 30.0f * k, 22.0f * k, 5.0f * k);
            StrokeRoundRectF(g, ring, w.x - 15.0f * k, w.y - 11.0f * k, 30.0f * k, 22.0f * k, 5.0f * k);
            SolidBrush wh(Color((BYTE)(245 * a), 250, 250, 250));
            if (w.data == 0) {
                g.FillRectangle(&wh, w.x - 6.5f * k, w.y - 6.0f * k, 4.0f * k, 12.0f * k);
                g.FillRectangle(&wh, w.x + 2.5f * k, w.y - 6.0f * k, 4.0f * k, 12.0f * k);
            } else if (w.data == 1) {
                PointF t1[3] = { {w.x + 1.0f * k, w.y - 6.0f * k}, {w.x + 1.0f * k, w.y + 6.0f * k},
                                 {w.x - 6.0f * k, w.y} };
                PointF t2[3] = { {w.x + 8.5f * k, w.y - 6.0f * k}, {w.x + 8.5f * k, w.y + 6.0f * k},
                                 {w.x + 1.5f * k, w.y} };
                g.FillPolygon(&wh, t1, 3);
                g.FillPolygon(&wh, t2, 3);
            } else {
                PointF sp[4] = { {w.x - 8.0f * k, w.y - 3.0f * k}, {w.x - 4.0f * k, w.y - 3.0f * k},
                                 {w.x + 1.0f * k, w.y - 7.0f * k}, {w.x + 1.0f * k, w.y + 7.0f * k} };
                g.FillPolygon(&wh, sp, 4);
                g.DrawArc(&ring, w.x + 1.0f * k, w.y - 6.0f * k, 8.0f * k, 12.0f * k, -55.0f, 110.0f);
                g.DrawArc(&ring, w.x + 1.0f * k, w.y - 9.0f * k, 13.0f * k, 18.0f * k, -50.0f, 100.0f);
            }
            break;
        }
        default: break;
    }
}

// A recognizable doodle (cat / pig / flower / smiley) sampled by arc-length so
// the pencil can trace it point by point.
static Vec2 DoodlePoint(int shape, float u, float ax, float ay, float rr, float facing) {
    const float TAU = 6.2831853f;
    auto circle = [&](float from, float to, float cx, float cy, float r, float squash) {
        float k = ClampF((u - from) / (to - from), 0.0f, 1.0f);
        float a = k * TAU;
        return V2(ax + cx + std::cos(a) * r, ay + cy + std::sin(a) * r * squash);
    };
    if (shape == 1) {              // cat: head, ears, tail
        if (u < 3.0f) {
            float k = u / 3.0f;
            float a = k * TAU;
            return V2(ax + std::cos(a) * rr, ay + std::sin(a) * rr * 0.9f);
        }
        if (u < 3.9f) {
            float k = (u - 3.0f) / 0.9f;
            float ex = (k < 0.5f) ? -0.55f : 0.55f;
            float kk = (k < 0.5f) ? k * 2.0f : (1.0f - k) * 2.0f;
            return V2(ax + ex * rr + facing * 0.0f, ay - rr * (0.85f - 0.5f * kk));
        }
        if (u < 5.4f) {
            float k = (u - 3.9f) / 1.5f;
            return V2(ax - facing * rr * (0.9f + 0.5f * k), ay + rr * (0.2f - 0.9f * std::sin(k * 3.0f)));
        }
        return circle(5.4f, 6.6f, 0.0f, 0.0f, rr * 0.12f, 1.0f);
    }
    if (shape == 2) {              // pig: body, snout, curly tail
        if (u < 2.6f) {
            float k = u / 2.6f;
            float a = k * TAU;
            return V2(ax + std::cos(a) * rr * 1.2f, ay + std::sin(a) * rr * 0.95f);
        }
        if (u < 3.6f) {
            float k = (u - 2.6f) / 1.0f;
            float a = k * TAU;
            return V2(ax + facing * rr * 1.15f + std::cos(a) * rr * 0.3f,
                      ay + rr * 0.1f + std::sin(a) * rr * 0.24f);
        }
        if (u < 4.8f) {
            float k = (u - 3.6f) / 1.2f;
            return V2(ax - facing * rr * (1.15f + 0.25f * std::cos(k * 6.0f)),
                      ay - rr * (0.5f - 0.5f * k) + std::sin(k * 6.0f) * rr * 0.16f);
        }
        return circle(4.8f, 5.8f, facing * rr * 1.05f, -rr * 0.55f, rr * 0.1f, 1.0f);
    }
    if (shape == 3) {              // flower: stem, petals, leaves
        if (u < 1.6f) {
            float k = u / 1.6f;
            return V2(ax + std::sin(k * 1.4f) * rr * 0.15f, ay + rr * 0.9f - k * rr * 1.7f);
        }
        if (u < 4.4f) {
            float k = (u - 1.6f) / 2.8f;
            float a = k * TAU * 2.0f;
            float rad = rr * (0.42f + 0.34f * std::fabs(std::cos(k * TAU * 3.0f)));
            return V2(ax + std::cos(a) * rad, ay - rr * 0.95f + std::sin(a) * rad * 0.9f);
        }
        float k = ClampF((u - 4.4f) / 1.2f, 0.0f, 1.0f);
        return V2(ax + (k < 0.5f ? -1.0f : 1.0f) * rr * 0.5f * (1.0f - std::fabs(k * 2.0f - 1.0f)),
                  ay + rr * 0.1f);
    }
    // smiley with a moustache (the wall prank)
    if (u < 3.2f) {
        float k = u / 3.2f;
        float a = k * TAU;
        return V2(ax + std::cos(a) * rr, ay + std::sin(a) * rr * 0.95f);
    }
    if (u < 4.0f) {
        float k = (u - 3.2f) / 0.8f;
        return V2(ax + (k - 0.5f) * rr * 1.1f, ay - rr * 0.18f + std::sin(k * 3.14f) * rr * 0.12f);
    }
    float k = (u - 4.0f) / 1.6f;
    return V2(ax + std::cos(k * 3.14f + 3.14f) * rr * 0.75f,
              ay + rr * 0.34f + std::sin(k * 6.28f) * rr * 0.22f);
}

// held things: mug, magnifier, extinguisher, ladle, tape roll, marker
static void DrawCarriedItem(Graphics& g, const Stickman& s, float sc, float mulf) {
    Vec2 h = s.pose.handR;
    auto A = [&](int a) { return (BYTE)ClampF((float)a * mulf, 0.0f, 255.0f); };
    switch (s.carry) {
        case 1: {   // tea mug on a little tray
            SolidBrush mug(Color(A(245), 246, 248, 250));
            Pen out(Color(A(200), 120, 128, 140), 1.2f * sc);
            float mx = h.x, my = h.y - 2.0f * sc;
            g.FillRectangle(&mug, mx - 5.0f * sc, my - 8.0f * sc, 10.0f * sc, 9.0f * sc);
            g.DrawRectangle(&out, mx - 5.0f * sc, my - 8.0f * sc, 10.0f * sc, 9.0f * sc);
            Pen handle(Color(A(220), 120, 128, 140), 1.6f * sc);
            g.DrawArc(&handle, mx + 3.4f * sc, my - 6.6f * sc, 6.0f * sc, 6.0f * sc, -80.0f, 200.0f);
            SolidBrush tea(Color(A(230), 190, 120, 40));
            g.FillEllipse(&tea, mx - 4.2f * sc, my - 8.4f * sc, 8.4f * sc, 2.6f * sc);
            break;
        }
        case 3: {   // magnifier over one eye
            Vec2 e = s.pose.head;
            Pen rim2(Color(A(245), 90, 96, 106), 2.6f * sc);
            SolidBrush lens(Color(A(90), 190, 235, 255));
            g.FillEllipse(&lens, e.x - 2.0f * sc, e.y - 2.0f * sc, 11.0f * sc, 11.0f * sc);
            g.DrawEllipse(&rim2, e.x - 2.0f * sc, e.y - 2.0f * sc, 11.0f * sc, 11.0f * sc);
            g.DrawLine(&rim2, e.x + 8.0f * sc, e.y + 7.0f * sc, h.x, h.y);
            break;
        }
        case 4: {   // fire extinguisher
            SolidBrush red(Color(A(245), 226, 58, 50));
            float bx = h.x - 4.0f * sc, by = h.y - 16.0f * sc;
            FillRoundRect(g, red, bx, by, 11.0f * sc, 20.0f * sc, 3.0f * sc);
            Pen nozzle(Color(A(240), 40, 42, 48), 2.4f * sc);
            nozzle.SetStartCap(LineCapRound);
            g.DrawLine(&nozzle, bx + 9.0f * sc, by + 3.0f * sc, bx + 18.0f * sc, by + 8.0f * sc);
            SolidBrush band(Color(A(235), 240, 236, 210));
            g.FillRectangle(&band, bx + 2.0f * sc, by + 8.0f * sc, 7.0f * sc, 4.0f * sc);
            break;
        }
        case 5: {   // cooking ladle reaching into the pot
            WorldProp* pot = FindProp(PR_CAULDRON, s.id);
            Pen ladle(Color(A(245), 128, 96, 64), 2.4f * sc);
            ladle.SetStartCap(LineCapRound);
            if (pot) {
                g.DrawLine(&ladle, h.x, h.y, pot->x, pot->y - 20.0f * sc);
                SolidBrush cup(Color(A(245), 150, 112, 76));
                g.FillEllipse(&cup, pot->x - 4.0f * sc, pot->y - 24.0f * sc, 8.0f * sc, 5.0f * sc);
            } else {
                g.DrawLine(&ladle, h.x, h.y, h.x + s.facing * 14.0f * sc, h.y + 8.0f * sc);
            }
            break;
        }
        case 6: {   // tape roll for the "cracked screen" fix
            SolidBrush roll(Color(A(245), 236, 230, 196));
            Pen hole(Color(A(220), 150, 140, 110), 1.4f * sc);
            g.FillEllipse(&roll, h.x - 5.0f * sc, h.y - 5.0f * sc, 10.0f * sc, 10.0f * sc);
            g.DrawEllipse(&hole, h.x - 2.0f * sc, h.y - 2.0f * sc, 4.0f * sc, 4.0f * sc);
            break;
        }
        case 7: {   // marker for the moustache prank
            Pen mark(Color(A(245), 40, 40, 46), 3.0f * sc);
            mark.SetStartCap(LineCapRound);
            g.DrawLine(&mark, h.x - s.facing * 3.0f * sc, h.y - 9.0f * sc, h.x, h.y + 4.0f * sc);
            break;
        }
        case 8: {   // a drawn folder carried to you
            SolidBrush back(Color(A(245), 246, 190, 60));
            SolidBrush front(Color(A(235), 252, 214, 96));
            float fx = h.x, fy = h.y + 4.0f * sc;
            g.FillRectangle(&back, fx - 10.0f * sc, fy - 9.0f * sc, 20.0f * sc, 9.0f * sc);
            g.FillRectangle(&front, fx - 11.0f * sc, fy - 6.0f * sc, 22.0f * sc, 6.0f * sc);
            break;
        }
        case 9: {   // the power plug, cable trailing behind
            SolidBrush plug(Color(A(245), 60, 66, 78));
            float px = h.x, py = h.y;
            g.FillRectangle(&plug, px - 4.0f * sc, py - 5.0f * sc, 9.0f * sc, 10.0f * sc);
            Pen pin(Color(A(240), 200, 204, 212), 1.6f * sc);
            g.DrawLine(&pin, px + 5.0f * sc, py - 2.4f * sc, px + 9.0f * sc, py - 2.4f * sc);
            g.DrawLine(&pin, px + 5.0f * sc, py + 2.4f * sc, px + 9.0f * sc, py + 2.4f * sc);
            Pen cable(Color(A(230), 44, 48, 56), 2.2f * sc);
            cable.SetStartCap(LineCapRound);
            GraphicsPath path;
            path.AddBezier(s.pose.handL.x, s.pose.handL.y,
                           s.pose.handL.x - s.facing * 22.0f * sc, s.pose.handL.y + 22.0f * sc,
                           s.x - s.facing * 34.0f * sc, s.y - 6.0f * sc,
                           s.x - s.facing * 44.0f * sc, s.y + 2.0f * sc);
            g.DrawPath(&cable, &path);
            break;
        }
        case 10: {  // a tiny umbrella
            float ux = h.x, uy = h.y - 16.0f * sc;
            Pen shaft(Color(A(245), 92, 78, 66), 2.0f * sc);
            shaft.SetStartCap(LineCapRound);
            g.DrawLine(&shaft, ux, uy - 8.0f * sc, ux, uy + 18.0f * sc);
            SolidBrush canopy(Color(A(240), 88, 156, 236));
            g.FillPie(&canopy, ux - 17.0f * sc, uy - 15.0f * sc, 34.0f * sc, 26.0f * sc, 180.0f, 180.0f);
            Pen ribs(Color(A(200), 250, 252, 255), 1.0f * sc);
            for (int i = -1; i <= 1; ++i)
                g.DrawLine(&ribs, ux, uy - 13.5f * sc, ux + i * 12.0f * sc, uy - 1.0f * sc);
            break;
        }
        case 11: {  // a hammer for the "crack" gag
            Pen handle(Color(A(245), 150, 104, 60), 3.0f * sc);
            handle.SetStartCap(LineCapRound);
            g.DrawLine(&handle, h.x, h.y, h.x + s.facing * 13.0f * sc, h.y - 15.0f * sc);
            SolidBrush head(Color(A(245), 96, 102, 114));
            g.FillRectangle(&head, h.x + s.facing * 8.0f * sc, h.y - 21.0f * sc, 13.0f * sc, 7.0f * sc);
            break;
        }
        case 12: {  // a pink eraser
            SolidBrush rub(Color(A(240), 244, 130, 150));
            SolidBrush sleeve(Color(A(240), 96, 130, 200));
            g.FillRectangle(&rub, h.x - 6.0f * sc, h.y - 4.0f * sc, 12.0f * sc, 7.0f * sc);
            g.FillRectangle(&sleeve, h.x - 1.0f * sc, h.y - 4.0f * sc, 5.0f * sc, 7.0f * sc);
            break;
        }
        case 13: {  // the stolen cursor: a drawn copy of the arrow, never the real one
            Vec2 c = g_app.ghostCursor;
            SolidBrush fill(Color(A(250), 250, 250, 250));
            Pen out(Color(A(240), 30, 32, 38), 1.6f * sc);
            PointF arw[7] = { {c.x, c.y}, {c.x, c.y + 16.0f * sc}, {c.x + 3.6f * sc, c.y + 12.6f * sc},
                             {c.x + 6.6f * sc, c.y + 18.6f * sc}, {c.x + 9.2f * sc, c.y + 17.4f * sc},
                             {c.x + 6.2f * sc, c.y + 11.4f * sc}, {c.x + 11.0f * sc, c.y + 11.0f * sc} };
            g.FillPolygon(&fill, arw, 7);
            g.DrawPolygon(&out, arw, 7);
            break;
        }
        default: break;
    }
}

// The notepad they hold up: characters appear one by one, as if typed.
static void DrawSpeech(Graphics& g, const Stickman& s, float sc) {
    if (s.speech[0] == 0) return;
    float a = ClampF(s.speechT / 0.25f, 0.0f, 1.0f) *
              ClampF((7.0f - s.speechT) / 0.6f, 0.0f, 1.0f);
    if (a < 0.02f) return;
    int total = (int)wcslen(s.speech);
    int shown = (int)ClampF(s.speechSeen, 1.0f, (float)total);
    wchar_t buf[164];
    for (int i = 0; i < shown && i < 159; ++i) buf[i] = s.speech[i];
    buf[shown > 159 ? 159 : shown] = 0;
    Vec2 h = s.pose.head;
    float wpx = 34.0f * sc + 8.2f * sc * (float)shown;
    wpx = std::min(wpx, 540.0f * sc);
    float bx = ClampF(h.x - wpx * 0.5f, 4.0f, (float)g_app.width - wpx - 4.0f);
    float by = h.y - 54.0f * sc - 30.0f * sc;
    by = std::max(by, 6.0f);
    BYTE aa = (BYTE)(245 * a);
    SolidBrush pad(Color(aa, 252, 250, 244));
    Pen edge(Color(aa, 120, 116, 108), 1.6f * sc);
    FillRoundRect(g, pad, bx, by, wpx, 30.0f * sc, 6.0f * sc);
    StrokeRoundRectF(g, edge, bx, by, wpx, 30.0f * sc, 6.0f * sc);
    // little tail pointing at the head
    SolidBrush tail(Color(aa, 252, 250, 244));
    PointF tl[3] = { {h.x - 4.0f * sc, by + 30.0f * sc}, {h.x + 4.0f * sc, by + 30.0f * sc},
                     {h.x, by + 37.0f * sc} };
    g.FillPolygon(&tail, tl, 3);
    FontFamily fam(L"Segoe UI");
    Font font(&fam, 13.5f * sc, FontStyleBold, UnitPixel);
    SolidBrush txt(Color(aa, 46, 44, 40));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(buf, shown, &font, RectF(bx, by, wpx, 22.0f * sc), &sf, &txt);
    if (shown < total && ((int)(s.speechT * 6.0f) % 2) == 0) {
        SolidBrush caret(Color(aa, 200, 90, 60));
        g.FillRectangle(&caret, bx + wpx * 0.5f + 6.1f * (float)shown * 0.5f, by + 5.0f * sc,
                        1.6f * sc, 12.0f * sc);
    }
}

// face details: closed happy eyes, a yawn, soot, the late-night cap
static void DrawFaceExtras(Graphics& g, const Stickman& s, float sc, float mulf) {
    Vec2 hd = s.pose.head;
    auto A = [&](int a) { return (BYTE)ClampF((float)a * mulf, 0.0f, 255.0f); };
    float f = (float)s.facing;
    bool blind = (s.act == Act::PETTED || s.pet > 0.25f || s.act == Act::LAUGH || s.act == Act::COOK);
    if (blind || s.act == Act::YAWN || s.act == Act::NAP) {
        Pen eye(Color(A(220), 30, 30, 36), 1.5f * sc);
        eye.SetStartCap(LineCapRound);
        eye.SetEndCap(LineCapRound);
        for (int i = -1; i <= 1; i += 2) {
            float ex = hd.x + i * 3.2f * sc + f * 1.4f * sc;
            float ey = hd.y - 1.0f * sc;
            g.DrawArc(&eye, ex - 2.2f * sc, ey - 2.0f * sc, 4.4f * sc, 3.4f * sc, 20.0f, 140.0f);
        }
    }
    if (s.act == Act::YAWN) {
        SolidBrush mouth(Color(A(210), 60, 26, 30));
        g.FillEllipse(&mouth, hd.x + f * 2.4f * sc - 2.4f * sc, hd.y + 3.0f * sc, 4.8f * sc, 5.4f * sc);
    }
    if (s.soot > 0.05f) {
        SolidBrush smudge(Color(A((int)(190 * s.soot)), 44, 42, 48));
        g.FillEllipse(&smudge, hd.x - 6.0f * sc, hd.y - 3.0f * sc, 5.0f * sc, 3.6f * sc);
        g.FillEllipse(&smudge, hd.x + 1.0f * sc, hd.y + 2.0f * sc, 4.4f * sc, 3.0f * sc);
    }
    if (g_app.nightMode && s.act != Act::GRABBED) {
        SolidBrush cap(Color(A(245), 86, 106, 200));
        float hr = kProp.headR * sc;
        PointF tri[3] = { {hd.x - hr * 0.85f, hd.y - hr * 0.72f},
                          {hd.x + hr * 0.55f, hd.y - hr * 0.9f},
                          {hd.x + f * hr * 1.5f, hd.y - hr * 2.0f} };
        g.FillPolygon(&cap, tri, 3);
        SolidBrush bob(Color(A(245), 240, 246, 252));
        g.FillEllipse(&bob, tri[2].X - 2.4f * sc, tri[2].Y - 2.4f * sc, 4.8f * sc, 4.8f * sc);
    }
}

static void RenderFrame() {
    if (!g_app.memDC || !g_app.pixels) return;

    if (g_app.hasPrevDirty) ClearRectInBuffer(g_app.prevDirty);

    RECT currDirty = {0, 0, 0, 0};
    bool hasCurr = false;
    for (const auto& s : g_app.stickmen) {
        ExpandRect(currDirty, (int)s.pose.head.x,  (int)s.pose.head.y,  70, hasCurr);
        ExpandRect(currDirty, (int)s.pose.footL.x, (int)s.pose.footL.y, 60, hasCurr);
        ExpandRect(currDirty, (int)s.pose.footR.x, (int)s.pose.footR.y, 60, hasCurr);
        ExpandRect(currDirty, (int)s.x, (int)(s.y - 30.0f * g_app.scale), 120, hasCurr);
        for (const auto& d : s.doodle) ExpandRect(currDirty, (int)d.x, (int)d.y, 8, hasCurr);
        if (s.speech[0]) {
            float sc = g_app.scale;
            int total = (int)wcslen(s.speech);
            float wpx = std::min(34.0f * sc + 8.2f * sc * (float)total, 540.0f * sc);
            float bx = ClampF(s.pose.head.x - wpx * 0.5f, 4.0f, (float)g_app.width - wpx - 4.0f);
            float by = std::max(s.pose.head.y - 54.0f * sc - 30.0f * sc, 6.0f);
            ExpandRect(currDirty, (int)(bx + wpx * 0.5f), (int)(by + 18.0f * sc), (int)(wpx * 0.5f + 24.0f * sc), hasCurr);
        }
    }
    for (const auto& o : g_app.stickmen) {
        if (o.carry == 13) ExpandRect(currDirty, (int)g_app.ghostCursor.x, (int)g_app.ghostCursor.y, 40, hasCurr);
        break;
    }
    for (const auto& p : g_app.particles) ExpandRect(currDirty, (int)p.x, (int)p.y, (int)p.size + 16, hasCurr);
    for (const auto& b : g_app.balls) ExpandRect(currDirty, (int)b.x, (int)b.y, (int)b.r + 16, hasCurr);
    for (const auto& sn : g_app.snacks) ExpandRect(currDirty, (int)sn.x, (int)sn.y, 34, hasCurr);
    if (g_app.couchOn) {
        ExpandRect(currDirty, (int)g_app.couchX, (int)(g_app.couchY - 22.0f * g_app.scale),
                   (int)(g_app.couchW * 0.5f + 46.0f), hasCurr);
    }
    for (const auto& w : g_app.props) {
        if (!w.alive) continue;
        if (w.kind == PR_CRACK) {
            ExpandRect(currDirty, (int)w.x, (int)w.y, (int)(64.0f + 34.0f * g_app.scale), hasCurr);
        } else if (w.kind == PR_FEED || w.kind == PR_BOX || w.kind == PR_ICON ||
                   w.kind == PR_FOLDER || w.kind == PR_TAB) {
            ExpandRect(currDirty, (int)w.x, (int)w.y, (int)(24.0f + 16.0f * g_app.scale), hasCurr);
        } else {
            ExpandRect(currDirty, (int)w.x, (int)(w.y - 30.0f * g_app.scale),
                       (int)(56.0f + 22.0f * g_app.scale), hasCurr);
        }
    }
    if (g_app.heatGlow > 0.01f) ExpandRect(currDirty, g_app.width / 2, 6, g_app.width / 2 + 6, hasCurr);
    for (const auto& s2 : g_app.stickmen) {
        if (s2.inVideo && g_app.mediaWinValid) {
            RECT mr = g_app.mediaWinRect;
            ExpandRect(currDirty, (mr.left + mr.right) / 2, (mr.top + mr.bottom) / 2,
                       (mr.right - mr.left) / 2 + 6, hasCurr);
            ClampRectToScreen(currDirty);
        }
    }
    if (g_app.deskOwnerId != -1) {
        if (const Stickman* s = FindStickmanById(g_app.deskOwnerId)) {
            ExpandRect(currDirty, (int)(s->pose.handR.x), (int)(s->y - 26.0f * g_app.scale), 90, hasCurr);
        }
    }
    ClampRectToScreen(currDirty);

    if (hasCurr && currDirty.right > currDirty.left && currDirty.bottom > currDirty.top) {
        Bitmap canvas(g_app.width, g_app.height, g_app.width * 4,
                      PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(g_app.pixels));
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetCompositingMode(CompositingModeSourceOver);
        g.SetClip(Rect(currDirty.left, currDirty.top,
                       currDirty.right - currDirty.left,
                       currDirty.bottom - currDirty.top));

        float sc = g_app.scale;

        if (g_app.heatGlow > 0.01f) {
            BYTE ha = (BYTE)(90.0f * g_app.heatGlow);
            LinearGradientBrush gr(PointF(0, 0), PointF(0, 34.0f * sc),
                                   Color(ha, 255, 92, 40), Color(0, 255, 160, 60));
            g.FillRectangle(&gr, 0.0f, 0.0f, (REAL)g_app.width, 34.0f * sc);
        }

        if (g_app.couchOn) DrawCouch(g);
        for (const auto& sn : g_app.snacks) DrawSnack(g, sn);
        for (const auto& b : g_app.balls) DrawBallProp(g, b);
        for (const auto& w : g_app.props) {
            if (!w.alive) continue;
            if (w.kind == PR_CRACK || w.kind == PR_SIGN || w.kind == PR_FEED) continue;
            DrawWorldProp(g, w, sc);
        }

        for (const auto& s : g_app.stickmen) {
            if (s.grounded && s.act != Act::GRABBED && s.act != Act::HANG_CURSOR &&
                !s.inVideo && !ActIsSeated(s.act)) {
                SolidBrush shadowBrush(Color(60, 0, 0, 0));
                g.FillEllipse(&shadowBrush, s.x - 15.0f * sc, s.y - 3.0f * sc, 30.0f * sc, 6.0f * sc);
            }
            if (s.act == Act::CAMPFIRE) DrawCampfire(g, s);
            if (s.act == Act::SKETCH || !s.doodle.empty()) DrawPencilAndDoodle(g, s);
            if (s.act == Act::BUILD) DrawBlocks(g, s);
            if (s.act == Act::MINI_LAPTOP) DrawMiniDesk(g, s);
        }

        // ---- stickmen: far limbs, torso+head, near limbs ----
        for (const auto& s : g_app.stickmen) {
            const Pose& p = s.pose;
            GraphicsState st = 0;
            bool clipped = false;
            const float mulf = s.inVideo ? 0.5f : (s.hidden ? 0.38f : 1.0f);
            if (s.inVideo && g_app.mediaWinValid) {
                st = g.Save();
                clipped = true;
                RECT mr = g_app.mediaWinRect;
                g.IntersectClip(Rect(mr.left, mr.top, mr.right - mr.left, mr.bottom - mr.top));
            }
            auto A = [&](int a) { return (BYTE)ClampF((float)a * mulf, 0.0f, 255.0f); };
            Color haloCol = (s.type == CharType::TCO_BLACK)
                ? Color(A(150), 255, 110, 20) : Color(A(95), 12, 14, 20);
            Pen haloPen(haloCol, 8.0f * sc);
            haloPen.SetStartCap(LineCapRound);
            haloPen.SetEndCap(LineCapRound);
            haloPen.SetLineJoin(LineJoinRound);

            auto limb = [&](Pen& pen, Vec2 a, Vec2 b, Vec2 c) {
                PointF pts[3] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}};
                g.DrawLines(&pen, pts, 3);
            };
            limb(haloPen, p.neck, p.elbowL, p.handL);
            limb(haloPen, p.neck, p.elbowR, p.handR);
            limb(haloPen, p.hip, p.kneeL, p.footL);
            limb(haloPen, p.hip, p.kneeR, p.footR);
            g.DrawLine(&haloPen, p.neck.x, p.neck.y, p.hip.x, p.hip.y);

            BYTE rr = s.r, gg = s.g, bb = s.b;
            Pen farPen(Color(A(215), (BYTE)(rr * 0.86f), (BYTE)(gg * 0.86f), (BYTE)(bb * 0.86f)), 4.1f * sc);
            farPen.SetStartCap(LineCapRound);
            farPen.SetEndCap(LineCapRound);
            farPen.SetLineJoin(LineJoinRound);
            limb(farPen, p.neck, p.elbowL, p.handL);
            limb(farPen, p.hip, p.kneeL, p.footL);

            Pen bodyPen(Color(A(255), rr, gg, bb), 4.6f * sc);
            bodyPen.SetStartCap(LineCapRound);
            bodyPen.SetEndCap(LineCapRound);
            bodyPen.SetLineJoin(LineJoinRound);
            g.DrawLine(&bodyPen, p.neck.x, p.neck.y, p.hip.x, p.hip.y);

            Pen nearPen(Color(A(255), rr, gg, bb), 4.6f * sc);
            nearPen.SetStartCap(LineCapRound);
            nearPen.SetEndCap(LineCapRound);
            nearPen.SetLineJoin(LineJoinRound);
            limb(nearPen, p.neck, p.elbowR, p.handR);
            limb(nearPen, p.hip, p.kneeR, p.footR);

            float hr = kProp.headR * sc;
            if (s.hollowHead) {
                Pen headHalo(haloCol, 7.2f * sc);
                g.DrawEllipse(&headHalo, p.head.x - hr, p.head.y - hr, hr * 2.0f, hr * 2.0f);
                Pen headPen(Color(A(255), rr, gg, bb), 4.6f * sc);
                g.DrawEllipse(&headPen, p.head.x - hr, p.head.y - hr, hr * 2.0f, hr * 2.0f);
            } else {
                SolidBrush headHaloBrush(haloCol);
                g.FillEllipse(&headHaloBrush, p.head.x - hr - 1.4f * sc, p.head.y - hr - 1.4f * sc,
                              (hr + 1.4f * sc) * 2.0f, (hr + 1.4f * sc) * 2.0f);
                SolidBrush headBrush(Color(A(255), rr, gg, bb));
                g.FillEllipse(&headBrush, p.head.x - hr, p.head.y - hr, hr * 2.0f, hr * 2.0f);
            }

            DrawFaceExtras(g, s, sc, mulf);

            // held / worn props
            if (s.hasSword) {
                bool diamond = (s.type == CharType::BLUE || s.type == CharType::TSC_ORANGE);
                if (s.act == Act::SPARRING || s.act == Act::HANDSHAKE) {  // motion trails
                    DrawSword(g, p.handR, p.elbowR, diamond, sc, A(50), -0.44f);
                    DrawSword(g, p.handR, p.elbowR, diamond, sc, A(95), -0.21f);
                }
                DrawSword(g, p.handR, p.elbowR, diamond, sc, A(255), 0.0f);
            }
            if (s.carry) DrawCarriedItem(g, s, sc, mulf);
            if (s.act == Act::BUILD && s.blocks >= 0) {
                float sz = 13.0f * sc;
                SolidBrush br(Color(245, 96, 210, 120));
                g.FillRectangle(&br, p.handR.x - sz * 0.5f, p.handR.y - sz * 0.5f, sz, sz);
                Pen pk(Color(150, 30, 30, 40), 1.2f * sc);
                g.DrawRectangle(&pk, p.handR.x - sz * 0.5f, p.handR.y - sz * 0.5f, sz, sz);
            }
            if (s.act == Act::MUSIC) DrawGuitar(g, s);
            if (s.act == Act::FISH) DrawFishing(g, s);
            if (s.act == Act::READ) DrawBook(g, s);
            if (s.act == Act::EAT_SNACK) {
                float k = 7.0f * sc;
                Vec2 h = p.handR;
                PointF tri[3] = {{h.x - k, h.y - k}, {h.x + k, h.y - k}, {h.x, h.y + k}};
                SolidBrush cheese(Color(245, 250, 202, 78));
                g.FillPolygon(&cheese, tri, 3);
                Pen crust(Color(245, 224, 158, 74), 2.6f * sc);
                g.DrawLine(&crust, tri[0].X, tri[0].Y, tri[1].X, tri[1].Y);
            }
            // fire hands for The Chosen One
            if (s.type == CharType::TCO_BLACK && (s.act == Act::AIRBORNE || s.act == Act::CAMPFIRE)) {
                SolidBrush flame(Color(A(180), 255, 150, 40));
                g.FillEllipse(&flame, p.handL.x - 5.0f * sc, p.handL.y - 5.0f * sc, 10.0f * sc, 10.0f * sc);
                g.FillEllipse(&flame, p.handR.x - 5.0f * sc, p.handR.y - 5.0f * sc, 10.0f * sc, 10.0f * sc);
            }
            if (clipped) g.Restore(st);
        }

        // foreground props: banner, the "pressed" player button, the fake crack
        for (const auto& w : g_app.props) {
            if (!w.alive) continue;
            if (w.kind != PR_CRACK && w.kind != PR_SIGN && w.kind != PR_FEED) continue;
            DrawWorldProp(g, w, sc);
        }

        // v4: hand-written notes (they type on their own notepad, never in your windows)
        for (const auto& sn : g_app.stickmen) DrawSpeech(g, sn, sc);

        // ---- particles ----
        for (const auto& pt : g_app.particles) {
            float alphaRatio = ClampF(pt.life / pt.maxLife, 0.0f, 1.0f);
            BYTE a = (BYTE)(alphaRatio * 235.0f);
            if (pt.kind == 1) {
                Pen ringPen(Color(a, pt.r, pt.g, pt.b), 2.4f);
                g.DrawEllipse(&ringPen, pt.x - pt.size, pt.y - pt.size, pt.size * 2.0f, pt.size * 2.0f);
            } else if (pt.kind == 3) {
                SolidBrush nb(Color(a, pt.r, pt.g, pt.b));
                Pen np(Color(a, pt.r, pt.g, pt.b), 2.0f);
                g.FillEllipse(&nb, pt.x - 4.0f, pt.y, 7.0f, 5.5f);
                g.DrawLine(&np, pt.x + 2.5f, pt.y + 2.0f, pt.x + 2.5f, pt.y - 10.0f);
                g.DrawLine(&np, pt.x + 2.5f, pt.y - 10.0f, pt.x + 8.5f, pt.y - 7.0f);
            } else if (pt.kind == 5) {
                // code spark: tiny "</>"
                Pen cp(Color(a, pt.r, pt.g, pt.b), 1.5f);
                cp.SetStartCap(LineCapRound);
                cp.SetEndCap(LineCapRound);
                float k = ClampF(pt.size, 2.0f, 5.0f);
                g.DrawLine(&cp, pt.x - k, pt.y - k * 0.6f, pt.x - k * 1.8f, pt.y + k * 0.6f);
                g.DrawLine(&cp, pt.x - k * 0.6f, pt.y + k * 0.7f, pt.x + k * 0.2f, pt.y - k * 0.7f);
                g.DrawLine(&cp, pt.x + k * 0.6f, pt.y - k * 0.6f, pt.x + k * 1.4f, pt.y + k * 0.6f);
            } else if (pt.kind == 6) {
                // Zzz
                Pen zp(Color(a, pt.r, pt.g, pt.b), 1.8f);
                zp.SetStartCap(LineCapRound);
                zp.SetEndCap(LineCapRound);
                float k = ClampF(pt.size, 3.0f, 7.0f);
                g.DrawLine(&zp, pt.x - k, pt.y - k, pt.x + k, pt.y - k);
                g.DrawLine(&zp, pt.x + k, pt.y - k, pt.x - k, pt.y + k);
                g.DrawLine(&zp, pt.x - k, pt.y + k, pt.x + k, pt.y + k);
            } else if (pt.kind == 8) {
                Pen sp(Color(a, pt.r, pt.g, pt.b), 1.6f);
                g.DrawLine(&sp, pt.x - 3.0f, pt.y, pt.x + 3.0f, pt.y);
            } else if (pt.kind == 9) {
                // rain streak
                Pen rp(Color((BYTE)(a * 0.8f), pt.r, pt.g, pt.b), 1.5f);
                rp.SetStartCap(LineCapRound);
                g.DrawLine(&rp, pt.x, pt.y, pt.x - 1.2f, pt.y + 9.0f);
            } else {
                SolidBrush pb(Color(a, pt.r, pt.g, pt.b));
                float r = pt.size * (pt.kind == 4 ? alphaRatio : ClampF(0.4f + alphaRatio * 0.6f, 0.0f, 1.0f));
                g.FillEllipse(&pb, pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
            }
        }
    }

    RECT dirtyUnion = currDirty;
    if (g_app.hasPrevDirty && hasCurr) UnionRect(&dirtyUnion, &g_app.prevDirty, &currDirty);
    else if (g_app.hasPrevDirty) dirtyUnion = g_app.prevDirty;

    POINT ptDst = {g_app.originX, g_app.originY};
    SIZE  szWin = {g_app.width, g_app.height};
    POINT ptSrc = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.BlendFlags          = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    UPDATELAYEREDWINDOWINFO ulwi = {};
    ulwi.cbSize   = sizeof(ulwi);
    ulwi.hdcDst   = g_app.screenDC;
    ulwi.pptDst   = &ptDst;
    ulwi.psize    = &szWin;
    ulwi.hdcSrc   = g_app.memDC;
    ulwi.pptSrc   = &ptSrc;
    ulwi.crKey    = 0;
    ulwi.pblend   = &blend;
    ulwi.dwFlags  = ULW_ALPHA;
    ulwi.prcDirty = (hasCurr || g_app.hasPrevDirty) ? &dirtyUnion : nullptr;

    UpdateLayeredWindowIndirect(g_app.hwnd, &ulwi);

    g_app.prevDirty = currDirty;
    g_app.hasPrevDirty = hasCurr;
}

// ============================================================================
// Interaction: hit testing, tray menu, window procedure
// ============================================================================
static Stickman* HitTestStickman(float x, float y) {
    float sc = g_app.scale;
    for (int i = (int)g_app.stickmen.size() - 1; i >= 0; --i) {
        Stickman& s = g_app.stickmen[i];
        Vec2 c = LerpV(s.pose.head, s.pose.hip, 0.5f);
        if (std::fabs(x - c.x) < 24.0f * sc && std::fabs(y - c.y) < 40.0f * sc) return &s;
        Vec2 feet = LerpV(s.pose.footL, s.pose.footR, 0.5f);
        if (std::fabs(x - feet.x) < 20.0f * sc && std::fabs(y - feet.y) < 22.0f * sc) return &s;
    }
    return nullptr;
}

// A little "!" bubble so the user knows the stickman noticed the click
// ============================================================================
// v4: they cannot speak, so they write you notes (The Second Coming style)
// ============================================================================
// ============================================================================
// DeepSeek API (async WinHTTP, non-blocking) + mini-dialog (Ctrl+Alt+A)
// ============================================================================
static std::string WToUtf8(const wchar_t* w) {
    if (!w || !w[0]) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return "";
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], n, nullptr, nullptr);
    return out;
}

static std::string JsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (unsigned char c : in) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') {}
        else if (c < 0x20) out += ' ';
        else out += (char)c;
    }
    return out;
}

static void AskDeepSeekAsync(int stickmanId, const std::wstring& userPrompt) {
    if (!g_app.deepseekOn || !g_app.deepseekKey[0]) return;
    if (g_app.aiBusy.exchange(true)) return;

    std::string key(g_app.deepseekKey);
    std::string fg = WToUtf8(g_app.fgTitle);
    std::string prompt = WToUtf8(userPrompt.c_str());
    int batt = g_app.batteryPct;
    bool hot = g_app.heatGlow > 0.5f;
    bool rain = g_app.raining, snow = g_app.snowing;

    std::string persona = "Ты Оранжевый — любопытный лидер, любишь рисовать и приключения.";
    if (Stickman* st = FindStickmanById(stickmanId)) {
        switch (st->type) {
            case CharType::RED:        persona = "Ты Красный — энергичный задира, любишь спорт, экшен и шутки."; break;
            case CharType::BLUE:       persona = "Ты Синий — спокойный умник, любишь кофе, книги и сарказм."; break;
            case CharType::YELLOW:     persona = "Ты Жёлтый — технарь-изобретатель, любишь код, гаджеты и мемы."; break;
            case CharType::GREEN:      persona = "Ты Зелёный — весёлый музыкант и шутник, любишь танцы и розыгрыши."; break;
            default: break;
        }
    }

    std::thread([stickmanId, key, fg, prompt, batt, hot, rain, snow, persona]() {
        std::string ctx = fg.empty() ? "" : ("Открыто окно: " + fg + ". ");
        if (batt >= 0 && batt <= 20) ctx += "Батарея садится (" + std::to_string(batt) + "%). ";
        if (hot) ctx += "Процессор горячий. ";
        if (rain) ctx += "Идёт дождь. ";
        else if (snow) ctx += "Идёт снег. ";

        std::string sys = persona +
            " Отвечай коротко (до 105 символов), дерзко, с подколами и юмором, по-русски, без markdown. "
            "ЗАПРЕЩЕНО говорить 'рабочий стол', 'иконки', 'человечек'. "
            "Если спросят 'где ты', подколи ('тут, в твоём мониторе, слежу за тобой' и т.п.). "
            "Если просят действие, начни с тега: [ACT:WAVE], [ACT:TEA], [ACT:DANCE], [ACT:DUEL], [ACT:NAP], [ACT:BALL], [ACT:THEFT], [ACT:COUCH], [ACT:FOLDER], [ACT:LAUGH], [ACT:CHEER].";

        std::string body =
            "{\"model\":\"deepseek-chat\",\"max_tokens\":70,\"temperature\":0.92,\"messages\":["
            "{\"role\":\"system\",\"content\":\"" + JsonEscape(sys) + "\"},"
            "{\"role\":\"user\",\"content\":\"" + JsonEscape(ctx + prompt) + "\"}"
            "]}";

        std::wstring replyW;
        HINTERNET hSession = WinHttpOpen(L"desktop-stickman/4.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (hConnect) {
                HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", L"/chat/completions", nullptr,
                                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
                if (hReq) {
                    std::wstring hdr = L"Content-Type: application/json\r\nAuthorization: Bearer " +
                                       std::wstring(key.begin(), key.end()) + L"\r\n";
                    if (WinHttpSendRequest(hReq, hdr.c_str(), (DWORD)hdr.size(),
                                           (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) &&
                        WinHttpReceiveResponse(hReq, nullptr)) {
                        std::string resp;
                        DWORD avail = 0;
                        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                            std::string chunk(avail, '\0');
                            DWORD read = 0;
                            if (WinHttpReadData(hReq, &chunk[0], avail, &read)) {
                                chunk.resize(read);
                                resp += chunk;
                            } else break;
                        }
                        size_t pos = resp.find("\"content\":\"");
                        if (pos != std::string::npos) {
                            pos += 11;
                            std::string raw;
                            for (size_t i = pos; i < resp.size(); ++i) {
                                if (resp[i] == '\\' && i + 1 < resp.size()) {
                                    char nx = resp[++i];
                                    if (nx == 'n') raw += ' ';
                                    else raw += nx;
                                } else if (resp[i] == '"') {
                                    break;
                                } else {
                                    raw += resp[i];
                                }
                            }
                            if (!raw.empty()) {
                                int wn = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, nullptr, 0);
                                if (wn > 1) {
                                    replyW.resize(wn - 1);
                                    MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), -1, &replyW[0], wn);
                                }
                            }
                        }
                    }
                    WinHttpCloseHandle(hReq);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }

        if (!replyW.empty()) {
            std::lock_guard<std::mutex> lk(g_app.aiMutex);
            int i = 0;
            for (; i < (int)replyW.size() && i < 159; ++i) g_app.aiReply[i] = replyW[i];
            g_app.aiReply[i] = 0;
            g_app.aiTargetId = stickmanId;
            g_app.aiHasReply = true;
        }
        g_app.aiBusy = false;
    }).detach();
}

static LRESULT CALLBACK AskAIWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hEdit = nullptr;
    switch (msg) {
        case WM_CREATE: {
            hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    12, 12, 300, 26, hwnd, (HMENU)101, nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"\u0421\u043f\u0440\u043e\u0441\u0438\u0442\u044c",
                            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                            320, 12, 82, 26, hwnd, (HMENU)IDOK, nullptr, nullptr);
            SetFocus(hEdit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                wchar_t buf[256] = {};
                GetWindowTextW(hEdit, buf, 255);
                if (buf[0]) {
                    std::wstring q(buf);
                    auto has = [&](const wchar_t* a, const wchar_t* b, const wchar_t* c) {
                        return q.find(a) != std::wstring::npos || q.find(b) != std::wstring::npos || (c && q.find(c) != std::wstring::npos);
                    };
                    CharType want = (CharType)-1;
                    if (has(L"жёлт", L"Жёлт", L"желт") || has(L"Желт", L"yellow", L"Yellow")) want = CharType::YELLOW;
                    else if (has(L"красн", L"Красн", L"red") || has(L"Red", L"RED", nullptr)) want = CharType::RED;
                    else if (has(L"син", L"Син", L"blue") || has(L"Blue", L"BLUE", nullptr)) want = CharType::BLUE;
                    else if (has(L"зел", L"Зел", L"green") || has(L"Green", L"GREEN", nullptr)) want = CharType::GREEN;
                    else if (has(L"оранж", L"Оранж", L"orange") || has(L"Orange", L"ORANGE", nullptr)) want = CharType::TSC_ORANGE;

                    Stickman* who = nullptr;
                    if ((int)want >= 0) {
                        for (auto& m : g_app.stickmen) if (m.type == want) { who = &m; break; }
                    }
                    if (!who) who = PickFreeGuy(0);
                    int id = who ? who->id : 0;
                    if (who) SayText(*who, L"...");
                    AskDeepSeekAsync(id, buf);
                }
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowAskAIDialog(HWND parent) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = AskAIWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"DesktopStickmanAskAI";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        reg = true;
    }
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND dlg = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"DesktopStickmanAskAI",
                               L"desktop-stickman \u2014 DeepSeek",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               (sw - 420) / 2, (sh - 90) / 2, 420, 86,
                               parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetForegroundWindow(dlg);
}

static void SayText(Stickman& s, const wchar_t* text) {
    int i = 0;
    for (; text[i] && i < 159; ++i) s.speech[i] = text[i];
    s.speech[i] = 0;
    s.speechT = 0.0f;
    s.speechSeen = 0.0f;
}

static void SayNote(Stickman& s, int reason) {
    switch (reason) {
        case 0: SayText(s, L"\u041f\u0440\u0438\u0432\u0435\u0442!"); break;                       // hello!
        case 1: SayText(s, L"\u041f\u043e\u0433\u043b\u0430\u0434\u044c \u043c\u0435\u043d\u044f"); break;   // pet me
        case 2: SayText(s, L"\u0412\u043a\u0443\u0441\u043d\u0435\u043d\u044c\u043a\u043e\u0433\u043e?"); break; // treat?
        case 3: SayText(s, L"\u041d\u0435 \u043c\u0435\u0448\u0430\u0439!"); break;                      // don't bother me
        case 4: SayText(s, L"\u041e\u0439, \u0438\u0437\u0432\u0438\u043d\u0438"); break;                // oops, sorry
        case 5: SayText(s, L"\u041f\u043e\u0447\u0438\u043d\u0438\u043b!"); break;                      // fixed it!
        case 6: SayText(s, L"\u0414\u0435\u0440\u0436\u0438"); break;                                // here you go
        case 7: SayText(s, L"\u041f\u043e\u0440\u0430\u0437\u043c\u044f\u0442\u044c\u0441\u044f!"); break; // stretch time!
        case 8: SayText(s, L"\u0421\u043a\u0443\u0447\u043d\u043e..."); break;                        // boring...
        case 9: SayText(s, L"\u041d\u0443 \u043d\u0443 \u043d\u0430"); break;                        // go on, send it
        case 10: SayText(s, L"\u041d\u0435 \u043e\u0442\u043f\u0440\u0430\u0432\u043b\u044f\u0439!!"); break; // don't send!!
        case 11: SayText(s, L"\u041f\u043e\u0431\u0435\u0434\u0430!"); break;                          // victory!
        case 12: SayText(s, L"\u0425\u043e\u0440\u043e\u0448\u0430\u044f \u0434\u0440\u0430\u043a\u0430"); break; // good fight
        case 13: SayText(s, L"\u0417\u0430\u0440\u044f\u0434!!"); break;                              // charge!!
        case 14: SayText(s, L"\u0414\u043e\u0436\u0434\u044c!!"); break;                              // wait!!
        case 15: SayText(s, L"\u0422\u044f\u0436\u0451\u043b\u044b\u0439..."); break;                // heavy...
        case 16: SayText(s, L"\u041a\u0430\u043f\u0443\u0441\u0442! \u042f \u043d\u0435 \u043d\u0430\u0440\u043e\u0447\u043d\u043e"); break; // dropped it! on purpose not
        case 17: SayText(s, L"\u0421\u043f\u0430\u0441\u0438\u0431\u043e!"); break;                     // thanks!
        case 18: SayText(s, L"\u0421\u044b\u0433\u0440\u0430\u0435\u043c?"); break;                     // wanna play?
        case 19: SayText(s, L"\u0413\u043e\u043b! \u041c\u043e\u0439!"); break;                       // goal! mine!
        default: SayText(s, L"\u041f\u0440\u0438\u0432\u0435\u0442!"); break;
    }
    if (g_app.deepseekOn && g_app.aiAutoCool <= 0.0f && !g_app.aiBusy.load()) {
        g_app.aiAutoCool = 60.0f;
        AskDeepSeekAsync(s.id, std::wstring(L"Скажи короткую смешную реакцию по ситуации."));
    }
}

// Stand still and write you a note (no walking, no clicking, no typing for real)
static void StartNoteHere(Stickman& s, int reason) {
    s.act = Act::SPEAK;
    s.actTimer = RandRange(5.0f, 8.0f);
    s.animTime = 0.0f;
    s.blocks = 0;
    s.facing = (g_app.cursor.x >= s.x) ? 1 : -1;
    SayNote(s, reason);
}

// True if someone is holding a note up right now (avoids a crowd of bubbles)
static bool AnyoneSpeaking() {
    for (const auto& o : g_app.stickmen) if (o.speech[0] && o.speechT < 5.0f) return true;
    return false;
}

static void PokeReaction(Stickman& s, float x, float y) {
    s.act = Act::FLINCH;
    s.actTimer = 1.1f;
    s.animTime = 0.0f;
    s.squashVel = -2.6f;
    s.grounded = false;
    s.standingHwnd = nullptr;
    s.vy = -300.0f;
    s.vx = (float)(-s.facing * 160.0f);
    s.facing = (s.x >= x) ? 1 : -1;
    s.doingFlip = (RandInt(3) == 0);
    s.socialCooldown = 1.2f;
    SpawnBurst(x, y, 255, 232, 120, 8, 2);
    SpawnParticle(x, y - 26.0f * g_app.scale, 0, 0, 8.0f, 0.3f, 255, 255, 255, 1);
    // v4: poke them often enough and they poke back
    g_app.pokeCount += 1.0f;
    g_app.mood = ClampF(g_app.mood - 0.04f, 0.0f, 1.0f);
    s.mood = ClampF(s.mood - 0.08f, 0.0f, 1.0f);
    if (g_app.pokeCount >= 3.0f) {
        g_app.pokeCount = 0.0f;
        if (RandInt(2) == 0) {
            s.act = Act::FIGHT_CURSOR;
            s.actTimer = RandRange(4.0f, 7.0f);
            s.animTime = 0.0f;
            s.facing = (x >= s.x) ? 1 : -1;
            SayNote(s, 3);
        } else if (!AnyoneSpeaking()) {
            s.act = Act::SPEAK;
            s.actTimer = RandRange(4.0f, 6.0f);
            SayNote(s, 3);
        }
    }
    for (auto& o : g_app.stickmen) {
        if (&o == &s || o.act == Act::GRABBED) continue;
        if (std::fabs(o.x - s.x) > 240.0f) continue;
        if (RandInt(3) == 0) { o.act = Act::LAUGH; o.actTimer = RandRange(2.6f, 4.6f); o.animTime = 0.0f; }
    }
}

static void SetupTrayIcon(HWND hwnd) {
    g_app.nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_app.nid.hWnd   = hwnd;
    g_app.nid.uID    = 1;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    g_app.nid.uCallbackMessage = WM_TRAYICON;
    g_app.nid.hIcon = LoadIconW(g_app.hInst, MAKEINTRESOURCEW(IDI_APPICON));
    if (!g_app.nid.hIcon) g_app.nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    // "Alan Becker Stickmen v4 - right click for the menu"
    wcscpy_s(g_app.nid.szTip,
        L"desktop-stickman v4 \u2014 \u041F\u041A\u041C \u0434\u043B\u044F \u043C\u0435\u043D\u044E");
    // "They are here! They wave hello. Menu - in the tray."
    wcscpy_s(g_app.nid.szInfoTitle,
        L"\u041E\u043D\u0438 \u0437\u0434\u0435\u0441\u044C! \u041C\u0430\u0445\u0430\u044E\u0442 \u0440\u0443\u043A\u043E\u0439 \u043F\u0440\u0438\u0432\u0435\u0442 :)");
    // "They live on your desktop: they wave, drink tea, work when you type,
    //  and watch videos with you on the couch."
    wcscpy_s(g_app.nid.szInfo,
        L"\u0416\u0438\u0432\u0443\u0442 \u043D\u0430 \u0440\u0430\u0431\u043E\u0447\u0435\u043C \u0441\u0442\u043E\u043B\u0435: \u043C\u0430\u0445\u0430\u044E\u0442, "
        L"\u0440\u0430\u0431\u043E\u0442\u0430\u044E\u0442, \u043A\u043E\u0433\u0434\u0430 \u0432\u044B \u043F\u0435\u0447\u0430\u0442\u0430\u0435\u0442\u0435, "
        L"\u0438 \u0441\u043C\u043E\u0442\u0440\u044F\u0442 \u0432\u0438\u0434\u0435\u043E \u043D\u0430 \u0434\u0438\u0432\u0430\u043D\u0435.");
    g_app.nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_ADD, &g_app.nid);
}

static void RefreshTrayTip() {
    wchar_t* tip = g_app.nid.szTip;
    const wchar_t* mode = g_app.calmMode
        ? L"\u0421\u043F\u043E\u043A\u043E\u0439\u043D\u044B\u0439"      // Calm
        : L"\u0410\u043A\u0442\u0438\u0432\u043D\u044B\u0439";        // Active
    const wchar_t* scene = g_app.couchOn
        ? L" \u00B7 \u0434\u0438\u0432\u0430\u043D"                       // couch
        : (g_app.deskOn ? L" \u00B7 \u043C\u0438\u043D\u0438-\u0441\u0442\u043E\u043B" : L"");
    const wchar_t* scaleName = (g_app.scaleIdx == 0) ? L"S" : (g_app.scaleIdx == 2 ? L"L" : L"M");
    // "Alan Becker Stickmen v4 - <mode><scene> - size - battery - right click"
    wchar_t extra[48] = L"";
    if (g_app.batteryPct >= 0) {
        swprintf(extra, 48, L" \u00B7 %d%%%s", g_app.batteryPct,
               g_app.onBattery ? L"\u26A1" : L"\u26A0");
    }
    swprintf(tip, 128,
        L"desktop-stickman v4 \u2014 %s%s \u2014 %s%s \u2014 \u041F\u041A\u041C",
        mode, scene, scaleName, extra);
    g_app.nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
}

static bool IsAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;
    DWORD type = 0, cb = 0;
    bool ok = (RegQueryValueExW(hKey, L"desktop-stickman", nullptr, &type, nullptr, &cb) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return ok;
}

static void SetAutoStart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_WRITE, &hKey) != ERROR_SUCCESS) return;
    if (enable) {
        wchar_t exePath[MAX_PATH + 4] = {};
        DWORD n = GetModuleFileNameW(nullptr, exePath + 1, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            exePath[0] = L'"';
            exePath[n + 1] = L'"';
            exePath[n + 2] = 0;
            RegSetValueExW(hKey, L"desktop-stickman", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(exePath),
                           (DWORD)((n + 3) * sizeof(wchar_t)));
        }
    } else {
        RegDeleteValueW(hKey, L"desktop-stickman");
    }
    RegCloseKey(hKey);
}

static void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // --- the squad ---
    AppendMenuW(hMenu, MF_STRING, CMD_ADD_STICKMAN,
        L"\u0414\u043E\u0431\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u0438\u043A\u043C\u0435\u043D\u0430 (+1)\tCtrl+Alt+S");   // add stickman
    AppendMenuW(hMenu, MF_STRING, CMD_ADD_TCO,
        L"\u041F\u0440\u0438\u0437\u0432\u0430\u0442\u044C The Chosen One (\u041E\u0433\u043E\u043D\u044C)\tCtrl+Alt+T");
    AppendMenuW(hMenu, MF_STRING, CMD_RESET_SQUAD,
        L"\u041A\u043B\u0430\u0441\u0441\u0438\u0447\u0435\u0441\u043A\u0430\u044F \u043F\u044F\u0442\u0451\u0440\u043A\u0430 (5 \u0448\u0442.)");
    AppendMenuW(hMenu, MF_STRING, CMD_REMOVE_ONE,
        L"\u0423\u0431\u0440\u0430\u0442\u044C \u043E\u0434\u043D\u043E\u0433\u043E (-1)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // --- greetings & social ---
    AppendMenuW(hMenu, MF_STRING, CMD_SAY_HELLO,
        L"\u041F\u043E\u043C\u0430\u0445\u0430\u0442\u044C \u043F\u0440\u0438\u0432\u0435\u0442!\tCtrl+Alt+H");       // wave hello
    AppendMenuW(hMenu, MF_STRING, CMD_CALL_HERE,
        L"\u041F\u043E\u0437\u0432\u0430\u0442\u044C \u0432\u0441\u0435\u0445 \u043A \u043A\u0443\u0440\u0441\u043E\u0440\u0443\tCtrl+Alt+C");
    AppendMenuW(hMenu, MF_STRING, CMD_SPARRING,
        L"\u0421\u0442\u0440\u0430\u0432\u043D\u0438\u0442\u044C \u0434\u0440\u0430\u043A\u0443 (\u0432\u0440\u0443\u0447\u043D\u0443\u044E)\tCtrl+Alt+F");
    AppendMenuW(hMenu, MF_STRING | (g_app.sparAI ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_SPARAI,
        L"\u0420\u0435\u0434\u043A\u0438\u0435 \u0434\u0440\u0430\u043A\u0438 \u0441\u0430\u043C\u0438 \u043F\u043E \u0441\u0435\u0431\u0435");

    // --- toys ---
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, CMD_DROP_PIZZA,
        L"\u0411\u0440\u043E\u0441\u0438\u0442\u044C \u043F\u0438\u0446\u0446\u0443 \u043F\u043E\u0434 \u043A\u0443\u0440\u0441\u043E\u0440\tCtrl+Alt+P");
    AppendMenuW(hMenu, MF_STRING, CMD_SPAWN_BALL,
        L"\u041C\u044F\u0447\u0438\u043A \u0434\u043B\u044F \u043D\u0438\u0445\tCtrl+Alt+B");
    AppendMenuW(hMenu, MF_STRING, CMD_CLEAR_TOYS,
        L"\u0423\u0431\u0440\u0430\u0442\u044C \u043C\u044F\u0447\u0438 \u0438 \u0435\u0434\u0443");

    // --- v3: treats, hand-outs, group scenes ---
    HMENU hTreat = CreatePopupMenu();
    AppendMenuW(hTreat, MF_STRING, CMD_TREAT_PIZZA,  L"\u041f\u0438\u0446\u0446\u0430 \u043f\u043e\u0434 \u043a\u0443\u0440\u0441\u043e\u0440\tCtrl+Alt+P");
    AppendMenuW(hTreat, MF_STRING, CMD_TREAT_CAKE,   L"\u041a\u0443\u0441\u043e\u043a \u0442\u043e\u0440\u0442\u0430");
    AppendMenuW(hTreat, MF_STRING, CMD_TREAT_MELON,  L"\u041a\u043e\u0440\u043a\u0430 \u0430\u0440\u0431\u0443\u0437\u0430");
    AppendMenuW(hTreat, MF_STRING, CMD_TREAT_POTION, L"\u0412\u043e\u043b\u0448\u0435\u0431\u043d\u043e\u0435 \u0437\u0435\u043b\u044c\u0435");
    AppendMenuW(hTreat, MF_STRING, CMD_TREAT_CUBE,   L"\u041a\u0443\u0431\u0438\u043a (\u043a\u0430\u043a \u0432 \u041c\u0430\u0439\u043d\u043a\u0440\u0430\u0444\u0442\u0435)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTreat, L"\u0423\u0433\u043e\u0441\u0442\u0438\u0442\u044c \u0438\u0445");

    HMENU hGive = CreatePopupMenu();
    AppendMenuW(hGive, MF_STRING, CMD_GIVE_SWORD,  L"\u041c\u0435\u0447");
    AppendMenuW(hGive, MF_STRING, CMD_GIVE_PENCIL, L"\u041a\u0430\u0440\u0430\u043d\u0434\u0430\u0448 (\u043f\u0443\u0441\u0442\u044c \u0440\u0438\u0441\u0443\u0435\u0442 \u043f\u0438\u0442\u043e\u043c\u0446\u0430)");
    AppendMenuW(hGive, MF_STRING, CMD_GIVE_GUITAR, L"\u0413\u0438\u0442\u0430\u0440\u0430");
    AppendMenuW(hGive, MF_STRING, CMD_GIVE_NONE,   L"\u041d\u0438\u0447\u0435\u0433\u043e (\u0437\u0430\u0431\u0440\u0430\u0442\u044c)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hGive, L"\u0414\u0430\u0442\u044c \u043f\u0440\u0435\u0434\u043c\u0435\u0442 \u0431\u043b\u0438\u0436\u0430\u0439\u0448\u0435\u043c\u0443");

    AppendMenuW(hMenu, MF_STRING, CMD_PARTY,    L"\u0412\u0435\u0447\u0435\u0440\u0438\u043d\u043a\u0430: \u043a\u0430\u0440\u0442\u044b / \u0448\u0430\u0445\u043c\u0430\u0442\u044b\tCtrl+Alt+Y");
    AppendMenuW(hMenu, MF_STRING, CMD_DUEL,     L"\u041f\u043e\u0435\u0434\u0438\u043d\u043e\u043a: \u0430\u0440\u043c\u0440\u0435\u0441\u0442\u043b\u0438\u043d\u0433 \u0438\u043b\u0438 \u043a\u0430\u043d\u0430\u0442\tCtrl+Alt+D");
    AppendMenuW(hMenu, MF_STRING, CMD_PAT_HEAD, L"\u041f\u043e\u0433\u043b\u0430\u0434\u0438\u0442\u044c \u043f\u043e \u0433\u043e\u043b\u043e\u0432\u0435\tCtrl+Alt+E");
    AppendMenuW(hMenu, MF_STRING, CMD_SNOWMAN,  L"\u041b\u0435\u043f\u0438\u043c \u0441\u043d\u0435\u0433\u043e\u0432\u0438\u043a\u0430\tCtrl+Alt+W");

    // --- v4: pointer games, notes, drawn mischief (nothing real is touched) ---
    HMENU hV4 = CreatePopupMenu();
    AppendMenuW(hV4, MF_STRING, CMD_HIDE_ICON,
        L"\u0421\u043f\u0440\u044f\u0442\u0430\u0442\u044C \u0438\u043A\u043E\u043D\u043A\u0443 \u0438 \u0432\u0435\u0440\u043D\u0443\u0442\u044C\tCtrl+Alt+I");
    AppendMenuW(hV4, MF_STRING, CMD_ICON_KICK,
        L"\u041f\u043D\u0443\u0442\u044C \u043D\u0430\u0440\u0438\u0441\u043E\u0432\u0430\u043D\u043D\u044B\u0439 \u044F\u0440\u043B\u044B\u043A");
    AppendMenuW(hV4, MF_STRING, CMD_TAB_OOPS,
        L"\u0421\u043E\u0440\u0432\u0430\u0442\u044C \u0432\u043A\u043B\u0430\u0434\u043A\u0443 \u0438 \u043F\u043E\u0434\u043A\u043B\u0435\u0438\u0442\u044C");
    AppendMenuW(hV4, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hV4, MF_STRING, CMD_BRING_FOLDER,
        L"\u041F\u0440\u0438\u043D\u0435\u0441\u0442\u0438 \u043F\u0430\u043F\u043A\u0443 \u043A \u043A\u0443\u0440\u0441\u043E\u0440\u0443\tCtrl+Alt+K");
    AppendMenuW(hV4, MF_STRING, CMD_SEND_GUARD,
        L"\u041E\u0445\u0440\u0430\u043D\u044F\u0442\u044C \u043A\u043D\u043E\u043F\u043A\u0443 \u00AB\u041E\u0442\u043F\u0440\u0430\u0432\u0438\u0442\u044C\u00BB");
    AppendMenuW(hV4, MF_STRING, CMD_NOTE,
        L"\u041F\u0443\u0441\u0442\u044C \u043D\u0430\u043F\u0438\u0448\u0443\u0442 \u0437\u0430\u043F\u0438\u0441\u043A\u0443\tCtrl+Alt+O");
    AppendMenuW(hV4, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hV4, MF_STRING, CMD_CURSOR_THEFT,
        L"\u0423\u043A\u0440\u0430\u0441\u0442\u044C \u043A\u0443\u0440\u0441\u043E\u0440 (2 \u0441, \u043D\u0430\u0440\u0438\u0441\u043E\u0432\u0430\u043D\u043D\u044B\u0439)");
    AppendMenuW(hV4, MF_STRING, CMD_SHOW_DUEL,
        L"\u041F\u043E\u043A\u0430\u0437\u0430\u0442\u044C \u0431\u043E\u0439 \u043D\u0430 \u043C\u0435\u0447\u0430\u0445\tCtrl+Alt+U");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hV4, L"\u0418\u0433\u0440\u044B \u0438 \u043F\u0440\u043E\u0434\u0435\u043B\u043A\u0438 v4");

    HMENU hWeather = CreatePopupMenu();
    AppendMenuW(hWeather, MF_STRING | (g_app.weatherMode == 0 ? MF_CHECKED : MF_UNCHECKED), CMD_WEATHER_AUTO,
        L"\u041F\u043E \u0441\u0435\u0437\u043E\u043D\u0443 (\u0434\u043E\u0436\u0434\u044C/\u0441\u043D\u0435\u0433)");
    AppendMenuW(hWeather, MF_STRING | (g_app.weatherMode == 1 ? MF_CHECKED : MF_UNCHECKED), CMD_WEATHER_RAIN,
        L"\u0414\u043E\u0436\u0434\u044C");
    AppendMenuW(hWeather, MF_STRING | (g_app.weatherMode == 2 ? MF_CHECKED : MF_UNCHECKED), CMD_WEATHER_SNOW,
        L"\u0421\u043D\u0435\u0433");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hWeather, L"\u041F\u043E\u0433\u043E\u0434\u0430 \u0437\u0430 \u043E\u043A\u043D\u043E\u043C");
    AppendMenuW(hMenu, MF_STRING | (g_app.weatherOn ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_WEATHER,
        L"\u0420\u0435\u0430\u0433\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u043D\u0430 \u043F\u043E\u0433\u043E\u0434\u0443");

    HMENU hBreak = CreatePopupMenu();
    AppendMenuW(hBreak, MF_STRING | (g_app.breakMinutes < 22.5f ? MF_CHECKED : MF_UNCHECKED), CMD_BREAK_15, L"15 \u043C\u0438\u043D\u0443\u0442");
    AppendMenuW(hBreak, MF_STRING | (g_app.breakMinutes > 22.5f && g_app.breakMinutes < 37.5f ? MF_CHECKED : MF_UNCHECKED), CMD_BREAK_30, L"30 \u043C\u0438\u043D\u0443\u0442");
    AppendMenuW(hBreak, MF_STRING | (g_app.breakMinutes > 37.5f && g_app.breakMinutes < 52.5f ? MF_CHECKED : MF_UNCHECKED), CMD_BREAK_45, L"45 \u043C\u0438\u043D\u0443\u0442");
    AppendMenuW(hBreak, MF_STRING | (g_app.breakMinutes > 52.5f ? MF_CHECKED : MF_UNCHECKED), CMD_BREAK_60, L"60 \u043C\u0438\u043D\u0443\u0442");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBreak, L"\u041F\u0435\u0440\u0435\u0440\u044B\u0432 \u0447\u0430\u044E \u043A\u0430\u0436\u0434\u044B\u0435");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // --- smart scenes ---
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (g_app.couchOn ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_COUCH,
        L"\u0414\u0438\u0432\u0430\u043D: \u0441\u043C\u043E\u0442\u0440\u0435\u0442\u044C \u0432\u0438\u0434\u0435\u043E \u0432\u043C\u0435\u0441\u0442\u0435\tCtrl+Alt+V");
    AppendMenuW(hMenu, MF_STRING | (g_app.autoCouch ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_AUTOTV,
        L"\u0410\u0432\u0442\u043E-\u0434\u0438\u0432\u0430\u043D (\u043C\u043E\u043C\u0435\u043D\u0442 \u0432\u0438\u0434\u0435\u043E)");
    AppendMenuW(hMenu, MF_STRING | (g_app.deskOn ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_DESK,
        L"\u041C\u0438\u043D\u0438-\u0441\u0442\u043E\u043B \u0441 \u043D\u043E\u0443\u0442\u0431\u0443\u043A\u043E\u043C\tCtrl+Alt+L");
    AppendMenuW(hMenu, MF_STRING | (g_app.pranksOn ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_PRANKS,
        L"\u041f\u0440\u043e\u0434\u0435\u043b\u043a\u0438: \u0442\u0440\u0435\u0449\u0438\u043d\u0430, \u043a\u0443\u0431\u0438\u043a, \u0443\u0441\u044b");
    AppendMenuW(hMenu, MF_STRING | (g_app.pushWindows ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_PUSH,
        L"\u0422\u043e\u043b\u043a\u0430\u0442\u044c \u0444\u043e\u043d\u043e\u0432\u044b\u0435 \u043e\u043a\u043d\u0430 (\u043f\u043e 7 px)");
    AppendMenuW(hMenu, MF_STRING | (g_app.autoNight ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_NIGHT,
        L"\u041d\u043e\u0447\u043d\u043e\u0439 \u0440\u0435\u0436\u0438\u043c \u043f\u043e \u0447\u0430\u0441\u0430\u043c (\u0432\u044b\u043a\u043b = \u0432\u0435\u0447\u043d\u044b\u0439 \u0434\u0435\u043d\u044c)");
    AppendMenuW(hMenu, MF_STRING | (g_app.teaReminders ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_TEA,
        L"\u041d\u0430\u043f\u043e\u043c\u0438\u043d\u0430\u0442\u044c \u043f\u0440\u043e \u0447\u0430\u0439 \u043f\u043e\u0441\u043b\u0435 45 \u043c\u0438\u043d\u0443\u0442");

    // --- mode & size ---
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (g_app.calmMode ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_CALM,
        L"\u0421\u043F\u043E\u043A\u043E\u0439\u043D\u044B\u0439 \u0440\u0435\u0436\u0438\u043C (\u0431\u0435\u0437 \u043F\u0430\u0440\u043A\u0443\u0440\u0430)\tCtrl+Alt+M");
    HMENU hScale = CreatePopupMenu();
    AppendMenuW(hScale, MF_STRING | (g_app.scaleIdx == 0 ? MF_CHECKED : MF_UNCHECKED), CMD_SCALE_SMALL,
        L"\u041C\u0430\u043B\u0435\u043D\u044C\u043A\u0438\u0435");
    AppendMenuW(hScale, MF_STRING | (g_app.scaleIdx == 1 ? MF_CHECKED : MF_UNCHECKED), CMD_SCALE_NORMAL,
        L"\u041E\u0431\u044B\u0447\u043D\u044B\u0435");
    AppendMenuW(hScale, MF_STRING | (g_app.scaleIdx == 2 ? MF_CHECKED : MF_UNCHECKED), CMD_SCALE_LARGE,
        L"\u0411\u043E\u043B\u044C\u0448\u0438\u0435");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hScale,
        L"\u0420\u0430\u0437\u043C\u0435\u0440 \u0441\u0442\u0438\u043A\u043C\u0435\u043D\u043E\u0432\t(\u043A\u043E\u043B\u0435\u0441\u043E \u043C\u044B\u0448\u0438)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (g_app.ghostMode ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_GHOST,
        L"\u0420\u0435\u0436\u0438\u043C \u00AB\u041F\u0440\u0438\u0437\u0440\u0430\u043A\u00BB (\u043A\u043B\u0438\u043A\u0438 \u0441\u043A\u0432\u043E\u0437\u044C)\tCtrl+Alt+G");
    AppendMenuW(hMenu, MF_STRING | (g_app.interactWindows ? MF_CHECKED : MF_UNCHECKED), CMD_TOGGLE_WINS,
        L"\u0425\u043E\u0434\u0438\u0442\u044C \u0438 \u043B\u0430\u0437\u0430\u0442\u044C \u043F\u043E \u043E\u043A\u043D\u0430\u043C \u041F\u041A");
    AppendMenuW(hMenu, MF_STRING, CMD_ASK_AI,
        L"\u0421\u043f\u0440\u043e\u0441\u0438\u0442\u044c \u0441\u0442\u0438\u043a\u043c\u0435\u043d\u0430 (DeepSeek)\tCtrl+Alt+A");
    AppendMenuW(hMenu, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED), CMD_AUTOSTART,
        L"\u0417\u0430\u043f\u0443\u0441\u043a\u0430\u0442\u044c \u0432\u043c\u0435\u0441\u0442\u0435 \u0441 Windows");
    AppendMenuW(hMenu, MF_STRING, CMD_EXIT,
        L"\u0412\u044B\u0445\u043E\u0434\tCtrl+Alt+Q");

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void SetGhostWindowStyle(HWND hwnd, bool ghost) {
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ghost) ex |= WS_EX_TRANSPARENT;
    else       ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
}

// ============================================================================
// Tray-triggered scene helpers (v3)
// ============================================================================
static Stickman* NearestIdleToCursor() {
    Stickman* best = nullptr;
    float bd = 1.0e9f;
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED || s.propIndex >= 0) continue;
        float d = std::fabs(s.x - g_app.cursor.x) + std::fabs(s.y - g_app.cursor.y) * 0.5f;
        if (d < bd) { bd = d; best = &s; }
    }
    if (!best) {
        for (auto& s : g_app.stickmen) {
            if (s.act == Act::GRABBED) continue;
            best = &s;
            break;
        }
    }
    return best;
}

// 0 = nothing, 1 = sword, 2 = pencil, 3 = guitar
static void GiveItemToNearest(int what) {
    Stickman* s = NearestIdleToCursor();
    if (!s) return;
    Stickman& r = *s;
    ReleaseProp(r);
    r.carry = 0;
    r.weapon = 0;
    r.blocks = 0;
    if (what == 0) {
        r.hasSword = false;
        r.swordTimer = 0.0f;
        SetAct(r, Act::IDLE);
        r.actTimer = RandRange(1.2f, 2.4f);
        return;
    }
    if (what == 1) {
        r.hasSword = true;
        r.weapon = 1;                    // diamond sword (also used by the pointer duel)
        r.swordTimer = 1.0e7f;               // stays until you take it away
        r.facing = (g_app.cursor.x >= r.x) ? 1 : -1;
        SetAct(r, Act::POINT_AT);
        r.actTimer = RandRange(2.4f, 4.0f);
        SpawnBurst(r.pose.handR.x, r.pose.handR.y, r.r, r.g, r.b, 6, 4);
        return;
    }
    if (what == 2) {
        r.weapon = 3;                    // a pencil, for doodling their pet
        float x = PickSceneSpot(200.0f);
        if (x < 0.0f) x = ClampF(r.x, 80.0f, (float)g_app.width - 80.0f);
        r.doodle.clear();
        r.doodleShape = 1 + RandInt(3);
        BeginTravel(r, Act::SKETCH, x);
        r.travelFloor = true;
        r.doodleFade = 14.0f;
        return;
    }
    r.weapon = 4;                        // guitar
    BeginTravel(r, Act::MUSIC, ClampF(r.x + RandRange(-60.0f, 60.0f), 60.0f, (float)g_app.width - 60.0f));
    r.travelFloor = true;
}

// two guys, one table: armwrestling or tug of war
static void StartDuelNearCursor() {
    Stickman* a = nullptr;
    Stickman* b = nullptr;
    float bestD = 1.0e9f;
    for (auto& s : g_app.stickmen) {
        if (s.act == Act::GRABBED || s.propIndex >= 0 || !s.grounded) continue;
        float d = std::fabs(s.x - g_app.cursor.x);
        if (d < bestD) { bestD = d; a = &s; }
    }
    if (!a) return;
    for (auto& s : g_app.stickmen) {
        if (s.id == a->id) continue;
        if (s.act == Act::GRABBED || s.propIndex >= 0 || !s.grounded) continue;
        if (!b) b = &s;
    }
    if (!b) {                              // alone -> spar with an invisible rival
        a->hasSword = true;
        a->swordTimer = 20.0f;
        BeginTravel(*a, Act::POINT_AT, g_app.cursor.x);
        return;
    }
    ReleaseProp(*a);
    ReleaseProp(*b);
    if (RandInt(2) == 0) StartArmWrestle(*a, *b);
    else StartTugWar(*a, *b);
}

// pack a snowman right here (a drawn one, of course)
static void StartSnowmanHere() {
    float sc = g_app.scale;
    float x = ClampF(g_app.cursor.x, 90.0f * sc, (float)g_app.width - 90.0f * sc);
    float y = (float)FloorYAt(x);
    int idx = AddProp(PR_PILLOW, x, y, 34.0f, -1, -1, 1);
    if (idx < 0) return;
    int helpers0 = 0;
    for (auto& s : g_app.stickmen) {
        if (helpers0 >= 2) break;
        if (s.act == Act::GRABBED || s.propIndex >= 0) continue;
        SeatAt(s, Act::BUILD, idx, (helpers0 == 0 ? -26.0f : 26.0f) * sc, (helpers0 == 0) ? 1 : -1);
        ++helpers0;
    }
    if (helpers0 == 0) {
        KillProp(g_app.props[idx]);
        return;
    }
    g_app.props[idx].data = 1;
}

// stroke one of them like The Ball does
static void PatHeadNearest() {
    Stickman* s = NearestIdleToCursor();
    if (!s) return;
    if (s->act == Act::GRABBED) ReleaseGrab();
    s->pet = 1.6f;
    s->act = Act::PETTED;
    s->actTimer = RandRange(2.6f, 4.0f);
    s->animTime = 0.0f;
    SpawnBurst(s->pose.head.x, s->pose.head.y - 14.0f * g_app.scale, 255, 140, 190, 5, 9);
}

// hand a treat to the closest guy instead of dropping it on the floor
static void TossTreatToNearest(int kind) {
    Stickman* s = NearestIdleToCursor();
    if (!s) { DropSnackAtCursor(kind); return; }
    float tx = ClampF(g_app.cursor.x, 60.0f, (float)g_app.width - 60.0f);
    DropSnackAtCursor(kind);
    if (!g_app.snacks.empty()) {
        g_app.snacks.back().x = tx;
        g_app.snacks.back().y = s->y - 60.0f * g_app.scale;
        g_app.snacks.back().vx = (tx > s->x) ? -30.0f : 30.0f;
    }
    if (s->act != Act::GRABBED && s->propIndex < 0) {
        s->nextAct = Act::EAT_SNACK;
        s->act = Act::POINT_AT;
        s->actTimer = 0.8f;
        s->facing = (tx >= s->x) ? 1 : -1;
    }
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_NCHITTEST: {
            if (g_app.ghostMode) return HTTRANSPARENT;
            if (g_app.grabbedId != -1) return HTCLIENT;
            float x = (float)(GET_X_LPARAM(lParam) - g_app.originX);
            float y = (float)(GET_Y_LPARAM(lParam) - g_app.originY);
            return HitTestStickman(x, y) ? HTCLIENT : HTTRANSPARENT;
        }

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, (g_app.grabbedId != -1) ? IDC_SIZEALL : IDC_HAND));
            return TRUE;

        case WM_LBUTTONDOWN: {
            POINT pt = {};
            GetCursorPos(&pt);
            float x = (float)(pt.x - g_app.originX);
            float y = (float)(pt.y - g_app.originY);
            if (Stickman* s = HitTestStickman(x, y)) {
                LeaveSeat(*s);
                g_app.grabbedId = s->id;
                s->act = Act::GRABBED;
                s->x = x;
                s->y = y;
                s->grabAngle = 0.0f;
                s->grabAngVel = 0.0f;
                s->grounded = false;
                s->standingHwnd = nullptr;
                s->pendingHello = false;
                SetCapture(hwnd);
                SpawnBurst(x, y, s->r, s->g, s->b, 6, 2);
            }
            return 0;
        }

        case WM_LBUTTONUP:
            ReleaseGrab();
            return 0;

        case WM_LBUTTONDBLCLK: {
            POINT pt = {};
            GetCursorPos(&pt);
            float x = (float)(pt.x - g_app.originX);
            float y = (float)(pt.y - g_app.originY);
            if (Stickman* s = HitTestStickman(x, y)) {
                g_app.grabbedId = -1;
                ReleaseCapture();
                // double click: they cheer and wave at YOU
                s->act = Act::WAVE_HELLO;
                s->actTimer = RandRange(2.2f, 3.4f);
                s->animTime = 0.0f;
                if (s->grounded) s->vx = 0.0f;
                s->squashVel = -1.8f;
                s->facing = (x >= s->x) ? 1 : -1;
                SpawnBurst(s->x, s->y - 40.0f * g_app.scale, s->r, s->g, s->b, 10, 2);
            }
            return 0;
        }

        case WM_RBUTTONDOWN: {
            POINT pt = {};
            GetCursorPos(&pt);
            float x = (float)(pt.x - g_app.originX);
            float y = (float)(pt.y - g_app.originY);
            if (Stickman* s = HitTestStickman(x, y)) PokeReaction(*s, x, y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt = {};
            GetCursorPos(&pt);
            float x = (float)(pt.x - g_app.originX);
            float y = (float)(pt.y - g_app.originY);
            if (HitTestStickman(x, y)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                SetScaleIdx(g_app.scaleIdx + (delta > 0 ? 1 : -1));
                RefreshTrayTip();
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_FRAME) {
                DWORD now = GetTickCount();
                float dt = (float)(now - g_app.lastTick) / 1000.0f;
                g_app.lastTick = now;
                dt = ClampF(dt, 0.004f, 0.05f);
                UpdateSimulation(dt);
                RenderFrame();
                if (++g_app.topmostCounter >= 150) {
                    g_app.topmostCounter = 0;
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            } else if (wParam == TIMER_WINSCAN) {
                ScanDesktopWindows();
            }
            return 0;
        }

        case WM_DISPLAYCHANGE:
            RebuildBackbuffer();
            break;

        case WM_SETTINGCHANGE:
            RebuildBackbuffer();
            return 0;

        case WM_HOTKEY: {
            switch (wParam) {
                case HK_SPAWN:
                    SpawnStickman(PickRandomType(), (float)g_app.width * 0.5f + RandRange(-140, 140), 100.0f, true);
                    break;
                case HK_TCO:
                    SpawnStickman(CharType::TCO_BLACK, (float)g_app.width * 0.5f, 90.0f, true);
                    break;
                case HK_SPAR:
                    StartSparringMatch();
                    break;
                case HK_CALL:
                    CallAllToCursor();
                    break;
                case HK_HELLO:
                    SayHelloAll();
                    break;
                case HK_PIZZA:
                    DropSnackAtCursor(RandInt(2) == 0 ? 0 : 1);
                    break;
                case HK_BALL:
                    SpawnBallAtCursor();
                    break;
                case HK_COUCH:
                    g_app.autoCouch = false;
                    SetCouchMode(!g_app.couchOn);
                    RefreshTrayTip();
                    break;
                case HK_MODE:
                    g_app.calmMode = !g_app.calmMode;
                    for (auto& s : g_app.stickmen) if (s.act == Act::RUN) { s.act = Act::WALK; s.actTimer = 1.5f; }
                    RefreshTrayTip();
                    break;
                case HK_SCALE:
                    SetScaleIdx((g_app.scaleIdx + 1) % 3);
                    RefreshTrayTip();
                    break;
                case HK_TREAT:
                    PatHeadNearest();
                    break;
                case HK_DUEL:
                    StartDuelNearCursor();
                    break;
                case HK_PARTY:
                    StartPartyCorner();
                    break;
                case HK_SNOWMAN:
                    StartSnowmanHere();
                    break;
                case HK_ICON_HIDE:
                    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(CMD_HIDE_ICON, 0), 0);
                    break;
                case HK_BRING_FOLDER:
                    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(CMD_BRING_FOLDER, 0), 0);
                    break;
                case HK_WRITE_NOTE:
                    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(CMD_NOTE, 0), 0);
                    break;
                case HK_ASK_AI:      ShowAskAIDialog(hwnd); return 0;
                case HK_DUEL_SHOW:
                    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(CMD_SHOW_DUEL, 0), 0);
                    break;
                case HK_GHOST:
                    g_app.ghostMode = !g_app.ghostMode;
                    SetGhostWindowStyle(hwnd, g_app.ghostMode);
                    break;
                case HK_QUIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayMenu(hwnd);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                CallAllToCursor();
            }
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) >= CMD_HIDE_ICON && LOWORD(wParam) <= CMD_ASK_AI) {
                switch (LOWORD(wParam)) {
                    case CMD_HIDE_ICON: {
                        Stickman* who = PickFreeGuy(1);
                        if (who) StartCursorHide(*who);
                        return 0;
                    }
                    case CMD_ICON_KICK: {
                        Stickman* who = PickFreeGuy(1);
                        if (who) StartIconGame(*who);
                        return 0;
                    }
                    case CMD_TAB_OOPS: {
                        Stickman* who = PickFreeGuy(1);
                        if (who) StartTabOops(*who);
                        return 0;
                    }
                    case CMD_BRING_FOLDER: {
                        Stickman* who = PickFreeGuy(2);
                        if (who) StartBring(*who);
                        return 0;
                    }
                    case CMD_SEND_GUARD: {
                        Stickman* who = PickFreeGuy(2);
                        if (who) StartSendGuard(*who);
                        return 0;
                    }
                    case CMD_NOTE: {
                        Stickman* who = PickFreeGuy(0);
                        if (who) StartNoteHere(*who, (g_app.mood > 0.5f) ? 0 : 8);
                        return 0;
                    }
                    case CMD_CURSOR_THEFT: {
                        Stickman* who = PickFreeGuy(1);
                        if (who) {
                            who->act = Act::CURSOR_THEFT;
                            who->actTimer = 7.0f;
                            who->animTime = 0.0f;
                            who->floatT = 0.0f;
                            who->blocks = 0;
                            who->rides = 2;
                            who->carry = 13;
                            g_app.ghostCursor = g_app.cursor;
                            SayNote(*who, 15);
                        }
                        return 0;
                    }
                    case CMD_SHOW_DUEL:
                        StartShowDuel();
                        return 0;
                    case CMD_TOGGLE_WEATHER:
                        g_app.weatherOn = !g_app.weatherOn;
                        g_app.weatherT = 0.05f;
                        RefreshTrayTip();
                        return 0;
                    case CMD_WEATHER_AUTO:
                        g_app.weatherMode = 0; g_app.weatherOn = true; g_app.weatherT = 0.05f;
                        return 0;
                    case CMD_WEATHER_RAIN:
                        g_app.weatherMode = 1; g_app.weatherOn = true; g_app.weatherT = 0.05f;
                        return 0;
                    case CMD_WEATHER_SNOW:
                        g_app.weatherMode = 2; g_app.weatherOn = true; g_app.weatherT = 0.05f;
                        return 0;
                    case CMD_BREAK_15: g_app.breakMinutes = 15.0f; return 0;
                    case CMD_BREAK_30: g_app.breakMinutes = 30.0f; return 0;
                    case CMD_BREAK_45: g_app.breakMinutes = 45.0f; return 0;
                    case CMD_BREAK_60: g_app.breakMinutes = 60.0f; return 0;
                    case CMD_AUTOSTART: SetAutoStart(!IsAutoStartEnabled()); return 0;
                    case CMD_ASK_AI: ShowAskAIDialog(hwnd); return 0;
                    default: break;
                }
            }
            switch (LOWORD(wParam)) {
                case CMD_ADD_STICKMAN:
                    SpawnStickman(PickRandomType(), (float)g_app.width * 0.5f + RandRange(-140, 140), 100.0f, true);
                    break;
                case CMD_ADD_TCO:
                    SpawnStickman(CharType::TCO_BLACK, (float)g_app.width * 0.5f, 90.0f, true);
                    break;
                case CMD_RESET_SQUAD:
                    ResetSquad();
                    break;
                case CMD_REMOVE_ONE:
                    if (!g_app.stickmen.empty()) {
                        ReleaseCouchSeat(g_app.stickmen.back().id);
                        if (g_app.deskOwnerId == g_app.stickmen.back().id) g_app.deskOwnerId = -1;
                        g_app.stickmen.pop_back();
                        RecomputeCouchAnchor();
                    }
                    break;
                case CMD_SAY_HELLO:
                    SayHelloAll();
                    break;
                case CMD_SPARRING:
                    StartSparringMatch();
                    break;
                case CMD_TOGGLE_SPARAI:
                    g_app.sparAI = !g_app.sparAI;
                    break;
                case CMD_CALL_HERE:
                    CallAllToCursor();
                    break;
                case CMD_DROP_PIZZA:
                case CMD_TREAT_PIZZA:
                    DropSnackAtCursor(RandInt(2) == 0 ? 0 : 1);
                    break;
                case CMD_TREAT_CAKE:    TossTreatToNearest(1); break;
                case CMD_TREAT_MELON:   TossTreatToNearest(2); break;
                case CMD_TREAT_POTION:  TossTreatToNearest(3); break;
                case CMD_TREAT_CUBE:    TossTreatToNearest(4); break;
                case CMD_GIVE_SWORD:    GiveItemToNearest(1); break;
                case CMD_GIVE_PENCIL:   GiveItemToNearest(2); break;
                case CMD_GIVE_GUITAR:   GiveItemToNearest(3); break;
                case CMD_GIVE_NONE:     GiveItemToNearest(0); break;
                case CMD_PARTY:
                    StartPartyCorner();
                    break;
                case CMD_DUEL:
                    StartDuelNearCursor();
                    break;
                case CMD_PAT_HEAD:
                    PatHeadNearest();
                    break;
                case CMD_SNOWMAN:
                    StartSnowmanHere();
                    break;
                case CMD_TOGGLE_PRANKS:
                    g_app.pranksOn = !g_app.pranksOn;
                    break;
                case CMD_TOGGLE_PUSH:
                    g_app.pushWindows = !g_app.pushWindows;
                    break;
                case CMD_TOGGLE_NIGHT:
                    if (g_app.autoNight) { g_app.autoNight = false; g_app.nightForced = true; }
                    else g_app.nightForced = !g_app.nightForced;
                    break;
                case CMD_TOGGLE_TEA:
                    g_app.teaReminders = !g_app.teaReminders;
                    break;
                case CMD_SPAWN_BALL:
                    SpawnBallAtCursor();
                    break;
                case CMD_CLEAR_TOYS:
                    g_app.balls.clear();
                    g_app.snacks.clear();
                    break;
                case CMD_TOGGLE_COUCH:
                    g_app.autoCouch = false;
                    SetCouchMode(!g_app.couchOn);
                    RefreshTrayTip();
                    break;
                case CMD_TOGGLE_AUTOTV:
                    g_app.autoCouch = !g_app.autoCouch;
                    if (!g_app.autoCouch) g_app.couchHold = 0.0f;
                    break;
                case CMD_TOGGLE_DESK:
                    g_app.autoDesk = false;
                    SetDeskMode(!g_app.deskOn);
                    RefreshTrayTip();
                    break;
                case CMD_TOGGLE_CALM:
                    g_app.calmMode = !g_app.calmMode;
                    RefreshTrayTip();
                    break;
                case CMD_SCALE_SMALL:  SetScaleIdx(0); RefreshTrayTip(); break;
                case CMD_SCALE_NORMAL: SetScaleIdx(1); RefreshTrayTip(); break;
                case CMD_SCALE_LARGE:  SetScaleIdx(2); RefreshTrayTip(); break;
                case CMD_TOGGLE_GHOST:
                    g_app.ghostMode = !g_app.ghostMode;
                    SetGhostWindowStyle(hwnd, g_app.ghostMode);
                    break;
                case CMD_TOGGLE_WINS:
                    g_app.interactWindows = !g_app.interactWindows;
                    ScanDesktopWindows();
                    break;
                case CMD_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;
        }

        case WM_DESTROY:
            for (auto& sv : g_app.stickmen) {
                if (sv.pushHwnd && IsWindow(sv.pushHwnd)) {
                    SetWindowPos(sv.pushHwnd, nullptr, (int)sv.pushOrigin.x, (int)sv.pushOrigin.y, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    sv.pushHwnd = nullptr;
                }
            }
            KillTimer(hwnd, TIMER_FRAME);
            KillTimer(hwnd, TIMER_WINSCAN);
            for (int id = HK_SPAWN; id <= HK_ASK_AI; ++id) UnregisterHotKey(hwnd, id);
            AudioMeterRelease();
            Shell_NotifyIconW(NIM_DELETE, &g_app.nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Entry point
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AlanBeckerStickmenOverlayMutex_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    srand((unsigned int)time(nullptr) ^ (unsigned int)GetTickCount());
    EnableDpiAwareness();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    g_app.hInst = hInstance;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_app.gdiplusToken, &gdiplusStartupInput, nullptr);
    SetScaleIdx(1);

    const wchar_t* CLASS_NAME = L"AlanBeckerStickmenOverlayWnd";
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = OverlayWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor       = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    RebuildBackbuffer();

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    g_app.hwnd = CreateWindowExW(
        exStyle, CLASS_NAME, L"Alan Becker Stickmen", WS_POPUP,
        g_app.originX, g_app.originY, g_app.width, g_app.height,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_app.hwnd) return 1;

    ShowWindow(g_app.hwnd, SW_SHOWNOACTIVATE);
    SetupTrayIcon(g_app.hwnd);

    RegisterHotKey(g_app.hwnd, HK_SPAWN, MOD_CONTROL | MOD_ALT, 'S');
    RegisterHotKey(g_app.hwnd, HK_TCO,   MOD_CONTROL | MOD_ALT, 'T');
    RegisterHotKey(g_app.hwnd, HK_SPAR,  MOD_CONTROL | MOD_ALT, 'F');
    RegisterHotKey(g_app.hwnd, HK_CALL,  MOD_CONTROL | MOD_ALT, 'C');
    RegisterHotKey(g_app.hwnd, HK_HELLO, MOD_CONTROL | MOD_ALT, 'H');
    RegisterHotKey(g_app.hwnd, HK_PIZZA, MOD_CONTROL | MOD_ALT, 'P');
    RegisterHotKey(g_app.hwnd, HK_BALL,  MOD_CONTROL | MOD_ALT, 'B');
    RegisterHotKey(g_app.hwnd, HK_COUCH, MOD_CONTROL | MOD_ALT, 'V');
    RegisterHotKey(g_app.hwnd, HK_MODE,  MOD_CONTROL | MOD_ALT, 'M');
    RegisterHotKey(g_app.hwnd, HK_SCALE, MOD_CONTROL | MOD_ALT, 'R');
    RegisterHotKey(g_app.hwnd, HK_GHOST, MOD_CONTROL | MOD_ALT, 'G');
    RegisterHotKey(g_app.hwnd, HK_QUIT,  MOD_CONTROL | MOD_ALT, 'Q');
    RegisterHotKey(g_app.hwnd, HK_TREAT, MOD_CONTROL | MOD_ALT, 'E');
    RegisterHotKey(g_app.hwnd, HK_DUEL,  MOD_CONTROL | MOD_ALT, 'D');
    RegisterHotKey(g_app.hwnd, HK_PARTY, MOD_CONTROL | MOD_ALT, 'Y');
    RegisterHotKey(g_app.hwnd, HK_SNOWMAN, MOD_CONTROL | MOD_ALT, 'W');
    RegisterHotKey(g_app.hwnd, HK_ICON_HIDE,    MOD_CONTROL | MOD_ALT, 'I');
    RegisterHotKey(g_app.hwnd, HK_BRING_FOLDER, MOD_CONTROL | MOD_ALT, 'K');
    RegisterHotKey(g_app.hwnd, HK_WRITE_NOTE,   MOD_CONTROL | MOD_ALT, 'O');
    RegisterHotKey(g_app.hwnd, HK_DUEL_SHOW,    MOD_CONTROL | MOD_ALT, 'U');
    RegisterHotKey(g_app.hwnd, HK_ASK_AI,       MOD_CONTROL | MOD_ALT, 'A');

    ScanDesktopWindows();
    ResetSquad();
    RecomputeCouchAnchor();
    RefreshTrayTip();

    GetCursorPos(&g_app.prevMouse);
    g_app.lastTick = GetTickCount();

    SetTimer(g_app.hwnd, TIMER_FRAME,   16,  nullptr); // ~60 fps
    SetTimer(g_app.hwnd, TIMER_WINSCAN, 180, nullptr); // window scan ~5.5x/sec

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(g_app.hwnd, TIMER_FRAME);
    KillTimer(g_app.hwnd, TIMER_WINSCAN);
    if (g_app.memDC) {
        if (g_app.oldBmp) SelectObject(g_app.memDC, g_app.oldBmp);
        if (g_app.dib) DeleteObject(g_app.dib);
        DeleteDC(g_app.memDC);
    }
    if (g_app.screenDC) ReleaseDC(nullptr, g_app.screenDC);
    GdiplusShutdown(g_app.gdiplusToken);
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
