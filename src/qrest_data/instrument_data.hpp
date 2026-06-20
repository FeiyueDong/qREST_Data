/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
**
** Project: qREST
** File: \src\qrest_algorithm\data_struct\instrument_data.hpp
** -----
** File Created: Tuesday, 5th May 2026 17:29:18
** Author: Dong Feiyue (FeiyueDong@outlook.com)
** -----
** Last Modified: Tuesday, 5th May 2026 17:29:40
** Modified By: Dong Feiyue (FeiyueDong@outlook.com)
*/

#ifndef QREST_INSTRUMENT_DATA_HPP
#define QREST_INSTRUMENT_DATA_HPP

#include <cstddef>

#include "msl/difference.hpp"
#include "msl/matrix.hpp"

namespace qrest::data_struct
{
// 原始的全部加速度监测数据
class InstrumentData
{
public:
    InstrumentData() = default;
    explicit InstrumentData(std::size_t num_time_steps,
                            std::size_t num_measurements,
                            double initial_value = 0.0)
        : acc_data_(num_time_steps, num_measurements, initial_value)
    {}
    InstrumentData(const msl::matrix::matrixd &acc_data) : acc_data_(acc_data)
    {}
    InstrumentData(std::span<double> acc_data_span,
                   std::size_t num_time_steps,
                   std::size_t num_measurements)
        : acc_data_(num_time_steps, num_measurements, acc_data_span)
    {}
    ~InstrumentData() = default;

    void update(const msl::matrix::matrixd &acc_data) { acc_data_ = acc_data; }
    msl::matrix::matrixd &get_data() { return acc_data_; }
    const msl::matrix::matrixd &get_data() const { return acc_data_; }

    double *data() { return acc_data_.data(); }

    void scale(double factor) { acc_data_ *= factor; }

private:
    // Acceleration data matrix (num_measurements x num_time_steps)
    msl::matrix::matrixd acc_data_;
}; // class InstrumentData

}; // namespace qrest::data_struct

#endif // QREST_INSTRUMENT_DATA_HPP
