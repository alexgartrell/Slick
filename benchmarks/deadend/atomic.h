#ifndef _ATOMIC_H
#define _ATOMIC_H

typedef long int atomic_t;

extern long int atomic_add(atomic_t *, long int);
extern long int atomic_xchg(atomic_t *, long int);

#endif
