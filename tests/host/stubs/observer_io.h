#ifndef TEST_OBSERVER_IO_H
#define TEST_OBSERVER_IO_H

/* Only the observer's I/O boundary; no host filesystem calls are made. */
#define SCE_O_RDONLY 0x0001
#define SCE_O_WRONLY 0x0002
#define SCE_O_APPEND 0x0100
#define SCE_O_CREAT  0x0200
#define SCE_O_TRUNC  0x0400
int sceIoOpen(const char *path, int flags, int mode);
int sceIoRead(int fd, void *buffer, unsigned size);
int sceIoWrite(int fd, const void *buffer, unsigned size);
int sceIoClose(int fd);

#endif
