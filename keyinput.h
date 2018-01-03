/*
   This file is provided under the LGPL license ver 2.1.
   Written by K.Tanaka
   http://www.ze.em-net.ne.jp/~kenken/index.html
*/

#define _MIPS32 __attribute__ ( ( nomips16,noinline) )
#define BLINKTIME 15
#define CURSORCHAR 0x87
#define CURSORCHAR2 0x80
#define CURSORCOLOR 7
void getcursorchar();
// カーソル点滅用に元の文字コードを退避

void resetcursorchar();
// カーソルを元の文字に戻す

void blinkcursorchar();
// 定期的に呼び出すことでカーソルを点滅表示させる
// BLINKTIMEで点滅間隔を設定
// 事前にgetcursorchar()を呼び出しておく

int lineinput(char *s,int n);
// キー入力して文字列配列sに格納
// sに初期文字列を入れておくと最初に表示して文字列の最後にカーソル移動する
// 初期文字列を使用しない場合は*s=0としておく
// カーソル位置はsetcursor関数で指定しておく
// 最大文字数n、最後に0を格納するのでn+1バイトの領域必要、ただしnの最大値は255
// 戻り値　Enterで終了した場合0、ESCで終了時は-1（sは壊さない）

unsigned char _MIPS32 inputchar(void);
// キーボードから1キー入力待ち
// 戻り値 通常文字の場合ASCIIコード、その他は0、グローバル変数vkeyに仮想キーコード

unsigned char printinputchar(void);
// カーソル表示しながらキーボードから通常文字キー入力待ちし、入力された文字を表示
// 戻り値 入力された文字のASCIIコード、グローバル変数vkeyに最後に押されたキーの仮想キーコード

unsigned char _MIPS32 cursorinputchar(void);
// カーソル表示しながらキーボードから1キー入力待ち
// 戻り値 通常文字の場合ASCIIコード、その他は0、グローバル変数vkeyに仮想キーコード

extern unsigned char blinkchar,blinkcolor, blinktimer, insertmode; //挿入モード：1、上書きモード：0
