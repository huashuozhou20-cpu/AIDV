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
#include <algorithm>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class SortExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    ColMeta sort_col_;                          // 排序键的列元数据
    size_t tuple_num_;
    bool is_desc_;
    std::vector<std::unique_ptr<RmRecord>> tuples_;
    size_t current_idx_;
    bool is_end_;

   public:
    SortExecutor(std::unique_ptr<AbstractExecutor> prev, TabCol sel_cols, bool is_desc) {
        prev_ = std::move(prev);
        sort_col_ = prev_->get_col_offset(sel_cols);
        is_desc_ = is_desc;
        tuple_num_ = 0;
        current_idx_ = 0;
        is_end_ = true;
    }

    void beginTuple() override {
        prev_->beginTuple();
        // 读取所有元组到内存
        tuples_.clear();
        for (; !prev_->is_end(); prev_->nextTuple()) {
            auto rec = prev_->Next();
            if (rec) {
                tuples_.push_back(std::move(rec));
            }
        }
        tuple_num_ = tuples_.size();

        // 按排序键排序
        auto cmp = [this](const std::unique_ptr<RmRecord> &a, const std::unique_ptr<RmRecord> &b) {
            int res = 0;
            char *data_a = a->data + sort_col_.offset;
            char *data_b = b->data + sort_col_.offset;
            switch (sort_col_.type) {
                case TYPE_INT: {
                    int ia = *(int *)data_a, ib = *(int *)data_b;
                    res = (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
                    break;
                }
                case TYPE_FLOAT: {
                    float fa = *(float *)data_a, fb = *(float *)data_b;
                    res = (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
                    break;
                }
                case TYPE_STRING:
                    res = memcmp(data_a, data_b, sort_col_.len);
                    break;
            }
            return is_desc_ ? (res > 0) : (res < 0);
        };
        std::sort(tuples_.begin(), tuples_.end(), cmp);

        current_idx_ = 0;
        is_end_ = (current_idx_ >= tuple_num_);
    }

    void nextTuple() override {
        current_idx_++;
        is_end_ = (current_idx_ >= tuple_num_);
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return prev_->tupleLen(); }

    const std::vector<ColMeta> &cols() const override { return prev_->cols(); }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_) return nullptr;
        // Return a non-owning view into the sorted tuples_ vector.
        // Safe because tuples_ persists for the lifetime of this SortExecutor.
        auto& src = *tuples_[current_idx_];
        auto rec = std::make_unique<RmRecord>();
        rec->size = src.size;
        rec->data = src.data;
        rec->allocated_ = false;
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};