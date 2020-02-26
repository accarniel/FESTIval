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

/* 
 * File:   fast_max_heap.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 4, 2016, 8:53 PM
 */

#ifndef FAST_MAX_HEAP_H
#define FAST_MAX_HEAP_H

typedef struct {
    int fu;
    double priority;
} HeapElement;

typedef struct {
    int n; //number of elements in the binary_heap
    int max;
    HeapElement *binary_heap;
    int *keys; //keys represent the index of flushing units
} MaxHeap;

extern HeapElement get_maxheap(MaxHeap *heap);
extern void insert_maxheap(MaxHeap *heap, int key, int p);
extern void modify_maxheap(MaxHeap *heap, int key, int p);
extern MaxHeap *create_maxheap(int capacity);
extern void destroy_maxheap(MaxHeap *heap);

#endif /* FAST_MAX_HEAP_H */

