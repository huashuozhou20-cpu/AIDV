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

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "system/sm.h"

class LimitExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> child_;
    std::vector<ColMeta> cols_;
    size_t len_;
    bool is_end_;
    int limit_;
    int offset_;
    int count_;
    std::unique_ptr<RmRecord> current_;

   public:
    LimitExecutor(std::unique_ptr<AbstractExecutor> child, int limit, int offset)
        : child_(std::move(child)), limit_(limit), offset_(offset) {
        cols_ = child_->cols();
        len_ = child_->tupleLen();
        is_end_ = true;
        count_ = 0;
    }

    void beginTuple() override {
        count_ = 0;
        child_->beginTuple();

        // Skip offset rows
        while (offset_ > 0 && !child_->is_end()) {
            child_->nextTuple();
            offset_--;
        }

        if (child_->is_end() || limit_ <= 0) {
            is_end_ = true;
            return;
        }
        current_ = child_->Next();
        count_ = 1;
        is_end_ = false;
    }

    void nextTuple() override {
        if (count_ >= limit_) {
            is_end_ = true;
            return;
        }
        child_->nextTuple();
        if (child_->is_end()) {
            is_end_ = true;
            return;
        }
        current_ = child_->Next();
        count_++;
    }

    bool is_end() const override { return is_end_; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end_) return nullptr;
        auto rec = std::make_unique<RmRecord>(len_);
        memcpy(rec->data, current_->data, len_);
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
};
