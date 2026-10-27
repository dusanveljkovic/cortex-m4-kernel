#include "../../include/net.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"
#include "../../include/utils.h"
#include "fpu_test.h"
#include "uprintf.h"
#include <stdint.h>

typedef struct {
  int number;
  semaphore_t *sem1;
  semaphore_t *sem2;
  mutex_t *m;
} wrapper_t;

void print(const char *str) {
  while (*str) {
    sys_putc(*str++);
  }
}

void high_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    sys_sleep(1000);
    // sys_mutex_lock(w.m);
    // print("high task mutex lock\r\n");
    // data += w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    //   print("high task mutex unlock\r\n");
    //   sys_mutex_unlock(w.m);
    // }
    // sys_yield();
    // sys_sem_post(w.sem1);
    // sys_sem_wait(w.sem2);
    // sys_putc('H');
  }
}

void med_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    sys_sleep(1000);
    // print("medium task processing\r\n");
    // data -= w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    // }
    // sys_yield();
    // // sys_sem_post(w.sem2);
    // // sys_sem_wait(w.sem1);
    // sys_putc('M');
  }
}

void low_task(void *arg) {
  tcb_t *t1 = sys_task_create(1, high_task, arg);
  tcb_t *t2 = sys_task_create(1, med_task, arg);
  sys_task_set_name(t1, "high task");
  sys_task_set_name(t2, "med task");
  // tcb_t *t3 = sys_task_create(2, med_task, arg);

  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    sys_sleep(1000);
    // sys_mutex_lock(w.m);
    // print("low task mutex lock\r\n");
    // data -= w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    //   print("low task mutex unlock\r\n");
    //   sys_mutex_unlock(w.m);
    // }
    // sys_yield();
    // // sys_sem_post(w.sem2);
    // // sys_sem_wait(w.sem1);
    // sys_putc('L');
  }
}

void print_net_error(net_reply_t *reply) {
  uprintf("![user] NET ERROR: ");
  switch (reply->status) {
  case NET_STATUS_OK:
    uprintf("OK\r\n");
    break;
  case NET_STATUS_RETRY:
    uprintf("RETRY\r\n");
    break;
  case NET_STATUS_BAD_REQUEST:
    uprintf("BAD REQUEST\r\n");
    break;
  case NET_STATUS_FAULT:
    uprintf("FAULT\r\n");
    break;
  case NET_STATUS_DEAD:
    uprintf("DEAD\r\n");
    break;
  case NET_STATUS_UNREACHABLE:
    uprintf("UNREACHABLE\r\n");
    break;
  }
}

void net_test(void) {
  uprintf("[user] net test\r\n");
  int32_t link = -1;
  net_reply_t reply_slot = {0, 0, 0, 0, 0};

  while (true) {
    sys_net_link(&link, &reply_slot);
    if (reply_slot.status != NET_STATUS_OK) {
      print_net_error(&reply_slot);
      return;
    }
    uprintf("[user] link status %d\r\n", link);
    sys_sleep(1000);
    if (link == 1)
      break;
  }

  socket_t *socket = sys_socket_get(0);
  socket->protocol = UDP;
  socket->port = 6767;
  sys_socket_open(socket, &reply_slot);
  if (reply_slot.status != NET_STATUS_OK) {
    print_net_error(&reply_slot);
    return;
  }
  uint8_t *buffer = (uint8_t *)sys_malloc(1024 * sizeof(uint8_t));

  // while (true) {
  //   sys_net_recv(socket, buffer, 1024, &reply_slot);
  //   if (reply_slot.status == NET_STATUS_OK && reply_slot.got != 0) {
  //     uprintf("[user] received udp datagram:\r\n");
  //     uprintf("\tIP:PORT\t%d.%d.%d.%d:%d\r\n", reply_slot.src_ip[0],
  //             reply_slot.src_ip[1], reply_slot.src_ip[2],
  //             reply_slot.src_ip[3], reply_slot.port);
  //     uprintf("\tgot\t%d\r\n", reply_slot.got);
  //     uprintf("\top\t%x\r\n", buffer[0]);
  //     uprintf("\txid\t%x%x%x%x\r\n", buffer[4], buffer[5], buffer[6],
  //             buffer[7]);
  //     uprintf("\tMAC\t%x%x%x%x%x%x\r\n", buffer[28], buffer[29], buffer[30],
  //             buffer[31], buffer[32], buffer[33]);
  //   }
  //   if (reply_slot.status != NET_STATUS_OK)
  //     print_net_error(&reply_slot);
  //
  //   sys_sleep(2000);
  // }
  uint8_t *send_buffer = (uint8_t *)sys_malloc(300 * sizeof(uint8_t));
  uint8_t count = 0;

  // while (true) {
  memset(send_buffer, count++, 300);
  uprintf("[user]sent buffer: \r\n");
  for (int i = 0; i < 300; i++) {
    uprintf("%x ", send_buffer[i]);
  }
  uprintf("\r\n");
  net_send_args_t args = {.tx_data = send_buffer,
                          .tx_length = 300,
                          .dst_ip = {192, 168, 0, 200},
                          .dst_port = 6767};
  sys_net_send(socket, &args, &reply_slot);
  print_net_error(&reply_slot);
  sys_sleep(2000);
  net_send_args_t args2 = {.tx_data = send_buffer,
                           .tx_length = 300,
                           .dst_ip = {255, 255, 255, 255},
                           .dst_port = 6767};
  sys_net_send(socket, &args2, &reply_slot);
  print_net_error(&reply_slot);
  sys_sleep(2000);
  // }
}

void user_main(void *arg) {
  // semaphore_t *sem1 = sys_sem_create(0);
  // semaphore_t *sem2 = sys_sem_create(0);
  // mutex_t *m1 = sys_mutex_create();
  // wrapper_t *parg1 = sys_malloc(sizeof(wrapper_t));
  // parg1->number = 1;
  // parg1->sem1 = sem1;
  // parg1->sem2 = sem2;
  // parg1->m = m1;
  uprintf("[user] user task\r\n");
  // tcb_t *t3 = sys_task_create(1, low_task, parg1);
  // sys_task_set_name(t3, "low task");
  // float x1 = 0.222;
  // float x2 = 0.333;
  // float x3 = x1 + x2;

  // *((uint32_t *)0x20000c00) = 1;
  // *((uint32_t *)0x20001000) = 1;
  // *((uint32_t *)0x20001004) = 1;
  // *((uint32_t *)0x20001008) = 1;
  // *((uint32_t *)0x2000100C) = 1;
  // uprintf("[user] testing FPU");
  // fpu_test_start();
  net_test();
}
