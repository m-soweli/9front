#pragma src "/sys/src/libdraw"
#pragma lib "libdraw.a"

typedef struct 	Keyboardctl Keyboardctl;
typedef struct	Channel	Channel;

struct	Keyboardctl
{
	Channel	*c;	/* chan(Rune)[20] */

	char		*file;
	int		consfd;		/* to cons file */
	int		ctlfd;		/* to ctl file */
	int		pid;		/* of slave proc */
};


extern	Keyboardctl*	initkeyboard(char*);
extern	int		ctlkeyboard(Keyboardctl*, char*);
extern	void		closekeyboard(Keyboardctl*);

enum {
	KF=	0xF000,	/* Rune: beginning of private Unicode space */
	Spec=	0xF800,
	PF=	Spec|0x20,	/* num pad function key */
	Kview=	Spec|0x00,	/* view (shift window up) */
	/* KF|1, KF|2, ..., KF|0xC is *respectively* F1, F2, ..., F12 */
	Khome=	KF|0x0D,
	Kup=	KF|0x0E,
	Kdown=	Kview,
	Kpgup=	KF|0x0F,
	Kprint=	KF|0x10,
	Kleft=	KF|0x11,
	Kright=	KF|0x12,
	Kpgdown=	KF|0x13,
	Kins=	KF|0x14,

	Kalt=	KF|0x15,
	Kshift=	KF|0x16,
	Kctl=	KF|0x17,

	Kend=	KF|0x18,
	Kscroll=	KF|0x19,
	Kscrolloneup=	KF|0x20,
	Kscrollonedown=	KF|0x21,

	/* multimedia keys - no refunds */
	Ksbwd=	KF|0x22,	/* skip backwards */
	Ksfwd=	KF|0x23,	/* skip forward */
	Kpause=	KF|0x24,	/* play/pause */
	Kvoldn=	KF|0x25,	/* volume decrement */
	Kvolup=	KF|0x26,	/* volume increment */
	Kmute=	KF|0x27,	/* (un)mute */
	Kbrtdn=	KF|0x28,	/* brightness decrement */
	Kbrtup=	KF|0x29,	/* brightness increment */

	Ksoh=	0x01,
	Kstx=	0x02,
	Ketx=	0x03,
	Keof=	0x04,
	Kenq=	0x05,
	Kack=	0x06,
	Kbs=	0x08,
	Knack=	0x15,
	Ketb=	0x17,
	Kdel=	0x7f,
	Kesc=	0x1b,

	Kbreak=	Spec|0x61,
	Kcaps=	Spec|0x64,
	Knum=	Spec|0x65,
	Kmiddle=	Spec|0x66,
	Kaltgr=	Spec|0x67,
	Kmod4=	Spec|0x68,
	Kmouse=	Spec|0x100,
};
