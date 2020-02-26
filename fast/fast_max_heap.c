/**********************************************************************
 *
 * FESTIval - Framework to Evaluate SpaTial Indices in non-VolAtiLe memories and hard disk drives.
 * https://accarniel.github.io/FESTIval/
 *
 * Copyright (C) 2016-2020 Anderson Chaves Carniel <accarniel@gmail.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 * Fully developed by Anderson Chaves Carniel
 *
 **********************************************************************/

#include "fast_max_heap.h"
#include <liblwgeom.h>

static void swap(MaxHeap *heap, int i, int j);
static void swim(MaxHeap *heap, int k);
static void sink(MaxHeap *heap, int k);

void swap(MaxHeap *heap, int i, int j) {
    HeapElement s;
    s = heap->binary_heap[i];
    heap->binary_heap[i] = heap->binary_heap[j];
    heap->binary_heap[j] = s;

    heap->keys[heap->binary_heap[i].fu] = i;
    heap->keys[heap->binary_heap[j].fu] = j;
}

void swim(MaxHeap *heap, int k) {
    int m;
    m = k / 2.0;
    while (k > 1 && heap->binary_heap[m].priority < heap->binary_heap[k].priority) {
        swap(heap, k, m);
        k = m;
        m = k / 2.0;
    }
}

void sink(MaxHeap *heap, int k) {
    int j;
    while (2 * k <= heap->n) {
        j = 2 * k;
        if (j < heap->n && heap->binary_heap[j].priority < heap->binary_heap[j + 1].priority)
            j++;
        if (!(heap->binary_heap[k].priority < heap->binary_heap[j].priority))
            break;
        swap(heap, k, j);
        k = j;
    }
}

MaxHeap *create_maxheap(int capacity) {
    int i;

    MaxHeap *ret;
    ret = (MaxHeap*) lwalloc(sizeof (MaxHeap));
    ret->n = 0;
    ret->binary_heap = (HeapElement*) lwalloc(sizeof (HeapElement) * (capacity + 1));
    ret->binary_heap[0].fu = 0;
    ret->binary_heap[0].priority = 0;
    ret->max = capacity;

    ret->keys = (int*) lwalloc(sizeof (int) * (capacity + 1));
    for (i = 0; i < capacity + 1; i++) {
        ret->keys[i] = -1;
    }

    return ret;
}

HeapElement get_maxheap(MaxHeap *heap) {
    HeapElement ret;
    if (heap->n == 0) {
        ret.fu = -1;
        ret.priority = -1;
        return ret;
    }
    ret = heap->binary_heap[1];
    swap(heap, 1, heap->n--);

    sink(heap, 1);

    heap->keys[ret.fu] = -1;

    return ret;
}

void insert_maxheap(MaxHeap *heap, int fu, int p) {
    if (fu >= heap->max) {
        int i;
        heap->max *= 2;
        heap->keys = (int*) lwrealloc(heap->keys, sizeof (int) * (heap->max + 1));
        heap->binary_heap = (HeapElement*) lwrealloc(heap->binary_heap, sizeof (HeapElement) * (heap->max + 1));
        for (i = heap->n + 1; i < heap->max + 1; i++) {
            heap->keys[i] = -1;
        }
    }

    heap->n++;
    heap->binary_heap[heap->n].fu = fu;
    heap->binary_heap[heap->n].priority = p;
    heap->keys[fu] = heap->n;

    swim(heap, heap->n);
}

void modify_maxheap(MaxHeap *heap, int fu, int p) {
    int i;

    if (fu > heap->n || heap->keys[fu] == -1) {
        insert_maxheap(heap, fu, p);
        return;
    }
    
    i = heap->keys[fu];

    heap->binary_heap[i].priority = p;
    swim(heap, i);
    sink(heap, i);
}

void destroy_maxheap(MaxHeap *heap) {
    if (heap->n > 0) {
        lwfree(heap->keys);
        lwfree(heap->binary_heap);
    }
    lwfree(heap);
}
