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

#include <unordered_set>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "system/sm.h"

class DistinctExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> child_;
    std::vector<ColMeta> cols_;
    size_t len_;
    bool is_end_;
    std::unique_ptr<RmRecord> current_;

    struct RowHash {
        size_t operator()(const std::vector<char> &row) const {
            size_t h = 0;
            for (size_t i = 0; i < row.size() && i < 64; i++)
                h = h * 31 + row[i];
            return h;
        }
    };
    std::unordered_set<std::vector<char>, RowHash> seen_;

   public:
    DistinctExecutor(std::unique_ptr<AbstractExecutor> child)
        : child_(std::move(child)) {
        cols_ = child_->cols();
        len_ = child_->tupleLen();
        is_end_ = true;
    }

    void beginTuple() override {
        seen_.clear();
        child_->beginTuple();
        is_end_ = child_->is_end();
        if (!is_end_) {
            current_ = child_->Next();
            seen_.insert(std::vector<char>(current_->data, current_->data + len_));
        }
    }

    void nextTuple() override {
        while (!child_->is_end()) {
            child_->nextTuple();
            if (child_->is_end()) {
                is_end_ = true;
                return;
            }
            auto rec = child_->Next();
            std::vector<char> row(rec->data, rec->data + len_);
            if (seen_.find(row) == seen_.end()) {
                seen_.insert(std::move(row));
                current_ = std::move(rec);
                return;
            }
        }
        is_end_ = true;
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
