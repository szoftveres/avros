#ifndef __COMMONDEF_H__
#define __COMMONDEF_H__

#define UNUSED      __attribute__ ((unused))

#define STRINGIFY_(a) #a
#define STRINGIFY(a) STRINGIFY_(a)



#define LOW(val) ((unsigned int)(val) & 0xFF)
#define HIGH(val) (((unsigned int)(val) >> 8) & 0xFF)


#endif
