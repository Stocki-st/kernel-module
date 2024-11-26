#ifndef IS18_IOCTL_H
#define IS18_IOCTL_H

// see: http://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/plain/Documentation/ioctl/ioctl-number.txt?id=HEAD
#define IS18_IOC_MY_MAGIC 0xCE // freie Nummer laut link (oberhalb)

#define IS18_IOC_NR_OPENREADCNT 5           // number of open fd with read permissions
#define IS18_IOC_NR_OPENWRITECNT 6          // number of open fd with write permissions
#define IS18_IOC_NR_EMPTY_BUFFER 7          // flag for set buffer empty
#define IS18_IOC_NR_DEL_COUNT 8
#define IS18_IOC_NR_READ_INDEX 9            // current buffer position for reading
#define IS18_IOC_NR_WRITE_INDEX 10          // current buffer position for writing
#define IS18_IOC_NR_NUM_BUFFERED_BYTES 11   // numer of bytes which are stored in the buffer

#define IS18_IOC_READ_INDEX _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_READ_INDEX)
#define IS18_IOC_WRITE_INDEX _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_WRITE_INDEX)
#define IS18_IOC_EMPTY_BUFFER _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_EMPTY_BUFFER)
#define IS18_IOC_NUM_BUFFERED_BYTES _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_NUM_BUFFERED_BYTES)
#define IS18_IOC_OPENWRITECNT _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_OPENWRITECNT)


// makro erklaerung siehe: Linux Device Drivers (eCampus pdf Buch) S.138 (pdf 156)

#if 0
int fd = open("/dev/is18dev0", ...);
int read_count = ioctl(fd, is18_IOC_OPENREADCNT);
close(fd);
printf("read count %d\n", read_count);
#endif

#define IS18_IOC_OPENREADCNT _IO(IS18_IOC_MY_MAGIC, IS18_IOC_NR_OPENREADCNT)
// Abfragen des read counts per im Usermode:
// int open_read_cnt = ioctl(fd, is18_IOC_OPENREADCNT);
// Testprogramm muss natuerlich die is18_ioctl.h inkludieren.

/*
Hier noch ein fiktives Beispiel fuer die Verwendung des optionalen
Arguments vom ioctl Aufruf.
Normalerweise uebergibt man beim dritten Argument einen Zeiger auf einen Buffer.
Hierfür sollte statt _IO dann _IOW, _IOR oder _IORW verwendet werden.
Weiters kann die Groesse vom uebergebenen Typ angegeben werden.
int als dritter Parameter im _IOW Makro gibt an, dass ein Integer bei
der Verwendung von ioctl als dritter Parameter verwendet werden muss. (per addresse uebergeben)
Dass ein dritter Parameter bei ioctl verwendet werden muss,
kann durch die Makros _IOW, _IOR und _IORW bestimmt werden. _IO hingegen
bedeutet, dass kein Puffer beim dritten Parameter verwendet wird (siehe is18_IOC_OPENREADCNT).
Fiktives Beispiel (nur zur Erklaerung. Muss euer Treiber nicht unterstuetzen!!!!):
Bsp: Per "is18_IOC_DEL_COUNT" kann eine gewünschte Anzahl an Bytes von der Pipe gelöscht werden.
*/
// 71 Zeichen von der Pipe verwerfen
// int cnt = 71;
// ioctl(fd, IOC_DEL_COUNT, cnt);
#define IS18_IOC_DEL_COUNT _IOW(is18_IOC_MY_MAGIC, IS18_IOC_NR_DEL_COUNT, int)
// Dieses Beispiel zeigt, wie der Wert 71 als int per ioctl uebergeben wird.
// Natuerlich kann jeder beliebige Integerwert so per ioctl uebergeben werden.

// 71 Zeichen von der Pipe verwerfen
// int cnt = 71;
// ioctl(fd, IOC_DEL_COUNT, cnt);
//#define IS18_IOC_DEL_COUNT _IO(is18_IOC_MY_MAGIC, IS18_IOC_NR_DEL_COUNT)
// Dieses Beispiel zeigt, wie der Wert 71 als int per ioctl uebergeben wird.
// _IO reicht auch, da kein zeiger uebergeben wird, sondern ein wert.

#endif

