#ifndef _QUEUE_H_
#define _QUEUE_H_

/*    QUEUE - General purpose queue (doubly liked list) implementation
 *    2012 Marton Kun-Szabo
 */

#define QUEUE_HEADER            \
    struct q_item_s*    next;   \
    struct q_item_s*    prev;   \


typedef struct q_item_s {
    QUEUE_HEADER
} q_item_t, *q_item_p;

typedef struct q_head_s {
    struct q_item_s*  head;
    struct q_item_s*  tail;
} q_head_t, *q_head_p;

void        q_init      (q_head_t *que);
q_item_t*   q_front     (q_head_t *que, q_item_t* item);
q_item_t*   q_end       (q_head_t *que, q_item_t* item);
q_item_t*   q_remv      (q_head_t *que, q_item_t* item);
q_item_t*   q_forall    (q_head_t *que, q_item_t*(*callback)(q_head_t*,q_item_t*));

#define Q_FRONT(Q,I)    q_front((Q),(q_item_t*)(I))
#define Q_END(Q,I)      q_end((Q),(q_item_t*)(I))
#define Q_REMV(Q,I)     q_remv((Q),(q_item_t*)(I))
#define Q_FIRST(Q)      ((Q).head)
#define Q_LAST(Q)       ((Q).tail)
#define Q_NEXT(I)       (((q_item_t*)(I))->next)
#define Q_PREV(I)       (((q_item_t*)(I))->prev)
#define Q_EMPTY(Q)      ((!(Q).head) && (!(Q).tail))

#endif /* _QUEUE_H_ */

