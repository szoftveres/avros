#include <stddef.h>
#include "../include/queue.h"

/*    
 *    QUEUE - General purpose queue (doubly liked list) implementation
 *    2012 Marton Kun-Szabo
 */

/**
 * q_init - initialize the queue
 *   param1[in]: pointer to the queue
 */
void
q_init (q_head_t *que) {
    if (que) {
        que->head = NULL;
        que->tail = NULL;
    }
    return;
}

/** 
 * q_front - put an item to the front of the queue
 *   param1[in]: pointer to the queue
 *   param2[in]: pointer to the item
 *   return:     pointer to the item on success, NULL on error
 */
q_item_t*
q_front (q_head_t *que, q_item_t* item) {
    if (!item || !que) return (NULL);
    if (que->head) {
        item->next = que->head;
        item->prev = item->next->prev;
        item->next->prev = item;
    } else {
        item->next = que->tail;
        item->prev = que->head;
        que->tail = item;
    }
    que->head = item;
    return (item);
}

/** 
 * q_end - put an item to the end of the queue
 *   param1[in]: pointer to the queue
 *   param2[in]: pointer to the item
 *   return:     pointer to the item on success, NULL on error
 */
q_item_t*
q_end (q_head_t *que, q_item_t* item) {
    if (!item || !que) return (NULL);
    if (que->tail) {
        item->prev = que->tail;
        item->next = item->prev->next;
        item->prev->next = item;
    } else {
        item->next = que->tail;
        item->prev = que->head;
        que->head = item;
    }
    que->tail = item;
    return (item);
}

/** 
 * q_remv - remove an item from the queue
 *   param1[in]: pointer to the queue
 *   param2[in]: pointer to the item
 *   return:     pointer to the item on success, NULL on error
 */
q_item_t*
q_remv (q_head_t *que, q_item_t* item) {
    if (!item || !que) return (NULL);
    if (item->prev) item->prev->next = item->next;
    else que->head = item->next;
    if (item->next) item->next->prev = item->prev;
    else que->tail = item->prev;
    return (item);
}

/** 
 * q_forall - iterate through the queue starting from the front to the end
 *     If callback function returns with a pointer to an item, iteration 
 *     stops and function returns with this pointer
 *   param1[in]: pointer to the queue
 *   param2[in]: pointer to the callback function
 *   return:     NULL, or pointer to an item if callback function returns 
 *               with non-NULL
 *   Note:       callback function is allowed to removed the current item 
 *               from the queue, this operation is safe
 */
q_item_t*
q_forall (q_head_t *que, q_item_t*(*callback)(q_head_t*, q_item_t*)) {
    q_item_t* ret = (NULL);
    q_item_t* it;
    q_item_t* next;

    if (!que || !callback) return (NULL);
    for (it = que->head; it; it = next) {
        next = it->next;
        if ((ret = (*callback)(que, it)) != NULL) break;
    }
    return (ret);
}


