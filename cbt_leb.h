#ifndef LEB_INCLUDE_LEB_H
#define LEB_INCLUDE_LEB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LEB_STATIC
#define LEBDEF static
#else
#define LEBDEF extern
#endif

struct leb_Node;
struct leb_DiamondParent;
struct leb_NodeAndNeighbors;

// manipulation
LEBDEF void cbt_leb_Split2D     (cbt_Tree *leb, const leb_Node node);
LEBDEF void cbt_leb_Merge2D     (cbt_Tree *leb, const leb_Node node);
LEBDEF void cbt_leb_Split2D_Quad(cbt_Tree *leb, const leb_Node node);
LEBDEF void cbt_leb_Merge2D_Quad(cbt_Tree *leb, const leb_Node node);
typedef void (*cbt_leb_MergeCallback)(cbt_Tree *leb, const leb_DiamondParent diamond);
LEBDEF void cbt_leb_MergeUpdate(cbt_Tree *leb, cbt_leb_MergeCallback updater);
typedef void (*cbt_leb_SplitCallback)(cbt_Tree *leb, const leb_Node node);
LEBDEF void cbt_leb_SplitUpdate(cbt_Tree *leb, cbt_leb_SplitCallback updater);

// subdivision routines
LEBDEF void leb_DecodeNodeAttributeArray2D(const leb_Node node,
                                           int32_t attributeArraySize,
                                           float attributeArray[][3]);
LEBDEF void leb_DecodeNodeAttributeArray2D_Quad(const leb_Node node,
                                                int32_t attributeArraySize,
                                                float attributeArray[][3]);

// intersection test O(depth)
LEBDEF bool leb_BoundingNode2D     (const leb_Heap *leb, float x, float y);
LEBDEF bool leb_BoundingNode2D_Quad(const leb_Heap *leb, float x, float y);

#ifdef __cplusplus
} // extern "C"
#endif

//
//
//// end header file ///////////////////////////////////////////////////////////
#endif // LEB_INCLUDE_LEB_H

#if 1
//#ifdef LEB_IMPLEMENTATION

#ifndef LEB_ASSERT
#    include <assert.h>
#    define LEB_ASSERT(x) assert(x)
#endif

#ifndef LEB_LOG
#    include <stdio.h>
#    define LEB_LOG(format, ...) do { fprintf(stdout, format, ##__VA_ARGS__); fflush(stdout); } while(0)
#endif

#ifndef LEB_MALLOC
#    include <stdlib.h>
#    define LEB_MALLOC(x) (malloc(x))
#    define LEB_FREE(x) (free(x))
#else
#    ifndef LEB_FREE
#        error LEB_MALLOC defined without LEB_FREE
#    endif
#endif


typedef struct cbt_Tree leb_Heap;


#endif // LEB_IMPLEMENTATION
