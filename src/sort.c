#include "sort.h"
#include "ints.h"
#include "mid_alloc.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// swap two elements of given size
static void swap(void *v1, void *v2, mid_isize size, char *buffer)
{
    // copy bytes using memcpy
    memcpy(buffer, v1, size);
    memcpy(v1, v2, size);
    memcpy(v2, buffer, size);
}

// honestly ripped this straight off geeksforgeeks.org
// generic quicksort
// v: array, size: element size
// left/right: range
// comp: comparison function
static void
mid_qsort_impl(void *v, mid_isize size, mid_isize left, mid_isize right,
               int (*comp)(const void *, const void *, const void *),
               const void *info, char *buffer)
{
    void *vt, *v3;
    mid_isize i, last, mid = (left + right) / 2;

    if (left >= right)
        return;

    // cast to char* for pointer arithmetic
    void *vl = ((char *)v + (left * size));
    void *vr = ((char *)v + (mid * size));

    swap(vl, vr, size, buffer);
    last = left;

    for (i = left + 1; i <= right; i++) {

        // element address
        vt = ((char *)v + (i * size));

        if ((*comp)(vl, vt, info) > 0) {
            ++last;
            v3 = ((char *)v + (last * size));
            swap(vt, v3, size, buffer);
        }
    }

    v3 = ((char *)v + (last * size));
    swap(vl, v3, size, buffer);

    mid_qsort_impl(v, size, left, last - 1, comp, info, buffer);
    mid_qsort_impl(v, size, last + 1, right, comp, info, buffer);
}

void Mid_qsort(void *v, mid_isize nmemb, mid_isize size,
               int comp(const void *, const void *, const void *),
               const void *info)
{
    char *buffer = Mid_malloc(size);
    mid_qsort_impl(v, size, 0, nmemb - 1, comp, info, buffer);
    free(buffer);
}
