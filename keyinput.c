/*
   This file is provided under the LGPL license ver 2.1.
   Written by K.Tanaka
   http://www.ze.em-net.ne.jp/~kenken/index.html
*/
// キー入力、カーソル表示関連機能 by K.Tanaka
// PS/2キーボード入力システム、カラーテキスト出力システム利用

#include "videoout.h"
#include "ps2keyboard.h"
#include "keyinput.h"

unsigned char blinkchar,blinkcolor,insertmode,blinktimer;

void getcursorchar(){
// カーソル点滅用に元の文字コードを退避
	blinkchar=*cursor;
	if(twidth==WIDTH_X1) blinkcolor=*(cursor+ATTROFFSET1);
	else blinkcolor=*(cursor+ATTROFFSET2);
}
void resetcursorchar(){
// カーソルを元の文字に戻す
	*cursor=blinkchar;
	if(twidth==WIDTH_X1) *(cursor+ATTROFFSET1)=blinkcolor;
	else *(cursor+ATTROFFSET2)=blinkcolor;
}
void blinkcursorchar(){
// 定期的に呼び出すことでカーソルを点滅表示させる
// BLINKTIMEで点滅間隔を設定
// 事前にgetcursorchar()を呼び出しておく
	blinktimer++;
	if(blinktimer>=BLINKTIME*2) blinktimer=0;
	if(blinktimer<BLINKTIME){
		if(insertmode) *cursor=CURSORCHAR;
		else *cursor=CURSORCHAR2;
		if(twidth==WIDTH_X1) *(cursor+ATTROFFSET1)=CURSORCOLOR;
		else *(cursor+ATTROFFSET2)=CURSORCOLOR;
	}
	else{
		*cursor=blinkchar;
		if(twidth==WIDTH_X1) *(cursor+ATTROFFSET1)=blinkcolor;
		else *(cursor+ATTROFFSET2)=blinkcolor;
	}
}

unsigned char _MIPS32 inputchar(void){
// キーボードから1キー入力待ち
// 戻り値 通常文字の場合ASCIIコード、その他は0、グローバル変数vkeyに仮想キーコード
	unsigned char k;
	unsigned short d;
	d=drawcount;
	while(1){
		while(d==drawcount) asm("wait"); //60分の1秒ウェイト
		d=drawcount;
		k=ps2readkey();  //キーバッファから読み込み、k1:通常文字入力の場合ASCIIコード
		if(vkey) return k;
	}
}

unsigned char _MIPS32 cursorinputchar(void){
// カーソル表示しながらキーボードから1キー入力待ち
// 戻り値 通常文字の場合ASCIIコード、その他は0、グローバル変数vkeyに仮想キーコード
	unsigned char k;
	unsigned short d;
	getcursorchar(); //カーソル位置の文字を退避（カーソル点滅用）
	d=drawcount;
	while(1){
		while(d==drawcount) asm("wait"); //60分の1秒ウェイト
		d=drawcount;
		blinkcursorchar(); //カーソル点滅させる
		k=ps2readkey();  //キーバッファから読み込み、k1:通常文字入力の場合ASCIIコード
		if((vkey&0xFF) == VK_RETURN||
		   (vkey&0xFF) == VK_SEPARATOR){
		  k = '\n';
		}else if((vkey&0xFF) == VK_TAB){
		  k = '\t';
		}else if((vkey&0xFF) == VK_BACK){
		  k = '\b';
		}
		if(k) break;  //キーが押された場合ループから抜ける
	}
	resetcursorchar(); //カーソルを元の文字表示に戻す
	return k;
}

unsigned char printinputchar(void){
// カーソル表示しながらキーボードから通常文字キー入力待ちし、入力された文字を表示
// 戻り値 入力された文字のASCIIコード、グローバル変数vkeyに最後に押されたキーの仮想キーコード
	unsigned char k;
	while(1){
		k=cursorinputchar();
		if(k) break;
	}
	printchar(k);
	return k;
}






