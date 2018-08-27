#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

const int BLACK = 0;
const int RED = 1;


/*the node of the red&black tree*/
class node {
public:
    node(uint32_t _key = 0, uint32_t _value = 0);
    void free_tree(node *nil);

    uint32_t key;
    node *parent;
    node *left;
    node *right;
    int color;

    uint32_t value;
};


/*the tree*/
class rb_tree {
public:
    rb_tree();
    ~rb_tree();

    uint32_t search(uint32_t key);
    /**
    重复时返回之前的value
    */
    uint32_t insert(uint32_t key, uint32_t value);
    uint32_t remove(uint32_t key);

private:
    node *nil;
    node *root;

    inline void rb_reset_root(){
        if (root == nil)
            return;
        while (root->parent != nil)
            root = root->parent;
        return;
    }

    void left_rotate(node *x);

    void right_rotate(node *x);

    void rb_insert_fixup(node *z);

    void rb_delete_fixup(node *x);

    void rb_delete(node *z);

    uint32_t rb_insert(uint32_t _key, uint32_t value);

    int rb_search(uint32_t _key, node **result);

    inline node *tree_minimum(node *x){
        while (x->left != nil)
            x = x->left;
        return x;
    }

    inline node *tree_maximum(node *x){
        while (x->right != nil)
            x = x->right;
        return x;
    }
    inline node *tree_successor(node *x){
        if (x->right != nil)
            return tree_minimum(x->right);
        node *y = x->parent;
        while (y != nil && x == y->right) {
            x = y;
            y = y->parent;
        }
        return y;
    }
};



