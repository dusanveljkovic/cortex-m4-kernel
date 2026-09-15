#ifndef _SHEDULER_H
#define _SHEDULER_H

#include "tcb.h"

typedef struct tcb_list {
  tcb_t *head;
  tcb_t *tail;
} tcb_list_t;

tcb_t *queue_pop(tcb_list_t *q);
void queue_push(tcb_list_t *q, tcb_t *task);

void scheduler_init(void);
void scheduler_select_next(void);
void scheduler_put_task(tcb_t *task);

extern volatile tcb_t *current_task;
extern volatile tcb_t *next_task;

#endif // !_SHEDULER_H
