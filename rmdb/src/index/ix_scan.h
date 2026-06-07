/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "ix_defs.h"
#include "ix_index_handle.h"

// class IxIndexHandle;

// 用于遍历叶子结点
// 用于直接遍历叶子结点，而不用findleafpage来得到叶子结点
class IxScan : public RecScan {
    const IxIndexHandle *ih_;
    Iid iid_;            // 初始为lower（用于遍历的指针）
    Iid end_;            // 初始为upper
    BufferPoolManager *bpm_;
    IxNodeHandle *curr_node_;  // 当前持有的页面读锁

   public:
    IxScan(const IxIndexHandle *ih, const Iid &lower, const Iid &upper, BufferPoolManager *bpm)
        : ih_(ih), iid_(lower), end_(upper), bpm_(bpm), curr_node_(nullptr) {}

    ~IxScan() override {
        if (curr_node_ != nullptr) {
            bpm_->unpin_page(curr_node_->get_page_id(), false);
            delete curr_node_;
        }
    }

    void next() override;

    bool is_end() const override { return iid_ == end_; }

    Rid rid() const override;

    const Iid &iid() const { return iid_; }

    // Return the raw index key bytes at the current scan position.
    const char* get_key() const {
        return curr_node_->get_key(iid_.slot_no);
    }
};