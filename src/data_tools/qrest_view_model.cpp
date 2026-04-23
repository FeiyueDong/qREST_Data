#include "qrest_view_model.h"
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QUrl>

// ================= DataTableModel 实现 =================

DataTableModel::DataTableModel(QObject *parent) : QAbstractTableModel(parent) {}

#ifndef Q_MOC_RUN
void DataTableModel::loadData(const qrest_data::DataPacket *packet)
{
    beginResetModel(); // 告诉 QML 准备彻底刷新表格
    m_packet = packet;
    endResetModel(); // 刷新完成
}
#endif

void DataTableModel::clear()
{
    beginResetModel();
    m_packet = nullptr;
    endResetModel();
}

int DataTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_packet)
        return 0;
    return m_packet->get_data_point_count(); // 行数为采样点数 (NPTS)
}

int DataTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_packet)
        return 0;
    return m_packet->get_channel_count(); // 列数为通道数
}

QVariant DataTableModel::data(const QModelIndex &index, int role) const
{
    if (!m_packet || !index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    int row = index.row();
    int col = index.column();
    size_t data_index =
        static_cast<size_t>(col) * m_packet->get_data_point_count() + row;

    const auto &raw_data = m_packet->get_data();
    if (data_index < raw_data.size())
    {
        // 使用 'g' 格式，自动处理科学计数法，保持 8 位有效数字
        return QString::number(raw_data[data_index], 'g', 8);
    }
    return QVariant();
}

QVariant DataTableModel::headerData(int section,
                                    Qt::Orientation orientation,
                                    int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal)
    {
        return QString("CH %1").arg(section + 1);
    }
    else
    {
        // 垂直表头：计算时间 (row + 1) / fs
        if (m_packet && m_packet->get_sampling_rate() > 0)
        {
            double fs = static_cast<double>(m_packet->get_sampling_rate());
            double time = static_cast<double>(section + 1) / fs;
            // 保留 4 位小数，方便观察高采样率数据
            return QString::number(time, 'f', 4);
        }
        return QString::number(section + 1);
    }
}

// ================= QrestViewModel 实现 =================

QrestViewModel::QrestViewModel(QObject *parent) : QObject(parent)
{
    m_tableModel = new DataTableModel(this);
    // 实例化选择模型并绑定到我们的数据模型
    m_selectionModel = new QItemSelectionModel(m_tableModel, this);
    newFile();
}

QString QrestViewModel::headerMagic() const
{
    return QString::fromStdString(m_fileHeader.get_magic());
}
int QrestViewModel::metadataSize() const
{
    return m_fileHeader.get_metadata_size();
}
int QrestViewModel::dataSize() const { return m_fileHeader.get_data_size(); }
QJsonObject QrestViewModel::metadataJson() const
{
    std::string jsonStr = m_metadata.to_bytes();
    return QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr)).object();
}
void QrestViewModel::setMetadataJson(const QJsonObject &json)
{
    std::string jsonStr =
        QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    try
    {
        m_metadata = qrest_data::Metadata::from_bytes(jsonStr);
        emit metadataUpdated();
        m_fileHeader.set_metadata_size(static_cast<uint32_t>(jsonStr.size()));
        emit headerUpdated();
        emit showMessage("元数据已修改");
    }
    catch (const std::exception &e)
    {
        emit showMessage(QString("更新元数据失败: %1").arg(e.what()), true);
    }
}

QString QrestViewModel::headerHex() const
{
    std::string bytes = m_fileHeader.to_bytes();
    QString hexStr;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        // 格式化为: 71 52 45 53 ...
        hexStr +=
            QString("%1 ")
                .arg(static_cast<unsigned char>(bytes[i]), 2, 16, QChar('0'))
                .toUpper();
        if ((i + 1) % 8 == 0)
            hexStr += "  "; // 每 8 个字节加个大间距
    }
    return hexStr.trimmed();
}

// 数据包 Getter
QItemSelectionModel *QrestViewModel::selectionModel() const
{
    return m_selectionModel;
}

// 辅助函数：将一段 QByteArray 格式化为标准的 Hex 视图行
QString formatHexBlock(const QByteArray &block, int startOffset)
{
    QString result;
    const int bytesPerLine = 32; // 在这里调整每行显示的字节数

    for (int i = 0; i < block.size(); i += bytesPerLine)
    {
        // 1. 地址偏移 (8位十六进制)
        result +=
            QString("%1  ").arg(startOffset + i, 8, 16, QChar('0')).toUpper();

        QString hexPart;
        QString asciiPart;

        for (int j = 0; j < bytesPerLine; ++j)
        {
            if (i + j < block.size())
            {
                unsigned char b = static_cast<unsigned char>(block[i + j]);

                // 十六进制部分
                hexPart += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                // 在每行中间 (第16个字节后) 加一个额外空格作为视觉分隔
                if (j == 15)
                    hexPart += " ";

                // ASCII 字符部分
                asciiPart += (b >= 32 && b <= 126) ? QChar(b) : QChar('.');
            }
            else
            {
                // 补齐最后一行不足的空格，保持 ASCII 栏右侧对齐
                hexPart += "   ";
                if (j == 15)
                    hexPart += " ";
            }
        }
        result += hexPart + " |" + asciiPart + "|\n";
    }
    return result;
}

QString QrestViewModel::fileHexPreview() const
{
    if (m_rawFileBytes.isEmpty())
        return "文件未加载...";

    QString finalView;
    int totalSize = m_rawFileBytes.size();

    // --- 1. 格式化文件头 (Fixed 16 Bytes) ---
    finalView += ">>> [PART 0: FILE HEADER] (16 Bytes)\n";
    QByteArray headerBlock = m_rawFileBytes.mid(0, 16);
    finalView += formatHexBlock(headerBlock, 0);
    finalView += "\n";

    // --- 2. 格式化元数据 (Metadata) ---
    int metaSize = m_fileHeader.get_metadata_size();
    int metaOffset = 16;
    finalView += QString(">>> [PART 1: METADATA] (%1 Bytes)\n").arg(metaSize);

    if (totalSize >= metaOffset + metaSize)
    {
        // 如果元数据太长，可以只展示前 2KB，防止 UI 渲染压力过大
        // int displayMetaSize = qMin(metaSize, 2048);
        int displayMetaSize = metaSize;
        QByteArray metaBlock = m_rawFileBytes.mid(metaOffset, displayMetaSize);
        finalView += formatHexBlock(metaBlock, metaOffset);
        // if (metaSize > 2048)
        //     finalView += "... (中间数据已略过) ...\n";
    }
    else
    {
        finalView += "[Error] 元数据区域超出文件范围\n";
    }
    finalView += "\n";

    // --- 3. 格式化数据包头 (Packet Header, Fixed 32 Bytes) ---
    int packetHeaderOffset = metaOffset + metaSize;
    int packetHeaderSize = 32;
    finalView += ">>> [PART 2: DATA PACKET HEADER] (32 Bytes)\n";

    if (totalSize >= packetHeaderOffset + packetHeaderSize)
    {
        QByteArray pHeaderBlock =
            m_rawFileBytes.mid(packetHeaderOffset, packetHeaderSize);
        finalView += formatHexBlock(pHeaderBlock, packetHeaderOffset);
    }
    else
    {
        finalView += "[Error] 数据包头区域超出文件范围\n";
    }

    finalView += "\n--- [DATA BODY OMITTED] ---";

    return finalView;
}

int QrestViewModel::packetSourceId() const
{
    return m_dataPacket.get_source_id();
}
int QrestViewModel::packetChannelCount() const
{
    return m_dataPacket.get_channel_count();
}
int QrestViewModel::packetSamplingRate() const
{
    return m_dataPacket.get_sampling_rate();
}
int QrestViewModel::packetDataPointCount() const
{
    return m_dataPacket.get_data_point_count();
}
QAbstractTableModel *QrestViewModel::tableModel() const { return m_tableModel; }
int QrestViewModel::packetDataEncodings() const
{
    return m_dataPacket.get_data_encodings();
}
qlonglong QrestViewModel::packetTimestamp() const
{
    return static_cast<qlonglong>(m_dataPacket.get_timestamp());
}

void QrestViewModel::newFile()
{
    m_metadata = qrest_data::Metadata();
    m_dataPacket = qrest_data::DataPacket();
    m_fileHeader = qrest_data::FileHeader();

    m_fileHeader.set_metadata_size(
        static_cast<uint32_t>(m_metadata.to_bytes().size()));
    m_fileHeader.set_data_size(m_dataPacket.get_packet_size());

    std::string emptyFile = m_fileHeader.to_bytes() + m_metadata.to_bytes()
                            + m_dataPacket.to_bytes();
    m_rawFileBytes = QByteArray::fromStdString(emptyFile);

    m_tableModel->clear(); // 清空表格

    emit headerUpdated();
    emit metadataUpdated();
    emit packetUpdated();
    emit fileLoaded();
    emit showMessage("已创建新文件");
}

void QrestViewModel::openFile(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit showMessage(QString("无法打开文件: %1").arg(file.errorString()),
                         true);
        return;
    }

    m_rawFileBytes = file.readAll(); // 重点：将完整文件保存在内存中
    std::string bytes = m_rawFileBytes.toStdString();
    QByteArray fileData = file.readAll();
    file.close();

    try
    {
        // 1. 文件头
        m_fileHeader = qrest_data::FileHeader::from_bytes(bytes);
        emit headerUpdated();

        // 2. 元数据
        size_t headerSize = 16;
        size_t metaSize = m_fileHeader.get_metadata_size();
        if (bytes.size() >= headerSize + metaSize)
        {
            std::string metaStr = bytes.substr(headerSize, metaSize);
            m_metadata = qrest_data::Metadata::from_bytes(metaStr);
            emit metadataUpdated();
        }

        // 3. 数据包
        size_t packetOffset = headerSize + metaSize;
        if (bytes.size() >= packetOffset + m_fileHeader.get_data_size())
        {
            std::string packetStr =
                bytes.substr(packetOffset, m_fileHeader.get_data_size());
            // 解析数据包并灌入表格模型
            m_dataPacket = qrest_data::DataPacket::from_bytes(packetStr);
            m_tableModel->loadData(&m_dataPacket);
            emit packetUpdated();
        }

        emit fileLoaded(); // 通知 Hex 视图刷新
        emit showMessage(
            QString("成功打开文件: %1").arg(QFileInfo(localPath).fileName()));
    }
    catch (const std::exception &e)
    {
        emit showMessage(QString("文件解析异常: %1").arg(e.what()), true);
    }
}

void QrestViewModel::saveFile(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    // 1. 获取三个部分的序列化字节流
    std::string metaBytes = m_metadata.to_bytes();
    std::string dataBytes = m_dataPacket.to_bytes();

    // 2. 确保文件头的大小信息是最新的
    m_fileHeader.set_metadata_size(static_cast<uint32_t>(metaBytes.size()));
    m_fileHeader.set_data_size(static_cast<uint32_t>(dataBytes.size()));
    std::string headerBytes = m_fileHeader.to_bytes();

    // 3. 依次写入文件
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        emit showMessage(QString("保存失败: %1").arg(file.errorString()), true);
        return;
    }

    file.write(headerBytes.data(), headerBytes.size());
    file.write(metaBytes.data(), metaBytes.size());
    file.write(dataBytes.data(), dataBytes.size());
    file.close();

    emit headerUpdated(); // 保存时如果大小有更新，刷新界面
    emit showMessage(
        QString("成功保存至: %1").arg(QFileInfo(localPath).fileName()));
}

void QrestViewModel::copySelectedCells()
{
    if (!m_selectionModel || !m_selectionModel->hasSelection())
    {
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

    for (int i = 0; i < indexes.size(); ++i)
    {
        const QModelIndex &idx = indexes[i];
        if (idx.row() != currentRow)
        {
            tsvText += "\n";
            currentRow = idx.row();
        }
        else if (i != 0)
        {
            tsvText += "\t";
        }
        // 这里获取的是 data()，即数值部分，不会包含 headerData 里的时间
        tsvText += m_tableModel->data(idx).toString();
    }

    QGuiApplication::clipboard()->setText(tsvText);
    emit showMessage(QString("已成功复制 %1 个数据点").arg(indexes.size()));
}
void QrestViewModel::selectAllData()
{
    if (!m_tableModel || !m_selectionModel || m_tableModel->rowCount() == 0)
        return;
    // 构建从左上角到右下角的矩形选区
    QItemSelection selection(
        m_tableModel->index(0, 0),
        m_tableModel->index(m_tableModel->rowCount() - 1,
                            m_tableModel->columnCount() - 1));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

void QrestViewModel::selectColumn(int col)
{
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

void QrestViewModel::selectRow(int row)
{
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
                                        qlonglong timestamp)
{
    // 1. 安全性验证：重塑(Reshape)矩阵时，数据总量不能改变
    size_t newTotal = static_cast<size_t>(channelCount) * dataPointCount;
    size_t currentTotal = m_dataPacket.get_data().size();

    if (newTotal != currentTotal)
    {
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

    try
    {
        // 2. 利用现有的数据，重新构造包头
        m_dataPacket =
            qrest_data::DataPacket(static_cast<uint16_t>(sourceId),
                                   static_cast<uint16_t>(channelCount),
                                   static_cast<uint16_t>(encoding),
                                   static_cast<uint16_t>(sampleRate),
                                   static_cast<uint32_t>(dataPointCount),
                                   static_cast<uint64_t>(timestamp),
                                   m_dataPacket.get_data());

        // 3. 刷新模型和界面视图
        m_metadata.InstrumentInfo.ChannelNum = channelCount;
        m_metadata.DataInfo.NPTS = dataPointCount;
        if (sampleRate > 0)
            m_metadata.DataInfo.DT = 1.0 / sampleRate;
        m_metadata.DataInfo.StartTime =
            QDateTime::fromMSecsSinceEpoch(timestamp)
                .toString(Qt::ISODateWithMs)
                .toStdString();

        // 4. 更新文件头信息并刷新 Hex 预览
        m_fileHeader.set_data_size(m_dataPacket.get_packet_size());
        m_fileHeader.set_metadata_size(
            static_cast<uint32_t>(m_metadata.to_bytes().size()));
        m_tableModel->loadData(&m_dataPacket);
        m_fileHeader.set_data_size(m_dataPacket.get_packet_size());

        // 重新构建全文件预览
        std::string fullBytes = m_fileHeader.to_bytes() + m_metadata.to_bytes()
                                + m_dataPacket.to_bytes();
        m_rawFileBytes = QByteArray::fromStdString(fullBytes);

        emit headerUpdated();
        emit metadataUpdated();
        emit packetUpdated();
        emit fileLoaded(); // 刷新 Hex 视图
        emit showMessage("包头与元数据已同步更新");
    }
    catch (const std::exception &e)
    {
        emit showMessage(QString("更新包头异常: %1").arg(e.what()), true);
    }
}

// ================= 局部数据操作 =================

void QrestViewModel::importMetadata(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit showMessage(QString("无法读取 JSON: %1").arg(file.errorString()),
                         true);
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject())
    {
        emit showMessage("无效的 JSON 文件", true);
        return;
    }

    setMetadataJson(doc.object()); // 复用已有的更新逻辑
    emit showMessage("元数据导入成功");
}

void QrestViewModel::exportMetadata(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit showMessage(QString("导出失败: %1").arg(file.errorString()), true);
        return;
    }

    // 获取格式化(换行缩进)的 JSON 字符串写入文件
    QJsonDocument doc(metadataJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit showMessage("元数据成功导出为 JSON");
}

void QrestViewModel::importDataBody(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit showMessage("无法打开文本文件", true);
        return;
    }

    QTextStream in(&file);
    QList<QList<double>> matrix; // 临时存储：[行][列]
    int maxCols = 0;

    // 1. 读取文本数据 (假设空格、制表符或逗号分隔)
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // 分割字符串并转为 double
        QStringList parts =
            line.split(QRegularExpression("[\\s,\t]+"), Qt::SkipEmptyParts);
        QList<double> row;
        for (const QString &val : parts)
            row.append(val.toDouble());

        if (maxCols == 0)
            maxCols = row.size();
        matrix.append(row);
    }
    file.close();

    if (matrix.isEmpty())
    {
        emit showMessage("文件内容为空", true);
        return;
    }

    // 2. 转换数据排布 (从 [行][列] 转为 qREST 要求的 [列][行])
    int rows = matrix.size();
    int cols = maxCols;
    std::vector<double> flattenedData;
    flattenedData.reserve(rows * cols);

    for (int c = 0; c < cols; ++c)
    {
        for (int r = 0; r < rows; ++r)
        {
            flattenedData.push_back(matrix[r][c]);
        }
    }

    // 3. 更新 DataPacket 对象
    try
    {
        // 使用现有属性，仅更新通道数、采样点数和具体数据
        m_dataPacket = qrest_data::DataPacket(m_dataPacket.get_source_id(),
                                              static_cast<uint16_t>(cols),
                                              m_dataPacket.get_data_encodings(),
                                              m_dataPacket.get_sampling_rate(),
                                              static_cast<uint32_t>(rows),
                                              m_dataPacket.get_timestamp(),
                                              flattenedData);

        // 刷新模型和文件头
        m_tableModel->loadData(&m_dataPacket);
        m_fileHeader.set_data_size(m_dataPacket.get_packet_size());

        emit headerUpdated();
        emit packetUpdated();
        emit showMessage(
            QString("数据包体导入成功: %1 行, %2 通道").arg(rows).arg(cols));
    }
    catch (const std::exception &e)
    {
        emit showMessage(QString("导入失败: %1").arg(e.what()), true);
    }
}

void QrestViewModel::exportDataBody(const QString &fileUrl)
{
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit showMessage("导出失败，文件无法写入", true);
        return;
    }

    QTextStream out(&file);
    int rows = m_dataPacket.get_data_point_count();
    int cols = m_dataPacket.get_channel_count();
    const auto &data = m_dataPacket.get_data();

    // 按照矩阵形式写出：每行一个采样点，各列为不同通道
    for (int r = 0; r < rows; ++r)
    {
        QStringList rowStrings;
        for (int c = 0; c < cols; ++c)
        {
            // 计算索引：第 c 个通道块的第 r 个元素
            size_t idx = static_cast<size_t>(c) * rows + r;
            rowStrings.append(QString::number(data[idx], 'g', 10));
        }
        out << rowStrings.join("\t") << "\n";
    }

    file.close();
    emit showMessage("数据包体已成功导出为文本 (矩阵格式)");
}
