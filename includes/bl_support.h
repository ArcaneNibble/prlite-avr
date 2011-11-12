#ifndef BL_SUPPORT_H
#define BL_SUPPORT_H

extern void bl_reboot(void);
extern unsigned char bl_get_addr(void);
extern void bl_erase_lib_csum(void);
extern void bl_erase_app_csum(void);

#endif
