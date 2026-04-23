#ifndef QREST_VIEW_MODEL_H
#define QREST_VIEW_MODEL_H

#include <QAbstractTableModel> // 新增：用于高性能表格显示
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtQml>


#ifndef Q_MOC_RUN
#include "data_struct/data_packet.hpp"
#include "data_struct/file_header.hpp"
#include "data_struct/metadata.hpp"
#endif

// =========================================================
// 新增：专为 QML TableView 设计的高性能数据模型
// =========================================================
class DataTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DataTableModel(QObject *parent = nullptr);

    // 必须重写的三个核心函数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    // 用于提供表头名称 (如 Channel 1, Channel 2)
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // 自定义方法：加载数据包指针并刷新表格
#ifndef Q_MOC_RUN
    void loadData(const qrest_data::DataPacket *packet);
#endif
    void clear();

private:
#ifndef Q_MOC_RUN
    const qrest_data::DataPacket *m_packet = nullptr;
#endif
};


// =========================================================
// 主控制器
// =========================================================
class QrestViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString headerMagic READ headerMagic NOTIFY headerUpdated)
    Q_PROPERTY(int metadataSize READ metadataSize NOTIFY headerUpdated)
    Q_PROPERTY(int dataSize READ dataSize NOTIFY headerUpdated)
    Q_PROPERTY(QJsonObject metadataJson READ metadataJson WRITE setMetadataJson
                   NOTIFY metadataUpdated)

    // 数据包头部信息属性
    Q_PROPERTY(int packetSourceId READ packetSourceId NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetChannelCount READ packetChannelCount NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetSamplingRate READ packetSamplingRate NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetDataPointCount READ packetDataPointCount NOTIFY packetUpdated)
    Q_PROPERTY(QString headerHex READ headerHex NOTIFY headerUpdated)
    // 暴露给 QML 表格使用的模型
    Q_PROPERTY(QAbstractTableModel *tableModel READ tableModel CONSTANT)
    // 全文件 Hex 预览属性
    Q_PROPERTY(QString fileHexPreview READ fileHexPreview NOTIFY fileLoaded)
    // 供 QML 表格使用的选择模型
    Q_PROPERTY(QItemSelectionModel *selectionModel READ selectionModel CONSTANT)
    Q_PROPERTY(
        int packetDataEncodings READ packetDataEncodings NOTIFY packetUpdated)
    Q_PROPERTY(
        qlonglong packetTimestamp READ packetTimestamp NOTIFY packetUpdated)

public:
    explicit QrestViewModel(QObject *parent = nullptr);

    QString headerMagic() const;
    int metadataSize() const;
    int dataSize() const;
    QJsonObject metadataJson() const;
    void setMetadataJson(const QJsonObject &json);
    QString headerHex() const;

    // 数据包 Getter
    QString fileHexPreview() const;
    QItemSelectionModel *selectionModel() const;
    int packetSourceId() const;
    int packetChannelCount() const;
    int packetSamplingRate() const;
    int packetDataPointCount() const;
    QAbstractTableModel *tableModel() const;
    int packetDataEncodings() const;
    qlonglong packetTimestamp() const;

    Q_INVOKABLE void newFile();
    Q_INVOKABLE void openFile(const QString &fileUrl);
    Q_INVOKABLE void saveFile(const QString &fileUrl);
    Q_INVOKABLE void importMetadata(const QString &fileUrl);
    Q_INVOKABLE void exportMetadata(const QString &fileUrl);
    Q_INVOKABLE void importDataBody(const QString &fileUrl);
    Q_INVOKABLE void exportDataBody(const QString &fileUrl);
    Q_INVOKABLE void copySelectedCells();
    Q_INVOKABLE void selectAllData();
    Q_INVOKABLE void selectColumn(int col);
    Q_INVOKABLE void selectRow(int row);
    Q_INVOKABLE void updatePacketHeader(int sourceId,
                                        int sampleRate,
                                        int channelCount,
                                        int dataPointCount,
                                        int encoding,
                                        qlonglong timestamp);

signals:
    void headerUpdated();
    void metadataUpdated();
    void packetUpdated(); // 数据包更新信号
    void fileLoaded();    // 新增：文件整体加载完成信号
    void showMessage(const QString &message, bool isError = false);

private:
    DataTableModel *m_tableModel;          // 表格模型实例
    QItemSelectionModel *m_selectionModel; // 新增：选择模型实例
    QByteArray m_rawFileBytes; // 新增：暂存读取到的文件字节，用于 Hex 预览

#ifndef Q_MOC_RUN
    qrest_data::FileHeader m_fileHeader;
    qrest_data::Metadata m_metadata;
    qrest_data::DataPacket m_dataPacket;
#endif
};

#endif // QREST_VIEW_MODEL_H