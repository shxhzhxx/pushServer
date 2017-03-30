#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

const int BLACK = 0;
const int RED = 1;

class satellite {
public:
    satellite();
    virtual ~satellite();
    int mutex_trylock();
    void mutex_lock();
    void mutex_unlock();
private:
    pthread_mutex_t *mutex;
};


/*the node of the red&black tree*/
class node {
public:
    node(long _key = 0, satellite *_p = 0);
    ~node();
    void free_tree(node *nil);

    long key;
    node *parent;
    node *left;
    node *right;
    int color;

    satellite *p;
};


/*the tree*/
class rb_tree {
public:
    rb_tree();
    ~rb_tree();

    /**
     * 查找一条数据，未找到时返回NULL，找到时返回指向数据的指针
     */
    satellite *search(long _key, bool auto_lock = false);

    /**
     * 查找数据，
     * lock=0 不上锁           找到数据时返回0，否则返回-1
     * lock=1 上锁             找到数据时返回0，否则返回-1   可能阻塞
     * lock=2 尝试上锁         未找到数据时返回-1，找到数据但上锁失败时返回1，找到数据并上锁成功时返回0
     */
    int search_try(long _key, satellite **result,int lock=0);

    /**
     * 插入一条数据，key重复时返回1，并覆盖原数据，正常时返回0
     */
    int insert(long _key, satellite *data, bool auto_lock = false);


    /**
     * 尝试插入一条数据，key重复时返回1，并覆盖原数据，
     * 正常时返回0，
     * key重复且插入（上锁）失败时返回-1，同时释放data的内存
     */
    int insert_try(long _key, satellite *data, bool auto_lock = false);

    /**
     * 移除一条数据，成功时返回0，未找到指定数据时返回-1
     */
    int remove(long _key);

    /**
     *从小到大，返回第index个数据
     */
    satellite *get(int index, bool auto_lock = false);

    int size();


private:
    node *nil;
    node *root;
    int count;
    pthread_rwlock_t *rwlock;

    void rwlock_rdlock() { pthread_rwlock_rdlock(rwlock); }

    void rwlock_wrlock() { pthread_rwlock_wrlock(rwlock); }

    void rwlock_unlock() { pthread_rwlock_unlock(rwlock); }

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

    int rb_delete(node *z);

    int rb_insert(long _key, satellite *data);

    int rb_insert_try(long _key, satellite *data);

    int rb_search(long _key, node **result);

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



