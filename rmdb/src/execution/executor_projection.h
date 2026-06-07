#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class ProjectionExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<size_t> sel_idxs_;  // -1 means concat/expr column
    std::vector<std::vector<size_t>> concat_cols_;  // child column indices for each concat

   public:
    ProjectionExecutor(std::unique_ptr<AbstractExecutor> prev, const std::vector<TabCol> &sel_cols) {
        prev_ = std::move(prev);
        size_t curr_offset = 0;
        auto &prev_cols = prev_->cols();
        std::vector<bool> used(prev_cols.size(), false);
        for (auto &sel_col : sel_cols) {
            // Check for concat placeholder
            if (sel_col.tab_name == "__expr__" || sel_col.col_name == "concat") {
                // Create a virtual STRING column with enough space
                ColMeta cm;
                cm.tab_name = "";
                cm.name = "concat";
                cm.type = TYPE_STRING;
                cm.len = 256;
                cm.offset = curr_offset;
                curr_offset += cm.len;
                cols_.push_back(cm);
                sel_idxs_.push_back((size_t)-1);
                // Collect all prev_col string indices as concat inputs
                std::vector<size_t> cidxs;
                for (size_t j = 0; j < prev_cols.size(); j++) {
                    cidxs.push_back(j);
                }
                concat_cols_.push_back(cidxs);
                continue;
            }
            // Find matching column
            size_t idx = 0;
            for (; idx < prev_cols.size(); idx++) {
                if (!used[idx] && prev_cols[idx].tab_name == sel_col.tab_name &&
                    prev_cols[idx].name == sel_col.col_name) {
                    used[idx] = true;
                    break;
                }
            }
            if (idx == prev_cols.size()) {
                throw ColumnNotFoundError(sel_col.tab_name + '.' + sel_col.col_name);
            }
            sel_idxs_.push_back(idx);
            auto col = prev_cols[idx];
            col.offset = curr_offset;
            curr_offset += col.len;
            cols_.push_back(col);
        }
        len_ = curr_offset;
    }

    void beginTuple() override { prev_->beginTuple(); }
    void nextTuple() override { prev_->nextTuple(); }
    bool is_end() const override { return prev_->is_end(); }
    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        auto prev_rec = prev_->Next();
        if (!prev_rec) return nullptr;
        auto proj_rec = std::make_unique<RmRecord>(len_);
        auto &prev_cols = prev_->cols();
        for (size_t i = 0; i < sel_idxs_.size(); i++) {
            auto &col = cols_[i];
            if (sel_idxs_[i] == (size_t)-1) {
                // Concat column: concatenate all string columns from child
                std::string result;
                for (auto cidx : concat_cols_[i]) {
                    auto &pc = prev_cols[cidx];
                    if (pc.type == TYPE_STRING) {
                        std::string s(prev_rec->data + pc.offset, pc.len);
                        s = s.c_str();  // trim nulls
                        result += s;
                    } else if (pc.type == TYPE_INT) {
                        result += std::to_string(*(int*)(prev_rec->data + pc.offset));
                    } else if (pc.type == TYPE_FLOAT) {
                        result += std::to_string(*(float*)(prev_rec->data + pc.offset));
                    }
                }
                memset(proj_rec->data + col.offset, 0, col.len);
                memcpy(proj_rec->data + col.offset, result.c_str(), std::min(result.size(), (size_t)col.len));
            } else {
                memcpy(proj_rec->data + col.offset, prev_rec->data + prev_cols[sel_idxs_[i]].offset, col.len);
            }
        }
        return proj_rec;
    }

    ColMeta get_col_offset(const TabCol &target) override {
        for (auto &col : cols_) {
            if (col.name == target.col_name &&
                (target.tab_name.empty() || col.tab_name == target.tab_name))
                return col;
        }
        return ColMeta{};
    }

    Rid &rid() override { return _abstract_rid; }
};
