#include <stdint.h>
#include <stdbool.h>

#define MB_MAGIC 0x1BADB002
#define MB_FLAGS 0x00000003
#define MB_CHECK -(MB_MAGIC + MB_FLAGS)

__attribute__((section(".multiboot")))
const unsigned int mb_header[] = { MB_MAGIC, MB_FLAGS, MB_CHECK };

#define W 80
#define H 25
#define VID_MEM 0xB8000
#define MAX_LEN 2000

unsigned int k_stack[4096];

typedef struct { int r; int c; } CursorPos;
CursorPos cur;

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}

void clr_screen(char color) {
    volatile char* v = (volatile char*)VID_MEM;
    for(int i=0;i<W*H;i++){ *v++=' '; *v++=color; }
    cur.r=0; cur.c=0;
}

void scroll_up() {
    volatile char* v = (volatile char*)VID_MEM;
    for(int r=1;r<H;r++)
        for(int c=0;c<W;c++){
            v[2*((r-1)*W+c)] = v[2*(r*W+c)];
            v[2*((r-1)*W+c)+1] = v[2*(r*W+c)+1];
        }
    for(int c=0;c<W;c++){ v[2*((H-1)*W+c)]=' '; v[2*((H-1)*W+c)+1]=0x0F; }
    if(cur.r>0) cur.r--;
}

void put_char(char ch, char color){
    if(ch=='\n'){ cur.c=0; cur.r++; if(cur.r>=H) scroll_up(); return; }
    volatile char* v = (volatile char*)VID_MEM + 2*(cur.r*W + cur.c);
    *v++=ch; *v++=color;
    cur.c++;
    if(cur.c>=W){ cur.c=0; cur.r++; if(cur.r>=H) scroll_up();}
}

void print_str(const char* s, char color){ while(*s) put_char(*s++,color); }

void print_num(int n,char color){
    if(n==0){ put_char('0',color); return; }
    char buf[10]; int i=0;
    while(n>0){ buf[i++]='0'+(n%10); n/=10; }
    for(int j=i-1;j>=0;j--) put_char(buf[j],color);
}

typedef struct { int x,y; } Pos;
typedef struct { Pos body[MAX_LEN]; int len; int dx,dy; } Snake;

static Snake sn;
static Pos food;
static int rnd_state = 0x1337;
static int scr = 0;

int rnd(){
    rnd_state = rnd_state*1103515245 + 12345;
    return (rnd_state>>16) & 0x7FFF;
}

bool same_pos(Pos a, Pos b){ return a.x==b.x && a.y==b.y; }

void draw_pos(Pos p,char ch,char color){
    if(p.x>=0 && p.x<W && p.y>=0 && p.y<H){
        volatile char* v=(volatile char*)VID_MEM + 2*(p.y*W + p.x);
        *v++=ch; *v++=color;
    }
}

void spawn_food(){
    Pos pnt; bool ok;
    do {
        pnt.x = rnd() % W;
        pnt.y = rnd() % H;
        ok = true;
        for(int i=0;i<sn.len;i++){
            if(same_pos(sn.body[i],pnt)){ ok=false; break; }
        }
    } while(!ok);
    food = pnt;
}

bool hit_self(){
    for(int i=1;i<sn.len;i++) if(same_pos(sn.body[0],sn.body[i])) return true;
    return false;
}

bool hit_wall(){
    Pos h = sn.body[0];
    return h.x<0 || h.x>=W || h.y<0 || h.y>=H;
}

void move_snake(){
    for(int i=sn.len-1;i>0;i--) sn.body[i]=sn.body[i-1];
    sn.body[0].x+=sn.dx;
    sn.body[0].y+=sn.dy;
}

void draw_snake(){
    for(int i=0;i<sn.len;i++) draw_pos(sn.body[i],'O',0x0A);
}

void draw_score(){
    cur.r=0; cur.c=0;
    print_str("Score: ",0x0E);
    print_num(scr,0x0E);
}

#define UP    0x48
#define DOWN  0x50
#define LEFT  0x4B
#define RIGHT 0x4D
#define W_KEY 0x11
#define S_KEY 0x1F
#define A_KEY 0x1E
#define D_KEY 0x20
#define SPACE 0x39

void handle_input(uint8_t* lk){
    if(inb(0x64)&1){
        uint8_t k = inb(0x60) & 0x7F;
        *lk = k;
        if(k==W_KEY && sn.dy!=1){ sn.dx=0; sn.dy=-1; }
        else if(k==S_KEY && sn.dy!=-1){ sn.dx=0; sn.dy=1; }
        else if(k==A_KEY && sn.dx!=1){ sn.dx=-1; sn.dy=0; }
        else if(k==D_KEY && sn.dx!=-1){ sn.dx=1; sn.dy=0; }
        else if(k==UP && sn.dy!=1){ sn.dx=0; sn.dy=-1; }
        else if(k==DOWN && sn.dy!=-1){ sn.dx=0; sn.dy=1; }
        else if(k==LEFT && sn.dx!=1){ sn.dx=-1; sn.dy=0; }
        else if(k==RIGHT && sn.dx!=-1){ sn.dx=1; sn.dy=0; }
    }
}

void game_loop(){
    clr_screen(0x0F);
    sn.len=3;
    sn.body[0].x=W/2; sn.body[0].y=H/2;
    sn.body[1].x=W/2-1; sn.body[1].y=H/2;
    sn.body[2].x=W/2-2; sn.body[2].y=H/2;
    sn.dx=1; sn.dy=0;
    scr=0;

    spawn_food();
    int frame=0;
    uint8_t lk=0;

    while(1){
        frame++;
        handle_input(&lk);

        int spd = (sn.dy!=0) ? 1300 : 700;

        if(frame%spd==0){
            move_snake();
            if(hit_wall() || hit_self()){
                print_str("\nGAME OVER\nPress SPACE to restart\n",0x0C);
                while(1){
                    handle_input(&lk);
                    if(lk==SPACE){ game_loop(); return; }
                }
            }

            if(same_pos(sn.body[0], food)){
                sn.len++;
                if(sn.len>MAX_LEN) sn.len=MAX_LEN;
                scr++;
                spawn_food();
            }

            clr_screen(0x0F);
            draw_pos(food,'X',0x0C);
            draw_snake();
            draw_score();
        }

        for(volatile int i=0;i<10000;i++);
    }
}

void kernel_main(){
    print_str("Snake Game Starting...\n",0x0A);
    game_loop();
}

__attribute__((naked)) void _start(){
    asm volatile(
        "movl $k_stack + 4096,%esp\n"
        "call kernel_main\n"
        "cli\n"
        "hlt\n"
    );
}
