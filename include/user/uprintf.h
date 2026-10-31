#ifndef _UPRINTF_H
#define _UPRINTF_H

#include "../../include/kernel/syscall.h"
#include "../../include/user/dhcp.h"
#include "stdarg.h"
#include "stdint.h"

// unprivileged printf: format locally then write via sys_console_write()
int uprintf(const char *fmt, ...);
void print_net_error(net_reply_t *reply);
void print_dhcp_error(dhcp_status_t status);
void dhcp_msg_print(dhcp_msg_t *msg, uint16_t len, uint8_t *buffer);
void dhcp_print_msg_type(dhcp_msg_type_t msg_type);

#endif
