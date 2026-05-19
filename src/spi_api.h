/*
 *	Susie Plug-in Interface
 *		SPI_API.TXTから仕様に従って作成
 *      SPI_API.TXTは「Copyright 竹村嘉人」です。
 *		Convert and modified by Nifty-serve PXB13767   kana
 *                              inokuchi@mvg.biglobe.ne.jp
 *    
 */
/*
 * 注意:
 *  VC++用です。BCはテストされていません。問題があれば連絡ください。
 *	以下の標準インクルードファイルが必要です
 *		WINDOWS.H
 *		time.h
 * History:
 *   2001/01/24: Borland系コンパイラ向けの記述を変更
 *        アライメント: VC++/C++ Builder/Borland C++ Compilerともに
 *                      pshpack1.h/poppack.hが使えるのでそのように。
 *        エクスポート: VC++ DEFファイルが結局必要なので_exportは何もしない
 *                      C++ Builder/Borland C++ Compiler _exportを解釈するのでそのまま
 *   1999/06/06: ざっくりと説明削除
 */
#ifndef	SUSIE_SPI_API_H
#define	SUSIE_SPI_API_H

#if defined(_MSC_VER)
#define	_export
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <pshpack1.h>

typedef struct PictureInfo
{
	long left,top;			/* 画像を展開する位置 */
	long width;				/* 画像の幅(pixel) */
	long height;			/* 画像の高さ(pixel) */
	WORD x_density;			/* 画素の水平方向密度 */
	WORD y_density;			/* 画素の垂直方向密度 */
	short colorDepth;		/* 画素当たりのbit数 */
	HLOCAL hInfo;			/* 画像内のテキスト情報[呼び出し側が解放] */
} PictureInfo;

typedef struct fileInfo
{
	unsigned char method[8];	/* 圧縮法の種類 */
	unsigned long position;		/* ファイル上での位置 */
	unsigned long compsize;		/* 圧縮されたサイズ */
	unsigned long filesize;		/* 元のファイルサイズ */
	time_t timestamp;			/* ファイルの更新日時 */
	char path[200];				/* 相対パス */
	char filename[200];			/* ファイルネーム */
	unsigned long crc;			/* CRC */
} fileInfo;

#include <poppack.h>


/* Common Function */
int _export PASCAL GetPluginInfo (int infono, LPSTR buf,int buflen);
int _export PASCAL IsSupported (LPSTR filename,DWORD dw);


/* '00IN'の関数 */
int _export PASCAL GetPictureInfo (LPSTR buf,long len,unsigned int flag, PictureInfo *lpInfo);
int _export PASCAL GetPicture (LPSTR buf,long len,unsigned int flag, HANDLE *pHBInfo,HANDLE *pHBm,
								FARPROC lpPrgressCallback,long lData);
int _export PASCAL GetPreview (LPSTR buf,long len,unsigned int flag, HANDLE *pHBInfo,HANDLE *pHBm,
								FARPROC lpPrgressCallback,long lData);

/* '00AM'の関数 */
int _export PASCAL GetArchiveInfo (LPSTR buf, long len, unsigned int flag, HLOCAL *lphInf);
int _export PASCAL GetFileInfo (LPSTR buf,long len, LPSTR filename, unsigned int flag, fileInfo *lpInfo);
int _export PASCAL GetFile (LPSTR src,long len, LPSTR dest,unsigned int flag, FARPROC prgressCallback,long lData);

/* Plug-inの設定ダイアログ
 *  Susie32 v0.40より追加されたSPI
 */
enum {
	SUSIE_CONFIGDLG_ABOUT= 0,
	SUSIE_CONFIGDLG_SETTING,
	SUSIE_CONFIGDLG_RESERVED
};
int _export PASCAL ConfigurationDlg(HWND parent,int fnc);

#ifdef __cplusplus
}
#endif

#endif	/* SUSIE_SPI_API_H */
