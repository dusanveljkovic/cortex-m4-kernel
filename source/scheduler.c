#include "../include/scheduler.h"
#include "../include/tcb.h"

static tcb_list_t ready_queue;

volatile tcb_t *current_task;
volatile tcb_t *next_task;

void scheduler_init(void) {
  ready_queue.head = 0;
  ready_queue.tail = 0;
  current_task = 0;
  next_task = 0;
}

// put task in respective queue based on its priority
// skip finished or blocked task
void scheduler_put_task(tcb_t *task) {
  if (task->state == TASK_FINISHED || task->state == TASK_BLOCKED)
    return;
  task->state = TASK_READY;

  priority_queue_push(&ready_queue, task);
}

// select the next task to run and get rid of all
// finished tasks in that queue
void scheduler_select_next(void) {
  if (ready_queue.head != 0) {
    next_task = priority_queue_pop(&ready_queue);
    return;
  }

  next_task = current_task;
}

tcb_t *queue_pop(tcb_list_t *q) {
  tcb_t *ret = q->head;
  if (ret == 0)
    return 0;

  q->head = q->head->next;

  if (q->head == 0)
    q->tail = 0;

  ret->next = 0;
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

void priority_queue_push(tcb_list_t *q, tcb_t *task) {
  task->next = 0;

  if (q->head == 0) {
    q->head = task;
    q->tail = task;
    return;
  }

  if (task->effective_priority < q->head->effective_priority) {
    task->next = q->head;
    q->head = task;
    return;
  }

  tcb_t *current = q->head;
  while (current->next != 0 &&
         current->next->effective_priority <= task->effective_priority)
    current = current->next;

  task->next = current->next;
  current->next = task;

  if (task->next == 0)
    q->tail = task;
}

tcb_t *priority_queue_pop(tcb_list_t *q) {
  tcb_t *task = q->head;

  if (task == 0)
    return 0;

  q->head = task->next;

  if (q->head == 0)
    q->tail = 0;

  task->next = 0;

  return task;
}
