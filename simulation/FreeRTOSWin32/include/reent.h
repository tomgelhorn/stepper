
#pragma once


/*
 * Stdio buffers.
 *
 * This and __FILE are defined here because we need them for struct _reent,
 * but we don't want stdio.h included when stdlib.h is.
 */

struct __sbuf {
    unsigned char* _base;
    int	_size;
};

typedef long _fpos_t;
typedef long _off_t;

#define _READ_WRITE_RETURN_TYPE int
#define _READ_WRITE_BUFSIZE_TYPE int

struct __sFILE {
    unsigned char* _p;	/* current position in (some) buffer */
    int	_r;		/* read space left for getc() */
    int	_w;		/* write space left for putc() */
    short	_flags;		/* flags, below; this FILE is free if 0 */
    short	_file;		/* fileno, if Unix descriptor, else -1 */
    struct __sbuf _bf;	/* the buffer (at least 1 byte, if !NULL) */
    int	_lbfsize;	/* 0 or -_bf._size, for inline putc */

#ifdef _REENT_SMALL
    struct _reent* _data;
#endif

    /* operations */
    void* _cookie;	/* cookie passed to io functions */

    _READ_WRITE_RETURN_TYPE(*_read) (struct _reent*, void*,
        char*, _READ_WRITE_BUFSIZE_TYPE);
    _READ_WRITE_RETURN_TYPE(*_write) (struct _reent*, void*,
        const char*,
        _READ_WRITE_BUFSIZE_TYPE);
    _fpos_t(*_seek) (struct _reent*, void*, _fpos_t, int);
    int (*_close) (struct _reent*, void*);

    /* separate buffer for long sequences of ungetc() */
    struct __sbuf _ub;	/* ungetc buffer */
    unsigned char* _up;	/* saved _p when _p is doing ungetc data */
    int	_ur;		/* saved _r when _r is counting ungetc data */

    /* tricks to meet minimum requirements even when malloc() fails */
    unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
    unsigned char _nbuf[1];	/* guarantee a getc() buffer */

    /* separate buffer for fgetline() when line crosses buffer boundary */
    struct __sbuf _lb;	/* buffer for fgetline() */

    /* Unix stdio files get aligned to block boundaries on fseek() */
    int	_blksize;	/* stat.st_blksize (may be != _bf._size) */
    _off_t _offset;	/* current lseek offset */

#ifndef _REENT_SMALL
    struct _reent* _data;	/* Here for binary compatibility? Remove? */
#endif

    int   _flags2;        /* for future use */
};

typedef struct __sFILE   __FILE;

/* This version of _reent is laid out with "int"s in pairs, to help
 * ports with 16-bit int's but 32-bit pointers, align nicely.  */
struct _reent
{
    /* FILE is a big struct and may change over time.  To try to achieve binary
       compatibility with future versions, put stdin,stdout,stderr here.
       These are pointers into member __sf defined below.  */
    __FILE* _stdin, * _stdout, * _stderr;	/* XXX */

    int  _inc;			/* used by tmpnam */

    char* _emergency;

#ifdef _REENT_BACKWARD_BINARY_COMPAT
    int _reserved_0;
    int _reserved_1;
#endif
    void* _locale;/* per-thread locale */

    void* _mp;

    void (*__cleanup) (struct _reent*);

    int _gamma_signgam;

    /* used by some fp conversion routines */
    int _cvtlen;			/* should be size_t */
    char* _cvtbuf;

    void* _r48;
    void* _localtime_buf;
    char* _asctime_buf;

    /* signal info */
    void (**_sig_func)(int);

#ifdef _REENT_BACKWARD_BINARY_COMPAT
    struct _atexit* _reserved_6;
    struct _atexit _reserved_7;
    struct _glue _reserved_8;
#endif

    __FILE* __sf;			        /* file descriptors */
    void* _misc;            /* strtok, multibyte states */
    char* _signal_buf;                    /* strsignal */
};