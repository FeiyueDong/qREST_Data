#include "qrest_view_model.h"
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cmath>

#include "../core/validation.hpp"

namespace {

struct ChannelDefaults {
    QString deviceType{"Accelerometer"};
    QString measurand{"Acceleration"};
    double scale{1.0};
};

struct Point2D {
    double x{};
    double y{};
};

struct Point3D {
    double x{};
    double y{};
    double z{};
};

struct ProjectedBounds {
    bool valid{false};
    double minX{};
    double maxX{};
    double minY{};
    double maxY{};
};

QString formatDouble(double value) { return QString::number(value, 'g', 12); }

QString formatDoubleList(const std::vector<double> &values) {
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(values.size()));
    for (double value : values) {
        parts.append(formatDouble(value));
    }
    return parts.join(", ");
}

bool parseElevationList(const QString &text,
                        std::vector<double> *values,
                        QString *error) {
    const QStringList parts =
        text.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (error) {
            *error = "Elevation 至少需要一个数值";
        }
        return false;
    }

    std::vector<double> parsed;
    parsed.reserve(static_cast<size_t>(parts.size()));
    for (const QString &part : parts) {
        bool ok = false;
        const double value = part.toDouble(&ok);
        if (!ok) {
            if (error) {
                *error = QString("Elevation 包含无法解析的数值: %1").arg(part);
            }
            return false;
        }
        parsed.push_back(value);
    }

    for (size_t i = 1; i < parsed.size(); ++i) {
        if (std::abs(parsed[i] - parsed[i - 1]) < 1e-12) {
            if (error) {
                *error = "Elevation 存在重复值";
            }
            return false;
        }
        if (parsed[i] <= parsed[i - 1]) {
            if (error) {
                *error = "Elevation 必须严格递增，请不要自动排序后提交";
            }
            return false;
        }
    }

    if (values) {
        *values = std::move(parsed);
    }
    return true;
}

bool parsePolygonCorners(const QString &text,
                         std::vector<std::array<double, 2>> *corners,
                         QString *error) {
    const QStringList rows =
        text.split(QRegularExpression("[;\\n\\r]+"), Qt::SkipEmptyParts);
    if (rows.size() < 3) {
        if (error) {
            *error = "Polygon 至少需要 3 个顶点";
        }
        return false;
    }

    std::vector<std::array<double, 2>> parsed;
    parsed.reserve(static_cast<size_t>(rows.size()));
    for (const QString &row : rows) {
        const QStringList parts = row.trimmed().split(
            QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
        if (parts.size() != 2) {
            if (error) {
                *error = QString("Polygon 顶点格式错误: %1").arg(row.trimmed());
            }
            return false;
        }

        bool okX = false;
        bool okY = false;
        const double x = parts[0].toDouble(&okX);
        const double y = parts[1].toDouble(&okY);
        if (!okX || !okY || !std::isfinite(x) || !std::isfinite(y)) {
            if (error) {
                *error =
                    QString("Polygon 顶点包含无效数值: %1").arg(row.trimmed());
            }
            return false;
        }
        parsed.push_back({x, y});
    }

    if (corners) {
        *corners = std::move(parsed);
    }
    return true;
}

void updateBoundingBox(qrest_data::Metadata &metadata) {
    auto &footprint = metadata.BuildingInfo.StructuralFootprint;
    auto &bbox = footprint.BoundingBox;

    if (footprint.Shape == "Rectangular") {
        const double halfLength = footprint.Parameters.Length / 2.0;
        const double halfWidth = footprint.Parameters.Width / 2.0;
        bbox.MinX = -halfLength;
        bbox.MaxX = halfLength;
        bbox.MinY = -halfWidth;
        bbox.MaxY = halfWidth;
    } else if (footprint.Shape == "Circular") {
        const double radius = footprint.Parameters.Radius;
        bbox.MinX = -radius;
        bbox.MaxX = radius;
        bbox.MinY = -radius;
        bbox.MaxY = radius;
    } else if (footprint.Shape == "Polygon"
               && !footprint.Parameters.Corners.empty()) {
        bbox.MinX = bbox.MaxX = footprint.Parameters.Corners.front()[0];
        bbox.MinY = bbox.MaxY = footprint.Parameters.Corners.front()[1];
        for (const auto &corner : footprint.Parameters.Corners) {
            bbox.MinX = std::min(bbox.MinX, corner[0]);
            bbox.MaxX = std::max(bbox.MaxX, corner[0]);
            bbox.MinY = std::min(bbox.MinY, corner[1]);
            bbox.MaxY = std::max(bbox.MaxY, corner[1]);
        }
    }
}

int samplingRateFromMetadata(const qrest_data::Metadata &metadata) {
    if (metadata.DataInfo.DT > 0.0) {
        return static_cast<int>(1.0 / metadata.DataInfo.DT + 0.5);
    }
    return 0;
}

double normalizedAzimuth(double azimuth) {
    double normalized = std::fmod(azimuth, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

bool nearlyAzimuth(double value, double target, double tolerance = 5.0) {
    const double diff = std::abs(normalizedAzimuth(value - target));
    return diff <= tolerance || diff >= 360.0 - tolerance;
}

bool isUnknownChannelId(const QString &channelId) {
    return channelId.trimmed() == "UNKNOWN";
}

QString channelDirectionFromAzimuth(double azimuth) {
    if (!std::isfinite(azimuth)) {
        return "UNSUPPORTED";
    }
    if (std::abs(azimuth + 1.0) < 1e-9) {
        return "Z";
    }
    if (nearlyAzimuth(azimuth, 90.0) || nearlyAzimuth(azimuth, 270.0)) {
        return "X";
    }
    if (nearlyAzimuth(azimuth, 0.0) || nearlyAzimuth(azimuth, 180.0)) {
        return "Y";
    }
    return "HORIZONTAL";
}

Point2D projectPoint(Point3D point) {
    return {.x = point.x - point.y, .y = (point.x + point.y) * 0.45 + point.z};
}

Point2D projectVector(Point3D vector) {
    const Point2D origin = projectPoint({0.0, 0.0, 0.0});
    const Point2D target = projectPoint(vector);
    return {.x = target.x - origin.x, .y = target.y - origin.y};
}

void includeProjected(ProjectedBounds &bounds, Point2D point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return;
    }
    if (!bounds.valid) {
        bounds.valid = true;
        bounds.minX = bounds.maxX = point.x;
        bounds.minY = bounds.maxY = point.y;
        return;
    }
    bounds.minX = std::min(bounds.minX, point.x);
    bounds.maxX = std::max(bounds.maxX, point.x);
    bounds.minY = std::min(bounds.minY, point.y);
    bounds.maxY = std::max(bounds.maxY, point.y);
}

std::vector<Point2D> footprintPoints(
    const qrest_data::Metadata::BuildingInfoStruct::StructuralFootprintStruct
        &footprint) {
    std::vector<Point2D> points;
    if (footprint.Shape == "Rectangular") {
        const double halfLength = footprint.Parameters.Length / 2.0;
        const double halfWidth = footprint.Parameters.Width / 2.0;
        points = {{-halfLength, -halfWidth},
                  {halfLength, -halfWidth},
                  {halfLength, halfWidth},
                  {-halfLength, halfWidth}};
    } else if (footprint.Shape == "Circular") {
        constexpr int segments = 48;
        constexpr double pi = 3.14159265358979323846;
        points.reserve(segments);
        for (int i = 0; i < segments; ++i) {
            const double angle = static_cast<double>(i) / segments * 2.0 * pi;
            points.push_back({std::cos(angle) * footprint.Parameters.Radius,
                              std::sin(angle) * footprint.Parameters.Radius});
        }
    } else if (footprint.Shape == "Polygon") {
        points.reserve(footprint.Parameters.Corners.size());
        for (const auto &corner : footprint.Parameters.Corners) {
            points.push_back({corner[0], corner[1]});
        }
    }
    return points;
}

Point3D sensorDirectionVector(double azimuth) {
    constexpr double pi = 3.14159265358979323846;
    if (std::abs(azimuth + 1.0) < 1e-9) {
        return {0.0, 0.0, 1.0};
    }
    if (std::isfinite(azimuth)) {
        const double radians = azimuth * pi / 180.0;
        return {std::sin(radians), std::cos(radians), 0.0};
    }
    return {0.0, 0.0, 0.0};
}

ProjectedBounds structureProjectedBounds(const qrest_data::Metadata &metadata) {
    ProjectedBounds bounds;
    const auto points =
        footprintPoints(metadata.BuildingInfo.StructuralFootprint);
    for (double elevation : metadata.BuildingInfo.Elevation) {
        for (const Point2D &point : points) {
            includeProjected(bounds,
                             projectPoint({point.x, point.y, elevation}));
        }
    }
    if (!bounds.valid) {
        includeProjected(bounds, {-0.5, -0.5});
        includeProjected(bounds, {0.5, 0.5});
    }
    const double marginX = std::max((bounds.maxX - bounds.minX) * 0.08, 0.5);
    const double marginY = std::max((bounds.maxY - bounds.minY) * 0.08, 0.5);
    bounds.minX -= marginX;
    bounds.maxX += marginX;
    bounds.minY -= marginY;
    bounds.maxY += marginY;
    return bounds;
}

void renumberChannels(qrest_data::Metadata &metadata) {
    auto &channels = metadata.InstrumentInfo.Channels;
    for (size_t i = 0; i < channels.size(); ++i) {
        channels[i].ChannelNo = static_cast<int>(i + 1);
    }
    metadata.InstrumentInfo.ChannelNum = static_cast<int>(channels.size());
}

void normalizeMetadata(qrest_data::Metadata &metadata) {
    metadata.Header = "qREST_DATA";
    metadata.Version = {1, 0, 0};
    if (metadata.Units[0].empty()) {
        metadata.Units[0] = "m";
    }
    metadata.Units[1] = "s";
    metadata.BuildingInfo.ElevationNum =
        static_cast<int>(metadata.BuildingInfo.Elevation.size());
    for (auto &channel : metadata.InstrumentInfo.Channels) {
        if (channel.DeviceType.empty()) {
            channel.DeviceType = "Unknown";
        }
    }
    renumberChannels(metadata);
    updateBoundingBox(metadata);
}

qrest_data::Metadata::InstrumentInfoStruct::ChannelStruct
makeDefaultChannel(int channelNo, const ChannelDefaults &defaults = {}) {
    qrest_data::Metadata::InstrumentInfoStruct::ChannelStruct channel;
    channel.ChannelNo = channelNo;
    channel.ChannelID = "UNKNOWN";
    channel.DeviceType = defaults.deviceType.toStdString();
    channel.Measurand = defaults.measurand.toStdString();
    channel.Scale = defaults.scale;
    channel.Azimuth = -1.0;
    channel.LocationXYZ = {0.0, 0.0, 0.0};
    return channel;
}

ChannelDefaults
channelDefaultsFromMetadata(const qrest_data::Metadata &metadata) {
    ChannelDefaults defaults;
    if (!metadata.InstrumentInfo.Channels.empty()) {
        const auto &last = metadata.InstrumentInfo.Channels.back();
        if (!last.DeviceType.empty()) {
            defaults.deviceType = QString::fromStdString(last.DeviceType);
        }
        if (!last.Measurand.empty()) {
            defaults.measurand = QString::fromStdString(last.Measurand);
        }
        if (std::isfinite(last.Scale) && last.Scale != 0.0) {
            defaults.scale = last.Scale;
        }
    }
    return defaults;
}

} // namespace

// ================= DataTableModel 实现 =================

DataTableModel::DataTableModel(QObject *parent) : QAbstractTableModel(parent) {}

#ifndef Q_MOC_RUN
void DataTableModel::loadData(const qrest_data::DataPacket *packet) {
    beginResetModel(); // 告诉 QML 准备彻底刷新表格
    m_packet = packet;
    endResetModel(); // 刷新完成
}
#endif

void DataTableModel::clear() {
    beginResetModel();
    m_packet = nullptr;
    endResetModel();
}

int DataTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_packet)
        return 0;
    return m_packet->get_data_point_count(); // 行数为采样点数 (NPTS)
}

int DataTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_packet)
        return 0;
    return m_packet->get_channel_count(); // 列数为通道数
}

QVariant DataTableModel::data(const QModelIndex &index, int role) const {
    if (!m_packet || !index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    int row = index.row();
    int col = index.column();
    size_t data_index =
        static_cast<size_t>(col) * m_packet->get_data_point_count() + row;

    const auto &raw_data = m_packet->get_data();
    if (data_index < raw_data.size()) {
        // 使用 'g' 格式，自动处理科学计数法，保持 8 位有效数字
        return QString::number(raw_data[data_index], 'g', 8);
    }
    return QVariant();
}

QVariant DataTableModel::headerData(int section,
                                    Qt::Orientation orientation,
                                    int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        return QString("CH %1").arg(section + 1);
    } else {
        // 垂直表头：计算时间 (row + 1) / fs
        if (m_packet && m_packet->get_sampling_rate() > 0) {
            double fs = static_cast<double>(m_packet->get_sampling_rate());
            double time = static_cast<double>(section + 1) / fs;
            // 保留 4 位小数，方便观察高采样率数据
            return QString::number(time, 'f', 4);
        }
        return QString::number(section + 1);
    }
}

// ================= ChannelTableModel 实现 =================

ChannelTableModel::ChannelTableModel(QObject *parent)
    : QAbstractTableModel(parent) {}

#ifndef Q_MOC_RUN
void ChannelTableModel::loadMetadata(const qrest_data::Metadata *metadata) {
    beginResetModel();
    m_metadata = metadata;
    endResetModel();
}
#endif

void ChannelTableModel::clear() {
    beginResetModel();
    m_metadata = nullptr;
    endResetModel();
}

int ChannelTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_metadata)
        return 0;
    return static_cast<int>(m_metadata->InstrumentInfo.Channels.size());
}

int ChannelTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_metadata)
        return 0;
    return 10;
}

QVariant ChannelTableModel::data(const QModelIndex &index, int role) const {
    if (!m_metadata || !index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const auto &channels = m_metadata->InstrumentInfo.Channels;
    if (index.row() < 0 || index.row() >= static_cast<int>(channels.size())) {
        return QVariant();
    }

    const auto &channel = channels[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0:
            return channel.ChannelNo;
        case 1:
            return QString::fromStdString(channel.ChannelID);
        case 2:
            return QString::fromStdString(channel.DeviceType);
        case 3:
            return QString::fromStdString(channel.Measurand);
        case 4:
            return channelDirectionFromAzimuth(channel.Azimuth);
        case 5:
            return formatDouble(channel.Scale);
        case 6:
            return formatDouble(channel.Azimuth);
        case 7:
            return formatDouble(channel.LocationXYZ[0]);
        case 8:
            return formatDouble(channel.LocationXYZ[1]);
        case 9:
            return formatDouble(channel.LocationXYZ[2]);
        default:
            return QVariant();
    }
}

QVariant ChannelTableModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    switch (section) {
        case 0:
            return "No";
        case 1:
            return "Channel ID";
        case 2:
            return "Device Type";
        case 3:
            return "Measurand";
        case 4:
            return "Direction";
        case 5:
            return "Scale";
        case 6:
            return "Azimuth";
        case 7:
            return "X";
        case 8:
            return "Y";
        case 9:
            return "Z";
        default:
            return QVariant();
    }
}

// ================= ValidationTableModel 实现 =================

ValidationTableModel::ValidationTableModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int ValidationTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_issues.size();
}

int ValidationTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return 3;
}

QVariant ValidationTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0
        || index.row() >= m_issues.size()) {
        return QVariant();
    }

    const Issue &issue = m_issues[index.row()];
    switch (index.column()) {
        case 0:
            if (issue.severity == Severity::Error) {
                return "Error";
            }
            if (issue.severity == Severity::Warning) {
                return "Warning";
            }
            return "Info";
        case 1:
            return issue.area;
        case 2:
            return issue.message;
        default:
            return QVariant();
    }
}

QVariant ValidationTableModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    switch (section) {
        case 0:
            return "Severity";
        case 1:
            return "Area";
        case 2:
            return "Message";
        default:
            return QVariant();
    }
}

void ValidationTableModel::setIssues(QList<Issue> issues) {
    beginResetModel();
    m_issues = std::move(issues);
    endResetModel();
}

void ValidationTableModel::clear() {
    beginResetModel();
    m_issues.clear();
    endResetModel();
}

int ValidationTableModel::errorCount() const {
    return static_cast<int>(std::count_if(
        m_issues.cbegin(), m_issues.cend(), [](const Issue &issue) {
            return issue.severity == Severity::Error;
        }));
}

int ValidationTableModel::warningCount() const {
    return static_cast<int>(std::count_if(
        m_issues.cbegin(), m_issues.cend(), [](const Issue &issue) {
            return issue.severity == Severity::Warning;
        }));
}

int ValidationTableModel::infoCount() const {
    return static_cast<int>(std::count_if(
        m_issues.cbegin(), m_issues.cend(), [](const Issue &issue) {
            return issue.severity == Severity::Info;
        }));
}

// ================= BinaryTableModel 实现 =================

BinaryTableModel::BinaryTableModel(QObject *parent)
    : QAbstractTableModel(parent) {}

void BinaryTableModel::loadBytes(const QByteArray &bytes) {
    beginResetModel();
    m_bytes = bytes;
    endResetModel();
}

void BinaryTableModel::clear() {
    beginResetModel();
    m_bytes.clear();
    endResetModel();
}

int BinaryTableModel::byteCount() const { return m_bytes.size(); }

int BinaryTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return (m_bytes.size() + 15) / 16;
}

int BinaryTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return 3;
}

QVariant BinaryTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0) {
        return QVariant();
    }

    const int offset = index.row() * 16;
    if (offset >= m_bytes.size()) {
        return QVariant();
    }

    switch (index.column()) {
        case 0:
            return QString("%1").arg(offset, 8, 16, QChar('0')).toUpper();
        case 1: {
            QStringList parts;
            parts.reserve(16);
            for (int i = 0; i < 16 && offset + i < m_bytes.size(); ++i) {
                const auto byte =
                    static_cast<unsigned char>(m_bytes[offset + i]);
                parts.append(
                    QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
            }
            return parts.join(' ');
        }
        case 2: {
            QString ascii;
            ascii.reserve(16);
            for (int i = 0; i < 16 && offset + i < m_bytes.size(); ++i) {
                const auto byte =
                    static_cast<unsigned char>(m_bytes[offset + i]);
                ascii.append(byte >= 32 && byte <= 126 ? QChar(byte)
                                                       : QChar('.'));
            }
            return ascii;
        }
        default:
            return QVariant();
    }
}

QVariant BinaryTableModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    switch (section) {
        case 0:
            return "Offset";
        case 1:
            return "Hex Bytes";
        case 2:
            return "ASCII";
        default:
            return QVariant();
    }
}

// ================= QrestViewModel 实现 =================

QrestViewModel::QrestViewModel(QObject *parent) : QObject(parent) {
    m_tableModel = new DataTableModel(this);
    // 实例化选择模型并绑定到我们的数据模型
    m_selectionModel = new QItemSelectionModel(m_tableModel, this);
    m_channelModel = new ChannelTableModel(this);
    m_channelSelectionModel = new QItemSelectionModel(m_channelModel, this);
    m_validationModel = new ValidationTableModel(this);
    m_binaryModel = new BinaryTableModel(this);
    m_tableModel->loadData(&m_document.dataPacket());
    refreshChannelModel();
    m_binaryModel->loadBytes(m_document.rawFileBytes());
    rebuildValidationReport();
}

QString QrestViewModel::headerMagic() const {
    return QString::fromStdString(m_document.fileHeader().get_magic());
}
int QrestViewModel::metadataSize() const {
    return m_document.fileHeader().get_metadata_size();
}
int QrestViewModel::dataSize() const {
    return m_document.fileHeader().get_data_size();
}
QJsonObject QrestViewModel::metadataJson() const {
    std::string jsonStr = m_document.metadata().to_bytes();
    return QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr)).object();
}
void QrestViewModel::setMetadataJson(const QJsonObject &json) {
    std::string jsonStr =
        QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    try {
        qrest_data::Metadata metadata =
            qrest_data::Metadata::from_bytes(jsonStr);
        normalizeMetadata(metadata);
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("元数据已修改，派生字段已重新计算");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新元数据失败: %1").arg(e.what()), true);
    }
}

// 数据包 Getter
QItemSelectionModel *QrestViewModel::selectionModel() const {
    return m_selectionModel;
}

int QrestViewModel::packetSourceId() const {
    return m_document.dataPacket().get_source_id();
}
int QrestViewModel::packetChannelCount() const {
    return m_document.dataPacket().get_channel_count();
}
int QrestViewModel::packetSamplingRate() const {
    return m_document.dataPacket().get_sampling_rate();
}
int QrestViewModel::packetDataPointCount() const {
    return m_document.dataPacket().get_data_point_count();
}
QAbstractTableModel *QrestViewModel::tableModel() const { return m_tableModel; }
QAbstractTableModel *QrestViewModel::channelModel() const {
    return m_channelModel;
}
QItemSelectionModel *QrestViewModel::channelSelectionModel() const {
    return m_channelSelectionModel;
}
int QrestViewModel::packetDataEncodings() const {
    return m_document.dataPacket().get_data_encodings();
}
qlonglong QrestViewModel::packetTimestamp() const {
    return static_cast<qlonglong>(m_document.dataPacket().get_timestamp());
}

QString QrestViewModel::documentModeName() const {
    switch (m_document.mode()) {
        case QrestDocument::Mode::View:
            return "Read Only";
        case QrestDocument::Mode::EditDraft:
            return "Editing Copy";
        case QrestDocument::Mode::NewDraft:
            return "New Draft";
    }
    return "Unknown";
}

QString QrestViewModel::documentStatus() const {
    QString status =
        QString("%1 - %2").arg(sourceFileName(), documentModeName());
    if (m_document.isDirty()) {
        status += " *";
    }
    return status;
}

QString QrestViewModel::sourceFileName() const {
    return m_document.displayName();
}

bool QrestViewModel::isDirty() const { return m_document.isDirty(); }

bool QrestViewModel::canModify() const { return m_document.canModify(); }

bool QrestViewModel::canEdit() const {
    return m_document.mode() == QrestDocument::Mode::View;
}

bool QrestViewModel::canSaveAs() const {
    return m_document.mode() == QrestDocument::Mode::EditDraft
           || m_document.mode() == QrestDocument::Mode::NewDraft;
}

QString QrestViewModel::metadataHeader() const {
    return QString::fromStdString(m_document.metadata().Header);
}

QString QrestViewModel::metadataVersionText() const {
    const auto &version = m_document.metadata().Version;
    return QString("%1.%2.%3").arg(version[0]).arg(version[1]).arg(version[2]);
}

QString QrestViewModel::distanceUnit() const {
    return QString::fromStdString(m_document.metadata().Units[0]);
}

QString QrestViewModel::timeUnit() const {
    return QString::fromStdString(m_document.metadata().Units[1]);
}

QString QrestViewModel::projectName() const {
    return QString::fromStdString(
        m_document.metadata().BuildingInfo.ProjectName);
}

QString QrestViewModel::structuralType() const {
    return QString::fromStdString(
        m_document.metadata().BuildingInfo.StructuralType);
}

double QrestViewModel::longitude() const {
    return m_document.metadata().BuildingInfo.GeoLocation.Longitude;
}

double QrestViewModel::latitude() const {
    return m_document.metadata().BuildingInfo.GeoLocation.Latitude;
}

double QrestViewModel::northAngle() const {
    return m_document.metadata().BuildingInfo.GeoLocation.NorthAngle;
}

QString QrestViewModel::footprintShape() const {
    return QString::fromStdString(
        m_document.metadata().BuildingInfo.StructuralFootprint.Shape);
}

double QrestViewModel::footprintLength() const {
    return m_document.metadata()
        .BuildingInfo.StructuralFootprint.Parameters.Length;
}

double QrestViewModel::footprintWidth() const {
    return m_document.metadata()
        .BuildingInfo.StructuralFootprint.Parameters.Width;
}

double QrestViewModel::footprintRadius() const {
    return m_document.metadata()
        .BuildingInfo.StructuralFootprint.Parameters.Radius;
}

QString QrestViewModel::boundingBoxText() const {
    const auto &bbox =
        m_document.metadata().BuildingInfo.StructuralFootprint.BoundingBox;
    return QString("X: %1 ~ %2, Y: %3 ~ %4")
        .arg(formatDouble(bbox.MinX),
             formatDouble(bbox.MaxX),
             formatDouble(bbox.MinY),
             formatDouble(bbox.MaxY));
}

QString QrestViewModel::polygonCornersText() const {
    const auto &corners =
        m_document.metadata()
            .BuildingInfo.StructuralFootprint.Parameters.Corners;
    QStringList rows;
    rows.reserve(static_cast<qsizetype>(corners.size()));
    for (const auto &corner : corners) {
        rows.append(QString("%1, %2").arg(formatDouble(corner[0]),
                                          formatDouble(corner[1])));
    }
    return rows.join('\n');
}

QString QrestViewModel::elevationText() const {
    return formatDoubleList(m_document.metadata().BuildingInfo.Elevation);
}

QString QrestViewModel::elevationSummary() const {
    return QString("Parsed: %1 levels").arg(elevationNum());
}

int QrestViewModel::elevationNum() const {
    return m_document.metadata().BuildingInfo.ElevationNum;
}

QString QrestViewModel::provider() const {
    return QString::fromStdString(
        m_document.metadata().InstrumentInfo.Provider);
}

int QrestViewModel::channelNum() const {
    return m_document.metadata().InstrumentInfo.ChannelNum;
}

QString QrestViewModel::eventName() const {
    return QString::fromStdString(m_document.metadata().DataInfo.EventName);
}

QString QrestViewModel::startTime() const {
    return QString::fromStdString(m_document.metadata().DataInfo.StartTime);
}

qlonglong QrestViewModel::startTimestamp() const {
    QDateTime parsed = QDateTime::fromString(startTime(), Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(startTime(), Qt::ISODate);
    }
    if (parsed.isValid()) {
        return parsed.toMSecsSinceEpoch();
    }
    return static_cast<qlonglong>(m_document.dataPacket().get_timestamp());
}

int QrestViewModel::samplingRate() const {
    const int packetRate = m_document.dataPacket().get_sampling_rate();
    if (packetRate > 0) {
        return packetRate;
    }
    return samplingRateFromMetadata(m_document.metadata());
}

QString QrestViewModel::samplingIntervalText() const {
    const double dt = m_document.metadata().DataInfo.DT;
    if (dt <= 0.0) {
        return "Auto";
    }
    return QString("%1 %2").arg(formatDouble(dt), timeUnit());
}

int QrestViewModel::dataNpts() const {
    const int packetNpts = m_document.dataPacket().get_data_point_count();
    if (packetNpts > 0) {
        return packetNpts;
    }
    return m_document.metadata().DataInfo.NPTS;
}

QString QrestViewModel::corrected() const {
    return QString::fromStdString(m_document.metadata().DataInfo.Corrected);
}

int QrestViewModel::selectedChannelRow() const { return m_selectedChannelRow; }

bool QrestViewModel::hasSelectedChannel() const {
    return m_selectedChannelRow >= 0
           && m_selectedChannelRow < static_cast<int>(
                  m_document.metadata().InstrumentInfo.Channels.size());
}

bool QrestViewModel::canEditChannelOrder() const {
    return canModify() && m_document.dataPacket().get_data().empty();
}

int QrestViewModel::selectedChannelNo() const {
    if (!hasSelectedChannel()) {
        return 0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .ChannelNo;
}

QString QrestViewModel::selectedChannelId() const {
    if (!hasSelectedChannel()) {
        return QString();
    }
    return QString::fromStdString(
        m_document.metadata()
            .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
            .ChannelID);
}

QString QrestViewModel::selectedChannelDeviceType() const {
    if (!hasSelectedChannel()) {
        return QString();
    }
    return QString::fromStdString(
        m_document.metadata()
            .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
            .DeviceType);
}

QString QrestViewModel::selectedChannelMeasurand() const {
    if (!hasSelectedChannel()) {
        return QString();
    }
    return QString::fromStdString(
        m_document.metadata()
            .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
            .Measurand);
}

double QrestViewModel::selectedChannelScale() const {
    if (!hasSelectedChannel()) {
        return 0.0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .Scale;
}

double QrestViewModel::selectedChannelAzimuth() const {
    if (!hasSelectedChannel()) {
        return 0.0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .Azimuth;
}

QString QrestViewModel::selectedChannelDirection() const {
    if (!hasSelectedChannel()) {
        return QString();
    }
    return channelDirectionFromAzimuth(selectedChannelAzimuth());
}

double QrestViewModel::selectedChannelX() const {
    if (!hasSelectedChannel()) {
        return 0.0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .LocationXYZ[0];
}

double QrestViewModel::selectedChannelY() const {
    if (!hasSelectedChannel()) {
        return 0.0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .LocationXYZ[1];
}

double QrestViewModel::selectedChannelZ() const {
    if (!hasSelectedChannel()) {
        return 0.0;
    }
    return m_document.metadata()
        .InstrumentInfo.Channels[static_cast<size_t>(m_selectedChannelRow)]
        .LocationXYZ[2];
}

QVariantList QrestViewModel::geometryFloorOutlines() const {
    const auto &metadata = m_document.metadata();
    const auto &footprint = metadata.BuildingInfo.StructuralFootprint;

    QVariantList outline2d;
    if (footprint.Shape == "Rectangular") {
        const double halfLength = footprint.Parameters.Length / 2.0;
        const double halfWidth = footprint.Parameters.Width / 2.0;
        const std::array<std::array<double, 2>, 5> points{{
            {-halfLength, -halfWidth},
            {halfLength, -halfWidth},
            {halfLength, halfWidth},
            {-halfLength, halfWidth},
            {-halfLength, -halfWidth},
        }};
        for (const auto &point : points) {
            outline2d.append(QVariantMap{{"x", point[0]}, {"y", point[1]}});
        }
    } else if (footprint.Shape == "Circular") {
        constexpr int segments = 48;
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i <= segments; ++i) {
            const double angle = static_cast<double>(i) / segments * 2.0 * pi;
            outline2d.append(QVariantMap{
                {"x", std::cos(angle) * footprint.Parameters.Radius},
                {"y", std::sin(angle) * footprint.Parameters.Radius}});
        }
    } else if (footprint.Shape == "Polygon") {
        for (const auto &corner : footprint.Parameters.Corners) {
            outline2d.append(QVariantMap{{"x", corner[0]}, {"y", corner[1]}});
        }
        if (!footprint.Parameters.Corners.empty()) {
            const auto &first = footprint.Parameters.Corners.front();
            outline2d.append(QVariantMap{{"x", first[0]}, {"y", first[1]}});
        }
    }

    QVariantList floors;
    for (double elevation : metadata.BuildingInfo.Elevation) {
        floors.append(QVariantMap{{"z", elevation}, {"points", outline2d}});
    }
    return floors;
}

QVariantList QrestViewModel::sensorLayoutPoints() const {
    constexpr double pi = 3.14159265358979323846;
    QVariantList sensors;
    const auto &channels = m_document.metadata().InstrumentInfo.Channels;
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        const auto &channel = channels[static_cast<size_t>(i)];
        double ux = 0.0;
        double uy = 0.0;
        double uz = 0.0;
        if (std::isfinite(channel.Azimuth)
            && std::abs(channel.Azimuth + 1.0) >= 1e-9) {
            const double radians = channel.Azimuth * pi / 180.0;
            ux = std::sin(radians);
            uy = std::cos(radians);
        } else if (std::abs(channel.Azimuth + 1.0) < 1e-9) {
            uz = 1.0;
        }

        sensors.append(QVariantMap{
            {"row", i},
            {"channelNo", channel.ChannelNo},
            {"channelId", QString::fromStdString(channel.ChannelID)},
            {"measurand", QString::fromStdString(channel.Measurand)},
            {"direction", channelDirectionFromAzimuth(channel.Azimuth)},
            {"x", channel.LocationXYZ[0]},
            {"y", channel.LocationXYZ[1]},
            {"z", channel.LocationXYZ[2]},
            {"ux", ux},
            {"uy", uy},
            {"uz", uz},
            {"selected", i == m_selectedChannelRow},
        });
    }
    return sensors;
}

QVariantList QrestViewModel::structureEdges() const {
    const auto &metadata = m_document.metadata();
    const auto footprint =
        footprintPoints(metadata.BuildingInfo.StructuralFootprint);
    const auto &elevations = metadata.BuildingInfo.Elevation;
    QVariantList edges;
    if (footprint.size() < 2 || elevations.empty()) {
        return edges;
    }

    for (double elevation : elevations) {
        for (size_t i = 0; i < footprint.size(); ++i) {
            const Point2D &from = footprint[i];
            const Point2D &to = footprint[(i + 1) % footprint.size()];
            const Point2D p1 = projectPoint({from.x, from.y, elevation});
            const Point2D p2 = projectPoint({to.x, to.y, elevation});
            edges.append(QVariantMap{{"x1", p1.x},
                                     {"y1", p1.y},
                                     {"x2", p2.x},
                                     {"y2", p2.y},
                                     {"kind", "floor"}});
        }
    }

    if (elevations.size() >= 2) {
        const double bottom = elevations.front();
        const double top = elevations.back();
        for (const Point2D &point : footprint) {
            const Point2D p1 = projectPoint({point.x, point.y, bottom});
            const Point2D p2 = projectPoint({point.x, point.y, top});
            edges.append(QVariantMap{{"x1", p1.x},
                                     {"y1", p1.y},
                                     {"x2", p2.x},
                                     {"y2", p2.y},
                                     {"kind", "vertical"}});
        }
    }
    return edges;
}

QVariantList QrestViewModel::structureSensors() const {
    QVariantList sensors;
    const auto &channels = m_document.metadata().InstrumentInfo.Channels;
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        const auto &channel = channels[static_cast<size_t>(i)];
        const Point3D direction = sensorDirectionVector(channel.Azimuth);
        const Point2D point = projectPoint({channel.LocationXYZ[0],
                                            channel.LocationXYZ[1],
                                            channel.LocationXYZ[2]});
        const Point2D projectedDirection = projectVector(direction);
        sensors.append(QVariantMap{
            {"row", i},
            {"channelNo", channel.ChannelNo},
            {"channelId", QString::fromStdString(channel.ChannelID)},
            {"deviceType", QString::fromStdString(channel.DeviceType)},
            {"direction", channelDirectionFromAzimuth(channel.Azimuth)},
            {"x", point.x},
            {"y", point.y},
            {"dx", projectedDirection.x},
            {"dy", projectedDirection.y},
            {"ux", direction.x},
            {"uy", direction.y},
            {"uz", direction.z},
            {"selected", i == m_selectedChannelRow},
        });
    }
    return sensors;
}

QVariantList QrestViewModel::structureAxes() const {
    const auto &metadata = m_document.metadata();
    constexpr double pi = 3.14159265358979323846;
    const double length =
        std::max(
            {metadata.BuildingInfo.StructuralFootprint.BoundingBox.MaxX
                 - metadata.BuildingInfo.StructuralFootprint.BoundingBox.MinX,
             metadata.BuildingInfo.StructuralFootprint.BoundingBox.MaxY
                 - metadata.BuildingInfo.StructuralFootprint.BoundingBox.MinY,
             metadata.BuildingInfo.Elevation.empty()
                 ? 1.0
                 : metadata.BuildingInfo.Elevation.back()
                       - metadata.BuildingInfo.Elevation.front(),
             1.0})
        * 0.22;
    const auto makeAxis = [](const QString &label, Point3D vector) {
        const Point2D p = projectVector(vector);
        return QVariantMap{{"label", label},
                           {"x1", 0.0},
                           {"y1", 0.0},
                           {"x2", p.x},
                           {"y2", p.y}};
    };

    QVariantList axes;
    axes.append(makeAxis("X", {length, 0.0, 0.0}));
    axes.append(makeAxis("Y", {0.0, length, 0.0}));
    axes.append(makeAxis("Z", {0.0, 0.0, length}));

    const double northAngle =
        metadata.BuildingInfo.GeoLocation.NorthAngle * pi / 180.0;
    axes.append(makeAxis(
        "N",
        {std::sin(northAngle) * length, std::cos(northAngle) * length, 0.0}));
    return axes;
}

QVariantMap QrestViewModel::structureViewBounds() const {
    const ProjectedBounds bounds =
        structureProjectedBounds(m_document.metadata());
    return QVariantMap{{"valid", bounds.valid},
                       {"minX", bounds.minX},
                       {"maxX", bounds.maxX},
                       {"minY", bounds.minY},
                       {"maxY", bounds.maxY}};
}

QString QrestViewModel::geometrySummary() const {
    const auto &metadata = m_document.metadata();
    return QString("%1 levels, %2 channels, %3 footprint")
        .arg(metadata.BuildingInfo.Elevation.size())
        .arg(metadata.InstrumentInfo.Channels.size())
        .arg(QString::fromStdString(
            metadata.BuildingInfo.StructuralFootprint.Shape));
}

QAbstractTableModel *QrestViewModel::validationModel() const {
    return m_validationModel;
}

int QrestViewModel::validationErrorCount() const {
    return m_validationModel ? m_validationModel->errorCount() : 0;
}

int QrestViewModel::validationWarningCount() const {
    return m_validationModel ? m_validationModel->warningCount() : 0;
}

int QrestViewModel::validationInfoCount() const {
    return m_validationModel ? m_validationModel->infoCount() : 0;
}

QString QrestViewModel::validationStatusText() const {
    const int errors = validationErrorCount();
    const int warnings = validationWarningCount();
    const int infos = validationInfoCount();
    if (errors == 0 && warnings == 0) {
        return "Validation passed";
    }
    return QString("%1 errors, %2 warnings, %3 info")
        .arg(errors)
        .arg(warnings)
        .arg(infos);
}

QAbstractTableModel *QrestViewModel::binaryModel() const {
    return m_binaryModel;
}

int QrestViewModel::binaryByteCount() const {
    return m_binaryModel ? m_binaryModel->byteCount() : 0;
}

QString QrestViewModel::binarySummary() const {
    return QString("%1 bytes, Metadata %2 bytes, DataPacket %3 bytes")
        .arg(binaryByteCount())
        .arg(metadataSize())
        .arg(dataSize());
}

int QrestViewModel::binaryRowForOffset(int offset) const {
    if (offset < 0 || offset >= binaryByteCount()) {
        return -1;
    }
    return offset / 16;
}

int QrestViewModel::findBinaryAscii(const QString &text, int startRow) const {
    if (text.isEmpty()) {
        return -1;
    }

    const QByteArray bytes = m_document.rawFileBytes();
    const int startByte = std::max(0, startRow) * 16;
    const int found = bytes.indexOf(text.toUtf8(), startByte);
    return found >= 0 ? found / 16 : -1;
}

int QrestViewModel::findBinaryHex(const QString &hex, int startRow) const {
    QString compact = hex;
    compact.remove(QRegularExpression("\\s+"));
    if (compact.isEmpty() || compact.size() % 2 != 0
        || compact.contains(QRegularExpression("[^0-9a-fA-F]"))) {
        return -1;
    }

    const QByteArray pattern = QByteArray::fromHex(compact.toLatin1());
    if (pattern.isEmpty()) {
        return -1;
    }

    const QByteArray bytes = m_document.rawFileBytes();
    const int startByte = std::max(0, startRow) * 16;
    const int found = bytes.indexOf(pattern, startByte);
    return found >= 0 ? found / 16 : -1;
}

void QrestViewModel::newFile() {
    m_document.newDocument();
    m_tableModel->loadData(&m_document.dataPacket());
    emitAllDocumentSignals();
    emit showMessage("已创建新文件");
}

void QrestViewModel::openFile(const QString &fileUrl) {
    try {
        m_document.openFile(fileUrl);
        m_tableModel->loadData(&m_document.dataPacket());
        emitAllDocumentSignals();
        emit showMessage(
            QString("成功打开文件: %1").arg(m_document.displayName()));
    } catch (const std::exception &e) {
        emit showMessage(QString("文件解析异常: %1").arg(e.what()), true);
    }
}

void QrestViewModel::saveFile(const QString &fileUrl) {
    try {
        rebuildValidationReport(true);
        if (validationErrorCount() > 0) {
            emit showMessage(QString("保存前校验失败: %1 个错误")
                                 .arg(validationErrorCount()),
                             true);
            return;
        }
        m_document.saveAs(fileUrl);
        emitAllDocumentSignals();
        emit showMessage(
            QString("成功另存为: %1").arg(m_document.displayName()));
    } catch (const std::exception &e) {
        emit showMessage(QString("保存失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::copySelectedCells() {
    if (!m_selectionModel || !m_selectionModel->hasSelection()) {
        qDebug() << "复制失败：当前没有选中的单元格";
        return;
    }

    QModelIndexList indexes = m_selectionModel->selectedIndexes();
    if (indexes.isEmpty())
        return;

    // 按行、列排序，确保复制的格式正确
    std::sort(indexes.begin(),
              indexes.end(),
              [](const QModelIndex &a, const QModelIndex &b) {
                  if (a.row() == b.row())
                      return a.column() < b.column();
                  return a.row() < b.row();
              });

    QString tsvText;
    int currentRow = indexes.first().row();

    for (int i = 0; i < indexes.size(); ++i) {
        const QModelIndex &idx = indexes[i];
        if (idx.row() != currentRow) {
            tsvText += "\n";
            currentRow = idx.row();
        } else if (i != 0) {
            tsvText += "\t";
        }
        // 这里获取的是 data()，即数值部分，不会包含 headerData 里的时间
        tsvText += m_tableModel->data(idx).toString();
    }

    QGuiApplication::clipboard()->setText(tsvText);
    emit showMessage(QString("已成功复制 %1 个数据点").arg(indexes.size()));
}

void QrestViewModel::beginEdit() {
    if (!canEdit()) {
        return;
    }

    m_document.beginEdit();
    emit documentUpdated();
    emit showMessage("已创建可编辑副本，原文件不会被直接修改");
}

void QrestViewModel::validateDocument() {
    rebuildValidationReport(true);
    if (validationErrorCount() == 0) {
        if (validationWarningCount() == 0) {
            emit showMessage("校验通过");
        } else {
            emit showMessage(QString("校验通过，包含 %1 个警告")
                                 .arg(validationWarningCount()));
        }
        return;
    }

    emit showMessage(QString("校验失败: %1 个错误，%2 个警告")
                         .arg(validationErrorCount())
                         .arg(validationWarningCount()),
                     true);
}

void QrestViewModel::runValidationReport() { validateDocument(); }

void QrestViewModel::updateUnits(const QString &distanceUnit,
                                 const QString &timeUnit) {
    Q_UNUSED(timeUnit)
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.Units = {distanceUnit.toStdString(), "s"};
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("距离单位已更新，时间单位固定为 s");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新单位失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateBuildingBasic(const QString &projectName,
                                         const QString &structuralType) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.BuildingInfo.ProjectName = projectName.toStdString();
        metadata.BuildingInfo.StructuralType = structuralType.toStdString();
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("建筑基本信息已更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新建筑信息失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateGeoLocation(double longitude,
                                       double latitude,
                                       double northAngle) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!std::isfinite(longitude) || !std::isfinite(latitude)
        || !std::isfinite(northAngle) || longitude < -180.0 || longitude > 180.0
        || latitude < -90.0 || latitude > 90.0 || northAngle < 0.0
        || northAngle >= 360.0) {
        emit showMessage(
            "地理位置超出范围：Longitude [-180,180], Latitude [-90,90], "
            "NorthAngle [0,360)",
            true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.BuildingInfo.GeoLocation.Longitude = longitude;
        metadata.BuildingInfo.GeoLocation.Latitude = latitude;
        metadata.BuildingInfo.GeoLocation.NorthAngle = northAngle;
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("地理位置已更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新地理位置失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateFootprint(const QString &shape,
                                     double length,
                                     double width,
                                     double radius) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    if (shape == "Rectangular"
        && (!std::isfinite(length) || !std::isfinite(width) || length <= 0.0
            || width <= 0.0)) {
        emit showMessage("矩形轮廓的 Length 和 Width 必须大于 0", true);
        return;
    }
    if (shape == "Circular" && (!std::isfinite(radius) || radius <= 0.0)) {
        emit showMessage("圆形轮廓的 Radius 必须大于 0", true);
        return;
    }
    if (shape != "Rectangular" && shape != "Circular" && shape != "Polygon") {
        emit showMessage(QString("暂不支持的轮廓类型: %1").arg(shape), true);
        return;
    }
    try {
        qrest_data::Metadata metadata = m_document.metadata();
        auto &footprint = metadata.BuildingInfo.StructuralFootprint;
        footprint.Shape = shape.toStdString();
        if (shape == "Rectangular") {
            footprint.Parameters.Length = length;
            footprint.Parameters.Width = width;
        } else if (shape == "Circular") {
            footprint.Parameters.Radius = radius;
        } else if (shape == "Polygon"
                   && footprint.Parameters.Corners.size() < 3) {
            footprint.Parameters.Corners = {
                {-0.5, -0.5},
                {0.5, -0.5},
                {0.5, 0.5},
                {-0.5, 0.5},
            };
        }
        updateBoundingBox(metadata);
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("结构轮廓已更新，BoundingBox 已自动计算");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新结构轮廓失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updatePolygonCornersText(const QString &text) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    std::vector<std::array<double, 2>> corners;
    QString error;
    if (!parsePolygonCorners(text, &corners, &error)) {
        emit showMessage(error, true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        auto &footprint = metadata.BuildingInfo.StructuralFootprint;
        footprint.Shape = "Polygon";
        footprint.Parameters.Corners = std::move(corners);
        updateBoundingBox(metadata);
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("Polygon 轮廓已更新，BoundingBox 已自动计算");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新 Polygon 轮廓失败: %1").arg(e.what()),
                         true);
    }
}

void QrestViewModel::updateElevationText(const QString &text) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    std::vector<double> elevations;
    QString error;
    if (!parseElevationList(text, &elevations, &error)) {
        emit showMessage(error, true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.BuildingInfo.Elevation = elevations;
        metadata.BuildingInfo.ElevationNum =
            static_cast<int>(elevations.size());
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage(
            QString("Elevation 已更新: %1 levels").arg(elevations.size()));
    } catch (const std::exception &e) {
        emit showMessage(QString("更新 Elevation 失败: %1").arg(e.what()),
                         true);
    }
}

void QrestViewModel::updateProvider(const QString &provider) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.InstrumentInfo.Provider = provider.toStdString();
        metadata.InstrumentInfo.ChannelNum =
            static_cast<int>(metadata.InstrumentInfo.Channels.size());
        m_document.replaceMetadata(metadata);
        emitAllDocumentSignals();
        emit showMessage("仪器提供方已更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新仪器信息失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateDataInfo(const QString &eventName,
                                    int samplingRate,
                                    int npts,
                                    const QString &corrected) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (samplingRate <= 0 || samplingRate > 65535) {
        emit showMessage("Sampling Rate 必须在 1 到 65535 Hz 之间", true);
        return;
    }
    if (npts < 0) {
        emit showMessage("NPTS 不能为负数", true);
        return;
    }
    if (!m_document.dataPacket().get_data().empty()
        && static_cast<uint32_t>(npts)
               != m_document.dataPacket().get_data_point_count()) {
        emit showMessage("已有数据导入后，NPTS 必须等于实际包体采样点数", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.DataInfo.EventName = eventName.toStdString();
        metadata.DataInfo.DT = 1.0 / static_cast<double>(samplingRate);
        metadata.DataInfo.Frequency = samplingRate;
        metadata.DataInfo.NPTS = npts;
        metadata.DataInfo.Corrected = corrected.toStdString();

        qrest_data::DataPacket packet = m_document.dataPacket();
        if (packet.get_data().empty()) {
            packet = qrest_data::DataPacket(packet.get_source_id(),
                                            0,
                                            packet.get_data_encodings(),
                                            static_cast<uint16_t>(samplingRate),
                                            0,
                                            packet.get_timestamp(),
                                            packet.get_data());
        } else {
            packet = qrest_data::DataPacket(packet.get_source_id(),
                                            packet.get_channel_count(),
                                            packet.get_data_encodings(),
                                            static_cast<uint16_t>(samplingRate),
                                            packet.get_data_point_count(),
                                            packet.get_timestamp(),
                                            packet.get_data());
        }

        m_document.replaceContent(metadata, packet);
        m_tableModel->loadData(&m_document.dataPacket());
        emitAllDocumentSignals();
        emit showMessage("数据信息已更新，DT 已自动计算");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新数据信息失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateStartTimestamp(qlonglong timestamp) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (timestamp < 0) {
        emit showMessage("StartTime timestamp 不能为负数", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        qrest_data::DataPacket packet = m_document.dataPacket();
        metadata.DataInfo.StartTime = QDateTime::fromMSecsSinceEpoch(timestamp)
                                          .toString(Qt::ISODateWithMs)
                                          .toStdString();
        packet = qrest_data::DataPacket(packet.get_source_id(),
                                        packet.get_channel_count(),
                                        packet.get_data_encodings(),
                                        packet.get_sampling_rate(),
                                        packet.get_data_point_count(),
                                        static_cast<uint64_t>(timestamp),
                                        packet.get_data());
        m_document.replaceContent(metadata, packet);
        m_tableModel->loadData(&m_document.dataPacket());
        emitAllDocumentSignals();
        emit showMessage("StartTime 已更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新 StartTime 失败: %1").arg(e.what()),
                         true);
    }
}

void QrestViewModel::selectChannel(int row) { setSelectedChannelRow(row); }

void QrestViewModel::addChannel() {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!canEditChannelOrder()) {
        emit showMessage("已有数据包体后暂不允许增删通道，请先处理数据矩阵",
                         true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        qrest_data::Metadata::InstrumentInfoStruct::ChannelStruct channel =
            makeDefaultChannel(
                static_cast<int>(metadata.InstrumentInfo.Channels.size()) + 1,
                channelDefaultsFromMetadata(metadata));

        metadata.InstrumentInfo.Channels.push_back(channel);
        renumberChannels(metadata);
        m_document.replaceMetadata(metadata);
        refreshChannelModel();
        setSelectedChannelRow(
            static_cast<int>(metadata.InstrumentInfo.Channels.size()) - 1);
        emitAllDocumentSignals();
        emit showMessage("已添加通道");
    } catch (const std::exception &e) {
        emit showMessage(QString("添加通道失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::duplicateSelectedChannel() {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!canEditChannelOrder()) {
        emit showMessage("已有数据包体后暂不允许增删通道，请先处理数据矩阵",
                         true);
        return;
    }
    if (!hasSelectedChannel()) {
        emit showMessage("请先选择一个通道", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        auto &channels = metadata.InstrumentInfo.Channels;
        auto duplicate = channels[static_cast<size_t>(m_selectedChannelRow)];
        duplicate.ChannelID.clear();
        const auto insertPos = channels.begin() + m_selectedChannelRow + 1;
        channels.insert(insertPos, duplicate);
        renumberChannels(metadata);
        m_document.replaceMetadata(metadata);
        refreshChannelModel();
        setSelectedChannelRow(m_selectedChannelRow + 1);
        emitAllDocumentSignals();
        emit showMessage("已复制通道，请补充新的 ChannelID");
    } catch (const std::exception &e) {
        emit showMessage(QString("复制通道失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::deleteSelectedChannel() {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!canEditChannelOrder()) {
        emit showMessage("已有数据包体后暂不允许增删通道，请先处理数据矩阵",
                         true);
        return;
    }
    if (!hasSelectedChannel()) {
        emit showMessage("请先选择一个通道", true);
        return;
    }

    try {
        const int oldRow = m_selectedChannelRow;
        qrest_data::Metadata metadata = m_document.metadata();
        auto &channels = metadata.InstrumentInfo.Channels;
        channels.erase(channels.begin() + oldRow);
        renumberChannels(metadata);
        m_document.replaceMetadata(metadata);
        refreshChannelModel();
        if (channels.empty()) {
            setSelectedChannelRow(-1);
        } else {
            setSelectedChannelRow(
                std::min(oldRow, static_cast<int>(channels.size()) - 1));
        }
        emitAllDocumentSignals();
        emit showMessage("已删除通道");
    } catch (const std::exception &e) {
        emit showMessage(QString("删除通道失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::updateSelectedChannel(const QString &channelId,
                                           const QString &deviceType,
                                           const QString &measurand,
                                           double scale,
                                           double azimuth,
                                           double x,
                                           double y,
                                           double z) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!hasSelectedChannel()) {
        emit showMessage("请先选择一个通道", true);
        return;
    }

    const QString trimmedId = channelId.trimmed();
    if (trimmedId.isEmpty()) {
        emit showMessage("ChannelID 不能为空", true);
        return;
    }
    if (deviceType.trimmed().isEmpty()) {
        emit showMessage("DeviceType 不能为空", true);
        return;
    }
    if (measurand.trimmed().isEmpty()) {
        emit showMessage("Measurand 不能为空", true);
        return;
    }
    if (!std::isfinite(scale) || scale == 0.0 || !std::isfinite(azimuth)
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        emit showMessage("通道数值字段必须是有效数字，Scale 不能为 0", true);
        return;
    }
    if (!(std::abs(azimuth + 1.0) < 1e-9
          || (azimuth >= 0.0 && azimuth < 360.0))) {
        emit showMessage("Azimuth 必须为 -1 或 [0, 360) 范围内的角度", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        auto &channels = metadata.InstrumentInfo.Channels;
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
            if (i == m_selectedChannelRow) {
                continue;
            }
            if (QString::fromStdString(
                    channels[static_cast<size_t>(i)].ChannelID)
                    == trimmedId
                && !isUnknownChannelId(trimmedId)) {
                emit showMessage("ChannelID 必须唯一", true);
                return;
            }
        }

        auto &channel = channels[static_cast<size_t>(m_selectedChannelRow)];
        channel.ChannelID = trimmedId.toStdString();
        channel.DeviceType = deviceType.trimmed().toStdString();
        channel.Measurand = measurand.trimmed().toStdString();
        channel.Scale = scale;
        channel.Azimuth = azimuth;
        channel.LocationXYZ = {x, y, z};
        renumberChannels(metadata);
        m_document.replaceMetadata(metadata);
        refreshChannelModel();
        setSelectedChannelRow(m_selectedChannelRow);
        emitAllDocumentSignals();
        emit showMessage("通道信息已更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新通道失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::setSelectedChannelUnknown() {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }
    if (!hasSelectedChannel()) {
        emit showMessage("请先选择一个通道", true);
        return;
    }

    try {
        qrest_data::Metadata metadata = m_document.metadata();
        auto &channel =
            metadata.InstrumentInfo
                .Channels[static_cast<size_t>(m_selectedChannelRow)];
        channel.ChannelID = "UNKNOWN";
        renumberChannels(metadata);
        m_document.replaceMetadata(metadata);
        refreshChannelModel();
        setSelectedChannelRow(m_selectedChannelRow);
        emitAllDocumentSignals();
        emit showMessage("ChannelID 已设置为 UNKNOWN");
    } catch (const std::exception &e) {
        emit showMessage(QString("设置 UNKNOWN 失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::selectAllData() {
    if (!m_tableModel || !m_selectionModel || m_tableModel->rowCount() == 0)
        return;
    // 构建从左上角到右下角的矩形选区
    QItemSelection selection(
        m_tableModel->index(0, 0),
        m_tableModel->index(m_tableModel->rowCount() - 1,
                            m_tableModel->columnCount() - 1));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

void QrestViewModel::selectColumn(int col) {
    if (!m_tableModel || !m_selectionModel || m_tableModel->rowCount() == 0)
        return;
    if (col < 0 || col >= m_tableModel->columnCount())
        return;
    // 构建整列选区
    QItemSelection selection(
        m_tableModel->index(0, col),
        m_tableModel->index(m_tableModel->rowCount() - 1, col));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

void QrestViewModel::selectRow(int row) {
    if (!m_tableModel || !m_selectionModel || m_tableModel->columnCount() == 0)
        return;
    if (row < 0 || row >= m_tableModel->rowCount())
        return;
    // 构建整行选区
    QItemSelection selection(
        m_tableModel->index(row, 0),
        m_tableModel->index(row, m_tableModel->columnCount() - 1));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

// 在文件末尾添加以下实现代码
void QrestViewModel::updatePacketHeader(int sourceId,
                                        int sampleRate,
                                        int channelCount,
                                        int dataPointCount,
                                        int encoding,
                                        qlonglong timestamp) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    const auto &channels = m_document.metadata().InstrumentInfo.Channels;
    if (!m_document.dataPacket().get_data().empty()
        && (channelCount != m_document.dataPacket().get_channel_count()
            || channelCount != static_cast<int>(channels.size()))) {
        emit showMessage("已有数据包体后不允许通过包头重排通道，请保持 Packet "
                         "与 Channels 数量一致",
                         true);
        return;
    }

    // 1. 安全性验证：重塑(Reshape)矩阵时，数据总量不能改变
    size_t newTotal = static_cast<size_t>(channelCount) * dataPointCount;
    size_t currentTotal = m_document.dataPacket().get_data().size();

    if (newTotal != currentTotal) {
        // 如果数据量不匹配，拒绝修改并向 UI 报错
        emit showMessage(
            QString(
                "修改失败：通道数(%1) × 点数(%2) = %3，不等于当前数据总量 %4！")
                .arg(channelCount)
                .arg(dataPointCount)
                .arg(newTotal)
                .arg(currentTotal),
            true);
        return;
    }

    try {
        // 2. 利用现有的数据，重新构造包头
        qrest_data::DataPacket dataPacket =
            qrest_data::DataPacket(static_cast<uint16_t>(sourceId),
                                   static_cast<uint16_t>(channelCount),
                                   static_cast<uint16_t>(encoding),
                                   static_cast<uint16_t>(sampleRate),
                                   static_cast<uint32_t>(dataPointCount),
                                   static_cast<uint64_t>(timestamp),
                                   m_document.dataPacket().get_data());

        // 3. 刷新模型和界面视图
        qrest_data::Metadata metadata = m_document.metadata();
        metadata.InstrumentInfo.ChannelNum = channelCount;
        metadata.DataInfo.NPTS = dataPointCount;
        if (sampleRate > 0)
            metadata.DataInfo.DT = 1.0 / sampleRate;
        metadata.DataInfo.StartTime = QDateTime::fromMSecsSinceEpoch(timestamp)
                                          .toString(Qt::ISODateWithMs)
                                          .toStdString();

        m_document.replaceContent(metadata, dataPacket);
        m_tableModel->loadData(&m_document.dataPacket());
        emitAllDocumentSignals();
        emit showMessage("包头与元数据已同步更新");
    } catch (const std::exception &e) {
        emit showMessage(QString("更新包头异常: %1").arg(e.what()), true);
    }
}

// ================= 局部数据操作 =================

void QrestViewModel::importMetadata(const QString &fileUrl) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit showMessage(QString("无法读取 JSON: %1").arg(file.errorString()),
                         true);
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        emit showMessage("无效的 JSON 文件", true);
        return;
    }

    setMetadataJson(doc.object()); // 复用已有的更新逻辑
    emit showMessage("元数据导入成功");
}

void QrestViewModel::exportMetadata(const QString &fileUrl) {
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit showMessage(QString("导出失败: %1").arg(file.errorString()), true);
        return;
    }

    // 获取格式化(换行缩进)的 JSON 字符串写入文件
    QJsonDocument doc(metadataJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit showMessage("元数据成功导出为 JSON");
}

void QrestViewModel::importDataBody(const QString &fileUrl) {
    importDataBodyInternal(fileUrl, false);
}

void QrestViewModel::confirmImportDataBody(const QString &fileUrl) {
    importDataBodyInternal(fileUrl, true);
}

void QrestViewModel::importDataBodyInternal(const QString &fileUrl,
                                            bool acceptNptsChange) {
    if (!canModify()) {
        emit showMessage("当前文件为只读，请先点击 Edit 创建编辑副本", true);
        return;
    }

    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit showMessage("无法打开文本文件", true);
        return;
    }

    QTextStream in(&file);
    QList<QList<double>> matrix; // 临时存储：[行][列]
    int maxCols = 0;
    int lineNo = 0;

    // 1. 读取文本数据 (假设空格、制表符或逗号分隔)
    while (!in.atEnd()) {
        ++lineNo;
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // 分割字符串并转为 double
        QStringList parts =
            line.split(QRegularExpression("[\\s,\t]+"), Qt::SkipEmptyParts);
        QList<double> row;
        for (const QString &val : parts) {
            bool ok = false;
            const double parsed = val.toDouble(&ok);
            if (!ok || !std::isfinite(parsed)) {
                emit showMessage(QString("第 %1 行包含无法解析的数据: %2")
                                     .arg(lineNo)
                                     .arg(val),
                                 true);
                return;
            }
            row.append(parsed);
        }

        if (maxCols == 0)
            maxCols = row.size();
        if (row.size() != maxCols) {
            emit showMessage(
                QString("第 %1 行列数为 %2，与首个数据行列数 %3 不一致")
                    .arg(lineNo)
                    .arg(row.size())
                    .arg(maxCols),
                true);
            return;
        }
        matrix.append(row);
    }
    file.close();

    if (matrix.isEmpty()) {
        emit showMessage("文件内容为空", true);
        return;
    }

    // 2. 转换数据排布 (从 [行][列] 转为 qREST 要求的 [列][行])
    int rows = matrix.size();
    int cols = maxCols;
    const int configuredNpts = m_document.metadata().DataInfo.NPTS;
    if (!acceptNptsChange && configuredNpts > 0 && configuredNpts != rows) {
        emit confirmDataImportNptsMismatch(fileUrl, configuredNpts, rows, cols);
        return;
    }

    std::vector<double> flattenedData;
    flattenedData.reserve(rows * cols);

    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            flattenedData.push_back(matrix[r][c]);
        }
    }

    // 3. 更新 DataPacket 对象
    try {
        // 使用现有属性，仅更新通道数、采样点数和具体数据
        qrest_data::DataPacket dataPacket =
            qrest_data::DataPacket(m_document.dataPacket().get_source_id(),
                                   static_cast<uint16_t>(cols),
                                   m_document.dataPacket().get_data_encodings(),
                                   m_document.dataPacket().get_sampling_rate(),
                                   static_cast<uint32_t>(rows),
                                   m_document.dataPacket().get_timestamp(),
                                   flattenedData);

        // 刷新模型和文件头
        qrest_data::Metadata metadata = m_document.metadata();
        if (metadata.InstrumentInfo.Channels.empty()) {
            metadata.InstrumentInfo.Channels.reserve(static_cast<size_t>(cols));
            for (int i = 0; i < cols; ++i) {
                metadata.InstrumentInfo.Channels.push_back(
                    makeDefaultChannel(i + 1));
            }
        } else if (static_cast<int>(metadata.InstrumentInfo.Channels.size())
                   != cols) {
            emit showMessage(QString("导入矩阵包含 %1 列，但当前 Channels "
                                     "配置为 %2 个。请先调整通道数量。")
                                 .arg(cols)
                                 .arg(metadata.InstrumentInfo.Channels.size()),
                             true);
            return;
        }
        renumberChannels(metadata);
        metadata.DataInfo.NPTS = rows;
        m_document.replaceContent(metadata, dataPacket);
        m_tableModel->loadData(&m_document.dataPacket());

        emitAllDocumentSignals();
        emit showMessage(
            QString("数据包体导入成功: %1 行, %2 通道").arg(rows).arg(cols));
    } catch (const std::exception &e) {
        emit showMessage(QString("导入失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::exportDataBody(const QString &fileUrl) {
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit showMessage("导出失败，文件无法写入", true);
        return;
    }

    QTextStream out(&file);
    int rows = m_document.dataPacket().get_data_point_count();
    int cols = m_document.dataPacket().get_channel_count();
    const auto &data = m_document.dataPacket().get_data();

    // 按照矩阵形式写出：每行一个采样点，各列为不同通道
    for (int r = 0; r < rows; ++r) {
        QStringList rowStrings;
        for (int c = 0; c < cols; ++c) {
            // 计算索引：第 c 个通道块的第 r 个元素
            size_t idx = static_cast<size_t>(c) * rows + r;
            rowStrings.append(QString::number(data[idx], 'g', 10));
        }
        out << rowStrings.join("\t") << "\n";
    }

    file.close();
    emit showMessage("数据包体已成功导出为文本 (矩阵格式)");
}

void QrestViewModel::emitAllDocumentSignals() {
    refreshChannelModel();
    if (m_binaryModel) {
        m_binaryModel->loadBytes(m_document.rawFileBytes());
    }
    rebuildValidationReport();
    emit headerUpdated();
    emit metadataUpdated();
    emit packetUpdated();
    emit fileLoaded();
    emit documentUpdated();
    emit channelsUpdated();
    emit geometryUpdated();
}

QList<ValidationTableModel::Issue>
QrestViewModel::collectValidationIssues(bool finalValidation) const {
    QList<ValidationTableModel::Issue> issues;
    auto addIssue = [&issues](ValidationTableModel::Severity severity,
                              const QString &area,
                              const QString &message) {
        issues.append({severity, area, message});
    };

    const qrest_data::Metadata &metadata = m_document.metadata();
    const qrest_data::DataPacket &packet = m_document.dataPacket();
    const qrest_data::tools::ValidationOptions options{
        finalValidation ? qrest_data::tools::ValidationMode::Final
                        : qrest_data::tools::ValidationMode::Draft};
    const auto coreReport =
        qrest_data::tools::validate_qrest_content(metadata,
                                                  packet.get_channel_count(),
                                                  packet.get_sampling_rate(),
                                                  packet.get_data_point_count(),
                                                  packet.get_timestamp(),
                                                  packet.get_data().size(),
                                                  options);

    for (const std::string &error : coreReport.errors) {
        addIssue(ValidationTableModel::Severity::Error,
                 "Core",
                 QString::fromStdString(error));
    }
    for (const std::string &warning : coreReport.warnings) {
        addIssue(ValidationTableModel::Severity::Warning,
                 "Core",
                 QString::fromStdString(warning));
    }

    qrest_data::Metadata expectedMetadata = metadata;
    updateBoundingBox(expectedMetadata);
    const auto &footprint = metadata.BuildingInfo.StructuralFootprint;
    const auto &bbox = footprint.BoundingBox;
    const auto &expectedBox =
        expectedMetadata.BuildingInfo.StructuralFootprint.BoundingBox;
    const auto near = [](double lhs, double rhs) {
        return std::abs(lhs - rhs) < 1e-9;
    };
    if (std::isfinite(expectedBox.MinX) && std::isfinite(expectedBox.MaxX)
        && (!near(bbox.MinX, expectedBox.MinX)
            || !near(bbox.MaxX, expectedBox.MaxX)
            || !near(bbox.MinY, expectedBox.MinY)
            || !near(bbox.MaxY, expectedBox.MaxY))) {
        addIssue(ValidationTableModel::Severity::Warning,
                 "Building",
                 "BoundingBox 与 Shape/Parameters 的计算结果不一致");
    }

    const auto &elevations = metadata.BuildingInfo.Elevation;
    const auto &channels = metadata.InstrumentInfo.Channels;
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        const auto &channel = channels[static_cast<size_t>(i)];
        const QString label =
            QString("Channel %1")
                .arg(channel.ChannelNo > 0 ? channel.ChannelNo : i + 1);

        if (std::isfinite(bbox.MinX) && std::isfinite(bbox.MaxX)
            && std::isfinite(bbox.MinY) && std::isfinite(bbox.MaxY)
            && (channel.LocationXYZ[0] < bbox.MinX - 1e-9
                || channel.LocationXYZ[0] > bbox.MaxX + 1e-9
                || channel.LocationXYZ[1] < bbox.MinY - 1e-9
                || channel.LocationXYZ[1] > bbox.MaxY + 1e-9)) {
            addIssue(ValidationTableModel::Severity::Warning,
                     "Geometry",
                     QString("%1 的位置超出 BoundingBox").arg(label));
        }

        if (!elevations.empty()
            && (channel.LocationXYZ[2] < elevations.front() - 1e-9
                || channel.LocationXYZ[2] > elevations.back() + 1e-9)) {
            addIssue(ValidationTableModel::Severity::Warning,
                     "Geometry",
                     QString("%1 的 Z 超出 Elevation 范围").arg(label));
        }
    }

    return issues;
}

void QrestViewModel::rebuildValidationReport(bool finalValidation) {
    if (!m_validationModel) {
        return;
    }

    QList<ValidationTableModel::Issue> issues =
        collectValidationIssues(finalValidation);
    if (issues.empty()) {
        issues.append({ValidationTableModel::Severity::Info,
                       "Document",
                       "Validation passed"});
    }
    m_validationModel->setIssues(std::move(issues));
    emit validationUpdated();
}

void QrestViewModel::refreshChannelModel() {
    if (!m_channelModel) {
        return;
    }

    m_channelModel->loadMetadata(&m_document.metadata());
    const int count = m_channelModel->rowCount();
    if (m_selectedChannelRow >= count) {
        m_selectedChannelRow = count - 1;
        emit selectedChannelUpdated();
        emit geometryUpdated();
    }

    if (!m_channelSelectionModel) {
        return;
    }

    m_channelSelectionModel->clearSelection();
    if (m_selectedChannelRow >= 0 && count > 0) {
        const QModelIndex index =
            m_channelModel->index(m_selectedChannelRow, 0);
        m_channelSelectionModel->select(index,
                                        QItemSelectionModel::ClearAndSelect
                                            | QItemSelectionModel::Rows);
    }
}

void QrestViewModel::setSelectedChannelRow(int row) {
    const int count =
        static_cast<int>(m_document.metadata().InstrumentInfo.Channels.size());
    if (row < 0 || row >= count) {
        row = -1;
    }

    if (m_selectedChannelRow == row) {
        refreshChannelModel();
        emit selectedChannelUpdated();
        emit geometryUpdated();
        return;
    }

    m_selectedChannelRow = row;
    refreshChannelModel();
    emit selectedChannelUpdated();
    emit geometryUpdated();
}
