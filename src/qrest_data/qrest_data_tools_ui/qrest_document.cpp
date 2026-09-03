#include "qrest_document.h"

#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

QString toLocalPath(const QString &fileUrl) {
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = fileUrl;
    }
    return localPath;
}

constexpr size_t fileHeaderSize = 16;

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

qrest_data::Metadata makeDefaultMetadata() {
    qrest_data::Metadata metadata;
    metadata.Units = {"m", "s"};
    metadata.BuildingInfo.StructuralFootprint.Shape = "Rectangular";
    metadata.BuildingInfo.StructuralFootprint.Parameters.Length = 1.0;
    metadata.BuildingInfo.StructuralFootprint.Parameters.Width = 1.0;
    metadata.BuildingInfo.Elevation = {0.0};
    metadata.BuildingInfo.ElevationNum = 1;
    metadata.DataInfo.DT = 0.01;
    metadata.DataInfo.Frequency = 100.0;
    metadata.DataInfo.Corrected = "NULL";
    updateBoundingBox(metadata);
    return metadata;
}

} // namespace

QrestDocument::QrestDocument() { newDocument(); }

void QrestDocument::newDocument() {
    m_mode = Mode::NewDraft;
    m_sourcePath.clear();
    m_dirty = false;
    m_originalBytes.clear();

    m_metadata = makeDefaultMetadata();
    m_dataPacket = qrest_data::DataPacket();
    m_fileHeader = qrest_data::FileHeader();
    synchronizeHeader();
    refreshRawBytes();
}

void QrestDocument::openFile(const QString &fileUrl) {
    const QString localPath = toLocalPath(fileUrl);
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            QString("无法打开文件: %1").arg(file.errorString()).toStdString());
    }

    const QByteArray rawBytes = file.readAll();
    const std::string bytes = rawBytes.toStdString();

    qrest_data::FileHeader fileHeader =
        qrest_data::FileHeader::from_bytes(bytes);

    const size_t metadataSize = fileHeader.get_metadata_size();
    if (bytes.size() < fileHeaderSize + metadataSize) {
        throw std::runtime_error("Metadata section exceeds file size");
    }

    const size_t packetOffset = fileHeaderSize + metadataSize;
    const size_t dataSize = fileHeader.get_data_size();
    if (bytes.size() < packetOffset + dataSize) {
        throw std::runtime_error("Data packet section exceeds file size");
    }

    qrest_data::Metadata metadata = qrest_data::Metadata::from_bytes(
        std::string_view(bytes.data() + fileHeaderSize, metadataSize));
    qrest_data::DataPacket dataPacket = qrest_data::DataPacket::from_bytes(
        bytes.substr(packetOffset, dataSize));

    m_mode = Mode::View;
    m_sourcePath = localPath;
    m_dirty = false;
    m_fileHeader = fileHeader;
    m_metadata = metadata;
    m_dataPacket = dataPacket;
    m_rawFileBytes = rawBytes;
    m_originalBytes = rawBytes;
}

void QrestDocument::beginEdit() {
    if (m_mode == Mode::View) {
        m_mode = Mode::EditDraft;
        m_dirty = false;
        refreshRawBytes();
    }
}

void QrestDocument::saveAs(const QString &fileUrl) {
    QStringList errors;
    if (!validate(&errors)) {
        throw std::runtime_error(
            QString("保存前校验失败: %1").arg(errors.join("; ")).toStdString());
    }

    const QString localPath = toLocalPath(fileUrl);
    const QByteArray bytes = serialize();

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly)) {
        throw std::runtime_error(
            QString("保存失败: %1").arg(file.errorString()).toStdString());
    }
    if (file.write(bytes) != bytes.size()) {
        throw std::runtime_error("保存失败: 文件写入不完整");
    }

    m_sourcePath = localPath;
    m_rawFileBytes = bytes;
    m_originalBytes = bytes;
    setDirty(false);
}

QrestDocument::Mode QrestDocument::mode() const noexcept { return m_mode; }

bool QrestDocument::isDirty() const noexcept { return m_dirty; }

bool QrestDocument::canModify() const noexcept {
    return m_mode == Mode::EditDraft || m_mode == Mode::NewDraft;
}

QString QrestDocument::sourcePath() const { return m_sourcePath; }

QString QrestDocument::displayName() const {
    if (m_sourcePath.isEmpty()) {
        return "Untitled.qrest";
    }
    return QFileInfo(m_sourcePath).fileName();
}

QByteArray QrestDocument::rawFileBytes() const { return m_rawFileBytes; }

#ifndef Q_MOC_RUN
const qrest_data::FileHeader &QrestDocument::fileHeader() const noexcept {
    return m_fileHeader;
}

const qrest_data::Metadata &QrestDocument::metadata() const noexcept {
    return m_metadata;
}

const qrest_data::DataPacket &QrestDocument::dataPacket() const noexcept {
    return m_dataPacket;
}

void QrestDocument::replaceMetadata(const qrest_data::Metadata &metadata) {
    if (!canModify()) {
        throw std::runtime_error("当前文件为只读，请先点击 Edit 创建编辑副本");
    }

    m_metadata = metadata;
    synchronizeHeader();
    refreshRawBytes();
    setDirty(true);
}

void QrestDocument::replaceDataPacket(const qrest_data::DataPacket &packet) {
    if (!canModify()) {
        throw std::runtime_error("当前文件为只读，请先点击 Edit 创建编辑副本");
    }

    m_dataPacket = packet;
    synchronizeHeader();
    refreshRawBytes();
    setDirty(true);
}

void QrestDocument::replaceContent(const qrest_data::Metadata &metadata,
                                   const qrest_data::DataPacket &packet) {
    if (!canModify()) {
        throw std::runtime_error("当前文件为只读，请先点击 Edit 创建编辑副本");
    }

    m_metadata = metadata;
    m_dataPacket = packet;
    synchronizeHeader();
    refreshRawBytes();
    setDirty(true);
}

bool QrestDocument::validate(QStringList *errors) const {
    QStringList localErrors;

    try {
        const std::string metadataBytes = m_metadata.to_bytes();
        (void)qrest_data::Metadata::from_bytes(metadataBytes);

        const std::string packetBytes = m_dataPacket.to_bytes();
        (void)qrest_data::DataPacket::from_bytes(packetBytes);

        qrest_data::FileHeader expectedHeader;
        expectedHeader.set_metadata_size(
            static_cast<uint32_t>(metadataBytes.size()));
        expectedHeader.set_data_size(static_cast<uint32_t>(packetBytes.size()));

        if (m_fileHeader.get_metadata_size()
            != expectedHeader.get_metadata_size()) {
            localErrors.append(
                "FileHeader.MetadataSize 与当前 Metadata 不一致");
        }
        if (m_fileHeader.get_data_size() != expectedHeader.get_data_size()) {
            localErrors.append("FileHeader.DataSize 与当前 DataPacket 不一致");
        }

        const auto channelCount = m_dataPacket.get_channel_count();
        const auto pointCount = m_dataPacket.get_data_point_count();
        const size_t expectedValues =
            static_cast<size_t>(channelCount) * pointCount;
        if (m_dataPacket.get_data().size() != expectedValues) {
            localErrors.append("Packet 数据长度与通道数/采样点数不一致");
        }
    } catch (const std::exception &e) {
        localErrors.append(QString::fromUtf8(e.what()));
    }

    if (errors) {
        *errors = localErrors;
    }
    return localErrors.isEmpty();
}
#endif

void QrestDocument::setDirty(bool dirty) noexcept { m_dirty = dirty; }

void QrestDocument::synchronizeHeader() {
    const std::string metadataBytes = m_metadata.to_bytes();
    const std::string packetBytes = m_dataPacket.to_bytes();

    if (metadataBytes.size() > std::numeric_limits<uint32_t>::max()
        || packetBytes.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error("qREST document section is larger than 4GB");
    }

    m_fileHeader.set_metadata_size(static_cast<uint32_t>(metadataBytes.size()));
    m_fileHeader.set_data_size(static_cast<uint32_t>(packetBytes.size()));
}

void QrestDocument::refreshRawBytes() { m_rawFileBytes = serialize(); }

QByteArray QrestDocument::serialize() const {
    const std::string bytes = m_fileHeader.to_bytes() + m_metadata.to_bytes()
                              + m_dataPacket.to_bytes();
    return QByteArray::fromStdString(bytes);
}
