// PS/2キーボード読み込みプログラム for PIC32MX by K.Tanaka　Rev1.5　2017.5.6
//  Rev 1.2 104英語キーボードサポート、Pause/Breakキーサポート
//  Rev 1.3 初期化時に日本語/英語キーボード種類、Lockキー状態指定に対応
//  Rev 1.4 コマンドタイムアウト延長
//  Rev 1.5 データ受信にDMA利用、PIC32MX370F版のポート変更
//
// PIC32MX1xx/2xx版  DAT:RB8、CLK:RB9
// PIC32MX370F512H版 DAT:RF0、CLK:RF1
//

#include "ps2keyboard.h"
#include <plib.h>

// PIC32MX1xx/2xxで利用する場合は、以下の行をコメントアウトする
//#define PIC32MX370F

#ifdef PIC32MX370F
#define PS2PORT PORTF
#define PS2DATBIT  0x01 // RF0
#define PS2CLKBIT  0x02 // RF1
#define PORTPS2DAT PORTFbits.RF0
#define PORTPS2CLK PORTFbits.RF1
#define mPS2DATSET() (LATFSET=PS2DATBIT)
#define mPS2DATCLR() (LATFCLR=PS2DATBIT)
#define mPS2CLKSET()  (LATFSET=PS2CLKBIT)
#define mPS2CLKCLR()  (LATFCLR=PS2CLKBIT)
#define mTRISPS2DATIN()  (TRISFSET=PS2DATBIT)
#define mTRISPS2DATOUT() (TRISFCLR=PS2DATBIT)
#define mTRISPS2CLKIN()  (TRISFSET=PS2CLKBIT)
#define mTRISPS2CLKOUT() (TRISFCLR=PS2CLKBIT)
#define mPS2CLKIntEnable(ie) (mCNFIntEnable(ie))
#define TIMER5_100us 597

#else
#define PS2PORT PORTB
#define PS2DATBIT  0x100 // RB8
#define PS2CLKBIT  0x200 // RB9
#define PORTPS2DAT PORTBbits.RB8
#define PORTPS2CLK PORTBbits.RB9
#define mPS2DATSET() (LATBSET=PS2DATBIT)
#define mPS2DATCLR() (LATBCLR=PS2DATBIT)
#define mPS2CLKSET()  (LATBSET=PS2CLKBIT)
#define mPS2CLKCLR()  (LATBCLR=PS2CLKBIT)
#define mTRISPS2DATIN()  (TRISBSET=PS2DATBIT)
#define mTRISPS2DATOUT() (TRISBCLR=PS2DATBIT)
#define mTRISPS2CLKIN()  (TRISBSET=PS2CLKBIT)
#define mTRISPS2CLKOUT() (TRISBCLR=PS2CLKBIT)
#define mPS2CLKIntEnable(ie) (mCNBIntEnable(ie))
#define TIMER5_100us 358

#endif

#define SCANCODEBUFSIZE 16 //スキャンコードバッファのサイズ
#define KEYCODEBUFSIZE 16 //キーコードバッファのサイズ

// 送受信エラーコード
#define PS2ERROR_PARITY 1
#define PS2ERROR_STARTBIT 2
#define PS2ERROR_STOPBIT 3
#define PS2ERROR_BUFFERFUL 4
#define PS2ERROR_TIMEOUT 5
#define PS2ERROR_TOOMANYRESENDREQ 6
#define PS2ERROR_NOACK 7
#define PS2ERROR_NOBAT 8

//コマンド送信状況を表すps2statusの値
#define PS2STATUS_SENDSTART 1
#define PS2STATUS_WAIT100us 2
#define PS2STATUS_SENDING 3
#define PS2STATUS_WAITACK 4
#define PS2STATUS_WAITBAT 5
#define PS2STATUS_INIT 6

#define PS2TIME_CLKDOWN 2     //コマンド送信時のCLKをLにする時間、200us(PS2STATUS_WAIT100usの時間)
#define PS2TIME_PRESEND 2     //コマンド送信前の待ち時間、200us
#define PS2TIME_CMDTIMEOUT 110 //コマンド送信タイムアウト時間、11ms
#define PS2TIME_ACKTIMEOUT 50 //ACK応答タイムアウト時間、5000us
#define PS2TIME_BATTIMEOUT 5000 //BAT応答タイムアウト時間、500ms
#define PS2TIME_INIT 5000    //電源オン後の起動時間、500ms
#define PS2TIME_RECEIVETIMEOUT 20 //データ受信タイムアウト時間、2000us

// PS/2コマンド
#define PS2CMD_RESET 0xff //キーボードリセット
#define PS2CMD_SETLED 0xed //ロックLED設定
#define PS2CMD_RESEND 0xfe //再送要求
#define PS2CMD_ACK 0xfa //ACK応答
#define PS2CMD_BAT 0xaa //テスト正常終了
#define PS2CMD_ECHO 0xee //エコー
#define PS2CMD_OVERRUN 0x00 //バッファオーバーラン
#define PS2CMD_BREAK 0xf0 //ブレイクプリフィックス

#define PS2DMABUFSIZE 16 // DMAバッファサイズ
#define mPS2DMAEnable() (DCH3CONSET=0x80) // DMA有効化
#define mPS2DMADisable() (DCH3CONCLR=0x80) // DMA無効化

// グローバル変数定義
volatile unsigned char keyboard_rcvdata[PS2DMABUFSIZE];//DMA受信用バッファ
volatile unsigned int dma_readpt;//DMA受信バッファの読み込み済みの位置

unsigned int ps2sendcomdata;//送信コマンドバッファ（最大4バイト、下位から順に）
unsigned char ps2sendcombyte;//送信コマンドの未送信バイト数
unsigned char volatile ps2status;//送信、受信のフェーズ
unsigned int volatile ps2statuscount;//ps2statusのカウンター。Timer5割り込みごとにカウントダウン
unsigned int volatile ps2receivetime;//受信中のタイマー
unsigned char receivecount;//受信中ビットカウンタ
unsigned char receivedata;//受信中データ
unsigned char sendcount;//送信中ビットカウンタ
unsigned short senddata;//送信中データ
unsigned char volatile ps2sending; //PICからKBに送信中
unsigned char volatile ps2receiveError; //読み込み時エラー
unsigned char volatile ps2sendError; //コマンド送信時エラー
unsigned char volatile ps2receivecommand;//受信コマンド
unsigned char volatile ps2receivecommandflag;//コマンド受信あり
unsigned short volatile ps2shiftkey_a; //シフト、コントロールキー等の状態（左右キー区別）
unsigned char volatile ps2shiftkey; //シフト、コントロールキー等の状態（左右キー区別なし）

unsigned char scancodebuf[SCANCODEBUFSIZE]; //スキャンコードバッファ
unsigned char * volatile scancodebufp1; //スキャンコード書き込み先頭ポインタ
unsigned char * volatile scancodebufp2; //スキャンコード読み出し先頭ポインタ
unsigned short keycodebuf[KEYCODEBUFSIZE]; //キーコードバッファ
unsigned short * volatile keycodebufp1; //キーコード書き込み先頭ポインタ
unsigned short * volatile keycodebufp2; //キーコード読み出し先頭ポインタ

//公開変数
unsigned volatile char ps2keystatus[256]; // 仮想コードに相当するキーの状態（Onの時1）
unsigned volatile short vkey; // ps2readkey()関数でセットされるキーコード、上位8ビットはシフト関連キー
unsigned char lockkey; // 初期化時にLockキーの状態指定。下位3ビットが<CAPSLK><NUMLK><SCRLK>
unsigned char keytype; // キーボードの種類。0：日本語109キー、1：英語104キー

// 日本語109key配列
const unsigned char ps2scan2vktable1_jp[]={
	// PS/2スキャンコードから仮想キーコードへの変換テーブル
	// 0x00-0x83まで
	0,VK_F9,0,VK_F5,VK_F3,VK_F1,VK_F2,VK_F12,0,VK_F10,VK_F8,VK_F6,VK_F4,VK_TAB,VK_KANJI,0,
	0,VK_LMENU,VK_LSHIFT,VK_KANA,VK_LCONTROL,'Q','1',0,0,0,'Z','S','A','W','2',0,
	0,'C','X','D','E','4','3',0,0,VK_SPACE,'V','F','T','R','5',0,
	0,'N','B','H','G','Y','6',0,0,0,'M','J','U','7','8',0,
	0,VK_OEM_COMMA,'K','I','O','0','9',0,0,VK_OEM_PERIOD,VK_OEM_2,'L',VK_OEM_PLUS,'P',VK_OEM_MINUS,0,
	0,VK_OEM_102,VK_OEM_1,0,VK_OEM_3,VK_OEM_7,0,0,VK_CAPITAL,VK_RSHIFT,VK_RETURN,VK_OEM_4,0,VK_OEM_6,0,0,
	0,0,0,0,VK_CONVERT,0,VK_BACK,VK_NONCONVERT,0,VK_NUMPAD1,VK_OEM_5,VK_NUMPAD4,VK_NUMPAD7,0,0,0,
	VK_NUMPAD0,VK_DECIMAL,VK_NUMPAD2,VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD8,VK_ESCAPE,VK_NUMLOCK,VK_F11,VK_ADD,VK_NUMPAD3,VK_SUBTRACT,VK_MULTIPLY,VK_NUMPAD9,VK_SCROLL,0,
	0,0,0,VK_F7
};
const unsigned char ps2scan2vktable2_jp[]={
	// PS/2スキャンコードから仮想キーコードへの変換テーブル(E0プリフィックス)
	// 0x00-0x83まで
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,VK_RMENU,/*VK_LSHIFT*/ 0,0,VK_RCONTROL,0,0,0,0,0,0,0,0,0,0,VK_LWIN,
	0,0,0,0,0,0,0,VK_RWIN,0,0,0,0,0,0,0,VK_APPS,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,VK_DIVIDE,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,VK_SEPARATOR,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,VK_END,0,VK_LEFT,VK_HOME,0,0,0,
	VK_INSERT,VK_DELETE,VK_DOWN,0,VK_RIGHT,VK_UP,0,0,0,0,VK_NEXT,0,VK_SNAPSHOT,VK_PRIOR,VK_CANCEL,0,
	0,0,0,0
};
const unsigned char vk2asc1_jp[]={
	// 仮想キーコードからASCIIコードへの変換テーブル（SHIFTなし）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	'0' ,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,'a' ,'b' ,'c' ,'d' ,'e' ,'f' ,'g' ,'h' ,'i' ,'j' ,'k' ,'l' ,'m' ,'n' ,'o' ,
	'p' ,'q' ,'r' ,'s' ,'t' ,'u' ,'v' ,'w' ,'x' ,'y' ,'z' ,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'*' ,'+' ,0x00,'-' ,0x00,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,':' ,';' ,',' ,'-', '.' ,'/' ,
	'@' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'[' ,'\\',']' ,'^' ,0x00,
	0x00,0x00,'\\',0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char vk2asc2_jp[]={
	// 仮想キーコードからASCIIコードへの変換テーブル（SHIFTあり）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,'!' ,0x22,'#' ,'$' ,'%' ,'&' ,0x27,'(' ,')' ,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,'A' ,'B' ,'C' ,'D' ,'E' ,'F' ,'G' ,'H' ,'I' ,'J' ,'K' ,'L' ,'M' ,'N' ,'O' ,
	'P' ,'Q' ,'R' ,'S' ,'T' ,'U' ,'V' ,'W' ,'X' ,'Y' ,'Z' ,0x00,0x00,0x00,0x00,0x00,
	'0' ,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,'*' ,'+' ,0x00,'-' ,'.' ,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'*' ,'+' ,'<' ,'=' ,'>' ,'?' ,
	'`' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'{' ,'|' ,'}' ,'~' ,0x00,
	0x00,0x00,'_' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char vk2kana1[]={
	// 仮想キーコードからカナコードへの変換テーブル（SHIFTなし）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xdc,0xc7,0xcc,0xb1,0xb3,0xb4,0xb5,0xd4,0xd5,0xd6,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0xc1,0xba,0xbf,0xbc,0xb2,0xca,0xb7,0xb8,0xc6,0xcf,0xc9,0xd8,0xd3,0xd0,0xd7,
	0xbe,0xc0,0xbd,0xc4,0xb6,0xc5,0xcb,0xc3,0xbb,0xdd,0xc2,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'*' ,'+' ,0x00,'-' ,0x00,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xb9,0xda,0xc8,0xce,0xd9,0xd2,
	0xde,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xdf,0xb0,0xd1,0xcd,0x00,
	0x00,0x00,0xdb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char vk2kana2[]={
	// 仮想キーコードからカナコードへの変換テーブル（SHIFTあり）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xa6,0xc7,0xcc,0xa7,0xa9,0xaa,0xab,0xac,0xad,0xae,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0xc1,0xba,0xbf,0xbc,0xa8,0xca,0xb7,0xb8,0xc6,0xcf,0xc9,0xd8,0xd3,0xd0,0xd7,
	0xbe,0xc0,0xbd,0xc4,0xb6,0xc5,0xcb,0xc3,0xbb,0xdd,0xaf,0x00,0x00,0x00,0x00,0x00,
	'0', '1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,'*' ,'+' ,0x00,'-' ,'.' ,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xb9,0xda,0xa4,0xce,0xa1,0xa5,
	0xde,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa2,0xb0,0xa3,0xcd,0x00,
	0x00,0x00,0xdb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// 英語104key配列
const unsigned char ps2scan2vktable1_en[]={
	// PS/2スキャンコードから仮想キーコードへの変換テーブル
	// 0x00-0x83まで
	0,VK_F9,0,VK_F5,VK_F3,VK_F1,VK_F2,VK_F12,0,VK_F10,VK_F8,VK_F6,VK_F4,VK_TAB,VK_OEM_3,0,
	0,VK_LMENU,VK_LSHIFT,0,VK_LCONTROL,'Q','1',0,0,0,'Z','S','A','W','2',0,
	0,'C','X','D','E','4','3',0,0,VK_SPACE,'V','F','T','R','5',0,
	0,'N','B','H','G','Y','6',0,0,0,'M','J','U','7','8',0,
	0,VK_OEM_COMMA,'K','I','O','0','9',0,0,VK_OEM_PERIOD,VK_OEM_2,'L',VK_OEM_1,'P',VK_OEM_MINUS,0,
	0,0,VK_OEM_7,0,VK_OEM_4,VK_OEM_PLUS,0,0,VK_CAPITAL,VK_RSHIFT,VK_RETURN,VK_OEM_6,0,VK_OEM_5,0,0,
	0,0,0,0,0,0,VK_BACK,0,0,VK_NUMPAD1,0,VK_NUMPAD4,VK_NUMPAD7,0,0,0,
	VK_NUMPAD0,VK_DECIMAL,VK_NUMPAD2,VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD8,VK_ESCAPE,VK_NUMLOCK,VK_F11,VK_ADD,VK_NUMPAD3,VK_SUBTRACT,VK_MULTIPLY,VK_NUMPAD9,VK_SCROLL,0,
	0,0,0,VK_F7
};
const unsigned char ps2scan2vktable2_en[]={
	// PS/2スキャンコードから仮想キーコードへの変換テーブル(E0プリフィックス)
	// 0x00-0x83まで
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,VK_RMENU,/*VK_LSHIFT*/ 0,0,VK_RCONTROL,0,0,0,0,0,0,0,0,0,0,VK_LWIN,
	0,0,0,0,0,0,0,VK_RWIN,0,0,0,0,0,0,0,VK_APPS,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,VK_DIVIDE,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,VK_SEPARATOR,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,VK_END,0,VK_LEFT,VK_HOME,0,0,0,
	VK_INSERT,VK_DELETE,VK_DOWN,0,VK_RIGHT,VK_UP,0,0,0,0,VK_NEXT,0,VK_SNAPSHOT,VK_PRIOR,VK_CANCEL,0,
	0,0,0,0
};
const unsigned char vk2asc1_en[]={
	// 仮想キーコードからASCIIコードへの変換テーブル（SHIFTなし）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	'0' ,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,'a' ,'b' ,'c' ,'d' ,'e' ,'f' ,'g' ,'h' ,'i' ,'j' ,'k' ,'l' ,'m' ,'n' ,'o' ,
	'p' ,'q' ,'r' ,'s' ,'t' ,'u' ,'v' ,'w' ,'x' ,'y' ,'z' ,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'*' ,'+' ,0x00,'-' ,0x00,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,';' ,'=' ,',' ,'-' ,'.' ,'/' ,
	'`' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'[' ,'\\',']' ,0x27,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char vk2asc2_en[]={
	// 仮想キーコードからASCIIコードへの変換テーブル（SHIFTあり）
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	' ' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	')' ,'!' ,'@' ,'#' ,'$' ,'%' ,'^' ,'&' ,'*' ,'(' ,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,'A' ,'B' ,'C' ,'D' ,'E' ,'F' ,'G' ,'H' ,'I' ,'J' ,'K' ,'L' ,'M' ,'N' ,'O' ,
	'P' ,'Q' ,'R' ,'S' ,'T' ,'U' ,'V' ,'W' ,'X' ,'Y' ,'Z' ,0x00,0x00,0x00,0x00,0x00,
	'0' ,'1' ,'2' ,'3' ,'4' ,'5' ,'6' ,'7' ,'8' ,'9' ,'*' ,'+' ,0x00,'-' ,'.' ,'/' ,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,':' ,'+' ,'<' ,'_' ,'>' ,'?' ,
	'~' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'{' ,'|' ,'}' ,0x22,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void ps2receive(unsigned int data)
{
	unsigned char d;
	unsigned char c;

	if(ps2receiveError) return; // エラー発生中は残りビット無視
#ifdef PIC32MX370F
	if(data & PS2CLKBIT) return; //CLK立ち上がりは無視
	d=data & PS2DATBIT;
#else
	if(data & (PS2CLKBIT>>8)) return; //CLK立ち上がりは無視
	d=data & (PS2DATBIT>>8);
#endif
	if(receivecount==0){
		//スタートビット
		if(d){
			//スタートビットエラー
			ps2receiveError=PS2ERROR_STARTBIT;
			return;
		}
		else{
			//バイト受信開始
			receivecount++;
			ps2receivetime=0;//受信タイマーカウント開始
		}
	}
	else if(receivecount<=8){
		//データ
		receivedata=(receivedata>>1)+(d<<7);
		receivecount++;
	}
	else if(receivecount==9){
		//パリティ
		c=receivedata;
		while(c){
			d^=c;
			c>>=1;
		}
		if((d&1)==0){
			//パリティエラー
			ps2receiveError=PS2ERROR_PARITY;
			return;
		}
		else receivecount++;
	}
	else{
		//ストップビット
		if(d==0){
			//ストップビットエラー
			ps2receiveError=PS2ERROR_STOPBIT;
			return;
		}
		//読み込み完了
		receivecount=0;
		if(receivedata==0 || receivedata==PS2CMD_BAT ||
			receivedata==PS2CMD_ECHO || receivedata>=PS2CMD_ACK){
			//キーボードからのコマンド受信の場合
			ps2receivecommand=receivedata;
			ps2receivecommandflag=1;
		}
		else{
			//データ受信の場合
			if((scancodebufp1+1==scancodebufp2) ||
				(scancodebufp1==scancodebuf+SCANCODEBUFSIZE-1)&&(scancodebufp2==scancodebuf)){
				//バッファフル
				ps2receiveError=PS2ERROR_BUFFERFUL;
				return;
			}
			*scancodebufp1++=receivedata;
			if(scancodebufp1>=scancodebuf+SCANCODEBUFSIZE) scancodebufp1=scancodebuf;
		}
	}
}
void ps2send()
{
	if(PORTPS2CLK){
		//CLK立ち上がり時
		if(sendcount==10){
			mTRISPS2DATIN(); //DATを入力に設定
		}
		else if(sendcount==11){
			ps2sending=0;
			mPS2CLKIntEnable(0); //CLKによる割り込み無効化
			mPS2DMAEnable(); //DMA有効化
		}
		return;
	}
// 以下CLK立下り時処理
	sendcount++;
	if(sendcount<=10){
		//データ出力
		if(senddata & 1) mPS2DATSET();
		else mPS2DATCLR();
		senddata>>=1;
	}
/*
	else{
		//ここでDAT入力=L（ACK）となっているはず
		//あえてチェックはしない
	}
*/
}

#ifdef PIC32MX370F
void __ISR(33, ipl6) CNHandler(void)
{
	//CN割り込みハンドラ
	unsigned int cnstat;
	unsigned int volatile port; //dummy

	cnstat=CNSTATF;
	if(cnstat){
		port=PS2PORT; //dummy read
		if(cnstat & PS2CLKBIT){
			// CLKが変化した時
			ps2send(); //コマンド送信中のCLK変化
		}
		mCNFClearIntFlag(); // CNF割り込みフラグクリア
	}
}
#else
void __ISR(34, ipl6) CNHandler(void)
{
	//CN割り込みハンドラ
	unsigned int cnstat;
	unsigned int volatile port; //dummy

	cnstat=CNSTATB;
	if(cnstat){
		port=PS2PORT; //dummy read
		if(cnstat & PS2CLKBIT){
			// CLKが変化した時
			ps2send(); //コマンド送信中のCLK変化
		}
		mCNBClearIntFlag(); // CNB割り込みフラグクリア
	}
}
#endif

void getps2DMABUFdata(void) {
	// DMAで転送されたデータを1ビットずつ読み込み
	while (dma_readpt != DCH3DPTR) {
		ps2receive(keyboard_rcvdata[dma_readpt]); //1ビット分処理
		dma_readpt++;
		if (dma_readpt == PS2DMABUFSIZE) dma_readpt = 0;
	}
}


void ps2statusprogress(){
	//コマンド送信状態で100マイクロ秒ごとに呼び出し
	//ps2statusで送信状況を表す
	unsigned int parity;
	unsigned char c;
	unsigned int volatile dummy;

	ps2statuscount--;
	switch(ps2status){
		case PS2STATUS_INIT:
			//システム起動時の安定待ち
			if(ps2statuscount==0) ps2status=0;
			return;

		case PS2STATUS_SENDSTART:
			//コマンド送信スタート
			if(ps2statuscount) return;//コマンド送信前待ち時間
			mPS2CLKIntEnable(0); //CLKによる割り込み無効化
			mPS2DMADisable(); //DMA無効化
			mTRISPS2CLKOUT();//CLKを出力に設定
			asm volatile ("nop");
			mPS2CLKCLR(); //CLK=L、ここから100us以上継続させる
			//奇数パリティ生成
			c=ps2sendcomdata;
			parity=1;
			while(c){
				parity^=c;
				c>>=1;
			}
			//送信データ（data,parity,stop bit）
			senddata=0x200+((parity&1)<<8)+(ps2sendcomdata & 0xff);
			receivecount=0;//もし受信途中なら強制終了
			ps2sending=1;//送信中フラグ
			sendcount=0;//送信カウンタ
			ps2status=PS2STATUS_WAIT100us;
			ps2statuscount=PS2TIME_CLKDOWN;
			return;

		case PS2STATUS_WAIT100us:
			//CLK=Lにして100us待ち状態
			if(ps2statuscount) return;//100us経過していない場合
			mTRISPS2DATOUT();//DATを出力に設定
			asm volatile ("nop");
			mPS2DATCLR(); //start bit:0
			mTRISPS2CLKIN();//CLKを入力に設定
			asm volatile ("nop"); //1クロック以上のウェイト必要
			dummy=PS2PORT;//割り込み無効化中のポート変化を吸収
			mPS2CLKIntEnable(1); //CLK変化による割り込み有効化
			ps2status=PS2STATUS_SENDING; //次は送信完了まで待つ
			ps2statuscount=PS2TIME_CMDTIMEOUT; //タイムアウトエラーの設定
			return;

		case PS2STATUS_SENDING:
			if(ps2sending==0){
				//送信完了
				c=ps2sendcomdata;
				if(c==PS2CMD_RESEND){
					//再送要求コマンド送信の場合は通常受信状態に戻す
					ps2status=0;
				}
				else{
					ps2status=PS2STATUS_WAITACK;//次はACK応答待ち
					ps2statuscount=PS2TIME_ACKTIMEOUT;
					ps2receiveError=0;
				}
			}
			else if(ps2statuscount==0){
				//タイムアウト処理
				ps2sendError=PS2ERROR_TIMEOUT;
				ps2sending=0; //通常受信状態に戻す
				mTRISPS2DATIN(); //DATを入力に設定
				ps2status=0;
				mPS2CLKIntEnable(0); //CLKによる割り込み無効化
				mPS2DMAEnable(); //DMA有効化
			}
			return;

		case PS2STATUS_WAITACK:
			//ACK応答待ち
			if(ps2receivecommandflag){
				//コマンド受信あり
				ps2receivecommandflag=0;
				if(ps2receivecommand==PS2CMD_RESEND){
					//再送要求だった場合、最初から再送する
					ps2status=PS2STATUS_SENDSTART;
					ps2statuscount=PS2TIME_PRESEND;
					ps2sending=1;
					return;
				}
				else if(ps2receivecommand==PS2CMD_ACK){
					//ACK受信完了
					c=ps2sendcomdata;
					if(c==PS2CMD_RESET){
						//リセットコマンドの場合、BAT返信待ち
						ps2sendcombyte=0;//コマンド送信バッファクリア
						ps2status=PS2STATUS_WAITBAT;
						ps2statuscount=PS2TIME_BATTIMEOUT;
						ps2receiveError=0;
						return;
					}
				}
				else {
					//ACKではなかった場合
					//ECHOなどの処理はここに記述
				}
				if(--ps2sendcombyte){
					//コマンドが残っている場合、次のバイト送信
					ps2sendcomdata>>=8;
					ps2status=PS2STATUS_SENDSTART;
					ps2statuscount=PS2TIME_PRESEND;
					ps2sending=1;
					return;
				}
				//全てのコマンド送受信完了
				ps2status=0;
				ps2sendcombyte=0;
				return;
			}
			else if(ps2statuscount==0){
				//応答なし、タイムアウト
				ps2sendError=PS2ERROR_NOACK;
				ps2status=0;
				ps2sendcombyte=0;
			}
			return;

		case PS2STATUS_WAITBAT:
			if(ps2receivecommandflag){
				//コマンド受信あり
				ps2receivecommandflag=0;
				if(ps2receivecommand!=PS2CMD_BAT){
					//BAT（セルフテスト）失敗の場合
					ps2sendError=PS2ERROR_NOBAT;
				}
				ps2status=0;
			}
			else if(ps2statuscount==0){
				//BAT完了せずタイムアウト
				ps2sendError=PS2ERROR_NOBAT;
				ps2status=0;
			}
			return;
	}
}
void ps2command(unsigned int d,unsigned char n){
// PS/2キーボードに対してコマンド送信
// d:コマンド
// n:コマンドのバイト数（dは下位から8ビットずつ）

	if(ps2status) return; //コマンド送信中の場合、あきらめる

	mPS2CLKIntEnable(0); //CLKによる割り込み無効化
	mPS2DMADisable(); //DMA無効化
	getps2DMABUFdata(); //受信中のデータ破棄
	ps2receivecommandflag=0; //コマンド受信フラグクリア
	ps2sendcomdata=d;
	ps2sendcombyte=n;
	ps2sendError=0;
	ps2status=PS2STATUS_SENDSTART; //次のTimer5割り込みで送信開始
	ps2statuscount=PS2TIME_PRESEND;
}

void shiftkeycheck(unsigned char vk,unsigned char breakflag){
// SHIFT,ALT,CTRL,Winキーの押下状態を更新
	unsigned short k;
	k=0;
	switch(vk){
		case VK_SHIFT:
		case VK_LSHIFT:
			k=CHK_SHIFT_L;
			break;
		case VK_RSHIFT:
			k=CHK_SHIFT_R;
			break;
		case VK_CONTROL:
		case VK_LCONTROL:
			k=CHK_CTRL_L;
			break;
		case VK_RCONTROL:
			k=CHK_CTRL_R;
			break;
		case VK_MENU:
		case VK_LMENU:
			k=CHK_ALT_L;
			break;
		case VK_RMENU:
			k=CHK_ALT_R;
			break;
		case VK_LWIN:
			k=CHK_WIN_L;
			break;
		case VK_RWIN:
			k=CHK_WIN_R;
			break;
	}
	if(breakflag) ps2shiftkey_a &= ~k;
	else ps2shiftkey_a |= k;

	ps2shiftkey &= CHK_SCRLK | CHK_NUMLK | CHK_CAPSLK;
	if(ps2shiftkey_a & (CHK_SHIFT_L | CHK_SHIFT_R)) ps2shiftkey|=CHK_SHIFT;
	if(ps2shiftkey_a & (CHK_CTRL_L | CHK_CTRL_R)) ps2shiftkey|=CHK_CTRL;
	if(ps2shiftkey_a & (CHK_ALT_L | CHK_ALT_R)) ps2shiftkey|=CHK_ALT;
	if(ps2shiftkey_a & (CHK_WIN_L | CHK_WIN_R)) ps2shiftkey|=CHK_WIN;
}
void lockkeycheck(unsigned char vk){
// NumLock,CapsLock,ScrollLockの状態更新し、インジケータコマンド発行
	switch(vk){
		case VK_SCROLL:
		case VK_KANA:
			ps2shiftkey_a^=CHK_SCRLK_A;
			ps2shiftkey  ^=CHK_SCRLK;
			break;
		case VK_NUMLOCK:
			ps2shiftkey_a^=CHK_NUMLK_A;
			ps2shiftkey  ^=CHK_NUMLK;
			break;
		case VK_CAPITAL:
			if((ps2shiftkey & CHK_SHIFT)==0) return;
			ps2shiftkey_a^=CHK_CAPSLK_A;
			ps2shiftkey  ^=CHK_CAPSLK;
			break;
		default:
			return;
	}
	ps2command(PS2CMD_SETLED+(ps2shiftkey_a & 0xff00),2); // キーボードのインジケータ設定
}
int isShiftkey(unsigned char vk){
// SHIFT,ALT,WIN,CTRLが押されたかのチェック（押された場合-1を返す）
	switch(vk){
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
		case VK_MENU:
		case VK_LMENU:
		case VK_RMENU:
		case VK_LWIN:
		case VK_RWIN:
			return -1;
		default:
			return 0;
	}
}
int isLockkey(unsigned char vk){
// NumLock,SCRLock,CapsLockが押されたかのチェック（押された場合-1を返す）
	switch(vk){
		case VK_SCROLL:
		case VK_NUMLOCK:
		case VK_CAPITAL:
		case VK_KANA:
			return -1;
		default:
			return 0;
	}
}
void readscancode(){
// scancodebufからスキャンコードを読み出し、キーコードに変換
// keycodebufにためる
	int e0flag,breakflag;
	unsigned char d;
	unsigned char vk;
	unsigned char *p;

	e0flag=0;
	breakflag=0;
	p=scancodebufp2;
	while(1){
		//プリフィックス（E0、E1、ブレイク）が終わるまでループ
		if(p==scancodebufp1) return;//まだ不完全にしかバッファにそろっていない
		d=*p++;
		if(p==scancodebuf+SCANCODEBUFSIZE) p=scancodebuf;
		if(d==PS2CMD_BREAK) breakflag=1;
		else if(d==0xe0) e0flag=1;
		else if(d!=0xe1) break; //E1は無視
	}
	scancodebufp2=p;
	if(d>0x83) return; //未対応スキャンコード
	if(e0flag){
		if(keytype==1) vk=ps2scan2vktable2_en[d];
		else vk=ps2scan2vktable2_jp[d];
	}
	else{
		if(keytype==1) vk=ps2scan2vktable1_en[d];
		else vk=ps2scan2vktable1_jp[d];
	}
	if(vk==0) return; //未対応スキャンコード

	if(isShiftkey(vk)){
		if(breakflag==0 && ps2keystatus[vk]) return; //キーリピートの場合、無視
		shiftkeycheck(vk,breakflag); //SHIFT系キーのフラグ処理
	}
	else if(breakflag==0 && isLockkey(vk)){
		if(ps2keystatus[vk]) return; //キーリピートの場合、無視
		if((ps2shiftkey & CHK_CTRL)==0) lockkeycheck(vk); //NumLock、CapsLock、ScrollLock反転処理
	}
	//キーコードに対する押下状態配列を更新
	if(breakflag){
		ps2keystatus[vk]=0;
		return;
	}
	ps2keystatus[vk]=1;

	if((keycodebufp1+1==keycodebufp2) ||
			(keycodebufp1==keycodebuf+KEYCODEBUFSIZE-1)&&(keycodebufp2==keycodebuf)){
		return; //バッファがいっぱいの場合無視
	}
	*keycodebufp1++=((unsigned short)ps2shiftkey<<8)+vk;
	if(keycodebufp1==keycodebuf+KEYCODEBUFSIZE) keycodebufp1=keycodebuf;
}
void __ISR(20, ipl4) T5Handler(void)
{
        getps2DMABUFdata(); //DMAによる読み込みデータの取り込み

	if(ps2status!=0) ps2statusprogress();
	if(receivecount){
		//データ受信中のタイムアウトチェック
		//一定時間データが来ない場合、あきらめる
		ps2receivetime++;
		if(ps2receivetime>PS2TIME_RECEIVETIMEOUT) receivecount=0;
	}
	if(ps2receiveError){
		// 受信エラー発生時は再送要求コマンド発行
		ps2receiveError=0;
		ps2command(PS2CMD_RESEND,1); //再送要求コマンド
	}

	//スキャンコードバッファから読み出し、キーコードに変換してキーコードバッファに格納
	if(scancodebufp1!=scancodebufp2) readscancode();
	mT5ClearIntFlag(); // T5割り込みフラグクリア
}

int ps2init()
{
// PS/2キーボードシステム初期化
// 正常終了で0を返す
// エラー終了で-1を返す

	int i;

	receivecount=0;
	scancodebufp1=scancodebuf;
	scancodebufp2=scancodebuf;
	keycodebufp1=keycodebuf;
	keycodebufp2=keycodebuf;
	ps2status=0;
	ps2sending=0;
	ps2receiveError=0;
	ps2sendError=0;
	ps2receivecommandflag=0;
	ps2shiftkey_a=lockkey<<8; //Lock関連キーを変数lockkeyで初期化
	ps2shiftkey=lockkey<<4; //Lock関連キーを変数lockkeyで初期化
	for(i=0;i<256;i++) ps2keystatus[i]=0; //全キー離した状態

	mTRISPS2CLKOUT();//CLKを出力に設定
	asm volatile ("nop");
	mPS2CLKCLR(); //CLK=L

	// Timer5割り込み有効化
	T5CON=0x0040; //Timer5 1:16 prescale
	PR5=TIMER5_100us;//100us周期
	TMR5=0;
	mT5SetIntPriority(4); // 割り込みレベル4
	mT5ClearIntFlag();
	mT5IntEnable(1); // T5割り込み有効化
	T5CONSET=0x8000; //Timer5 Start

	// 電源オン直後の起動待ち
	ps2status=PS2STATUS_INIT;
	ps2statuscount=PS2TIME_INIT;
	while(ps2status) ;

	// CN割り込み設定
	// 通常時：DMAのみ利用、コマンド送信時：割り込み有効
#ifdef PIC32MX370F
	mCNSetIntPriority(6); // 割り込みレベル6
	mCNFClearIntFlag();
	CNENFSET=PS2CLKBIT; // CLKの変化でCN割り込み
	CNCONF=0x8000; // PORTFに対するCN割り込みオン
#else
	mCNSetIntPriority(6); // 割り込みレベル6
	mCNBClearIntFlag();
	CNENBSET=PS2CLKBIT; // CLKの変化でCN割り込み
	CNCONB=0x8000; // PORTBに対するCN割り込みオン
#endif

	//DMA初期化
#ifdef PIC32MX370F
	DCH3ECON=(_CHANGE_NOTICE_F_IRQ<<8)+0x10; // start by CNF interrupt
	DCH3SSA=(void *)(&PORTF) & 0x1fffffff; // source start address
#else
	DCH3ECON=(_CHANGE_NOTICE_B_IRQ<<8)+0x10; // start by CNB interrupt
	DCH3SSA=((unsigned int)(&PORTB) & 0x1fffffff) +1; // source start address
#endif
	DCH3DSA=(unsigned int)keyboard_rcvdata & 0x1fffffff; // distination start address
	DCH3SSIZ=1;// source size
	DCH3DSIZ=PS2DMABUFSIZE;// destination size
	DCH3CSIZ=1;// cell size
	DCH3CON=0x10;//auto enabled
        dma_readpt = 0;
	DMACONSET=0x00008000;// enable the DMA controller
	//DCH3CONSET=0x80; //ch3 enabled --- PS/2リセットコマンドの途中で有効化するためここではしない

	ps2command(PS2CMD_RESET,1); // PS/2リセットコマンド発行し、応答待つ
	while(ps2status) ; //送信完了待ち
	if(ps2sendError) return -1; //エラー発生
	ps2command(PS2CMD_SETLED+(ps2shiftkey_a & 0xff00),2); // キーボードのインジケータ設定
	while(ps2status) ; //送信完了待ち
	if(ps2sendError) return -1; //エラー発生

	return 0;
}
unsigned char ps2readkey(){
// 入力された1つのキーのキーコードをグローバル変数vkeyに格納（押されていなければ0を返す）
// 下位8ビット：キーコード
// 上位8ビット：シフト状態
// 英数・記号文字の場合、戻り値としてASCIIコード（それ以外は0を返す）

	unsigned short k;
	unsigned char sh;

	vkey=0;
	if(keycodebufp1==keycodebufp2) return 0;
	vkey=*keycodebufp2++;
	if(keycodebufp2==keycodebuf+KEYCODEBUFSIZE) keycodebufp2=keycodebuf;
	sh=vkey>>8;
	if(sh & (CHK_CTRL | CHK_ALT | CHK_WIN)) return 0;
	k=vkey & 0xff;
	if(keytype==1){
	//英語キーボード
		if(k>='A' && k<='Z'){
			//SHIFTまたはCapsLock（両方ではない）
			if((sh & (CHK_SHIFT | CHK_CAPSLK))==CHK_SHIFT || (sh & (CHK_SHIFT | CHK_CAPSLK))==CHK_CAPSLK)
				return vk2asc2_en[k];
			else return vk2asc1_en[k];
		}
		if(k>=VK_NUMPAD0 && k<=VK_DIVIDE){ //テンキー
			if((sh & (CHK_SHIFT | CHK_NUMLK))==CHK_NUMLK) //NumLock（SHIFT＋NumLockは無効）
				return vk2asc2_en[k];
			else return vk2asc1_en[k];
		}
		if(sh & CHK_SHIFT) return vk2asc2_en[k];
		else return vk2asc1_en[k];
	}

	if(sh & CHK_SCRLK){
	//日本語キーボード（カナモード）
		if(k>=VK_NUMPAD0 && k<=VK_DIVIDE){ //テンキー
			if((sh & (CHK_SHIFT | CHK_NUMLK))==CHK_NUMLK) //NumLock（SHIFT＋NumLockは無効）
				return vk2kana2[k];
			else return vk2kana1[k];
		}
		if(sh & CHK_SHIFT) return vk2kana2[k];
		else return vk2kana1[k];
	}

	//日本語キーボード（英数モード）
	if(k>='A' && k<='Z'){
		//SHIFTまたはCapsLock（両方ではない）
		if((sh & (CHK_SHIFT | CHK_CAPSLK))==CHK_SHIFT || (sh & (CHK_SHIFT | CHK_CAPSLK))==CHK_CAPSLK)
			return vk2asc2_jp[k];
		else return vk2asc1_jp[k];
	}
	if(k>=VK_NUMPAD0 && k<=VK_DIVIDE){ //テンキー
		if((sh & (CHK_SHIFT | CHK_NUMLK))==CHK_NUMLK) //NumLock（SHIFT＋NumLockは無効）
			return vk2asc2_jp[k];
		else return vk2asc1_jp[k];
	}
	if(sh & CHK_SHIFT) return vk2asc2_jp[k];
	else return vk2asc1_jp[k];
}
unsigned char shiftkeys(){
// SHIFT関連キーの押下状態を返す
// 上位から<0><CAPSLK><NUMLK><SCRLK><Wiin><ALT><CTRL><SHIFT>
	return ps2shiftkey;
}
#ifndef PIC32MX370F
void ps2mode(){
	LATASET=2; //RA1オン（PS/2モード）
	T5CONSET=0x8000; //Timer5再開
	CNENBSET=PS2CLKBIT; //CLK割り込み再開
}
void buttonmode(){
	CNENBCLR=PS2CLKBIT; //CLK割り込み停止
	T5CONCLR=0x8000; //Timer5停止
	LATACLR=2; //RA1オフ（ボタンモード）
}
#endif
