/**
 *  File: huffman.h
 *  Description:
 *      Huffman Coding API
 */
#ifndef _HUFFMAN_H
#define _HUFFMAN_H

#include <stdint.h>


#define MAX_RUN_LEN 15
#define MAX_SIZE    10


/**
 *  Represents a node within a Huffman tree.
 */
struct huffman_tree_node
{
    uint8_t                   leaf;
    uint8_t                   symbol;
    struct huffman_tree_node* children[2];
};

/**
 *  Create a new huffman tree after the AC coefficients table codes
 *  with the supplied node as root.
 *  destroy_huffman_tree needs to be called on the root to free memory.
 */
void create_huffman_ac_tree (struct huffman_tree_node* root) ;

/**
 *  Create a new huffman tree after the DC coefficients table codes
 *  with the supplied node as root.
 *  destroy_huffman_tree needs to be called on the root to free memory.
 */
void create_huffman_dc_tree (struct huffman_tree_node* root) ;

/**
 *  Decode a Huffman value from buffer starting at bit p using the supplied
 *  Huffman tree 'tree'.
 *  Returns the decoded symbol. For AC coefficients this is (run length, size) and
 *  for DC coefficients (size of difference).
 */
uint8_t decode_huffman_value (struct huffman_tree_node* tree, uint8_t* buffer, int* p) ;

/**
 *  Encode DC or AC value to a buffer pointed by destination, starting at bit pointer p.
 */
void encode_huffman_dc_value (uint16_t amplitude, uint8_t* destination, int* p) ;
void encode_huffman_ac_value (uint16_t amplitude, uint8_t run_length, uint8_t* destination, int* p) ;

/**
 *  Destroy a Huffman tree freeing memory from all child nodes.
 */
void destroy_huffman_tree (struct huffman_tree_node* root) ;

#endif
