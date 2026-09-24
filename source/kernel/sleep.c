#include "../../include/sleep.h"
#include "../../include/arm.h"
#include "../../include/scheduler.h"

static uint32_t system_ticks = 0;
static tcb_list_t sleep_list = {0, 0};

void sleep_queue_push(tcb_list_t *q, tcb_t *task) {
  task->next = 0;

  if (q->head == 0) {
    q->head = task;
    q->tail = task;
    return;
  }

  if (task->wake_tick < q->head->wake_tick) {
    task->next = q->head;
    q->head = task;
    return;
  }

  tcb_t *current = q->head;
  while (current->next != 0 && current->next->wake_tick <= task->wake_tick)
    current = current->next;

  task->next = current->next;
  current->next = task;

  if (task->next == 0)
    q->tail = task;
}

void sleep_for_ticks(uint32_t ticks) {
  if (ticks == 0 || current_task == 0)
    return;
  if (current_task->state != TASK_RUNNING)
    return;
  uint32_t irq_state = irq_save();

  current_task->state = TASK_SLEEPING;
  current_task->wake_tick = system_ticks + ticks;
  sleep_queue_push(&sleep_list, current_task);

  irq_restore(irq_state);
}

tcb_t *sleep_queue_pop(tcb_list_t *q) {
  tcb_t *task = q->head;
  if (task != 0 && (int32_t)(system_ticks - task->wake_tick) >= 0) {
    q->head = q->head->next;
    if (q->head == 0)
      q->tail = 0;
    return task;
  }
  return 0;
}

void sleep_tick(void) {
  uint32_t irq_state = irq_save();
  system_ticks++;

  tcb_t *to_wakeup = sleep_queue_pop(&sleep_list);
  while (to_wakeup) {
    to_wakeup->state = TASK_READY;
    scheduler_put_task(to_wakeup);

    to_wakeup = sleep_queue_pop(&sleep_list);
  }

  irq_restore(irq_state);
}
