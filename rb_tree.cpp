#include "rb_tree.h"


//============================node===================================
node::node(uint32_t _key, uint32_t _value) : key(_key), parent(0), left(0), right(0), color(BLACK), value(_value){}

void node::free_tree(node *nil){
    if(left!=nil)
        left->free_tree(nil);
    if(right!=nil)
        right->free_tree(nil);
    delete this;
}


//============================rb_tree===================================
rb_tree::rb_tree() : nil(0), root(0){
    nil = new node();
    root = nil;
}

rb_tree::~rb_tree() {
    if(root!=nil)
        root->free_tree(nil);
    delete nil;
}

uint32_t rb_tree::search(uint32_t _key) {
    node *result = 0;
    if (rb_search(_key, &result) == -1){
        return 0;
    }
    return result->value;
}


uint32_t rb_tree::insert(uint32_t _key, uint32_t value) {
    return rb_insert(_key, value);
}


uint32_t rb_tree::remove(uint32_t _key) {
    node *result = 0;
    if (rb_search(_key, &result) == 0) {
        uint32_t prev=result.value;
        rb_delete(result);
        return prev;
    } else {
        return 0;
    }
}


void rb_tree::left_rotate(node *x) {
    node *y;
    y = x->right;
    x->right = y->left;
    if (y->left != nil)
        y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == nil) {
        root = y;
    } else {
        if (x == x->parent->left)
            x->parent->left = y;
        else
            x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
    rb_reset_root();
}

void rb_tree::right_rotate(node *x) {
    node *y;
    y = x->left;
    x->left = y->right;
    if (y->right != nil)
        y->right->parent = x;
    y->parent = x->parent;
    if (x->parent == nil) {
        root = y;
    } else {
        if (x == x->parent->right)
            x->parent->right = y;
        else
            x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
    rb_reset_root();
}

void rb_tree::rb_insert_fixup(node *z) {
    node *y;
    while (z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    left_rotate(z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                right_rotate(z->parent->parent);
            }
        } else {
            y = z->parent->parent->left;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                left_rotate(z->parent->parent);
            }
        }
    }
    root->color = BLACK;
}

uint32_t rb_tree::rb_insert(uint32_t _key, uint32_t _value) {
    node *z = new node(_key, _value);
    z->color = RED;
    z->left = nil;
    z->right = nil;
    z->parent = 0;

    node *y = nil;
    node *x = root;

    while (x != nil) {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        } else if (z->key > x->key) {
            x = x->right;
        } else {
            uint32_t prev=x->value;
            x->value=z->value;
            delete z;
            return prev;
        }
    }
    z->parent = y;

    if (y == nil) {
        root = z;
    } else {
        if (z->key < y->key)
            y->left = z;
        else
            y->right = z;
    }
    rb_insert_fixup(z);
    return 0;
}



void rb_tree::rb_delete_fixup(node *x) {
    node *w;
    while (x != root && x->color == BLACK) {
        if (x == x->parent->left) {
            w = x->parent->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                left_rotate(x->parent);
                w = x->parent->right;
            }
            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    right_rotate(w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                left_rotate(x->parent);
                x = root;
            }
        } else {
            w = x->parent->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                right_rotate(x->parent);
                w = x->parent->left;
            }
            if (w->right->color == BLACK && w->left->color == BLACK) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    left_rotate(w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                right_rotate(x->parent);
                x = root;
            }
        }
    }
    x->color = BLACK;
}

void rb_tree::rb_delete(node *z) {
    node *y;
    node *x;
    if (z->left == nil || z->right == nil) {
        y = z;
    } else {
        y = tree_successor(z);
    }
    if (y->left != nil) {
        x = y->left;
    } else {
        x = y->right;
    }
    x->parent = y->parent;
    if (y->parent == nil) {
        root = x;
    } else {
        if (y == y->parent->left) {
            y->parent->left = x;
        } else {
            y->parent->right = x;
        }
    }
    if (y != z) {
        z->key = y->key;
        z->value = y->value;
    }
    if (y->color == BLACK) {
        rb_delete_fixup(x);
    }
    delete y;
}


int rb_tree::rb_search(long _key, node **result) {
    node *_root = root;
    while (_root != nil && _root->key != _key) {
        if (_root->key > _key)
            _root = _root->left;
        else
            _root = _root->right;
    }
    *result = _root;
    if (_root == nil) {
        return -1;
    } else {
        return 0;
    }
}