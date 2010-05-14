// server.h

#define gamemode smode   // allows the gamemode macros to work with the server mode

#define SERVER_PROTOCOL_VERSION    (PROTOCOL_VERSION)    // server without any gameplay modification
//#define SERVER_PROTOCOL_VERSION   (-PROTOCOL_VERSION)  // server with gameplay modification but compatible to vanilla client (using /modconnect)
//#define SERVER_PROTOCOL_VERSION  (PROTOCOL_VERSION)    // server with incompatible protocol (change PROTOCOL_VERSION in file protocol.h to a negative number!)

#define valid_flag(f) (f >= 0 && f < 2)

enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_AKIMBO, GE_RELOAD, GE_SCOPING, GE_SUICIDE, GE_PICKUP };
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

extern int smode, servmillis;

struct shotevent
{
    int type;
    int millis, id;
    int gun;
    float from[3], to[3];
};

struct explodeevent
{
    int type;
    int millis, id;
    int gun;
};

struct hitevent
{
    int type;
    int target;
    int lifesequence;
    union
    {
        int info;
        float dist;
    };
    float dir[3];
};

struct suicideevent
{
    int type;
};

struct pickupevent
{
    int type;
    int ent;
};

struct akimboevent
{
    int type;
    int millis, id;
};

struct reloadevent
{
    int type;
    int millis, id;
    int gun;
};

struct scopeevent
{
    int type;
    int millis, id;
    bool scoped;
};

union gameevent
{
    int type;
    shotevent shot;
    explodeevent explode;
    hitevent hit;
    suicideevent suicide;
    pickupevent pickup;
    akimboevent akimbo;
    reloadevent reload;
    scopeevent scoping;
};

template <int N>
struct projectilestate
{
    int projs[N];
    int numprojs;

    projectilestate() : numprojs(0) {}

    void reset() { numprojs = 0; }

    void add(int val)
    {
        if(numprojs>=N) numprojs = 0;
        projs[numprojs++] = val;
    }

    bool remove(int val)
    {
        loopi(numprojs) if(projs[i]==val)
        {
            projs[i] = projs[--numprojs];
            return true;
        }
        return false;
    }
};

static const int DEATHMILLIS = 300;

struct clientstate : playerstate
{
    vec o;
    int state;
    int lastdeath, lastspawn, lifesequence;
    int lastshot;
    projectilestate<8> grenades;
    int akimbomillis;
    bool scoped;
    int flagscore, frags, teamkills, deaths, shotdamage, damage;

    clientstate() : state(CS_DEAD) {}

    bool isalive(int gamemillis)
    {
        return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
    }

    bool waitexpired(int gamemillis)
    {
        int wait = gamemillis - lastshot;
        loopi(NUMGUNS) if(wait < gunwait[i]) return false;
        return true;
    }

    void reset()
    {
        state = CS_DEAD;
        lifesequence = -1;
        grenades.reset();
        akimbomillis = 0;
        scoped = false;
        flagscore = frags = teamkills = deaths = shotdamage = damage = 0;
        respawn();
    }

    void respawn()
    {
        playerstate::respawn();
        o = vec(-1e10f, -1e10f, -1e10f);
        lastdeath = 0;
        lastspawn = -1;
        lastshot = 0;
        akimbomillis = 0;
        scoped = false;
    }
};

struct savedscore
{
    string name;
    uint ip;
    int frags, flagscore, deaths, teamkills, shotdamage, damage, team;
    bool valid;

    void save(clientstate &cs, int t)
    {
        frags = cs.frags;
        flagscore = cs.flagscore;
        deaths = cs.deaths;
        teamkills = cs.teamkills;
        shotdamage = cs.shotdamage;
        damage = cs.damage;
        team = t;
        valid = true;
    }

    void restore(clientstate &cs)
    {
        cs.frags = frags;
        cs.flagscore = flagscore;
        cs.deaths = deaths;
        cs.teamkills = teamkills;
        cs.shotdamage = shotdamage;
        cs.damage = damage;
    }
};

struct client                   // server side version of "dynent" type
{
    int type;
    int clientnum;
    ENetPeer *peer;
    string hostname;
    string name;
    int team;
    char lang[3];
    int ping;
    int skin[2];
    int vote;
    int role;
    int connectmillis;
    int acversion, acbuildtype;
    bool isauthed; // for passworded servers
    bool haswelcome;
    bool isonrightmap, loggedwrongmap, freshgame;
    bool timesync;
    int overflow;
    int gameoffset, lastevent, lastvotecall;
    int demoflags;
    clientstate state;
    int spectcn;
    vector<gameevent> events;
    vector<uchar> position, messages;
    string lastsaytext;
    int saychars, lastsay, spamcount;
    int at3_score, at3_lastforce;
    bool at3_dontmove;
    int spawnindex;
    int spawnperm, spawnpermsent;
    int salt;
    string pwd;
    int mapcollisions, farpickups;
    enet_uint32 bottomRTT;

    gameevent &addevent()
    {
        static gameevent dummy;
        if(events.length()>100) return dummy;
        return events.add();
    }

    void mapchange(bool getmap = false)
    {
        state.reset();
        events.setsize(0);
        overflow = 0;
        timesync = false;
        isonrightmap = false;
        spawnperm = SP_WRONGMAP;
        spawnpermsent = servmillis;
        if(!getmap)
        {
            loggedwrongmap = false;
            freshgame = true;         // permission to spawn at once
        }
        lastevent = 0;
        at3_lastforce = 0;
        mapcollisions = farpickups = 0;
    }

    void reset()
    {
        name[0] = demoflags = 0;
        bottomRTT = ping = 9999;
        team = TEAM_SPECT;
        loopi(2) skin[i] = 0;
        position.setsize(0);
        messages.setsize(0);
        isauthed = haswelcome = false;
        role = CR_DEFAULT;
        lastvotecall = 0;
        vote = VOTE_NEUTRAL;
        lastsaytext[0] = '\0';
        saychars = 0;
        spawnindex = -1;
        spectcn = FPCN_VOID;
        mapchange();
        freshgame = false;         // don't spawn into running games
    }

    void zap()
    {
        type = ST_EMPTY;
        role = CR_DEFAULT;
        isauthed = haswelcome = false;
    }
};

struct ban
{
    ENetAddress address;
    int millis;
};

struct worldstate
{
    enet_uint32 uses;
    vector<uchar> positions, messages;
};

struct server_entity            // server side version of "entity" type
{
    int type;
    bool spawned, legalpickup;
    int spawntime;
    short x, y;
};

struct demofile
{
    string info;
    string file;
    uchar *data;
    int len;
};

void startgame(const char *newname, int newmode, int newtime = -1, bool notify = true);
void disconnect_client(int n, int reason = -1);
int clienthasflag(int cn);
bool refillteams(bool now = false, int ftr = FTR_AUTOTEAM);
void changeclientrole(int client, int role, char *pwd = NULL, bool force=false);
mapstats *getservermapstats(const char *mapname, bool getlayout = false, int *maploc = NULL);
int findmappath(const char *mapname, char *filename = NULL);
int calcscores();
void recordpacket(int chan, void *data, int len);
void senddisconnectedscores(int cn);
void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(packetbuf &p, int n);
void sendwelcome(client *cl, int chan = 1);
void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1);
int numclients();
bool updateclientteam(int cln, int newteam, int ftr);
void forcedeath(client *cl);
void sendf(int cn, int chan, const char *format, ...);

extern bool isdedicated;
extern string smapname;
extern mapstats smapstats;
extern char *maplayout;

const char *messagenames[SV_NUM] =
{
    "SV_SERVINFO", "SV_WELCOME", "SV_INITCLIENT", "SV_POS", "SV_POSC", "SV_POSN", "SV_TEXT", "SV_TEAMTEXT", "SV_TEXTME", "SV_TEAMTEXTME",
    "SV_SOUND", "SV_VOICECOM", "SV_VOICECOMTEAM", "SV_CDIS",
    "SV_SHOOT", "SV_EXPLODE", "SV_SUICIDE", "SV_AKIMBO", "SV_RELOAD", "SV_SCOPE",
    "SV_GIBDIED", "SV_DIED", "SV_GIBDAMAGE", "SV_DAMAGE", "SV_HITPUSH", "SV_SHOTFX", "SV_THROWNADE",
    "SV_TRYSPAWN", "SV_SPAWNSTATE", "SV_SPAWN", "SV_SPAWNDENY", "SV_FORCEDEATH", "SV_RESUME",
    "SV_DISCSCORES", "SV_TIMEUP", "SV_EDITENT", "SV_ITEMACC",
    "SV_MAPCHANGE", "SV_ITEMSPAWN", "SV_ITEMPICKUP",
    "SV_PING", "SV_PONG", "SV_CLIENTPING", "SV_GAMEMODE",
    "SV_EDITMODE", "SV_EDITH", "SV_EDITT", "SV_EDITS", "SV_EDITD", "SV_EDITE", "SV_NEWMAP",
    "SV_SENDMAP", "SV_RECVMAP", "SV_REMOVEMAP",
    "SV_SERVMSG", "SV_ITEMLIST", "SV_WEAPCHANGE", "SV_PRIMARYWEAP",
    "SV_FLAGACTION", "SV_FLAGINFO", "SV_FLAGMSG", "SV_FLAGCNT",
    "SV_ARENAWIN", "SV_LMSITEM",
    "SV_SETADMIN", "SV_SERVOPINFO",
    "SV_CALLVOTE", "SV_CALLVOTESUC", "SV_CALLVOTEERR", "SV_VOTE", "SV_VOTERESULT",
    "SV_SETTEAM", "SV_TEAMDENY", "SV_SERVERMODE",
    "SV_WHOIS", "SV_WHOISINFO",
    "SV_LISTDEMOS", "SV_SENDDEMOLIST", "SV_GETDEMO", "SV_SENDDEMO", "SV_DEMOPLAYBACK",
    "SV_CONNECT", "SV_SPECTCN",
    "SV_SWITCHNAME", "SV_SWITCHSKIN", "SV_SWITCHTEAM",
    "SV_CLIENT",
    "SV_EXTENSION",
    "SV_MAPIDENT"
};

const char *entnames[MAXENTTYPES] =
{
    "none?", "light", "playerstart",
    "pistol", "ammobox","grenades",
    "health", "armour", "akimbo",
    "mapmodel", "trigger",
    "ladder", "ctf-flag",
    "sound", "clip", "plclip"
};

itemstat ammostats[NUMGUNS] =
{
    {1,  1,   1,   S_ITEMAMMO},   //knife dummy
    {16, 32,  72,  S_ITEMAMMO},   //pistol
    {15, 30,  20,  S_ITEMAMMO},    //rifle (temporary values)
    {14, 28,  21,  S_ITEMAMMO},   //shotgun
    {60, 90,  90,  S_ITEMAMMO},   //subgun
    {10, 20,  15,  S_ITEMAMMO},   //sniper
    {30, 60,  60,  S_ITEMAMMO},   //assault
    {2,  0,   2,   S_ITEMAMMO},   //grenade
    {72, 0,   72,  S_ITEMAKIMBO} //akimbo
};

itemstat powerupstats[I_ARMOUR-I_HEALTH+1] =
{
    {33, 100, 100, S_ITEMHEALTH}, //health
    {50, 100, 100, S_ITEMARMOUR}, //armour
};

guninfo guns[NUMGUNS] =
{
    { "knife",      S_KNIFE,      S_NULL,     0,      500,    50,     0,   0,  1,    1,   1,    0,  0,    0,  0,      0,      0,    1,      false },
    { "pistol",     S_PISTOL,     S_RPISTOL,  1400,   170,    19,     0,   0, 80,   10,   8,    6,  5,    1,  40,     75,     150,  1,      false },
    { "rifle",      S_ASSAULT,    S_RASSAULT, 1950,   500,    55,     0,   0, 60,   50,   10,   4,  4,    2,  100,    60,     150,  1,      false }, // temporary values
    { "shotgun",    S_SHOTGUN,    S_RSHOTGUN, 2400,   1000,   5,      0,   0,  1,   35,   7,    9,  9,    1,  130,    500,    150,  1,      false },
    { "subgun",     S_SUBGUN,     S_RSUBGUN,  1650,   80,     16,     0,   0, 70,   15,   30,   1,  2,    5,  15,     55,     250,  1,      true },
    { "sniper",     S_SNIPER,     S_RSNIPER,  1950,   1500,   85,     0,   0, 60,   50,   5,    4,  4,    1,  100,    500,    100,  1,      false },
    { "assault",    S_ASSAULT,    S_RASSAULT, 2000,   130,    24,     0,   0, 20,   40,   20,   0,  2,    2,  25,     60,     150,  1,      true },
    { "grenade",    S_NULL,       S_NULL,     1000,   650,    200,    20,  6,  1,    1,   1,    3,  1,    0,  0,      0,      0,    3,      false },
    { "pistol",     S_PISTOL,     S_RAKIMBO,  1400,   80,     19,     0,   0, 80,   10,   16,   6,  5,    4,  30,     75,     150,  1,      true },
};

const char *teamnames[TEAM_NUM+1] = {"CLA", "RVSF", "CLA-SPECT", "RVSF-SPECT", "SPECTATOR", "void"};
const char *teamnames_s[TEAM_NUM+1] = {"CLA", "RVSF", "CSPC", "RSPC", "SPEC", "void"};

