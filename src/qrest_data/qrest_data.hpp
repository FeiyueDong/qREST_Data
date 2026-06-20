/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
**
** Project: qREST
** File: \src\qrest_algorithm\data_struct\qrest_data.hpp
** -----
** File Created: Tuesday, 5th May 2026 17:29:18
** Author: Dong Feiyue (FeiyueDong@outlook.com)
** -----
** Last Modified: Tuesday, 5th May 2026 17:34:16
** Modified By: Dong Feiyue (FeiyueDong@outlook.com)
*/

// Description: qREST数据结构头文件，包含QrestData类

#ifndef QREST_QREST_DATA_HPP
#define QREST_QREST_DATA_HPP

#include <array>
#include <numbers>
#include <vector>

#include "instrument_data.hpp"
#include "metadata.hpp"
#include "single_direction_data.hpp"

namespace qrest::data_struct
{
class QrestData
{
public:
    QrestData() = default;
    explicit QrestData(std::string json_str,
                       std::size_t num_time_steps,
                       std::size_t num_measurements,
                       double initial_value = 0.0)
        : metadata_(json_str),
          instrument_data_(num_time_steps, num_measurements, initial_value)
    {}

    QrestData(std::string json_str, const msl::matrix::matrixd &acc_data)
        : metadata_(json_str), instrument_data_(acc_data)
    {}

    QrestData(std::string json_str,
              std::span<double> acc_data_span,
              std::size_t num_time_steps,
              std::size_t num_measurements)
        : metadata_(json_str),
          instrument_data_(acc_data_span, num_time_steps, num_measurements)
    {}

    QrestData(const qrest_data::Metadata &metadata,
              const InstrumentData &instrument_data)
        : metadata_(metadata), instrument_data_(instrument_data)
    {}

    ~QrestData() = default;

    void update(const qrest_data::Metadata &metadata,
                const InstrumentData &instrument_data)
    {
        metadata_ = metadata;
        instrument_data_ = instrument_data;
    }

    qrest_data::Metadata &get_metadata() { return metadata_; }
    const qrest_data::Metadata &get_metadata() const { return metadata_; }
    InstrumentData &get_instrument_data() { return instrument_data_; }
    const InstrumentData &get_instrument_data() const
    {
        return instrument_data_;
    }

    // 获取不重复的各层标高
    std::vector<double> get_instrument_heights() const
    {
        std::vector<double> all_heights(metadata_.InstrumentInfo.ChannelNum);
        for (size_t i = 0; i < all_heights.size(); ++i)
        {
            all_heights[i] =
                metadata_.InstrumentInfo.Channels[i].LocationXYZ[2];
        }
        std::sort(all_heights.begin(), all_heights.end());
        auto last = std::unique(
            all_heights.begin(), all_heights.end(), [](double a, double b) {
                return std::abs(a - b) < 1e-2; // 以1cm为阈值判断是否相同高度
            });

        all_heights.erase(last, all_heights.end());
        return all_heights;
    }

    // 获取最底层的测点索引
    // std::

    // 获取指定目标高度对应的通道索引
    std::vector<size_t>
    get_channel_indices_at_height(double target_height) const
    {
        std::vector<size_t> indices;
        for (size_t i = 0;
             i < static_cast<size_t>(metadata_.InstrumentInfo.ChannelNum);
             ++i)
        {
            if (std::abs(metadata_.InstrumentInfo.Channels[i].LocationXYZ[2]
                         - target_height)
                < 1e-2) // 以1cm为阈值判断是否相同高度
            {
                indices.push_back(i);
            }
        }
        return indices;
    }

    // 获取指定目标高度对应的 Jacobian 矩阵
    msl::matrix::matrixd get_jacobian_at_height(double target_height) const
    {
        std::vector<size_t> indices =
            get_channel_indices_at_height(target_height);
        msl::matrix::matrixd jacobian(indices.size(), 3);
        for (size_t i = 0; i < indices.size(); ++i)
        {
            size_t idx = indices[i];
            double az = metadata_.InstrumentInfo.Channels[idx].Azimuth;
            double x = metadata_.InstrumentInfo.Channels[idx].LocationXYZ[0];
            double y = metadata_.InstrumentInfo.Channels[idx].LocationXYZ[1];
            jacobian(i, 0) = std::cos(az * std::numbers::pi / 180.0);
            jacobian(i, 1) = std::sin(az * std::numbers::pi / 180.0);
            jacobian(i, 2) = x * std::sin(az * std::numbers::pi / 180.0)
                             - y * std::cos(az * std::numbers::pi / 180.0);
        }
        return jacobian;
    }

private:
    qrest_data::Metadata metadata_{};  // 元数据
    InstrumentData instrument_data_{}; // 所有通道的监测数据
}; // class QrestData

}; // namespace qrest::data_struct

#endif // QREST_QREST_DATA_HPP
