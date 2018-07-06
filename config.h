#ifndef __CONFIG_H__
#define __CONFIG_H__

#define USB_WIDGET_TEST
#define SPICE_COMPILATION
#define USE_NEW_USB_WIDGET
#define USE_CD_SHARING

#define _(x) x
#define ngettext(x,y,z) ((z) == 1 ? (x) : (y))
#define SPICE_DEBUG(fmt, ...) g_print(fmt "\n", ##__VA_ARGS__)

#endif
