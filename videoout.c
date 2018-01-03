// テキストVRAMコンポジットカラー出力プログラム PIC32MX170F256B用　by K.Tanaka Rev.6
// RAMフォント対応、グラフィック対応、6ドットフォント対応
// 出力 PORTB
// 横30×縦27キャラクター、256色カラーパレット　→横40キャラクター対応
// クロック周波数3.579545MHz×16倍　（グラフィック時は×15倍）
//　カラー信号はカラーサブキャリアの90度ごとに出力、270度で1ドット（4分の3周期）

#include "videoout.h"
#include <plib.h>

// テキストモード 5bit DA信号定数データ
#define C_SYN	0
#define C_BLK	6
#define C_BST1	6
#define C_BST2	3
#define C_BST3	6
#define C_BST4	9

// テキストモード パルス幅定数
#define V_NTSC		262					// 262本/画面
#define V_SYNC		10					// 垂直同期本数
#define V_PREEQ		26					// ブランキング区間上側（V_SYNC＋V_PREEQは偶数とすること）
#define V_LINE		(WIDTH_Y*8)			// 画像描画区間
#define H_NTSC		3632				// 約63.5μsec
#define H_SYNC		269					// 水平同期幅、約4.7μsec
#define H_CBST		304					// カラーバースト開始位置（水平同期立下りから）
#define H_BACK		339					// 左スペース（水平同期立ち上がりから）


// グラフィックモード 5bit DA信号定数データ
//#define GC_SYN	0
//#define GC_BLK	6
#define GC_BST1	6
#define GC_BST2	3
#define GC_BST3	8

// グラフィックモード パルス幅定数
//#define G_V_NTSC		262					// 262本/画面
//#define G_V_SYNC		10					// 垂直同期本数
#define G_V_PREEQ		18					// ブランキング区間上側
#define G_V_LINE		G_Y_RES				// 画像描画区間
#define G_H_NTSC		3405				// 約63.5μsec
#define G_H_SYNC		252					// 水平同期幅、約4.7μsec
#define G_H_CBST		285					// カラーバースト開始位置（水平同期立下りから）
#define G_H_BACK		483					// 左スペース（水平同期立ち上がりから）


// グローバル変数定義
// unsigned short GVRAM[G_H_WORD*G_Y_RES] __attribute__ ((aligned (4))); //グラフィックVRAM（テスト用）

unsigned int ClTable[256];	//カラーパレット信号テーブル、各色32bitを下位から8bitずつ順に出力
//unsigned char TVRAM[WIDTH_X*WIDTH_Y*2+1] __attribute__ ((aligned (4)));
unsigned char TVRAM[WIDTH_X2*WIDTH_Y*2+1] __attribute__ ((aligned (4)));
unsigned short *gVRAM; //グラフィックVRAM開始位置のポインタ
unsigned char *VRAMp; //VRAMと処理中VRAMアドレス。テキストVRAMは1バイトのダミー必要
unsigned char *fontp; //フォント格納アドレス、初期化時はFontData、RAM指定することでPCGを実現

unsigned int bgcolor;		// バックグランドカラー波形データ、32bitを下位から8bitずつ順に出力
volatile unsigned short LineCount;	// 処理中の行
volatile unsigned short drawcount;	//　1画面表示終了ごとに1足す。アプリ側で0にする。
					// 最低1回は画面表示したことのチェックと、アプリの処理が何画面期間必要かの確認に利用。
volatile char drawing;		//　映像区間処理中は-1、その他は0

unsigned char twidth; //テキスト1行文字数（30 or 40）
unsigned char graphmode; //テキストモード時 0、グラフィックモード時 0以外

//グラフィックモードでのカラー信号テーブル
//16色分のカラーパレットテーブル
//5ドット分の信号データを保持。順に出力する
//位相は15分の0,4,9,13,3,7,12,1,6,10の順
unsigned char gClTable[16*16];

/**********************
*  Timer2 割り込み処理関数
***********************/
void __ISR(8, ipl5) T2Handler(void)
{
	asm volatile("#":::"a0");
	asm volatile("#":::"v0");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("addiu	$a0,$a0,-22");
	asm volatile("bltz	$a0,label1_2");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label1_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label1");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

	asm volatile("label1:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

asm volatile("label1_2:");
	//LATB=C_SYN;
	asm volatile("addiu	$a0,$zero,%0"::"n"(C_SYN));
	asm volatile("la	$v0,%0"::"i"(&LATB));
	asm volatile("sb	$a0,0($v0)");// 同期信号立ち下がり。ここを基準に全ての信号出力のタイミングを調整する

	if(!graphmode){
		if(LineCount<V_SYNC){
			// 垂直同期期間
			OC3R = H_NTSC-H_SYNC-7;	// 切り込みパルス幅設定
			OC3CON = 0x8001;
		}
		else{
			OC1R = H_SYNC-7;		// 同期パルス幅4.7usec
			OC1CON = 0x8001;		// タイマ2選択ワンショット
			if(LineCount>=V_SYNC+V_PREEQ && LineCount<V_SYNC+V_PREEQ+V_LINE){
				// 画像描画区間
				OC2R = H_SYNC+H_BACK-7-47;// 画像信号開始のタイミング
				OC2CON = 0x8001;		// タイマ2選択ワンショット
				if(LineCount==V_SYNC+V_PREEQ){
					VRAMp=TVRAM;
					drawing=-1;
				}
				else{
					if((LineCount-(V_SYNC+V_PREEQ))%8==0){
						 //1キャラクタ縦8ドット
						VRAMp+=twidth;
					}
				}
			}
			else if(LineCount==V_SYNC+V_PREEQ+V_LINE){ // 1画面最後の描画終了
				drawing=0;
				drawcount++;
			}
		}
	}
	else{
		if(LineCount<V_SYNC){
			// 垂直同期期間
			OC3R = G_H_NTSC-G_H_SYNC-7;	// 切り込みパルス幅設定
			OC3CON = 0x8001;
		}
		else{
			OC1R = G_H_SYNC-7;		// 同期パルス幅4.7usec
			OC1CON = 0x8001;		// タイマ2選択ワンショット
			if(LineCount>=V_SYNC+G_V_PREEQ && LineCount<V_SYNC+G_V_PREEQ+G_V_LINE){
				// 画像描画区間
				OC2R = G_H_SYNC+G_H_BACK-7-4-20;// 画像信号開始のタイミング
				OC2CON = 0x8001;		// タイマ2選択ワンショット
				if(LineCount==V_SYNC+G_V_PREEQ){
					VRAMp=(unsigned char *)gVRAM;
					drawing=-1; // 画像描画区間
				}
				else{
					VRAMp+=G_H_WORD*2;// 次の行へ
				}
			}
			else if(LineCount==V_SYNC+G_V_PREEQ+G_V_LINE){
				drawing=0;
				drawcount++;
			}
		}
	}
	LineCount++;
	if(LineCount>=V_NTSC) LineCount=0;
	IFS0bits.T2IF = 0;			// T2割り込みフラグクリア
}

/*********************
*  OC3割り込み処理関数 垂直同期切り込みパルス
*********************/
void __ISR(14, ipl5) OC3Handler(void)
{
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("la	$v0,%0"::"i"(&graphmode));
	asm volatile("lbu	$v1,0($v0)");
	asm volatile("nop");
	asm volatile("bnez	$v1,label4_3");
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(H_NTSC-H_SYNC+18)));
	asm volatile("bltz	$a0,label4_2");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label4_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label4");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label4_3:");
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(G_H_NTSC-G_H_SYNC+18)));
	asm volatile("bltz	$a0,label4_2");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label4_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label4");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label4:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

asm volatile("label4_2:");
	// 同期信号のリセット
	//	LATB=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATB));

	//同期信号リセット。テキストモードでは同期信号立ち下がりから3363サイクル
	//グラフィックモードでは信号立ち下がりから3153サイクル
	asm volatile("sb	$v1,0($v0)");

	IFS0bits.OC3IF = 0;			// OC3割り込みフラグクリア
}

/*********************
*  OC1割り込み処理関数 水平同期立ち上がり～カラーバースト
*********************/
void __ISR(6, ipl5) OC1Handler(void)
{
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("la	$v0,%0"::"i"(&graphmode));
	asm volatile("lbu	$v1,0($v0)");
	asm volatile("nop");
	asm volatile("bnez	$v1,label2_3"); //グラフィックモード
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(H_SYNC+18)));
	asm volatile("bltz	$a0,label2_2");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label2_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label2");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label2:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

asm volatile("label2_2:");
	// 同期信号のリセット
	//	LATB=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATB));
	asm volatile("sb	$v1,0($v0)");	// 同期信号リセット。水平同期立ち下がりから269サイクル

	// 28クロックウェイト
	asm volatile("addiu	$a0,$zero,9");
asm volatile("loop2:");
	asm volatile("addiu	$a0,$a0,-1");
	asm volatile("nop");
	asm volatile("bnez	$a0,loop2");

	// カラーバースト信号 9周期出力
	asm volatile("addiu	$a0,$zero,4*9");
	asm volatile("la	$v0,%0"::"i"(&LATB));
	asm volatile("lui	$v1,%0"::"n"(C_BST3+(C_BST4<<8)));
	asm volatile("ori	$v1,$v1,%0"::"n"(C_BST1+(C_BST2<<8)));
asm volatile("loop3:");
	asm volatile("addiu	$a0,$a0,-1");	//ループカウンタ
	asm volatile("sb	$v1,0($v0)");	// カラーバースト開始。水平同期立ち下がりから304サイクル
	asm volatile("rotr	$v1,$v1,8");
	asm volatile("bnez	$a0,loop3");
	asm volatile("nop");
	asm volatile("b		oc1end");

// グラフィックモード
asm volatile("label2_3:");
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(G_H_SYNC+18)));
	asm volatile("bltz	$a0,label2_5");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label2_5");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label2_4");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label2_4:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

asm volatile("label2_5:");
	// 同期信号のリセット
	//	LATB=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATB));
	asm volatile("sb	$v1,0($v0)");	// 同期信号リセット。水平同期立ち下がりから252サイクル

	// 28クロックウェイト
	asm volatile("addiu	$a0,$zero,9");
asm volatile("loop2_1:");
	asm volatile("addiu	$a0,$a0,-1");
	asm volatile("nop");
	asm volatile("bnez	$a0,loop2_1");

	// カラーバースト信号 9周期出力
	asm volatile("addiu	$a0,$zero,9");
	asm volatile("la	$v0,%0"::"i"(&LATB));
asm volatile("loop3_1:");
	asm volatile("addiu	$v1,$zero,%0"::"n"(GC_BST1));
	asm volatile("sb	$v1,0($v0)");	// カラーバースト開始。水平同期立ち下がりから285サイクル
	asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("addiu	$v1,$zero,%0"::"n"(GC_BST2));
	asm volatile("sb	$v1,0($v0)");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("addiu	$v1,$zero,%0"::"n"(GC_BST3));
	asm volatile("sb	$v1,0($v0)");
	asm volatile("addiu	$a0,$a0,-1");//ループカウンタ
	asm volatile("nop");
	asm volatile("bnez	$a0,loop3_1");

asm volatile("oc1end:");
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("sb	$v1,0($v0)");

	IFS0bits.OC1IF = 0;			// OC1割り込みフラグクリア

}

/***********************
*  OC2割り込み処理関数　映像信号出力
***********************/
void __ISR(10, ipl5) OC2Handler(void)
{
// 映像信号出力
	//インラインアセンブラでの破壊レジスタを宣言（スタック退避させるため）
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");
	asm volatile("#":::"a1");
	asm volatile("#":::"a2");
	asm volatile("#":::"t0");
	asm volatile("#":::"t1");
	asm volatile("#":::"t2");
	asm volatile("#":::"t3");
	asm volatile("#":::"t4");
	asm volatile("#":::"t5");
	asm volatile("#":::"t6");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("la	$v0,%0"::"i"(&graphmode));
	asm volatile("lbu	$v1,0($v0)");
	asm volatile("nop");
	asm volatile("bnez	$v1,jump3");//グラフィックモードの場合jump3へ
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(H_SYNC+H_BACK+27-47)));
	asm volatile("bltz	$a0,label3_2");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label3_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label3");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label3:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

//テキストモード
asm volatile("label3_2:");
	//	drawing=-1;
	asm volatile("addiu	$t1,$zero,-1");
	asm volatile("la	$v0,%0"::"i"(&drawing));
	asm volatile("sb	$t1,0($v0)");
	//	a0=VRAMp;
	asm volatile("la	$v0,%0"::"i"(&VRAMp));
	asm volatile("lw	$a0,0($v0)");
	//	a1=ClTable;
	asm volatile("la	$a1,%0"::"i"(ClTable));
	//	a2=fontp+描画中の行%8
	asm volatile("la	$v0,%0"::"i"(&LineCount));
	asm volatile("lhu	$v1,0($v0)");
//	asm volatile("la	$a2,%0"::"i"(FontData));
	asm volatile("la	$v0,%0"::"i"(&fontp));
	asm volatile("lw	$a2,0($v0)");
	asm volatile("addiu	$v1,$v1,%0"::"n"(-V_SYNC-V_PREEQ-1));
	asm volatile("andi	$v1,$v1,7");
	asm volatile("addu	$a2,$a2,$v1");
	//	v1=bgcolor波形データ
	asm volatile("la	$v0,%0"::"i"(&bgcolor));
	asm volatile("lw	$v1,0($v0)");
	//	v0=&LATB;
	asm volatile("la	$v0,%0"::"i"(&LATB));
	//	t6=twidth
	asm volatile("la	$t0,%0"::"i"(&twidth));
	asm volatile("lbu	$t6,0($t0)");
	//twidthが40の時jump4にジャンプ
	asm volatile("sltiu	$t0,$t6,40");
	asm volatile("nop");
	asm volatile("beq	$t0,$zero,jump4");
// ここから30文字モード出力波形データ生成
	asm volatile("lbu	$t0,0($a0)");
	asm volatile("lbu	$t1,%0($a0)"::"n"(ATTROFFSET1));
	asm volatile("sll	$t0,$t0,3");
	asm volatile("addu	$t0,$t0,$a2");
	asm volatile("lbu	$t0,0($t0)");
	asm volatile("sll	$t1,$t1,2");
	asm volatile("addu	$t1,$t1,$a1");
	//fontpが0xa0000000以上の場合(RAMの場合)jump2にジャンプ
	asm volatile("lui	$t2,0xa000");
	asm volatile("sltu	$t2,$a2,$t2");
	asm volatile("nop");
	asm volatile("beq	$t2,$zero,jump2");
//	-----------------
	asm volatile("lw	$t1,0($t1)");
	asm volatile("addiu	$a0,$a0,1");//ここで水平クロック立ち下がりから604クロック

asm volatile("loop1:");
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)"); // ここで最初の信号出力
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($a0)"); //空いている時間に次の文字出力の準備開始
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET1));
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lw	$t5,0($t5)");
			asm volatile("addiu	$a0,$a0,1");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
					asm volatile("addiu	$t6,$t6,-1");//ループカウンタ
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,1,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
			asm volatile("lbu	$t4,0($t4)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("sb	$t2,0($v0)");
	
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,0,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("addu	$t1,$zero,$t5");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("addu	$t0,$zero,$t4");
	asm volatile("sb	$t2,0($v0)");//遅延スロットのため1クロック遅れる
	asm volatile("bnez	$t6,loop1");

	asm volatile("b		jump1");

// フォントがRAMの場合
//	-----------------
asm volatile("jump2:");
	asm volatile("nop");//RAM読み出しのためのクロック調整
	asm volatile("lw	$t1,0($t1)");
	asm volatile("addiu	$a0,$a0,1");//ここで水平クロック立ち下がりから604クロック
asm volatile("loop1_2:");
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)"); // ここで最初の信号出力
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($a0)"); //空いている時間に次の文字出力の準備開始
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET1));
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lw	$t5,0($t5)");
			asm volatile("addiu	$a0,$a0,1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
					asm volatile("addiu	$t6,$t6,-1");//ループカウンタ
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,1,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($t4)");
					asm volatile("nop");//RAM読み出しのため1クロック調整
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,0,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("addu	$t1,$zero,$t5");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("addu	$t0,$zero,$t4");
	asm volatile("sb	$t2,0($v0)");//遅延スロットのため1クロック遅れる
	asm volatile("bnez	$t6,loop1_2");

	asm volatile("nop");
	asm volatile("b		jump1");

// ここから40文字モード出力波形データ生成
asm volatile("jump4:");
	asm volatile("lbu	$t0,0($a0)");
	asm volatile("lbu	$t1,%0($a0)"::"n"(ATTROFFSET2));
	asm volatile("sll	$t0,$t0,3");
	asm volatile("addu	$t0,$t0,$a2");
	asm volatile("lbu	$t0,0($t0)");
	asm volatile("sll	$t1,$t1,2");
	asm volatile("addu	$t1,$t1,$a1");
	//fontpが0xa0000000以上の場合(RAMの場合)jump2_2にジャンプ
	asm volatile("lui	$t2,0xa000");
	asm volatile("sltu	$t2,$a2,$t2");
	asm volatile("nop");
	asm volatile("beq	$t2,$zero,jump2_2");
//	-----------------
	asm volatile("lw	$t1,0($t1)");
	asm volatile("addiu	$a0,$a0,1");//ここで水平クロック立ち下がりから604クロック

asm volatile("loop1_4:");
//偶数桁
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($a0)"); //空いている時間に次の文字出力の準備開始
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET2));
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lw	$t5,0($t5)");
			asm volatile("addiu	$t6,$t6,-2");//ループカウンタ
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("rotr	$t1,$t5,16");
	asm volatile("sb	$t2,0($v0)");
			asm volatile("lbu	$t0,0($t4)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("sb	$t2,0($v0)");

//奇数桁
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,1($a0)");
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET2+1));
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
			asm volatile("addiu	$a0,$a0,2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
			asm volatile("lbu	$t4,0($t4)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("lw	$t1,0($t5)");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("addu	$t0,$zero,$t4");
	asm volatile("sb	$t2,0($v0)");//遅延スロットのため1クロック遅れる
	asm volatile("bnez	$t6,loop1_4");

	asm volatile("b		jump1");
	asm volatile("nop");

// フォントがRAMの場合
//	-----------------
asm volatile("jump2_2:");
	asm volatile("nop");//RAM読み出しのためのクロック調整
	asm volatile("lw	$t1,0($t1)");
	asm volatile("addiu	$a0,$a0,1");//ここで水平クロック立ち下がりから604クロック

asm volatile("loop1_5:");
//偶数桁
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($a0)"); //空いている時間に次の文字出力の準備開始
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET2));
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lw	$t5,0($t5)");
			asm volatile("addiu	$t6,$t6,-2");//ループカウンタ
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
asm volatile("nop");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("rotr	$t1,$t5,16");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t0,0($t4)");
					asm volatile("nop");//RAM読み出しのためのクロック調整
	asm volatile("sb	$t2,0($v0)");

//奇数桁
	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,7,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,1($a0)");
			asm volatile("lbu	$t5,%0($a0)"::"n"(ATTROFFSET2+1));
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,6,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t4,$t4,3");
			asm volatile("addu	$t4,$t4,$a2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,5,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("sll	$t5,$t5,2");
			asm volatile("addu	$t5,$t5,$a1");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,4,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
asm volatile("nop");
			asm volatile("addiu	$a0,$a0,2");
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,3,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
	asm volatile("rotr	$t1,$t1,24");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("lbu	$t4,0($t4)");
					asm volatile("nop");//RAM読み出しのためのクロック調整
	asm volatile("sb	$t2,0($v0)");

	asm volatile("addu	$t2,$zero,$v1");
	asm volatile("ext	$t3,$t0,2,1");
	asm volatile("movn	$t2,$t1,$t3");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
	asm volatile("rotr	$v1,$v1,24");
			asm volatile("lw	$t1,0($t5)");
	asm volatile("sb	$t2,0($v0)");
	asm volatile("rotr	$t2,$t2,8");
			asm volatile("addu	$t0,$zero,$t4");
	asm volatile("sb	$t2,0($v0)");//遅延スロットのため1クロック遅れる
	asm volatile("bnez	$t6,loop1_5");

	asm volatile("b		jump1");
	asm volatile("nop");

//ここからグラフィックモードの出力
asm volatile("jump3:");
	//TMR2の値でタイミングのずれを補正
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(G_H_SYNC+G_H_BACK+18-15)));
	asm volatile("bltz	$a0,label3_4");
	asm volatile("addiu	$v0,$a0,-10");
	asm volatile("bgtz	$v0,label3_4");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label3_3");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label3_3:");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");asm volatile("nop");asm volatile("nop");
	asm volatile("nop");asm volatile("nop");

asm volatile("label3_4:");
	//	drawing=-1;
	asm volatile("addiu	$t1,$zero,-1");
	asm volatile("la	$v0,%0"::"i"(&drawing));
	asm volatile("sb	$t1,0($v0)");
	//	t1=VRAMp;
	asm volatile("la	$v0,%0"::"i"(&VRAMp));
	asm volatile("lw	$t1,0($v0)");
	//	v1=gClTable;
	asm volatile("la	$v1,%0"::"i"(gClTable));
	//	v0=&LATB;
	asm volatile("la	$v0,%0"::"i"(&LATB));
	//	t2=0;
	asm volatile("addiu	$t2,$zero,0");

	asm volatile("addiu	$a2,$zero,12"); //ループカウンタ
	asm volatile("lhu	$a1,0($t1)");
	asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
	asm volatile("rotr	$a1,$a1,12");

	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
	asm volatile("lbu	$t0,0($a0)");
	asm volatile("rotr	$a1,$a1,28");
//	asm volatile("nop");

//20ドット×12回ループ
//-------------------------------------------------
asm volatile("loop1_3:"); //ここでLATBに最初の出力。水平同期立ち下がりから735サイクルになるよう調整する
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,1($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,2($a0)");
				asm volatile("rotr	$a1,$a1,28");
asm volatile("nop");
asm volatile("nop");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,3($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,4($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,5($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,6($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,7($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,9($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,0($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,1($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,2($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,3($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,4($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,5($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,6($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,7($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,9($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,0($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,1($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,2($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,3($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,4($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,5($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,6($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,7($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,9($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,0($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,1($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,2($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,3($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,4($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,5($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,6($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,7($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,8($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,9($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,0($a0)");
							asm volatile("addiu	$a2,$a2,-1");//ループカウンタ
	asm volatile("rotr	$a1,$a1,28");
							asm volatile("bnez	$a2,loop1_3");
//-------------------------------------------------
//残16ドット
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,1($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,2($a0)");
				asm volatile("rotr	$a1,$a1,28");
asm volatile("nop");
asm volatile("nop");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,3($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,4($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,5($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,6($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,7($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,9($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,0($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,1($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,2($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,3($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,4($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,5($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,6($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,7($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,9($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,0($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,1($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,2($a0)");
							asm volatile("lhu	$a1,0($t1)");
							asm volatile("addiu	$t1,$t1,2"); //VRAMp++;
							asm volatile("rotr	$a1,$a1,12");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,3($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,4($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,5($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,6($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("rotr	$a1,$a1,28");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,7($a0)");
	asm volatile("ins	$t2,$a1,4,4");
	asm volatile("addu	$a0,$t2,$v1");
				asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,8($a0)");
asm volatile("nop");
asm volatile("nop");
	asm volatile("rotr	$a1,$a1,28");
	asm volatile("sb	$t0,0($v0)");
	asm volatile("lbu	$t0,9($a0)");
				asm volatile("ins	$t2,$a1,4,4");
				asm volatile("addu	$a0,$t2,$v1");
	asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,0($a0)");
asm volatile("nop");
asm volatile("nop");
asm volatile("nop");
				asm volatile("sb	$t0,0($v0)");
				asm volatile("lbu	$t0,1($a0)");
asm volatile("nop");
asm volatile("nop");
				asm volatile("sb	$t0,0($v0)");
//-------------------------------------------------

	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");


asm volatile("jump1:");

	//	LATB=C_BLK;
	asm volatile("addiu	$t0,$zero,%0"::"n"(C_BLK));
	asm volatile("sb	$t0,0($v0)");
 	IFS0bits.OC2IF = 0;			// OC2割り込みフラグクリア
}

// テキスト画面クリア
void clearscreen(void)
{
	unsigned int *vp;
	int i;
	vp=(unsigned int *)TVRAM;
	for(i=0;i<WIDTH_X2*WIDTH_Y*2/4;i++) *vp++=0;
	cursor=TVRAM;
}
// グラフィック画面クリア
void g_clearscreen(void)
{
	unsigned int *vp;
	int i;
	vp=(unsigned int *)gVRAM;
	for(i=0;i<G_H_WORD*G_Y_RES/2;i++) *vp++=0;
}

void set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g)
{
	// テキストカラーパレット設定（5ビットDA、電源3.3V、90度単位）
	// n:パレット番号、r,g,b:0～255
	int y,s0,s1,s2,s3;
	y=(150*g+29*b+77*r+128)/256;
	s0=(3525*y+3093*((int)r-y)+1440*256+32768)/65536;
	s1=(3525*y+1735*((int)b-y)+1440*256+32768)/65536;
	s2=(3525*y-3093*((int)r-y)+1440*256+32768)/65536;
	s3=(3525*y-1735*((int)b-y)+1440*256+32768)/65536;
	ClTable[n]=s0+(s1<<8)+(s2<<16)+(s3<<24);
}
void set_bgcolor(unsigned char b,unsigned char r,unsigned char g)
{
	// テキストバックグランドカラー設定 r,g,b:0～255
	int y,s0,s1,s2,s3;
	y=(150*g+29*b+77*r+128)/256;
	s0=(3525*y+3093*((int)r-y)+1440*256+32768)/65536;
	s1=(3525*y+1735*((int)b-y)+1440*256+32768)/65536;
	s2=(3525*y-3093*((int)r-y)+1440*256+32768)/65536;
	s3=(3525*y-1735*((int)b-y)+1440*256+32768)/65536;
	bgcolor=s0+(s1<<8)+(s2<<16)+(s3<<24);
}

void g_set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g)
{
	// グラフィックカラーパレット設定（5ビットDA、電源3.3V、10ドットで1周のパターン）
	// n:パレット番号、r,g,b:0～255
	// 輝度Y=0.587*G+0.114*B+0.299*R
	// 信号N=Y+0.4921*(B-Y)*sinθ+0.8773*(R-Y)*cosθ
	// 出力データS=(N*0.71[v]+0.29[v])/3.3[v]*64

	int y;
	if(n>=16) return;
	y=(150*g+29*b+77*r+128)/256;
	gClTable[n*16+0]=(3525*y+   0*((int)b-y)+3093*((int)r-y)+1440*256+32768)/65536;//θ=2Π*0/15
	gClTable[n*16+1]=(3525*y+1725*((int)b-y)- 323*((int)r-y)+1440*256+32768)/65536;//θ=2Π*4/15
	gClTable[n*16+2]=(3525*y-1020*((int)b-y)-2502*((int)r-y)+1440*256+32768)/65536;//θ=2Π*9/15
	gClTable[n*16+3]=(3525*y-1289*((int)b-y)+2069*((int)r-y)+1440*256+32768)/65536;//θ=2Π*13/15
	gClTable[n*16+4]=(3525*y+1650*((int)b-y)+ 956*((int)r-y)+1440*256+32768)/65536;//θ=2Π*3/15
	gClTable[n*16+5]=(3525*y+ 361*((int)b-y)-3025*((int)r-y)+1440*256+32768)/65536;//θ=2Π*7/15
	gClTable[n*16+6]=(3525*y-1650*((int)b-y)+ 956*((int)r-y)+1440*256+32768)/65536;//θ=2Π*12/15
	gClTable[n*16+7]=(3525*y+ 706*((int)b-y)+2825*((int)r-y)+1440*256+32768)/65536;//θ=2Π*1/15
	gClTable[n*16+8]=(3525*y+1020*((int)b-y)-2502*((int)r-y)+1440*256+32768)/65536;//θ=2Π*6/15
	gClTable[n*16+9]=(3525*y-1503*((int)b-y)-1546*((int)r-y)+1440*256+32768)/65536;//θ=2Π*10/15
}

void start_composite(void)
{
	// 変数初期設定
	LineCount=0;				// 処理中ラインカウンター
	drawing=0;
	if(!graphmode){
		VRAMp=TVRAM;
		PR2 = H_NTSC -1;		// 約63.5usecに設定
	}
	else{
		VRAMp=(unsigned char *)gVRAM;
		PR2 = G_H_NTSC -1; 		// 約63.5usecに設定
	}
	TMR2=0;
	T2CONSET=0x8000;			// タイマ2スタート
}
void stop_composite(void)
{
	T2CONCLR = 0x8000;			// タイマ2停止
	OC1CONCLR = 0x8000;			// OC1停止
	OC2CONCLR = 0x8000;			// OC2停止
	OC3CONCLR = 0x8000;			// OC3停止
}

// カラーコンポジット出力初期化
void init_composite(void)
{
	int i;
	graphmode=0;//テキストモード
	twidth=WIDTH_X1;//30文字モード
	clearscreen();

	fontp=(unsigned char *)FontData;
	//テキスト用カラー番号0～7のパレット初期化
	for(i=0;i<8;i++){
		set_palette(i,255*(i&1),255*((i>>1)&1),255*(i>>2));
	}
	for(;i<256;i++){
		set_palette(i,255,255,255); //8以上は全て白に初期化
	}
	set_bgcolor(0,0,0); //バックグランドカラーは黒
	setcursorcolor(7);

	// タイマ2の初期設定,内部クロックで63.5usec周期、1:1
	T2CON = 0x0000;				// タイマ2停止状態
	IPC2bits.T2IP = 5;			// 割り込みレベル5
	IFS0bits.T2IF = 0;
	IEC0bits.T2IE = 1;			// タイマ2割り込み有効化

	// OC1の割り込み有効化
	IPC1bits.OC1IP = 5;			// 割り込みレベル5
	IFS0bits.OC1IF = 0;
	IEC0bits.OC1IE = 1;			// OC1割り込み有効化

	// OC2の割り込み有効化
	IPC2bits.OC2IP = 5;			// 割り込みレベル5
	IFS0bits.OC2IF = 0;
	IEC0bits.OC2IE = 1;			// OC2割り込み有効化

	// OC3の割り込み有効化
	IPC3bits.OC3IP = 5;			// 割り込みレベル5
	IFS0bits.OC3IF = 0;
	IEC0bits.OC3IE = 1;			// OC3割り込み有効化

	OSCCONCLR=0x10; // WAIT命令はアイドルモード
	BMXCONCLR=0x40;	// RAMアクセスウェイト0
	INTEnableSystemMultiVectoredInt();
	start_composite();
}

//30文字モード(8ドットフォント)と40文字モード(6ドットフォント)の切り替え
void set_width(unsigned char m){
// m:0　30文字モード、1　40文字モード
// グラフモード時は無効
// PCG使用中はフォント変更しない
	if(graphmode) return;
	clearscreen();
	if(m){
		if(fontp<(unsigned char *)0xa0000000) fontp=(unsigned char *)FontData2;
		twidth=WIDTH_X2;
	}
	else{
		if(fontp<(unsigned char *)0xa0000000) fontp=(unsigned char *)FontData;
		twidth=WIDTH_X1;
	}
	return;
}

//テキストモードとグラフィックモードの切り替え
void set_graphmode(unsigned char m){
// m:0　テキストモード、0以外 グラフィックモード
	stop_composite();
	if(m){
		//グラフィックモード開始
		OSCConfig(OSC_POSC_PLL, OSC_PLL_MULT_15, OSC_PLL_POST_1, 0); //システムクロックをPLLで15倍に設定
		graphmode=1;
	}
	else{
		//テキストーモード開始
		OSCConfig(OSC_POSC_PLL, OSC_PLL_MULT_16, OSC_PLL_POST_1, 0); //システムクロックをPLLで16倍に設定
		graphmode=0;
	}
	start_composite();
}
void init_graphic(unsigned short *gvram){
// グラフィック機能を初期化
// パラメータとして、28KBのメモリ領域へのポインタを与える
	int i;
	gVRAM=gvram;
	g_clearscreen();

	//グラフィック用カラーパレット初期化
	for(i=0;i<8;i++){
		g_set_palette(i,255*(i&1),255*((i>>1)&1),255*(i>>2));
	}
	for(i=0;i<8;i++){
		g_set_palette(8+i,128*(i&1),128*((i>>1)&1),128*(i>>2));
	}
}