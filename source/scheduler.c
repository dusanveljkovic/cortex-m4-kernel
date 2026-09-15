#include "../include/scheduler.h"
#include "../include/tcb.h"

#define MAX_PRIORITY 15

static tcb_list_t ready_queues[MAX_PRIORITY];

volatile tcb_t *current_task;
volatile tcb_t *next_task;

void scheduler_init(void) {
  for (int i = 0; i < MAX_PRIORITY; i++) {
    ready_queues[i].head = 0;
    ready_queues[i].tail = 0;
  }
  current_task = 0;
  next_task = 0;
}

// put task in respective queue based on its priority
// skip finished or blocked task
void scheduler_put_task(tcb_t *task) {
  if (task->state == TASK_FINISHED || task->state == TASK_BLOCKED)
    return;

  queue_push(&ready_queues[task->priority], task);
}

// select the next task to run and get rid of all
// finished tasks in that queue
void scheduler_select_next(void) {
  for (int p = 0; p < MAX_PRIORITY; p++) {
    tcb_list_t *q = &ready_queues[p];

    while (q->head != 0 && q->head->state == TASK_FINISHED)
      queue_pop(q);

    if (q->head != 0) {
      next_task = queue_pop(q);
      return;
    }
  }

  next_task = current_task;
}

tcb_t *queue_pop(tcb_list_t *q) {
  tcb_t *ret = q->head;
  q->head = q->head->next;
  if (q->head == 0)
    q->tail = 0;
  return ret;
}

void queue_push(tcb_list_t *q, tcb_t *task) {
  if (q->tail == 0) {
    q->head = q->tail = task;
  } else {
    q->tail->next = task;
    q->tail = task;
  }
}
