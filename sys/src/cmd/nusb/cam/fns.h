char *ctlread(Cam *);
void printProbeControl(Fmt *fmt, void *vp);
void printDescriptor(Fmt *, Iface *, void *);
int videoopen(Cam *, int);
void videoclose(Cam *);
void videoread(Req *, Cam *, int);
void videoflush(Req *, Cam *);
int getframedesc(Cam *, int, int, Format **, VSUncompressedFrame **);
int ctlwrite(Cam *, char *);
