#include <stdlib.h>

#include "vector.h" // C:

/* PRIVATE */
static int vec_realloc(vector *vec, size_t reserve) {
  unsigned char *realloced_data =
      (unsigned char *)realloc(vec->data, sizeof(unsigned char) * reserve);
  if (realloced_data == NULL)
    return 1;

  vec->data = realloced_data;
  vec->size = reserve;
  if (vec->end > vec->size)
    vec->end = vec->size;

  return 0;
}

/* PUBLIC */
/* constructor */
vector *new_vector() {
  vector *vec = (vector *)malloc(sizeof(vector));
  if (!vec)
    return NULL;

  vec->data = malloc(sizeof(unsigned char) * DFLT_VEC_SIZE);
  if (!vec->data)
    return NULL;

  vec->size = DFLT_VEC_SIZE;
  /* memset(vec->data, NULL, sizeof(unsigned char) * vec->size); */
  vec->end = 0;
  return vec;
}

/* destructor */
void free_vector(vector *vec) {
  free(vec->data);
  free(vec);
}

bool vec_contains(vector *vec, unsigned char elem) {
  for (size_t i = 0; i < vec->end; ++i) {
    if (vec_at(vec, i) == elem)
      return true;
  }
  return false;
}

/* returns first elem */
unsigned char vec_begin(vector *vec) {
  if (vec->end == 0)
    return 0;

  return vec->data[0];
}

/* returns last elem */
unsigned char vec_end(vector *vec) {
  if (vec->end == 0)
    return 0;

  return vec->data[vec->end - 1];
}

/* max number of elements that can be used before realloc */
size_t vec_reserved(vector *vec) { return vec->size; }

/* reserve more elements */
size_t vec_reserve(vector *vec, size_t reserve) {
  if (vec->size >= reserve)
    return vec->size;

  if (vec_realloc(vec, reserve))
    return 0;
  return reserve;
}

/* insert element at the end */
int vec_push(vector *vec, unsigned char elem) {
  if (!vec->size) // 0 reserved space
    if (vec_realloc(vec, DFLT_VEC_SIZE))
      return 1;

  /* double alloced space if it is exhausted */
  if (vec->end == vec->size)
    if (vec_realloc(vec, vec->size * 2))
      return 1;

  vec->data[vec->end] = elem;
  ++vec->end;

  return 0;
}

/* delete element at the end */
void vec_pop(vector *vec) {
  if (!vec->end)
    return;

  vec->data[vec->end - 1] = 0;
  --vec->end;
}

/* change element at given index */
void vec_set(vector *vec, size_t i, unsigned char elem) {
  if (i >= vec->end)
    return;

  vec->data[i] = elem;
}

void vec_clear(vector *vec) {
  for (size_t i = 0; i < vec->end; ++i)
    vec_set(vec, i, 0);
  vec->end = 0;
}

/* get element at given index */
unsigned char vec_at(vector *vec, size_t i) {
  if (i >= vec->end)
    return 0;

  return vec->data[i];
}

void vec_insert(vector *vec, size_t i, unsigned char elem) {
  if (i >= vec->end)
    return;

  /* alloc new data array */
  unsigned char *tempvec;
  if (vec->end == vec->size) {
    tempvec = (unsigned char *)malloc(sizeof(unsigned char) * vec->size * 2);
    vec->size *= 2;
  } else
    tempvec = (unsigned char *)malloc(sizeof(unsigned char) * vec->size);

  /* move elements until the the index to insert */
  for (size_t j = 0; j < i; ++j) {
    tempvec[j] = vec->data[j];
  }
  tempvec[i] = elem;

  for (size_t j = i; j < vec->end; ++j) {
    tempvec[j + 1] = vec->data[j];
  }

  free(vec->data);
  vec->data = tempvec;
  ++vec->end;
}

/* delete element at given index */
void vec_delete(vector *vec, size_t i) {
  if (i >= vec->end)
    return;

  unsigned char *tempvec =
      (unsigned char *)malloc(sizeof(unsigned char) * vec->size);
  for (size_t j = 0; j < i; ++j) {
    tempvec[j] = vec->data[j];
  }

  for (size_t j = i + 1; j < vec->end; ++j) {
    tempvec[j - 1] = vec->data[j];
  }

  free(vec->data);
  vec->data = tempvec;
  --vec->end;
}
