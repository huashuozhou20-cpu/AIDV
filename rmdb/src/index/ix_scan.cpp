/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "ix_scan.h"

/**
 * @brief 移动到下一个entry，页面遍历时持有读锁
 */
void IxScan::next() {
    assert(!is_end());

    // 如果切换到了新页面，先释放旧页面的读锁
    if (curr_node_ != nullptr && curr_node_->get_page_no() != iid_.page_no) {
        bpm_->unpin_page(curr_node_->get_page_id(), false);
        delete curr_node_;
        curr_node_ = nullptr;
    }

    // 获取并固定当前页面（读锁）
    if (curr_node_ == nullptr) {
        auto guard = ih_->fetch_node(iid_.page_no);
        curr_node_ = guard.release();
    }

    assert(curr_node_->is_leaf_page());
    assert(iid_.slot_no < curr_node_->get_size());

    iid_.slot_no++;
    if (iid_.page_no != ih_->file_hdr_->last_leaf_ && iid_.slot_no == curr_node_->get_size()) {
        // 移动到下一个叶子页
        page_id_t next_page = curr_node_->get_next_leaf();
        bpm_->unpin_page(curr_node_->get_page_id(), false);
        delete curr_node_;
        curr_node_ = nullptr;
        iid_.slot_no = 0;
        iid_.page_no = next_page;
    }
}

Rid IxScan::rid() const {
    if (curr_node_ != nullptr) {
        return *curr_node_->get_rid(iid_.slot_no);
    }
    return ih_->get_rid(iid_);
}