#ifndef __CONFIG_H__
#define __CONFIG_H__

#define USB_WIDGET_TEST
#define SPICE_COMPILATION

#define _(x) x
#define ngettext(x,y,z) x
#define SPICE_DEBUG(fmt, ...) g_print(fmt "\n", ##__VA_ARGS__)

#endif