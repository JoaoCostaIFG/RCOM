/** @file vector.h */
#ifndef __vec_H__
#define __vec_H__

#include <stdbool.h>
#include <stddef.h>

/** @addtogroup	util_grp
 * @{
 */

/** Default starting size of a vector object */
#define DFLT_VEC_SIZE 100

/** @struct vec_t
 *  Vector object.
 */
typedef struct vec_t {
  unsigned char *data; /**< Array that holds the current vector information. */
  size_t size;         /**< Array size. */
  size_t end;          /**< How many elements are in the array. */
} vector;

/**
 * @brief   Creates a new vector object.
 * @return  Pointer to the new vector object, on success\n
 *          NULL, otherwise.
 */
vector *new_vector();

/**
 * @brief Free the vector object (leaves the data alloced).
 * @param vec Vector to free.
 */
void free_vector(vector *vec);

/**
 * @brief	Verifies if a given element is present in a given vector.
 *
 * @param vec	  Vector where the search will occur.
 * @param elem	Element to find.
 *
 * @return	True, if the element is present in the vector\n
 *          False, otherwise
 */
bool vec_contains(vector *vec, unsigned char elem);

/**
 * @brief Get a pointer to the first element in the vector.
 *
 * @param vec Vector to get the element from.
 *
 * @return  Pointer to the first element, on success\n
 *          NULL, otherwise.
 */
unsigned char vec_begin(vector *vec);

/**
 * @brief Get a pointer to the last element in the vector.
 *
 * @param vec Vector to get the element from.
 *
 * @return  Pointer to the last element, on success\n
 *          NULL, otherwise.
 */
unsigned char vec_end(vector *vec);

/**
 * @brief Get the current reserved space of a vector.
 *
 * @param vec Vector to get the size from.
 *
 * @return  Number of reserved elements.
 */
size_t vec_reserved(vector *vec);

/**
 * @brief   Attempt to reserve space in a vector.
 * @warning May reserve less elements than asked if there's not enough memory
 * for them.
 *
 * @param vec     Vector to work on.
 * @param reserve Number of elements to reserve.
 *
 * @return  Number of reserved elements.
 */
size_t vec_reserve(vector *vec, size_t reserve);

/**
 * @brief Push an element into a given vector.
 *
 * @param vec   Vector to insert element into.
 * @param elem  Element to insert.
 */
int vec_push(vector *vec, unsigned char elem);

/**
 * @brief Pop a given vector.
 * @param vec   Vector to remove element from.
 */
void vec_pop(vector *vec);

/**
 * @brief   Set a new value for a vector element.
 * @warning Doesn't free the old element.
 *
 * @param vec   Vector to work with.
 * @param i     Index of the element to change.
 * @param elem  New element to insert in the old one's place.
 */
void vec_set(vector *vec, size_t i, unsigned char elem);

/**
 * @brief	  Sets all pointers of a given vector to 0
 * @param vec Vector to be cleared
 */
void vec_clear(vector *vec);

/**
 * @brief Get pointer to an element at a given position.
 *
 * @param vec Vector to get the element from.
 * @param i   Index of the element in the vector.
 *
 * @return  Pointer to the element, on success\n
 *          NULL, otherwise.
 */
unsigned char vec_at(vector *vec, size_t i);

/**
 * @brief Insert element into a given index.
 *
 * @param vec   Vector to insert the element into.
 * @param i     Index to insert the element into.
 * @param elem  Element to insert.
 */
void vec_insert(vector *vec, size_t i, unsigned char elem);

/**
 * @brief Remove element in a given index.
 *
 * @param vec Vector to remove the element from.
 * @param i   Index of the element in the vector.
 */
void vec_delete(vector *vec, size_t i);

/** @} */

#endif // __vec_H__
