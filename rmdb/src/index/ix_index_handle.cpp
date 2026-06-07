/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "ix_index_handle.h"

#include "ix_scan.h"

/**
 * @brief 在当前node中查找第一个>=target的key_idx
 *
 * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于最后一个key
 * @note 返回key index（同时也是rid index），作为slot no
 */
int IxNodeHandle::lower_bound(const char *target) const {
    int lo = 0, hi = get_size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/**
 * @brief 在当前node中查找第一个>target的key_idx
 *
 * @return key_idx，范围为[1,num_key)，如果返回的key_idx=num_key，则表示target大于等于最后一个key
 * @note 注意此处的范围从1开始
 */
int IxNodeHandle::upper_bound(const char *target) const {
    int lo = 0, hi = get_size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) <= 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/**
 * @brief 用于叶子结点根据key来查找该结点中的键值对
 * 值value作为传出参数，函数返回是否查找成功
 *
 * @param key 目标key
 * @param[out] value 传出参数，目标key对应的Rid
 * @return 目标key是否存在
 */
bool IxNodeHandle::leaf_lookup(const char *key, Rid **value) {
    int pos = lower_bound(key);
    if (pos < get_size() && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        *value = get_rid(pos);
        return true;
    }
    return false;
}

/**
 * 用于内部结点（非叶子节点）查找目标key所在的孩子结点（子树）
 * @param key 目标key
 * @return page_id_t 目标key所在的孩子节点（子树）的存储页面编号
 */
page_id_t IxNodeHandle::internal_lookup(const char *key) {
    int pos = upper_bound(key);
    return value_at(pos);
}

/**
 * @brief 在指定位置插入n个连续的键值对
 * 将key的前n位插入到原来keys中的pos位置；将rid的前n位插入到原来rids中的pos位置
 *
 * @param pos 要插入键值对的位置
 * @param (key, rid) 连续键值对的起始地址，也就是第一个键值对，可以通过(key, rid)来获取n个键值对
 * @param n 键值对数量
 * @note [0,pos)           [pos,num_key)
 *                            key_slot
 *                            /      \
 *                           /        \
 *       [0,pos)     [pos,pos+n)   [pos+n,num_key+n)
 *                      key           key_slot
 */
void IxNodeHandle::insert_pairs(int pos, const char *key, const Rid *rid, int n) {
    assert(pos >= 0 && pos <= get_size());
    int num_key = get_size();
    int col_tot_len = file_hdr->col_tot_len_;
    // shift existing keys right by n
    memmove(keys + (pos + n) * col_tot_len, keys + pos * col_tot_len, (num_key - pos) * col_tot_len);
    // copy new keys
    memcpy(keys + pos * col_tot_len, key, n * col_tot_len);
    // shift existing rids right by n
    memmove(rids + pos + n, rids + pos, (num_key - pos) * sizeof(Rid));
    // copy new rids
    memcpy(rids + pos, rid, n * sizeof(Rid));
    // update key count
    set_size(num_key + n);
}

/**
 * @brief 用于在结点中插入单个键值对。
 * 函数返回插入后的键值对数量
 *
 * @param (key, value) 要插入的键值对
 * @return int 键值对数量
 */
int IxNodeHandle::insert(const char *key, const Rid &value) {
    int pos = lower_bound(key);
    if (pos < get_size() && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        // duplicate key, do not insert
        return get_size();
    }
    insert_pair(pos, key, value);
    return get_size();
}

/**
 * @brief 用于在结点中的指定位置删除单个键值对
 *
 * @param pos 要删除键值对的位置
 */
void IxNodeHandle::erase_pair(int pos) {
    assert(pos >= 0 && pos < get_size());
    int num_key = get_size();
    int col_tot_len = file_hdr->col_tot_len_;
    // shift keys left by 1 from pos+1
    if (pos + 1 < num_key) {
        memmove(keys + pos * col_tot_len, keys + (pos + 1) * col_tot_len, (num_key - pos - 1) * col_tot_len);
    }
    // shift rids left by 1 from pos+1
    if (pos + 1 < num_key) {
        memmove(rids + pos, rids + pos + 1, (num_key - pos - 1) * sizeof(Rid));
    }
    set_size(num_key - 1);
}

/**
 * @brief 用于在结点中删除指定key的键值对。函数返回删除后的键值对数量
 *
 * @param key 要删除的键值对key值
 * @return 完成删除操作后的键值对数量
 */
int IxNodeHandle::remove(const char *key) {
    int pos = lower_bound(key);
    if (pos < get_size() && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        erase_pair(pos);
    }
    return get_size();
}

IxIndexHandle::IxIndexHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd)
    : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager), fd_(fd) {
    char buf[PAGE_SIZE];
    memset(buf, 0, PAGE_SIZE);
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, buf, PAGE_SIZE);
    file_hdr_ = new IxFileHdr();
    file_hdr_->deserialize(buf);

    int now_page_no = disk_manager_->get_fd2pageno(fd);
    disk_manager_->set_fd2pageno(fd, now_page_no + 1);
}

IxIndexHandle::~IxIndexHandle() {
    delete file_hdr_;
}

/**
 * @brief 用于查找指定键所在的叶子结点
 * @param key 要查找的目标key值
 * @param operation 查找到目标键值对后要进行的操作类型
 * @param transaction 事务参数，如果不需要则默认传入nullptr
 * @return [leaf node] and [root_is_latched] 返回目标叶子结点以及根结点是否加锁
 * @note need to Unlatch and unpin the leaf node outside!
 * 注意：用了FindLeafPage之后一定要unlatch叶结点，否则下次latch该结点会堵塞！
 */
std::pair<NodeHandleGuard, bool> IxIndexHandle::find_leaf_page(const char *key, Operation operation,
                                                               Transaction *transaction, bool find_first) {
    page_id_t page_no = file_hdr_->root_page_;
    auto node = fetch_node(page_no);

    if (operation == Operation::FIND) {
        // ---- Lock-free optimistic traversal with adaptive fallback ----
        // Try lock-free up to 5 times.  Each failure means a concurrent
        // split changed an internal node's pointers mid-read.  After 5
        // failures, fall back to the SpinLatch rlock path which guarantees
        // forward progress (pure user-space CAS, no kernel calls).
        int restart_count = 0;
        for (int attempt = 0; attempt < 10; ++attempt) {
            page_id_t root_pn = file_hdr_->root_page_;
            auto curr = fetch_node(root_pn);
            uint64_t seq = curr->page->GetLatch().GetSequence();

            // Writer active on root — brief pause, don't count as failure
            if (seq & 1) { _mm_pause(); continue; }

            while (!curr->is_leaf_page()) {
                page_id_t child_pn = curr->internal_lookup(key);
                if (curr->page->GetLatch().GetSequence() != seq) {
                    ++restart_count;
                    if (restart_count > 5) goto fallback_rlock;
                    goto retry_attempt;
                }
                auto child = fetch_node(child_pn);
                curr = std::move(child);
                seq = curr->page->GetLatch().GetSequence();
                if (seq & 1) {
                    ++restart_count;
                    if (restart_count > 5) goto fallback_rlock;
                    goto retry_attempt;
                }
            }
            // Reached leaf — acquire rlock for safe data access
            curr->page->rlock();
            return std::make_pair(std::move(curr), false);
        retry_attempt:;
        }

    fallback_rlock:
        // Hand-over-hand rlock (SpinLatch — fast CAS, guaranteed progress)
        {
            page_id_t page_no = file_hdr_->root_page_;
            auto node = fetch_node(page_no);
            node->page->rlock();
            while (!node->is_leaf_page()) {
                page_id_t child_pn = node->internal_lookup(key);
                auto child = fetch_node(child_pn);
                child->page->rlock();
                node->page->runlock();
                node = std::move(child);
            }
            return std::make_pair(std::move(node), false);
        }
    }

    // INSERT / DELETE: basic hand-over-hand (no safety tracking).
    // For write operations use crabbing_find_leaf() instead.
    node->page->wlock();
    while (!node->is_leaf_page()) {
        page_id_t child_page_no = node->internal_lookup(key);
        auto child = fetch_node(child_page_no);
        child->page->wlock();
        node->page->wunlock();
        node = std::move(child);
    }
    return std::make_pair(std::move(node), false);
}

/**
 * @brief Crab-locking traversal for INSERT / DELETE with safe-node tracking.
 *        Unsafe ancestors (those whose child might split or merge) are kept
 *        wlocked so split / merge propagation has them ready.
 */
NodeHandleGuard IxIndexHandle::crabbing_find_leaf(const char *key, Operation operation,
                                                    std::vector<NodeHandleGuard> &retained) {
    retained.clear();
    page_id_t page_no = file_hdr_->root_page_;
    auto node = fetch_node(page_no);
    node->page->wlock();

    while (!node->is_leaf_page()) {
        page_id_t child_page_no = node->internal_lookup(key);
        auto child = fetch_node(child_page_no);
        child->page->wlock();

        bool safe = false;
        if (operation == Operation::INSERT) {
            safe = (child->get_size() < child->get_max_size());
        } else {
            safe = (child->get_size() > child->get_min_size());
        }

        if (safe) {
            for (auto &anc : retained) anc->page->wunlock();
            retained.clear();
            node->page->wunlock();
        } else {
            retained.push_back(std::move(node));
        }
        node = std::move(child);
    }

    return std::move(node);
}

/**
 * @brief 用于查找指定键在叶子结点中的对应的值result
 *
 * @param key 查找的目标key值
 * @param result 用于存放结果的容器
 * @param transaction 事务指针
 * @return bool 返回目标键值对是否存在
 */
bool IxIndexHandle::get_value(const char *key, std::vector<Rid> *result, Transaction *transaction) {
    auto leaf_result = find_leaf_page(key, Operation::FIND, transaction);
    auto leaf = std::move(leaf_result.first);
    Rid *rid;
    bool found = leaf->leaf_lookup(key, &rid);
    if (found) {
        result->push_back(*rid);
    }
    return found;
}

/**
 * @brief  将传入的一个node拆分(Split)成两个结点，在node的右边生成一个新结点new node
 * @param node 需要拆分的结点
 * @return 拆分得到的new_node
 * @note need to unpin the new node outside
 * 注意：本函数执行完毕后，原node和new node都需要在函数外面进行unpin
 */
NodeHandleGuard IxIndexHandle::split(IxNodeHandle *node) {
    auto new_node = create_node();
    new_node->page_hdr->is_leaf = node->page_hdr->is_leaf;
    new_node->page_hdr->parent = node->page_hdr->parent;

    int total = node->get_size();
    int mid = total / 2;

    if (node->is_leaf_page()) {
        // Leaf: right half starts at mid. The first key of the right leaf
        // (original key[mid]) becomes the separator pushed up to the parent.
        int n_right = total - mid;
        new_node->insert_pairs(0, node->get_key(mid), node->get_rid(mid), n_right);
        node->set_size(mid);

        new_node->set_next_leaf(node->get_next_leaf());
        new_node->set_prev_leaf(node->get_page_no());
        node->set_next_leaf(new_node->get_page_no());
        if (new_node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            auto next = fetch_node(new_node->get_next_leaf());
            next->set_prev_leaf(new_node->get_page_no());
            next.set_dirty(true);
        }
        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = new_node->get_page_no();
        }
    } else {
        // Internal node: key at position 'mid' is the separator — it is NOT
        // kept in either half. It is pushed up to the parent by the caller.
        //   Left:  keys[0..mid-1]        rids[0..mid]
        //   Right: keys[mid+1..total-1]  rids[mid+1..total]
        int n_right_keys = total - mid - 1;  // keys in the right half
        if (n_right_keys > 0) {
            new_node->insert_pairs(0, node->get_key(mid + 1),
                                   node->get_rid(mid + 1), n_right_keys);
        }
        // The right half must also inherit the last child pointer (rids[total]).
        new_node->set_rid(n_right_keys, *node->get_rid(total));
        new_node->set_size(n_right_keys);
        node->set_size(mid);

        // Update child→parent links for all children in the right half
        // (n_right_keys keys → n_right_keys + 1 children).
        for (int i = 0; i <= n_right_keys; i++) {
            maintain_child(new_node.get(), i);
        }
    }

    return new_node;
}

/**
 * @brief Insert key & value pair into internal page after split
 * 拆分(Split)后，向上找到old_node的父结点
 * 将new_node的第一个key插入到父结点，其位置在 父结点指向old_node的孩子指针 之后
 * 如果插入后>=maxsize，则必须继续拆分父结点，然后在其父结点的父结点再插入，即需要递归
 * 直到找到的old_node为根结点时，结束递归（此时将会新建一个根R，关键字为key，old_node和new_node为其孩子）
 *
 * @param (old_node, new_node) 原结点为old_node，old_node被分裂之后产生了新的右兄弟结点new_node
 * @param key 要插入parent的key
 * @note 一个结点插入了键值对之后需要分裂，分裂后左半部分的键值对保留在原结点，在参数中称为old_node，
 * 右半部分的键值对分裂为新的右兄弟节点，在参数中称为new_node（参考Split函数来理解old_node和new_node）
 * @note 本函数执行完毕后，new node和old node都需要在函数外面进行unpin
 */
void IxIndexHandle::insert_into_parent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node,
                                     Transaction *transaction, IxNodeHandle *retained_parent) {
    if (old_node->is_root_page()) {
        auto new_root = create_node();
        new_root->page_hdr->is_leaf = false;
        Rid rid0 = {.page_no = old_node->get_page_no(), .slot_no = 0};
        Rid rid1 = {.page_no = new_node->get_page_no(), .slot_no = 0};
        new_root->set_key(0, key);
        new_root->set_rid(0, rid0);
        new_root->set_rid(1, rid1);
        new_root->set_size(1);
        old_node->set_parent_page_no(new_root->get_page_no());
        new_node->set_parent_page_no(new_root->get_page_no());
        update_root_page_no(new_root->get_page_no());
        new_root.set_dirty(true);
        return;
    }

    std::unique_ptr<NodeHandleGuard> parent_guard;
    std::unique_ptr<PageLatchGuard> parent_latch;

    if (retained_parent != nullptr) {
        // Parent is already wlocked from the crab path — adopt it.
        // The retained_parent stays alive in the caller's CrabbingPath.
        parent_latch = std::make_unique<PageLatchGuard>(retained_parent->page, true, adopt_latch);
    } else {
        auto p = fetch_node(old_node->get_parent_page_no());
        parent_latch = std::make_unique<PageLatchGuard>(p->page, true);
        parent_guard = std::make_unique<NodeHandleGuard>(std::move(p));
    }

    IxNodeHandle *parent = retained_parent ? retained_parent
                           : parent_guard->get();
    int child_idx = parent->find_child(old_node);
    Rid rid = {.page_no = new_node->get_page_no(), .slot_no = 0};

    // child_idx can be parent->get_size() for internal nodes (rightmost child,
    // which is at rids[num_key] — one past the key array).
    if (child_idx == parent->get_size()) {
        // Rightmost child: rids[child_idx] already points to old_node.
        // Append the separator key and new child at the end.
        parent->set_key(child_idx, key);
        parent->set_rid(child_idx + 1, rid);
        parent->set_size(parent->get_size() + 1);
    } else {
        parent->insert_pair(child_idx + 1, key, rid);
    }
    new_node->set_parent_page_no(parent->get_page_no());

    if (parent->get_size() > parent->get_max_size()) {
        // Save the separator key before split — for internal nodes,
        // the key at position total/2 is removed from both halves and
        // must be pushed up to the grandparent.
        int sep_idx = parent->get_size() / 2;
        char *sep_key = new char[file_hdr_->col_tot_len_];
        memcpy(sep_key, parent->get_key(sep_idx), file_hdr_->col_tot_len_);
        auto new_parent = split(parent);
        insert_into_parent(parent, sep_key, new_parent.get(), transaction);
        delete[] sep_key;
        new_parent.set_dirty(true);
    }

    BufferPoolManager::mark_dirty(parent->page);
}

/**
 * @brief 将指定键值对插入到B+树中
 * @param (key, value) 要插入的键值对
 * @param transaction 事务指针
 * @return page_id_t 插入到的叶结点的page_no
 */
page_id_t IxIndexHandle::insert_entry(const char *key, const Rid &value, Transaction *transaction) {
    // ---- Optimistic path: read-lock traverse, upgrade leaf to write-lock.
    //      >90% of inserts do not cause a split, so ancestors are untouched.
    {
        auto result = find_leaf_page(key, Operation::FIND, transaction);
        auto leaf = std::move(result.first);
        page_id_t leaf_page_no = leaf->get_page_no();

        // Hand off: runlock → wlock on the same pinned page
        leaf->page->runlock();
        leaf->page->wlock();
        PageLatchGuard leaf_latch(leaf->page, true, adopt_latch);

        // Fast path: leaf has room
        if (leaf->get_size() < leaf->get_max_size()) {
            leaf->insert(key, value);
            leaf.set_dirty(true);
            return leaf_page_no;
        }

        // Leaf is full — fall back to pessimistic crab locking
        leaf_latch.release();
        leaf->page->wunlock();
    }

pessimistic_insert:
    // ---- Pessimistic path (split required) ----
    {
        std::vector<NodeHandleGuard> retained;
        auto leaf = crabbing_find_leaf(key, Operation::INSERT, retained);

        PageLatchGuard leaf_latch(leaf->page, true, adopt_latch);

        leaf->insert(key, value);
        page_id_t leaf_page_no = leaf->get_page_no();

        if (leaf->get_size() > leaf->get_max_size()) {
            auto new_leaf = split(leaf.get());
            if (file_hdr_->last_leaf_ == leaf->get_page_no()) {
                file_hdr_->last_leaf_ = new_leaf->get_page_no();
            }
            IxNodeHandle *ret_parent = nullptr;
            if (!retained.empty()) {
                ret_parent = retained.back().get();
            }
            insert_into_parent(leaf.get(), new_leaf->get_key(0), new_leaf.get(),
                              transaction, ret_parent);
            if (!retained.empty()) {
                retained.pop_back();
            }
            new_leaf.set_dirty(true);
        }

        for (auto &anc : retained) anc->page->wunlock();

        leaf.set_dirty(true);
        return leaf_page_no;
    }
}

/**
 * @brief 用于删除B+树中含有指定key的键值对
 * @param key 要删除的key值
 * @param transaction 事务指针
 */
bool IxIndexHandle::delete_entry(const char *key, Transaction *transaction) {
    // ---- Optimistic path: read-lock traverse, upgrade leaf to write-lock ----
    {
        auto result = find_leaf_page(key, Operation::FIND, transaction);
        auto leaf = std::move(result.first);

        leaf->page->runlock();
        leaf->page->wlock();
        PageLatchGuard leaf_latch(leaf->page, true, adopt_latch);

        // Fast path: leaf won't underflow
        if (leaf->get_size() > leaf->get_min_size()) {
            int old_size = leaf->get_size();
            leaf->remove(key);
            if (leaf->get_size() != old_size) {
                leaf.set_dirty(true);
                return true;
            }
            return false;
        }

        leaf_latch.release();
        leaf->page->wunlock();
    }

pessimistic_delete:
    {
        auto result = find_leaf_page(key, Operation::DELETE, transaction);
        auto leaf = std::move(result.first);
        PageLatchGuard leaf_latch(leaf->page, true, adopt_latch);
        int old_size = leaf->get_size();
        leaf->remove(key);
        if (leaf->get_size() == old_size) return false;

        bool node_consumed = coalesce_or_redistribute(leaf.get(), transaction, nullptr);
        if (node_consumed) {
            leaf.release();
        } else {
            leaf.set_dirty(true);
        }
        return true;
    }
}

/**
 * @brief 用于处理合并和重分配的逻辑，用于删除键值对后调用
 *
 * @param node 执行完删除操作的结点
 * @param transaction 事务指针
 * @param root_is_latched 传出参数：根节点是否上锁，用于并发操作
 * @return 是否需要删除结点
 * @note User needs to first find the sibling of input page.
 * If sibling's size + input page's size >= 2 * page's minsize, then redistribute.
 * Otherwise, merge(Coalesce).
 */
bool IxIndexHandle::coalesce_or_redistribute(IxNodeHandle *node, Transaction *transaction, bool *root_is_latched) {
    if (node->is_root_page()) {
        return adjust_root(node);
    }
    if (node->get_size() >= node->get_min_size()) {
        return false;
    }

    auto parent = fetch_node(node->get_parent_page_no());
    PageLatchGuard parent_latch(parent->page, true);
    int index = parent->find_child(node);

    int neighbor_index = (index > 0) ? index - 1 : 1;
    auto neighbor = fetch_node(parent->value_at(neighbor_index));
    PageLatchGuard neighbor_latch(neighbor->page, true);

    if (neighbor->get_size() + node->get_size() >= 2 * node->get_min_size()) {
        redistribute(neighbor.get(), node, parent.get(), index);
        parent.set_dirty(true);
        neighbor.set_dirty(true);
        // both latches auto-released by guards on return
        return false;  // node still valid, caller retains ownership
    }

    // coalesce: extract raw pointers since coalesce may swap them.
    // Disarm the latch guards so the write latches stay held through coalesce.
    IxNodeHandle *raw_neighbor = neighbor.release();
    IxNodeHandle *raw_parent = parent.release();
    neighbor_latch.disarm();
    parent_latch.disarm();
    bool parent_should_delete = coalesce(raw_neighbor, node, raw_parent, index, transaction, root_is_latched);
    // After coalesce the caller's node has been merged into the survivor —
    // its data is gone and its page may be recycled.  The caller MUST disarm
    // its guard and not touch the node.
    raw_neighbor->page->wunlock();
    raw_parent->page->wunlock();
    destroy_node(raw_neighbor, true);
    destroy_node(raw_parent, true);
    return true;  // caller's node was consumed by coalesce
}

/**
 * @brief 用于当根结点被删除了一个键值对之后的处理
 * @param old_root_node 原根节点
 * @return bool 根结点是否需要被删除
 * @note size of root page can be less than min size and this method is only called within coalesce_or_redistribute()
 */
bool IxIndexHandle::adjust_root(IxNodeHandle *old_root_node) {
    if (!old_root_node->is_leaf_page() && old_root_node->get_size() == 1) {
        page_id_t child_page_no = old_root_node->value_at(0);
        auto child = fetch_node(child_page_no);
        child->set_parent_page_no(IX_NO_PAGE);
        update_root_page_no(child_page_no);
        child.set_dirty(true);
        decrement_page_count();
        destroy_node(old_root_node, false);  // unpin + delete old root
        return true;
    }
    if (old_root_node->is_leaf_page() && old_root_node->get_size() == 0) {
        update_root_page_no(IX_NO_PAGE);
        decrement_page_count();
        destroy_node(old_root_node, false);  // unpin + delete empty root
        return true;
    }
    return false;
}

/**
 * @brief 重新分配node和兄弟结点neighbor_node的键值对
 * Redistribute key & value pairs from one page to its sibling page. If index == 0, move sibling page's first key
 * & value pair into end of input "node", otherwise move sibling page's last key & value pair into head of input "node".
 *
 * @param neighbor_node sibling page of input "node"
 * @param node input from method coalesceOrRedistribute()
 * @param parent the parent of "node" and "neighbor_node"
 * @param index node在parent中的rid_idx
 * @note node是之前刚被删除过一个key的结点
 * index=0，则neighbor是node后继结点，表示：node(left)      neighbor(right)
 * index>0，则neighbor是node前驱结点，表示：neighbor(left)  node(right)
 * 注意更新parent结点的相关kv对
 */
void IxIndexHandle::redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index) {
    if (index == 0) {
        // neighbor is right sibling: move neighbor's first pair to end of node
        node->insert_pair(node->get_size(), neighbor_node->get_key(0), *neighbor_node->get_rid(0));
        neighbor_node->erase_pair(0);
        // update parent key that separates these two
        parent->set_key(0, neighbor_node->get_key(0));
    } else {
        // neighbor is left sibling: move neighbor's last pair to beginning of node
        int last = neighbor_node->get_size() - 1;
        node->insert_pair(0, neighbor_node->get_key(last), *neighbor_node->get_rid(last));
        neighbor_node->erase_pair(last);
        // update parent key that separates these two (index is node's position, key at index separates neighbor and node)
        parent->set_key(index, node->get_key(0));
    }
}

/**
 * @brief 合并(Coalesce)函数是将node和其直接前驱进行合并，也就是和它左边的neighbor_node进行合并；
 * 假设node一定在右边。如果上层传入的index=0，说明node在左边，那么交换node和neighbor_node，保证node在右边；合并到左结点，实际上就是删除了右结点；
 * Move all the key & value pairs from one page to its sibling page, and notify buffer pool manager to delete this page.
 * Parent page must be adjusted to take info of deletion into account. Remember to deal with coalesce or redistribute
 * recursively if necessary.
 *
 * @param neighbor_node sibling page of input "node" (neighbor_node是node的前结点)
 * @param node input from method coalesceOrRedistribute() (node结点是需要被删除的)
 * @param parent parent page of input "node"
 * @param index node在parent中的rid_idx
 * @return true means parent node should be deleted, false means no deletion happend
 * @note Assume that *neighbor_node is the left sibling of *node (neighbor -> node)
 */
bool IxIndexHandle::coalesce(IxNodeHandle *&neighbor_node, IxNodeHandle *&node, IxNodeHandle *&parent, int index,
                             Transaction *transaction, bool *root_is_latched) {
    // ensure neighbor is left, node is right
    if (index == 0) {
        std::swap(neighbor_node, node);
        index = 1;
    }

    int neighbor_size = neighbor_node->get_size();
    int node_size = node->get_size();

    // move all pairs from right node into left neighbor
    neighbor_node->insert_pairs(neighbor_size, node->get_key(0), node->get_rid(0), node_size);

    // for leaf nodes, update linked list
    if (node->is_leaf_page()) {
        neighbor_node->set_next_leaf(node->get_next_leaf());
        if (node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            auto next = fetch_node(node->get_next_leaf());
            next->set_prev_leaf(neighbor_node->get_page_no());
            next.set_dirty(true);
        }
        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = neighbor_node->get_page_no();
        }
    } else {
        // update children's parent pointers for moved pairs
        for (int i = neighbor_size; i < neighbor_node->get_size(); i++) {
            maintain_child(neighbor_node, i);
        }
    }

    // remove node's entry from parent
    parent->erase_pair(index);

    // clean up node from leaf list if it's a leaf
    if (node->is_leaf_page()) {
        erase_leaf(node);
    }
    decrement_page_count();

    return parent->get_size() < parent->get_min_size();
}

/**
 * @brief 这里把iid转换成了rid，即iid的slot_no作为node的rid_idx(key_idx)
 * node其实就是把slot_no作为键值对数组的下标
 * 换而言之，每个iid对应的索引槽存了一对(key,rid)，指向了(要建立索引的属性首地址,插入/删除记录的位置)
 *
 * @param iid
 * @return Rid
 * @note iid和rid存的不是一个东西，rid是上层传过来的记录位置，iid是索引内部生成的索引槽位置
 */
Rid IxIndexHandle::get_rid(const Iid &iid) const {
    auto node = fetch_node(iid.page_no);
    PageLatchGuard latch(node->page, false);  // RAII read-latch
    if (iid.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    return *node->get_rid(iid.slot_no);
}

/**
 * @brief FindLeafPage + lower_bound
 *
 * @param key
 * @return Iid
 * @note 上层传入的key本来是int类型，通过(const char *)&key进行了转换
 * 可用*(int *)key转换回去
 */
Iid IxIndexHandle::lower_bound(const char *key) {
    auto result = find_leaf_page(key, Operation::FIND, nullptr, true);
    auto leaf = std::move(result.first);
    int slot_no = leaf->lower_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = slot_no};
    return iid;
}

/**
 * @brief FindLeafPage + upper_bound
 *
 * @param key
 * @return Iid
 */
Iid IxIndexHandle::upper_bound(const char *key) {
    auto result = find_leaf_page(key, Operation::FIND, nullptr, false);
    auto leaf = std::move(result.first);
    int slot_no = leaf->upper_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = slot_no};
    return iid;
}

/**
 * @brief 指向最后一个叶子的最后一个结点的后一个
 * 用处在于可以作为IxScan的最后一个
 *
 * @return Iid
 */
Iid IxIndexHandle::leaf_end() const {
    auto node = fetch_node(file_hdr_->last_leaf_);
    Iid iid = {.page_no = file_hdr_->last_leaf_, .slot_no = node->get_size()};
    return iid;
}

/**
 * @brief 指向第一个叶子的第一个结点
 * 用处在于可以作为IxScan的第一个
 *
 * @return Iid
 */
Iid IxIndexHandle::leaf_begin() const {
    Iid iid = {.page_no = file_hdr_->first_leaf_, .slot_no = 0};
    return iid;
}

/**
 * @brief 获取一个指定结点
 *
 * @param page_no
 * @return IxNodeHandle*
 * @note pin the page, remember to unpin it outside!
 */
NodeHandleGuard IxIndexHandle::fetch_node(int page_no) const {
    Page *page = buffer_pool_manager_->fetch_page(PageId{fd_, page_no});
    IxNodeHandle *node = new IxNodeHandle(file_hdr_, page);
    return NodeHandleGuard(buffer_pool_manager_, node);
}

/**
 * @brief 创建一个新结点
 *
 * @return IxNodeHandle*
 * @note pin the page, remember to unpin it outside!
 * 注意：对于Index的处理是，删除某个页面后，认为该被删除的页面是free_page
 * 而first_free_page实际上就是最新被删除的页面，初始为IX_NO_PAGE
 * 在最开始插入时，一直是create node，那么first_page_no一直没变，一直是IX_NO_PAGE
 * 与Record的处理不同，Record将未插入满的记录页认为是free_page
 */
NodeHandleGuard IxIndexHandle::create_node() {
    file_hdr_->num_pages_++;

    PageId new_page_id = {.fd = fd_, .page_no = INVALID_PAGE_ID};
    // 从3开始分配page_no，第一次分配之后，new_page_id.page_no=3，file_hdr_.num_pages=4
    Page *page = buffer_pool_manager_->new_page(&new_page_id);
    IxNodeHandle *node = new IxNodeHandle(file_hdr_, page);
    return NodeHandleGuard(buffer_pool_manager_, node);
}

void IxIndexHandle::destroy_node(IxNodeHandle *node, bool is_dirty) const {
    buffer_pool_manager_->unpin_page(node->get_page_id(), is_dirty);
    delete node;
}

void IxIndexHandle::decrement_page_count() {
    file_hdr_->num_pages_--;
}

/**
 * @brief 从node开始更新其父节点的第一个key，一直向上更新直到根节点
 *
 * @param node
 */
void IxIndexHandle::maintain_parent(IxNodeHandle *node) {
    IxNodeHandle *curr = node;
    IxNodeHandle *released = nullptr;
    while (curr->get_parent_page_no() != IX_NO_PAGE) {
        auto parent = fetch_node(curr->get_parent_page_no());
        PageLatchGuard parent_latch(parent->page, true);
        int rank = parent->find_child(curr);
        char *parent_key = parent->get_key(rank);
        char *child_first_key = curr->get_key(0);
        if (memcmp(parent_key, child_first_key, file_hdr_->col_tot_len_) == 0) {
            parent.set_dirty(true);
            break;  // parent_latch auto-releases on break
        }
        memcpy(parent_key, child_first_key, file_hdr_->col_tot_len_);
        parent.set_dirty(true);
        if (released != nullptr) {
            released->page->wunlock();
            destroy_node(released, true);
        }
        released = parent.release();
        parent_latch.disarm();  // keep latch held on the released page
        curr = released;
    }
    if (released != nullptr) {
        released->page->wunlock();
        destroy_node(released, true);
    }
}

/**
 * @brief 要删除leaf之前调用此函数，更新leaf前驱结点的next指针和后继结点的prev指针
 *
 * @param leaf 要删除的leaf
 */
void IxIndexHandle::erase_leaf(IxNodeHandle *leaf) {
    assert(leaf->is_leaf_page());

    auto prev = fetch_node(leaf->get_prev_leaf());
    prev->set_next_leaf(leaf->get_next_leaf());
    prev.set_dirty(true);

    auto next = fetch_node(leaf->get_next_leaf());
    next->set_prev_leaf(leaf->get_prev_leaf());
    next.set_dirty(true);
}

/**
 * @brief 将node的第child_idx个孩子结点的父节点置为node
 */
void IxIndexHandle::maintain_child(IxNodeHandle *node, int child_idx) {
    if (!node->is_leaf_page()) {
        int child_page_no = node->value_at(child_idx);
        auto child = fetch_node(child_page_no);
        child->set_parent_page_no(node->get_page_no());
        child.set_dirty(true);
    }
}