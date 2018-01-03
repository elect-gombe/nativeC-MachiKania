//#define WIDTH_X 30 // 横方向文字数
#define WIDTH_X1 30 // 横方向文字数1
#define WIDTH_X2 40 // 横方向文字数2(6ドットフォント利用時)
#define WIDTH_Y 27 // 縦方向文字数
//#define ATTROFFSET (WIDTH_X*WIDTH_Y) // VRAM上のカラーパレット格納位置
#define ATTROFFSET1 (WIDTH_X1*WIDTH_Y) // VRAM上のカラーパレット格納位置1
#define ATTROFFSET2 (WIDTH_X2*WIDTH_Y) // VRAM上のカラーパレット格納位置2

#define G_X_RES 256 // 横方向解像度
#define G_Y_RES 224 // 縦方向解像度
#define G_H_WORD (G_X_RES/4) // 1行当りのワード数(16bit単位)


// 入力ボタンのポート、ビット定義
#define KEYPORT PORTB
#define KEYUP 0x0400
#define KEYDOWN 0x0080
#define KEYLEFT 0x0100
#define KEYRIGHT 0x0200
#define KEYSTART 0x0800
#define KEYFIRE 0x4000

extern volatile char drawing;		//　表示期間中は-1
extern volatile unsigned short drawcount;		//　1画面表示終了ごとに1足す。アプリ側で0にする。
							// 最低1回は画面表示したことのチェックと、アプリの処理が何画面期間必要かの確認に利用。
extern unsigned char TVRAM[]; //テキストビデオメモリ
extern unsigned short *gVRAM; //グラフィックVRAM開始位置のポインタ

extern const unsigned char FontData[],FontData2[]; //フォントパターン定義
extern unsigned char *cursor;
extern unsigned char cursorcolor;
extern unsigned char *fontp;
extern unsigned char twidth; //テキスト1行文字数（30 or 40）

void start_composite(void); //カラーコンポジット出力開始
void stop_composite(void); //カラーコンポジット出力停止
void init_composite(void); //カラーコンポジット出力初期化
void clearscreen(void); //テキスト画面クリア
void set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g); //テキストパレット設定
void set_bgcolor(unsigned char b,unsigned char r,unsigned char g); //バックグランドカラー設定
void set_graphmode(unsigned char m); //グラフィックモード変更
void init_graphic(unsigned short *gvram); //グラフィック機能利用準備
void set_width(unsigned char m); //30文字モード(8ドットフォント)と40文字モード(6ドットフォント)の切り替え

void vramscroll(void);
	//1行スクロール
void vramscrolldown(void);
	//1行逆スクロール
void setcursor(unsigned char x,unsigned char y,unsigned char c);
	//カーソル位置とカラーを設定
void setcursorcolor(unsigned char c);
	//カーソル位置そのままでカラー番号をcに設定
void printchar(unsigned char n);
	//カーソル位置にテキストコードnを1文字表示し、カーソルを1文字進める
void printstr(unsigned char *s);
	//カーソル位置に文字列sを表示
void printnum(unsigned int n);
	//カーソル位置に符号なし整数nを10進数表示
void printnum2(unsigned int n,unsigned char e);
	//カーソル位置に符号なし整数nをe桁の10進数表示（前の空き桁部分はスペースで埋める）
void cls(void);
	//テキスト画面消去し、カーソルを先頭に移動
void startPCG(unsigned char *p,int a);
	// RAMフォント（PCG）の利用開始、pがフォント格納場所、aが0以外でシステムフォントをコピー
void stopPCG(void);
	// RAMフォント（PCG）の利用停止

void g_clearscreen(void);
//グラフィック画面クリア
void g_set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g);
//グラフィック用カラーパレット設定

void g_pset(int x,int y,unsigned int c);
// (x,y)の位置にカラーcで点を描画

void g_putbmpmn(int x,int y,char m,char n,const unsigned char bmp[]);
// 横m*縦nドットのキャラクターを座標x,yに表示
// unsigned char bmp[m*n]配列に、単純にカラー番号を並べる
// カラー番号が0の部分は透明色として扱う

void g_clrbmpmn(int x,int y,char m,char n);
// 縦m*横nドットのキャラクター消去
// カラー0で塗りつぶし

void g_gline(int x1,int y1,int x2,int y2,unsigned int c);
// (x1,y1)-(x2,y2)にカラーcで線分を描画

void g_hline(int x1,int x2,int y,unsigned int c);
// (x1,y)-(x2,y)への水平ラインを高速描画

void g_circle(int x0,int y0,unsigned int r,unsigned int c);
// (x0,y0)を中心に、半径r、カラーcの円を描画

void g_boxfill(int x1,int y1,int x2,int y2,unsigned int c);
// (x1,y1),(x2,y2)を対角線とするカラーcで塗られた長方形を描画

void g_circlefill(int x0,int y0,unsigned int r,unsigned int c);
// (x0,y0)を中心に、半径r、カラーcで塗られた円を描画

void g_putfont(int x,int y,unsigned int c,int bc,unsigned char n);
//8*8ドットのアルファベットフォント表示
//座標（x,y)、カラー番号c
//bc:バックグランドカラー、負数の場合無視
//n:文字番号

void g_printstr(int x,int y,unsigned int c,int bc,unsigned char *s);
//座標(x,y)からカラー番号cで文字列sを表示、bc:バックグランドカラー

void g_printnum(int x,int y,unsigned char c,int bc,unsigned int n);
//座標(x,y)にカラー番号cで数値nを表示、bc:バックグランドカラー

void g_printnum2(int x,int y,unsigned char c,int bc,unsigned int n,unsigned char e);
//座標(x,y)にカラー番号cで数値nを表示、bc:バックグランドカラー、e桁で表示

unsigned int g_color(int x,int y);
//座標(x,y)のVRAM上の現在のパレット番号を返す、画面外は0を返す
