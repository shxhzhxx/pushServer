#include "rb_tree.h"

//============================satellite===================================
satellite::satellite(){
	mutex = new pthread_mutex_t();
    pthread_mutex_init(mutex,NULL);
}

satellite::~satellite() {
    pthread_mutex_destroy(mutex);
    delete mutex;
}

int satellite::mutex_trylock() { return pthread_mutex_trylock(mutex); }

void satellite::mutex_lock() { pthread_mutex_lock(mutex); }

void satellite::mutex_unlock() { pthread_mutex_unlock(mutex); }


//============================node===================================
node::node(long _key, satellite *_p) : key(_key), parent(0), left(0), right(0), color(BLACK), p(_p){}

node::~node() {delete p;}

void node::free_tree(node *nil){
    if(left!=nil)
        left->free_tree(nil);
    if(right!=nil)
        right->free_tree(nil);
    delete this;
}


//============================rb_tree===================================
rb_tree::rb_tree() : nil(0), root(0), count(0), rwlock(0) {
    nil = new node();
    root = nil;
    rwlock = new pthread_rwlock_t();
    pthread_rwlock_init(rwlock, NULL);
}

rb_tree::~rb_tree() {
    rwlock_wrlock();
    if(root!=nil)
        root->free_tree(nil);
    delete nil;
    rwlock_unlock();
    pthread_rwlock_destroy(rwlock);
    delete rwlock;
}

satellite *rb_tree::search(long _key, bool lock) {
    rwlock_rdlock();
    node *result = 0;
    if (rb_search(_key, &result) == -1){
    	rwlock_unlock();
        return NULL;
    }
    if (lock){
        result->p->mutex_lock();
    }
    rwlock_unlock();
    return result->p;
}

int rb_tree::search_try(long _key, satellite **result){
    rwlock_rdlock();
    node *node_p;
    if (rb_search(_key, &node_p) == -1){
        rwlock_unlock();
        return -1;
    }
    int ret=0; 
    if(node_p->p->mutex_trylock()!=0){
        ret=1;
    }
    rwlock_unlock();
    (*result)=node_p->p;
    return ret;
}

int rb_tree::insert(long _key, satellite *data, bool lock) {
    rwlock_wrlock();
    int result_code = rb_insert(_key, data);
    if (lock)
        data->mutex_lock();
    if (result_code == 0)
        count++;
    rwlock_unlock();
    return result_code;
}

int rb_tree::insert_try(long _key, satellite *data, bool lock){
    rwlock_wrlock();
    int result_code = rb_insert_try(_key, data);
    if(result_code==-1){//加锁失败
        return -1;
    }
    if (lock){
        data->mutex_lock();
    }
    if (result_code == 0)
        count++;
    rwlock_unlock();
    return result_code;
}

int rb_tree::remove(long _key) {
    rwlock_wrlock();
    node *result = 0;
    if (rb_search(_key, &result) == 0) {
        rb_delete(result);
        count--;
        rwlock_unlock();
        return 0;
    } else {
        rwlock_unlock();
        return -1;
    }
}

satellite *rb_tree::value_at(int index, bool lock) {
    rwlock_rdlock();
    if (root == nil || index >= count || index < 0) {
        rwlock_unlock();
        return NULL;
    }
    node *result = tree_minimum(root);
    while (index > 0) {
        result = tree_successor(result);
        index--;
    }
    if (lock)
        result->p->mutex_lock();
    rwlock_unlock();
    return result->p;
}

int rb_tree::size(){
    return count;
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

int rb_tree::rb_insert(long _key, satellite *data) {
    node *z = new node(_key, data);
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
            x->p->mutex_lock();
            x->p->mutex_unlock();
            satellite *temp = x->p;
            x->p = z->p;
            z->p = temp;
            delete z;
            return 1;
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

int rb_tree::rb_insert_try(long _key, satellite *data) {
    node *z = new node(_key, data);
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
            if(x->p->mutex_trylock()==0){
                satellite *temp = x->p;
                x->p = z->p;
                z->p = temp;
                delete z;
                x->p->mutex_unlock();
                return 1;
            }else{
                delete z;
                return -1;
            }   
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

int rb_tree::rb_delete(node *z) {
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
        satellite *p_temp = z->p;
        z->key = y->key;
        z->p = y->p;
        y->p = p_temp;
    }
    if (y->color == BLACK) {
        rb_delete_fixup(x);
    }
    y->p->mutex_lock();
    y->p->mutex_unlock();
    delete y;
    return 0;
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